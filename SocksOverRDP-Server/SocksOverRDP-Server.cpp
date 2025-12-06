/*
 * MIT License
 *
 * Copyright(c) 2018 Balazs Bucsay
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files(the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions :
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <winsock2.h>
#include <Ws2tcpip.h>
#include <windows.h>
#include <wtsapi32.h>
#include <pchannel.h>
#include <crtdbg.h>
#include <stdio.h>
#include <strsafe.h>
#include <assert.h>
#include <math.h>
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "SocksOverRDP-Server.h"
#include "SocksServer.h"


#pragma comment(lib, "wtsapi32.lib")
#pragma comment(lib, "Ws2_32.lib")

#define SocksOverRDP_CHANNEL_NAME "SocksChannel"
#define DEBUG_PRINT_BUFFER_SIZE 1024
#define BUF_SIZE 4096

DWORD WINAPI HandleSocksClient(void* param);
DWORD WINAPI ChannelHandler(void* param);
DWORD OpenDynamicChannel(LPCSTR szChannelName, HANDLE* phFile);

struct arguments {
	WCHAR* ip;
	WCHAR* port;
	BYTE priority;
} running_args;

struct clientargs {
	SOCKET sockClient;
	DWORD dwThreadId;
	HANDLE hChannel;
};

struct socksthread {
	DWORD dwThreadId;
	SOCKET sockClient;
	HANDLE hThread;
	BOOL run;
	struct socksthread* next;
};

struct threads* ThreadHead = NULL;
struct socksthread* SocksThreadHead = NULL;

int	CTRLC = 0;
HANDLE ghMutex, ghLLMutex, ghSocksMutex;
HANDLE hChannel;
HANDLE hWTSHandle = NULL;
BOOL bVerbose = FALSE, bDebug = FALSE; // Debug disabled by default, only enabled with -v

VOID usage(WCHAR* cmdname)
{
	wprintf(L"Usage: %s [-v] [-p port]\n"
		"-h\t\tThis help\n"
		"-v\t\tVerbose Mode (shows debug output)\n"
		"-p port\t\tSocks server port (default: 1080)\n",
		cmdname);
}

BOOL parse_argv(INT argc, __in_ecount(argc) WCHAR** argv)
{
	int num = 0;

	while (num < argc - 1)
	{
		num++;

		if (wcsncmp(argv[num], L"-", 1))
		{
			wprintf(L"[-] Invalid argument: %s\n", argv[num]);
			usage(argv[0]);
			return FALSE;
		}

		switch (argv[num][1])
		{
		case 'h':
		case '?':
			usage(argv[0]);
			return FALSE;
		case 'v':
			bVerbose = TRUE;
			bDebug = TRUE; // Enable debug output when verbose is enabled
			break;
		case 'd':
			bDebug = TRUE;
			break;
		case 'p':
			if (num + 1 < argc)
			{
				num++;
				running_args.port = argv[num];
			}
			else
			{
				wprintf(L"[-] Port argument requires a value\n");
				usage(argv[0]);
				return FALSE;
			}
			break;

		default:
			wprintf(L"[-] Invalid argument: %s\n", argv[num]);
			usage(argv[0]);
			return FALSE;
		}
	}
	return TRUE;
}

// SOCKS thread management
struct socksthread* AddSocksThread(DWORD dwThreadId, SOCKET sockClient)
{
	struct socksthread* rolling;
	struct socksthread* ThreadStruct;

	DWORD dwWaitResult = WaitForSingleObject(ghSocksMutex, INFINITE);
	switch (dwWaitResult)
	{
	case WAIT_OBJECT_0:
		ThreadStruct = (struct socksthread*)malloc(sizeof(struct socksthread));
		ThreadStruct->dwThreadId = dwThreadId;
		ThreadStruct->sockClient = sockClient;
		ThreadStruct->hThread = NULL;
		ThreadStruct->run = TRUE;
		ThreadStruct->next = NULL;

		if (SocksThreadHead == NULL)
		{
			SocksThreadHead = ThreadStruct;
		}
		else
		{
			rolling = SocksThreadHead;
			while (rolling->next)
			{
				rolling = rolling->next;
			}
			rolling->next = ThreadStruct;
		}

		if (!ReleaseMutex(ghSocksMutex))
		{
			if (bVerbose) printf("AddSocksThread Release failed\n");
		}
		return ThreadStruct;
		break;

	case WAIT_ABANDONED:
		if (bVerbose) printf("AddSocksThread lock abandoned\n");
		return NULL;
	}

	return NULL;
}

struct socksthread* LookupSocksThread(DWORD dwThreadId)
{
	struct socksthread* rolling;

	DWORD dwWaitResult = WaitForSingleObject(ghSocksMutex, INFINITE);
	switch (dwWaitResult)
	{
	case WAIT_OBJECT_0:
		rolling = SocksThreadHead;
		while (rolling)
		{
			if (rolling->dwThreadId == dwThreadId)
			{
				if (!ReleaseMutex(ghSocksMutex))
				{
					if (bVerbose) printf("LookupSocksThread Release failed\n");
				}
				return rolling;
			}
			rolling = rolling->next;
		}

		if (!ReleaseMutex(ghSocksMutex))
		{
			if (bVerbose) printf("LookupSocksThread Release failed\n");
		}
		return NULL;
		break;
	case WAIT_ABANDONED:
		if (bVerbose) printf("LookupSocksThread lock abandoned\n");
		return NULL;
	}

	return NULL;
}

VOID DeleteSocksThread(DWORD dwThreadId)
{
	struct socksthread* rolling;
	struct socksthread* prev = NULL;

	DWORD dwWaitResult = WaitForSingleObject(ghSocksMutex, INFINITE);
	switch (dwWaitResult)
	{
	case WAIT_OBJECT_0:
		if (!SocksThreadHead)
		{
			if (!ReleaseMutex(ghSocksMutex))
			{
				if (bVerbose) printf("DeleteSocksThread: trying to delete empty list\n");
			}
			return;
		}

		rolling = SocksThreadHead;

		while (rolling && rolling->dwThreadId != dwThreadId)
		{
			prev = rolling;
			rolling = rolling->next;
		}

		if (rolling)
		{
			if (prev)
			{
				prev->next = rolling->next;
			}
			else
			{
				SocksThreadHead = rolling->next;
			}

			rolling->run = FALSE;
			if (rolling->sockClient != INVALID_SOCKET)
			{
				closesocket(rolling->sockClient);
			}
			free(rolling);
		}

		if (!ReleaseMutex(ghSocksMutex))
		{
			if (bVerbose) printf("DeleteSocksThread Release failed\n");
		}
		return;
		break;

	case WAIT_ABANDONED:
		if (bVerbose) printf("DeleteSocksThread lock abandoned\n");
		return;
	}
}

// Add missing functions that SocksServer.cpp expects
struct threads* AddThread(DWORD dwThreadId, DWORD dwRemoteThreadId, HANDLE hSlot_r, HANDLE hSlot_w)
{
	struct threads* rolling;
	struct threads* ThreadStruct;

	DWORD dwWaitResult = WaitForSingleObject(ghLLMutex, INFINITE);
	switch (dwWaitResult)
	{
	case WAIT_OBJECT_0:
		ThreadStruct = (threads*)malloc(sizeof(struct threads));
		ThreadStruct->dwThreadId = dwThreadId;
		ThreadStruct->dwRemoteThreadId = dwRemoteThreadId;
		ThreadStruct->hThread = NULL;
		ThreadStruct->run = TRUE;
		ThreadStruct->hSlot_r = hSlot_r;
		ThreadStruct->hSlot_w = hSlot_w;
		ThreadStruct->hSlot_event = NULL;
		ThreadStruct->next = NULL;

		if (ThreadHead == NULL)
		{
			ThreadHead = ThreadStruct;
		}
		else
		{
			rolling = ThreadHead;
			while (rolling->next)
			{
				rolling = rolling->next;
			}
			rolling->next = ThreadStruct;
		}

		if (!ReleaseMutex(ghLLMutex))
		{
			if (bVerbose) printf("AddThread Release failed\n");
		}
		return ThreadStruct;
		break;

	case WAIT_ABANDONED:
		if (bVerbose) printf("AddThread lock abandoned\n");
		return NULL;
	}

	return NULL;
}

struct threads* LookupThread(DWORD dwThreadId)
{
	struct threads* rolling;

	DWORD dwWaitResult = WaitForSingleObject(ghLLMutex, INFINITE);
	switch (dwWaitResult)
	{
	case WAIT_OBJECT_0:
		rolling = ThreadHead;
		while (rolling)
		{
			if (rolling->dwThreadId == dwThreadId)
			{
				if (!ReleaseMutex(ghLLMutex))
				{
					if (bVerbose) printf("LookupThread Release failed\n");
				}
				return rolling;
			}
			rolling = rolling->next;
		}

		if (!ReleaseMutex(ghLLMutex))
		{
			if (bVerbose) printf("LookupThread Release failed\n");
		}
		return NULL;
		break;
	case WAIT_ABANDONED:
		if (bVerbose) printf("LookupThread lock abandoned\n");
		return NULL;
	}

	return NULL;
}

struct threads* LookupThreadRemote(DWORD dwRemoteThreadId)
{
	struct threads* rolling;

	DWORD dwWaitResult = WaitForSingleObject(ghLLMutex, INFINITE);
	switch (dwWaitResult)
	{
	case WAIT_OBJECT_0:
		rolling = ThreadHead;
		while (rolling)
		{
			if (rolling->dwRemoteThreadId == dwRemoteThreadId)
			{
				if (!ReleaseMutex(ghLLMutex))
				{
					if (bVerbose) printf("LookupThreadRemote Release failed\n");
				}
				return rolling;
			}
			rolling = rolling->next;
		}

		if (!ReleaseMutex(ghLLMutex))
		{
			if (bVerbose) printf("LookupThreadRemote Release failed\n");
		}
		return NULL;
		break;
	case WAIT_ABANDONED:
		if (bVerbose) printf("LookupThreadRemote lock abandoned\n");
		return NULL;
	}
	return NULL;
}

VOID DeleteThread(DWORD dwThreadId)
{
	struct threads* rolling;
	struct threads* prev = NULL;

	DWORD dwWaitResult = WaitForSingleObject(ghLLMutex, INFINITE);
	switch (dwWaitResult)
	{
	case WAIT_OBJECT_0:
		if (!ThreadHead)
		{
			if (!ReleaseMutex(ghLLMutex))
			{
				if (bVerbose) printf("DeleteThread: trying to delete empty list\n");
			}
			return;
		}

		rolling = ThreadHead;

		while (rolling && rolling->dwThreadId != dwThreadId)
		{
			prev = rolling;
			rolling = rolling->next;
		}

		if (rolling)
		{
			if (prev)
			{
				prev->next = rolling->next;
			}
			else
			{
				ThreadHead = rolling->next;
			}

			rolling->run = FALSE;
			if (rolling->hSlot_r && rolling->hSlot_r != INVALID_HANDLE_VALUE)
			{
				CloseHandle(rolling->hSlot_r);
			}
			if (rolling->hSlot_w && rolling->hSlot_w != INVALID_HANDLE_VALUE)
			{
				CloseHandle(rolling->hSlot_w);
			}
			if (rolling->hSlot_event && rolling->hSlot_event != INVALID_HANDLE_VALUE)
			{
				CloseHandle(rolling->hSlot_event);
			}
			free(rolling);
		}

		if (!ReleaseMutex(ghLLMutex))
		{
			if (bVerbose) printf("DeleteThread Release failed\n");
		}
		return;
		break;

	case WAIT_ABANDONED:
		if (bVerbose) printf("DeleteThread lock abandoned\n");
		return;
	}
}

// Write data to the RDP channel
BOOL WriteChannelToRDP(char* Buffer, DWORD nBytesToWrite, DWORD* nBytesWritten, DWORD dwRemoteThreadId, BOOL bClose)
{
	HANDLE hEvent;
	OVERLAPPED Overlapped;
	DWORD dwToWrite, dwLocalSent, dwWaitResult;
	DWORD dwHeaderSize = sizeof(DWORD) + sizeof(DWORD) + sizeof(BYTE);
	BOOL bSucc = FALSE;

	*nBytesWritten = 0;
	dwLocalSent = 0;

	hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	Overlapped = { 0 };
	Overlapped.hEvent = hEvent;

	// Calculate actual data size to send
	dwToWrite = nBytesToWrite;

	// Prepare header
	memcpy_s(Buffer - dwHeaderSize, sizeof(DWORD), &dwRemoteThreadId, sizeof(DWORD));
	memcpy_s(Buffer - dwHeaderSize + sizeof(DWORD), sizeof(DWORD), &dwToWrite, sizeof(DWORD));
	Buffer[sizeof(DWORD) + sizeof(DWORD) - dwHeaderSize] = bClose ? 0x01 : 0x00;

	dwWaitResult = WaitForSingleObject(ghMutex, INFINITE);
	switch (dwWaitResult)
	{
	case WAIT_OBJECT_0:
		bSucc = WriteFile(hChannel, Buffer - dwHeaderSize, dwToWrite + dwHeaderSize, &dwLocalSent, &Overlapped);
		if (!bSucc)
		{
			if (GetLastError() == ERROR_IO_PENDING)
			{
				WaitForSingleObject(Overlapped.hEvent, INFINITE);
				bSucc = GetOverlappedResult(hChannel, &Overlapped, &dwLocalSent, TRUE);
			}
		}
		if (!bSucc)
		{
			if (bVerbose) printf("[-] WriteChannelToRDP error: %ld\n", GetLastError());
		}
		else
		{
			*nBytesWritten = dwLocalSent;
			if (bDebug) printf("[+] Sent %ld bytes to RDP channel for thread %08X\n", dwLocalSent, dwRemoteThreadId);
		}

		if (!ReleaseMutex(ghMutex))
		{
			if (bVerbose) printf("Release failed\n");
		}
		break;

	case WAIT_ABANDONED:
		bSucc = FALSE;
		break;
	}

	CloseHandle(hEvent);
	return bSucc;
}

// Handle SOCKS client connections - debug output controlled by -v
DWORD WINAPI HandleSocksClient(void* param)
{
	struct clientargs* pArgs = (struct clientargs*)param;
	SOCKET sockClient = pArgs->sockClient;
	DWORD dwThreadId = pArgs->dwThreadId;

	char buffer[BUF_SIZE + sizeof(DWORD) + sizeof(DWORD) + sizeof(BYTE)];
	char* dataBuffer = buffer + sizeof(DWORD) + sizeof(DWORD) + sizeof(BYTE);
	int ret;
	DWORD dwWritten;

	// Add to SOCKS thread list
	struct socksthread* pSocksThread = AddSocksThread(dwThreadId, sockClient);
	if (!pSocksThread)
	{
		if (bVerbose) printf("[-] Failed to add SOCKS thread %08X\n", dwThreadId);
		closesocket(sockClient);
		free(pArgs);
		return -1;
	}

	if (bVerbose) printf("[+] New SOCKS client connected, thread: %08X\n", dwThreadId);

	while (pSocksThread->run)
	{
		ret = recv(sockClient, dataBuffer, BUF_SIZE, 0);
		if (ret <= 0)
		{
			int error = WSAGetLastError();
			if (bVerbose) printf("[-] Client disconnected or error: %08X, WSA error: %d\n", dwThreadId, error);
			break;
		}

		if (bDebug) printf("[*] Received %d bytes from SOCKS client %08X, forwarding to RDP channel\n", ret, dwThreadId);

		// Print first few bytes for debugging
		if (bDebug)
		{
			printf("[*] First 16 bytes: ");
			for (int i = 0; i < min(16, ret); i++)
			{
				printf("%02X ", (unsigned char)dataBuffer[i]);
			}
			printf("\n");
		}

		// Forward ALL data to RDP channel
		if (!WriteChannelToRDP(dataBuffer, ret, &dwWritten, dwThreadId, FALSE))
		{
			if (bVerbose) printf("[-] Failed to write to RDP channel\n");
			break;
		}

		if (bDebug) printf("[+] Successfully forwarded %d bytes to channel\n", ret);
	}

	// Send close signal
	WriteChannelToRDP(dataBuffer, 0, &dwWritten, dwThreadId, TRUE);

	DeleteSocksThread(dwThreadId);
	closesocket(sockClient);
	free(pArgs);

	if (bVerbose) printf("[*] SOCKS client handler thread %08X terminated\n", dwThreadId);

	return 0;
}

// Handle RDP channel communication - debug output controlled by -v
DWORD WINAPI ChannelHandler(void* param)
{
	BOOL bSucc, ofused, bClose = FALSE;
	HANDLE hEvent;
	OVERLAPPED Overlapped;
	DWORD dwRecvdLen, dwRead, dwOverflow = 0;
	DWORD dwRemoteThreadId;
	ULONG cbFullSize;
	BYTE ReadBuffer[CHANNEL_PDU_LENGTH];
	CHANNEL_PDU_HEADER* pHdr = (CHANNEL_PDU_HEADER*)ReadBuffer;
	char* buf, * bufWrite, szOverflow[BUF_SIZE * 8]; // Increased overflow buffer
	struct socksthread* pSocksThread;

	hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	Overlapped = { 0 };
	Overlapped.hEvent = hEvent;

	if (bVerbose) printf("[*] Channel handler started\n");

	// Handle channel communication from client plugin
	while (TRUE)
	{
		bSucc = ReadFile(hChannel, ReadBuffer, sizeof(ReadBuffer), &dwRead, &Overlapped);
		if (!bSucc)
		{
			if (GetLastError() == ERROR_IO_PENDING)
			{
				WaitForSingleObject(Overlapped.hEvent, INFINITE);
				bSucc = GetOverlappedResult(hChannel, &Overlapped, &dwRead, FALSE);
			}
		}
		if (!bSucc)
		{
			if (bVerbose) printf("[-] ReadFile()/WaitForSingleObject() error: %ld\n", GetLastError());
			return -1;
		}

		if (bDebug) printf("[*] Received %ld bytes from RDP channel\n", dwRead);

		// Process received data and forward to appropriate SOCKS client
		bufWrite = (char*)(pHdr + 1);
		DWORD bufWritelen = dwRead - sizeof(CHANNEL_PDU_HEADER);

		buf = (char*)bufWrite;
		cbFullSize = bufWritelen;
		ofused = FALSE;

		// Handle overflow from previous call
		if (dwOverflow)
		{
			if (bDebug) printf("[*] Overflow handling: %ld + %ld = %ld bytes\n", dwOverflow, bufWritelen, dwOverflow + bufWritelen);

			// Check if overflow buffer is large enough
			if (dwOverflow + bufWritelen > sizeof(szOverflow))
			{
				if (bVerbose) printf("[-] Overflow buffer too small, discarding data\n");
				dwOverflow = 0;
				continue;
			}

			memcpy_s(szOverflow + dwOverflow, sizeof(szOverflow) - dwOverflow, bufWrite, bufWritelen);
			buf = szOverflow;
			cbFullSize = bufWritelen + dwOverflow;
			dwOverflow = 0;
			ofused = TRUE;
		}

		// Process data packets
		while (cbFullSize >= 9) // Ensure we have at least header
		{
			// Parse header
			memcpy(&dwRemoteThreadId, buf, sizeof(DWORD));
			memcpy(&dwRecvdLen, buf + sizeof(DWORD), sizeof(DWORD));
			bClose = (buf[8] == 0x01);

			if (bDebug) printf("[*] Processing packet: thread=%08X, len=%ld, close=%d\n", dwRemoteThreadId, dwRecvdLen, bClose);

			// Validate packet size
			DWORD packetSize = dwRecvdLen + 9;
			if (packetSize > cbFullSize)
			{
				if (bDebug) printf("[*] Incomplete packet (need %ld, have %ld), saving for next read\n", packetSize, cbFullSize);

				// Save incomplete packet for next iteration
				if (cbFullSize <= sizeof(szOverflow))
				{
					if (!ofused)
					{
						memcpy_s(szOverflow, sizeof(szOverflow), buf, cbFullSize);
					}
					dwOverflow = cbFullSize;
				}
				else
				{
					if (bVerbose) printf("[-] Packet too large for overflow buffer, discarding\n");
					dwOverflow = 0;
				}
				break;
			}

			// Find corresponding SOCKS thread
			pSocksThread = LookupSocksThread(dwRemoteThreadId);
			if (pSocksThread && pSocksThread->sockClient != INVALID_SOCKET)
			{
				if (dwRecvdLen > 0)
				{
					if (bDebug) printf("[*] Forwarding %ld bytes to SOCKS client %08X\n", dwRecvdLen, dwRemoteThreadId);

					// Print first few bytes for debugging
					if (bDebug)
					{
						printf("[*] Response bytes: ");
						for (DWORD i = 0; i < min(16, dwRecvdLen); i++)
						{
							printf("%02X ", (unsigned char)buf[9 + i]);
						}
						printf("\n");
					}

					// Forward data to SOCKS client
					int sent = send(pSocksThread->sockClient, buf + 9, dwRecvdLen, 0);
					if (sent == SOCKET_ERROR)
					{
						int error = WSAGetLastError();
						if (bVerbose) printf("[-] Failed to send data to SOCKS client %08X, error: %d\n", dwRemoteThreadId, error);
						DeleteSocksThread(dwRemoteThreadId);
					}
					else
					{
						if (bDebug) printf("[+] Forwarded %d bytes to SOCKS client %08X\n", sent, dwRemoteThreadId);
					}
				}

				if (bClose)
				{
					if (bDebug) printf("[*] Close signal received for SOCKS client %08X\n", dwRemoteThreadId);
					DeleteSocksThread(dwRemoteThreadId);
				}
			}
			else
			{
				if (bDebug) printf("[-] No SOCKS thread found for remote thread %08X\n", dwRemoteThreadId);
			}

			// Move to next packet
			cbFullSize -= packetSize;
			buf += packetSize;
		}

		// Handle any remaining incomplete data
		if (cbFullSize > 0 && cbFullSize < 9)
		{
			if (bDebug) printf("[*] Saving %ld incomplete header bytes for next read\n", cbFullSize);
			if (cbFullSize <= sizeof(szOverflow))
			{
				if (!ofused)
				{
					memcpy_s(szOverflow, sizeof(szOverflow), buf, cbFullSize);
				}
				else
				{
					// Data is already in szOverflow, just update the overflow size
				}
				dwOverflow = cbFullSize;
			}
		}
	}

	return 0;
}

// Ctrl+C Handler
BOOL WINAPI CtrlHandler(DWORD fdwCtrlType)
{
	switch (fdwCtrlType)
	{
	case CTRL_C_EVENT:
		if (CTRLC)
		{
			printf("[*] Forced terminating\n");
			WTSVirtualChannelClose(hWTSHandle);
			exit(0);
		}

		printf("[*] CTRL+C pressed. Closing down.\n");
		CTRLC = 1;

		if (hWTSHandle)
		{
			WTSVirtualChannelClose(hWTSHandle);
		}
		exit(0);
		return TRUE;
	default:
		return FALSE;
	}
}

INT _cdecl wmain(INT argc, __in_ecount(argc) WCHAR** argv)
{
	WSADATA	wsaData;
	SOCKET sockServer, sockClient;
	struct sockaddr_in serverAddr, clientAddr;
	int clientAddrSize = sizeof(clientAddr);
	int ret;
	HANDLE hChannelThread;
	DWORD dwChannelThreadId;
	DWORD dwClientThreadId = 1000;

	running_args.port = L"1080";
	running_args.priority = 4;
	running_args.ip = L"127.0.0.1";

	wprintf(L"=============================================================\n");
	wprintf(L"  SocksOverRDP Server\n");
	wprintf(L"=============================================================\n");
	wprintf(L"  This runs on the RDP SERVER side\n");
	wprintf(L"  Make sure the plugin DLL is installed on the CLIENT side\n");
	wprintf(L"  Use -v for verbose debug output\n");
	wprintf(L"=============================================================\n\n");

	if (argc > 1)
		if (!parse_argv(argc, argv))
			return -1;

	if ((ret = OpenDynamicChannel(SocksOverRDP_CHANNEL_NAME, &hChannel)) != ERROR_SUCCESS)
	{
		if (ret == 31)
			wprintf(L"[-] Could not open Dynamic Virtual Channel, plugin was not loaded on the client side: %ld\n", ret);
		else
			wprintf(L"[-] Could not open Dynamic Virtual Channel: %ld  %08X\n", ret, ret);
		wprintf(L"[-] Make sure the plugin DLL is properly registered on the CLIENT\n");
		return -1;
	}

	wprintf(L"[+] Channel opened over RDP - CLIENT PLUGIN IS CONNECTED!\n");

	ghMutex = CreateMutex(NULL, FALSE, NULL);
	ghLLMutex = CreateMutex(NULL, FALSE, NULL);
	ghSocksMutex = CreateMutex(NULL, FALSE, NULL);

	if ((ret = WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0)
	{
		wprintf(L"[-] WSAStartup() failed with error: %ld\n", ret);
		return -1;
	}

	// Create SOCKS server socket
	sockServer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sockServer == INVALID_SOCKET)
	{
		wprintf(L"[-] socket() failed with error: %ld\n", WSAGetLastError());
		WSACleanup();
		return -1;
	}

	// Bind to localhost on specified port
	serverAddr.sin_family = AF_INET;
	inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);
	serverAddr.sin_port = htons((u_short)_wtoi(running_args.port));

	if (bind(sockServer, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
	{
		wprintf(L"[-] bind() failed with error: %ld\n", WSAGetLastError());
		closesocket(sockServer);
		WSACleanup();
		return -1;
	}

	if (listen(sockServer, SOMAXCONN) == SOCKET_ERROR)
	{
		wprintf(L"[-] listen() failed with error: %ld\n", WSAGetLastError());
		closesocket(sockServer);
		WSACleanup();
		return -1;
	}

	wprintf(L"[+] SOCKS server listening on 127.0.0.1:%s\n", running_args.port);
	wprintf(L"[+] Configure your browser to use SOCKS proxy: 127.0.0.1:%s\n", running_args.port);

	// Start channel handler thread
	hChannelThread = CreateThread(NULL, 0, &ChannelHandler, NULL, 0, &dwChannelThreadId);
	if (hChannelThread == NULL)
	{
		wprintf(L"[-] Failed to create channel handler thread\n");
		closesocket(sockServer);
		WSACleanup();
		return -1;
	}

	// Set handler for Ctrl+C
	SetConsoleCtrlHandler(CtrlHandler, TRUE);

	wprintf(L"[*] Waiting for SOCKS client connections...\n");
	if (bVerbose) wprintf(L"[*] Verbose mode enabled - showing debug output\n");

	// Accept SOCKS client connections
	while (TRUE)
	{
		sockClient = accept(sockServer, (struct sockaddr*)&clientAddr, &clientAddrSize);
		if (sockClient == INVALID_SOCKET)
		{
			wprintf(L"[-] accept() failed with error: %ld\n", WSAGetLastError());
			continue;
		}

		// Convert IP address to string using inet_ntop (modern way)
		char ipStr[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &clientAddr.sin_addr, ipStr, sizeof(ipStr));
		if (bVerbose) wprintf(L"[+] SOCKS client connected from %S\n", ipStr);

		// Create new thread to handle this client
		struct clientargs* pArgs = (struct clientargs*)malloc(sizeof(struct clientargs));
		pArgs->sockClient = sockClient;
		pArgs->dwThreadId = dwClientThreadId++;
		pArgs->hChannel = hChannel;

		HANDLE hClientThread = CreateThread(NULL, 0, &HandleSocksClient, pArgs, 0, NULL);
		if (hClientThread == NULL)
		{
			wprintf(L"[-] Failed to create client handler thread\n");
			closesocket(sockClient);
			free(pArgs);
		}
		else
		{
			CloseHandle(hClientThread);
		}
	}

	CloseHandle(ghMutex);
	CloseHandle(ghLLMutex);
	CloseHandle(ghSocksMutex);
	CloseHandle(hChannel);
	closesocket(sockServer);
	WSACleanup();

	return 0;
}

DWORD OpenDynamicChannel(LPCSTR szChannelName, HANDLE* phFile)
{
	HANDLE	hWTSFileHandle;
	PVOID	vcFileHandlePtr = NULL;
	DWORD	len;
	DWORD	rc = ERROR_SUCCESS;
	BOOL	fSucc;

	hWTSHandle = WTSVirtualChannelOpenEx(WTS_CURRENT_SESSION, (LPSTR)szChannelName,
		WTS_CHANNEL_OPTION_DYNAMIC | running_args.priority);
	if (NULL == hWTSHandle)
	{
		rc = GetLastError();
		goto exitpt;
	}

	fSucc = WTSVirtualChannelQuery(hWTSHandle, WTSVirtualFileHandle,
		&vcFileHandlePtr, &len);
	if (!fSucc)
	{
		rc = GetLastError();
		goto exitpt;
	}
	if (len != sizeof(HANDLE))
	{
		rc = ERROR_INVALID_PARAMETER;
		goto exitpt;
	}

	hWTSFileHandle = *(HANDLE*)vcFileHandlePtr;
	fSucc = DuplicateHandle(GetCurrentProcess(), hWTSFileHandle,
		GetCurrentProcess(), phFile, 0, FALSE, DUPLICATE_SAME_ACCESS);

	if (!fSucc)
	{
		rc = GetLastError();
		goto exitpt;
	}

	rc = ERROR_SUCCESS;

exitpt:
	if (vcFileHandlePtr)
	{
		WTSFreeMemory(vcFileHandlePtr);
	}
	if (hWTSHandle)
	{
		WTSVirtualChannelClose(hWTSHandle);
	}

	return rc;
}