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

#include "stdafx.h"
#include "SocksOverRDP-Plugin.h"
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <stdio.h>
#include <process.h>
#include <math.h>

#define _WINSOCK_DEPRECATED_NO_WARNINGS

#pragma comment(lib, "Ws2_32.lib")

#define BUF_SIZE 4096
#define DEBUG_PRINT_BUFFER_SIZE 1024

DWORD dwOverflow = 0;
static HANDLE ghMutex, ghLLMutex;

struct threads
{
	DWORD	dwThreadId;
	HANDLE	hThread;
	BOOL	run;
	SOCKET	s;
	IWTSVirtualChannel* pChannel;
	BOOL	handshakeDone;
	BOOL	connected;
	struct threads* next;
};

struct threads* ThreadHead = NULL;
static IWTSVirtualChannel* g_pChannel = NULL;
static BOOL bVerbose = FALSE, bDebug = TRUE;

// Thread management functions
struct threads* AddThread(DWORD dwThreadId, SOCKET s, IWTSVirtualChannel* pChannel)
{
	struct threads* rolling;
	struct threads* ThreadStruct;

	DWORD dwWaitResult = WaitForSingleObject(ghLLMutex, INFINITE);
	switch (dwWaitResult)
	{
	case WAIT_OBJECT_0:
		ThreadStruct = (threads*)malloc(sizeof(struct threads));
		ThreadStruct->dwThreadId = dwThreadId;
		ThreadStruct->hThread = NULL;
		ThreadStruct->run = TRUE;
		ThreadStruct->s = s;
		ThreadStruct->pChannel = pChannel;
		ThreadStruct->handshakeDone = FALSE;
		ThreadStruct->connected = FALSE;
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
			printf("AddThread Release failed\n");
		}
		return ThreadStruct;
		break;

	case WAIT_ABANDONED:
		printf("AddThread lock abandoned\n");
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
		if (rolling == NULL)
		{
			if (!ReleaseMutex(ghLLMutex))
			{
				printf("LookupThread Release Failed0\n");
			}
			return NULL;
		}

		while (rolling)
		{
			if (rolling->dwThreadId == dwThreadId)
			{
				if (!ReleaseMutex(ghLLMutex))
				{
					printf("LookupThread Release Failed1\n");
				}
				return rolling;
			}
			rolling = rolling->next;
		}

		if (!ReleaseMutex(ghLLMutex))
		{
			printf("LookupThread Release Failed2\n");
		}
		return NULL;
		break;
	case WAIT_ABANDONED:
		printf("LookupThread lock abandoned\n");
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
				printf("DeleteThread: trying to delete empty list\n");
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
			if (rolling->s != INVALID_SOCKET)
			{
				closesocket(rolling->s);
			}
			free(rolling);
		}

		if (!ReleaseMutex(ghLLMutex))
		{
			printf("DeleteThread Release failed\n");
		}
		return;
		break;

	case WAIT_ABANDONED:
		printf("DeleteThread lock abandoned\n");
		return;
	}
}

VOID TerminateThreads()
{
	struct threads* rolling;

	DWORD dwWaitResult = WaitForSingleObject(ghLLMutex, INFINITE);
	switch (dwWaitResult)
	{
	case WAIT_OBJECT_0:
		rolling = ThreadHead;
		while (rolling)
		{
			rolling->run = FALSE;
			if (rolling->s != INVALID_SOCKET)
			{
				closesocket(rolling->s);
			}
			rolling = rolling->next;
		}

		if (!ReleaseMutex(ghLLMutex))
		{
			printf("TerminateThreads Release failed\n");
		}
		break;

	case WAIT_ABANDONED:
		printf("TerminateThreads lock abandoned\n");
		break;
	}
}

// Write data to RDP channel - FIXED buffer management
BOOL WriteChannel(IWTSVirtualChannel* pChannel, const char* Buffer, DWORD nBytesToWrite, DWORD dwRemoteThreadId, BOOL bClose)
{
	DWORD dwHeaderSize = sizeof(DWORD) + sizeof(DWORD) + sizeof(BYTE);
	DWORD dwTotalSize = dwHeaderSize + nBytesToWrite;
	char* packetBuffer = (char*)malloc(dwTotalSize);

	if (!packetBuffer)
	{
		if (bDebug) printf("[-] Failed to allocate packet buffer\n");
		return FALSE;
	}

	// Prepare header
	memcpy_s(packetBuffer, sizeof(DWORD), &dwRemoteThreadId, sizeof(DWORD));
	memcpy_s(packetBuffer + sizeof(DWORD), sizeof(DWORD), &nBytesToWrite, sizeof(DWORD));
	packetBuffer[sizeof(DWORD) + sizeof(DWORD)] = bClose ? (char)0x01 : (char)0x00;

	// Copy data
	if (nBytesToWrite > 0)
	{
		memcpy_s(packetBuffer + dwHeaderSize, nBytesToWrite, Buffer, nBytesToWrite);
	}

	HRESULT hr = pChannel->Write(dwTotalSize, (BYTE*)packetBuffer, NULL);

	BOOL result = SUCCEEDED(hr);
	if (bDebug && result)
	{
		printf("[+] Sent %ld bytes (header+data) for thread %08X\n", dwTotalSize, dwRemoteThreadId);
	}
	else if (bDebug)
	{
		printf("[-] Failed to send data for thread %08X, error: 0x%08X\n", dwRemoteThreadId, hr);
	}

	free(packetBuffer);
	return result;
}

// SOCKS connection handler
DWORD WINAPI HandleConnection(void* param)
{
	struct threads* pta = (struct threads*)param;
	SOCKET sock = pta->s;
	IWTSVirtualChannel* pChannel = pta->pChannel;
	DWORD dwThreadId = pta->dwThreadId;

	char* dataBuffer = (char*)malloc(BUF_SIZE);
	if (!dataBuffer)
	{
		if (bDebug) printf("[-] Failed to allocate data buffer for thread %08X\n", dwThreadId);
		return -1;
	}

	int ret;

	if (bDebug) printf("[*] Connection handler started for thread %08X\n", dwThreadId);

	while (pta->run)
	{
		ret = recv(sock, dataBuffer, BUF_SIZE, 0);
		if (ret <= 0)
		{
			if (bDebug) printf("[*] Connection closed or error for thread %08X, error: %d\n", dwThreadId, WSAGetLastError());
			break;
		}

		if (bDebug) printf("[*] Received %d bytes, forwarding to server\n", ret);

		if (!WriteChannel(pChannel, dataBuffer, ret, dwThreadId, FALSE))
		{
			if (bDebug) printf("[-] Failed to write to channel\n");
			break;
		}
	}

	// Send close signal
	WriteChannel(pChannel, "", 0, dwThreadId, TRUE);

	closesocket(sock);
	DeleteThread(dwThreadId);
	free(dataBuffer);

	if (bDebug) printf("[*] Connection handler terminated for thread %08X\n", dwThreadId);

	return 0;
}

// Handle SOCKS protocol with proper handshake
BOOL HandleSocksRequest(char* buffer, DWORD dwSize, IWTSVirtualChannel* pChannel, DWORD dwThreadId, struct threads* pta)
{
	char response[300];

	// SOCKS5 greeting
	if (!pta->handshakeDone && dwSize >= 3 && buffer[0] == 0x05)
	{
		if (bDebug) printf("[*] SOCKS5 greeting received for thread %08X\n", dwThreadId);

		// Send "no authentication required" response
		response[0] = 0x05; // SOCKS version
		response[1] = 0x00; // No authentication

		if (!WriteChannel(pChannel, response, 2, dwThreadId, FALSE))
		{
			if (bDebug) printf("[-] Failed to send SOCKS5 greeting response\n");
			return FALSE;
		}

		pta->handshakeDone = TRUE;
		if (bDebug) printf("[+] SOCKS5 greeting response sent\n");
		return TRUE;
	}

	// SOCKS5 connection request
	if (pta->handshakeDone && !pta->connected && dwSize >= 10 && buffer[0] == 0x05 && buffer[1] == 0x01)
	{
		if (bDebug) printf("[*] SOCKS5 connection request received\n");

		BYTE addrType = buffer[3];
		char* addrPtr = buffer + 4;
		WORD port;
		struct sockaddr_in addr;
		ADDRINFOA hints, * result = NULL;
		int ret;

		switch (addrType)
		{
		case 1: // IPv4
		{
			if (dwSize < 10)
				return FALSE;

			addr.sin_family = AF_INET;
			memcpy(&addr.sin_addr, addrPtr, 4);
			memcpy(&port, addrPtr + 4, 2);
			addr.sin_port = port;

			char ipStr[INET_ADDRSTRLEN] = { 0 };
			inet_ntop(AF_INET, &addr.sin_addr, ipStr, sizeof(ipStr));
			if (bDebug) printf("[*] Connecting to %s:%d\n", ipStr, ntohs(port));

			pta->s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (pta->s == INVALID_SOCKET)
			{
				if (bDebug) printf("[-] Failed to create socket\n");
				response[0] = 5; response[1] = 1; response[2] = 0; response[3] = 1; // General failure
				memset(response + 4, 0, 6);
				WriteChannel(pChannel, response, 10, dwThreadId, FALSE);
				return FALSE;
			}

			ret = connect(pta->s, (struct sockaddr*)&addr, sizeof(addr));
			if (ret == SOCKET_ERROR)
			{
				if (bDebug) printf("[-] Failed to connect, error: %d\n", WSAGetLastError());
				closesocket(pta->s);
				pta->s = INVALID_SOCKET;
				response[0] = 5; response[1] = 5; response[2] = 0; response[3] = 1; // Connection refused
				memset(response + 4, 0, 6);
				WriteChannel(pChannel, response, 10, dwThreadId, FALSE);
				return FALSE;
			}

			// Send success response
			response[0] = 5; response[1] = 0; response[2] = 0; response[3] = 1;
			memcpy(response + 4, &addr.sin_addr, 4);
			memcpy(response + 8, &addr.sin_port, 2);
			WriteChannel(pChannel, response, 10, dwThreadId, FALSE);

			if (bDebug) printf("[+] Connected successfully\n");
			break;
		}

		case 3: // Domain name
		{
			BYTE domainLen = (BYTE)addrPtr[0];
			if (dwSize < (DWORD)(7 + domainLen))
				return FALSE;

			char domain[256];
			memcpy(domain, addrPtr + 1, domainLen);
			domain[domainLen] = '\0';
			memcpy(&port, addrPtr + 1 + domainLen, 2);

			if (bDebug) printf("[*] Connecting to %s:%d\n", domain, ntohs(port));

			ZeroMemory(&hints, sizeof(hints));
			hints.ai_family = AF_INET;
			hints.ai_socktype = SOCK_STREAM;

			ret = GetAddrInfoA(domain, NULL, &hints, &result);
			if (ret != 0)
			{
				if (bDebug) printf("[-] DNS resolution failed for %s\n", domain);
				response[0] = 5; response[1] = 4; response[2] = 0; response[3] = 1; // Host unreachable
				memset(response + 4, 0, 6);
				WriteChannel(pChannel, response, 10, dwThreadId, FALSE);
				return FALSE;
			}

			pta->s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (pta->s == INVALID_SOCKET)
			{
				FreeAddrInfoA(result);
				if (bDebug) printf("[-] Failed to create socket\n");
				response[0] = 5; response[1] = 1; response[2] = 0; response[3] = 1; // General failure
				memset(response + 4, 0, 6);
				WriteChannel(pChannel, response, 10, dwThreadId, FALSE);
				return FALSE;
			}

			memcpy(&addr, result->ai_addr, sizeof(addr));
			addr.sin_port = port;
			FreeAddrInfoA(result);

			ret = connect(pta->s, (struct sockaddr*)&addr, sizeof(addr));
			if (ret == SOCKET_ERROR)
			{
				if (bDebug) printf("[-] Failed to connect to %s, error: %d\n", domain, WSAGetLastError());
				closesocket(pta->s);
				pta->s = INVALID_SOCKET;
				response[0] = 5; response[1] = 5; response[2] = 0; response[3] = 1; // Connection refused
				memset(response + 4, 0, 6);
				WriteChannel(pChannel, response, 10, dwThreadId, FALSE);
				return FALSE;
			}

			// Send success response
			response[0] = 5; response[1] = 0; response[2] = 0; response[3] = 1;
			memcpy(response + 4, &addr.sin_addr, 4);
			memcpy(response + 8, &addr.sin_port, 2);
			WriteChannel(pChannel, response, 10, dwThreadId, FALSE);

			if (bDebug) printf("[+] Connected successfully to %s\n", domain);
			break;
		}

		default:
			if (bDebug) printf("[-] Unsupported address type: %d\n", addrType);
			response[0] = 5; response[1] = 8; response[2] = 0; response[3] = 1; // Address type not supported
			memset(response + 4, 0, 6);
			WriteChannel(pChannel, response, 10, dwThreadId, FALSE);
			return FALSE;
		}

		pta->connected = TRUE;
		if (bDebug) printf("[+] SOCKS5 connection established for thread %08X\n", dwThreadId);
		return TRUE;
	}

	// SOCKS4 request
	if (!pta->handshakeDone && dwSize >= 8 && buffer[0] == 0x04 && buffer[1] == 0x01)
	{
		if (bDebug) printf("[*] SOCKS4 connection request received\n");

		struct sockaddr_in addr;
		addr.sin_family = AF_INET;
		memcpy(&addr.sin_port, buffer + 2, 2);
		memcpy(&addr.sin_addr, buffer + 4, 4);

		//if (bDebug) printf("[*] Connecting to %s:%d\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

		pta->s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (pta->s == INVALID_SOCKET)
		{
			response[0] = 0; response[1] = 91; // Request rejected
			memset(response + 2, 0, 6);
			WriteChannel(pChannel, response, 8, dwThreadId, FALSE);
			return FALSE;
		}

		int ret = connect(pta->s, (struct sockaddr*)&addr, sizeof(addr));
		if (ret == SOCKET_ERROR)
		{
			if (bDebug) printf("[-] Failed to connect, error: %d\n", WSAGetLastError());
			closesocket(pta->s);
			pta->s = INVALID_SOCKET;
			response[0] = 0; response[1] = 91; // Request rejected
			memset(response + 2, 0, 6);
			WriteChannel(pChannel, response, 8, dwThreadId, FALSE);
			return FALSE;
		}

		// Send success response
		memset(response, 0, 8);
		response[1] = 90; // Request granted
		WriteChannel(pChannel, response, 8, dwThreadId, FALSE);

		pta->handshakeDone = TRUE;
		pta->connected = TRUE;
		if (bDebug) printf("[+] SOCKS4 connection established for thread %08X\n", dwThreadId);
		return TRUE;
	}

	return FALSE;
}

// ATL COM Class implementation
class ATL_NO_VTABLE SocksOverRDPPlugin :
	public CComObjectRootEx<CComMultiThreadModel>,
	public CComCoClass<SocksOverRDPPlugin, &CLSID_CompReg>,
	public IWTSPlugin,
	public IWTSVirtualChannelCallback,
	public IWTSListenerCallback
{
public:
	CComPtr<IWTSVirtualChannel> m_ptrChannel;
	CComPtr<IWTSListener> m_ptrListener;

	DECLARE_REGISTRY_RESOURCEID(IDR_SocksOverRDPPLUGIN)

	BEGIN_COM_MAP(SocksOverRDPPlugin)
		COM_INTERFACE_ENTRY(IWTSPlugin)
		COM_INTERFACE_ENTRY(IWTSVirtualChannelCallback)
		COM_INTERFACE_ENTRY(IWTSListenerCallback)
	END_COM_MAP()

	DECLARE_PROTECT_FINAL_CONSTRUCT()

	HRESULT FinalConstruct()
	{
		return S_OK;
	}

	void FinalRelease()
	{
	}

	// IWTSPlugin methods
	HRESULT STDMETHODCALLTYPE Initialize(IWTSVirtualChannelManager* pChannelMgr);
	HRESULT STDMETHODCALLTYPE Connected();
	HRESULT STDMETHODCALLTYPE Disconnected(DWORD dwDisconnectCode) { return S_OK; }
	HRESULT STDMETHODCALLTYPE Terminated() { return S_OK; }

	// IWTSListenerCallback methods
	HRESULT STDMETHODCALLTYPE OnNewChannelConnection(
		IWTSVirtualChannel* pChannel,
		BSTR data,
		BOOL* pbAccept,
		IWTSVirtualChannelCallback** ppCallback);

	// IWTSVirtualChannelCallback methods
	HRESULT STDMETHODCALLTYPE OnDataReceived(ULONG cbSize, BYTE* pBuffer);
	HRESULT STDMETHODCALLTYPE OnClose();
};

// Implementation of COM interface methods
HRESULT STDMETHODCALLTYPE SocksOverRDPPlugin::Initialize(IWTSVirtualChannelManager* pChannelMgr)
{
	HRESULT hr;

	ghMutex = CreateMutex(NULL, FALSE, NULL);
	ghLLMutex = CreateMutex(NULL, FALSE, NULL);

	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		return E_FAIL;
	}

	hr = pChannelMgr->CreateListener("SocksChannel", 0, this, &m_ptrListener);
	if (FAILED(hr))
	{
		WSACleanup();
		return hr;
	}

	if (bDebug) printf("[*] Plugin initialized successfully\n");
	return S_OK;
}

HRESULT STDMETHODCALLTYPE SocksOverRDPPlugin::Connected()
{
	if (bDebug) printf("[*] Plugin connected\n");
	return S_OK;
}

HRESULT STDMETHODCALLTYPE SocksOverRDPPlugin::OnNewChannelConnection(
	IWTSVirtualChannel* pChannel,
	BSTR data,
	BOOL* pbAccept,
	IWTSVirtualChannelCallback** ppCallback)
{
	*pbAccept = TRUE;
	*ppCallback = this;
	this->AddRef();

	m_ptrChannel = pChannel;
	g_pChannel = pChannel;

	if (bDebug) printf("[*] New channel connection established\n");

	return S_OK;
}

HRESULT STDMETHODCALLTYPE SocksOverRDPPlugin::OnDataReceived(ULONG cbSize, BYTE* pBuffer)
{
	static char szOverflow[BUF_SIZE * 4];
	static DWORD dwOverflow = 0;
	BOOL ofused = FALSE;
	char* buf = (char*)pBuffer;
	ULONG cbFullSize = cbSize;
	DWORD dwRemoteThreadId, dwRecvdLen;
	struct threads* pta;

	if (bDebug) printf("[*] Received %d bytes from server\n", cbSize);

	// Handle overflow from previous call
	if (dwOverflow)
	{
		if (bDebug) printf("[*] Handling overflow: %ld + %ld bytes\n", dwOverflow, cbSize);
		memcpy_s(szOverflow + dwOverflow, BUF_SIZE * 4 - dwOverflow, pBuffer, cbSize);
		buf = szOverflow;
		cbFullSize = cbSize + dwOverflow;
		dwOverflow = 0;
		ofused = TRUE;
	}

	// Process data packets
	while (cbFullSize)
	{
		if (cbFullSize < 9)
		{
			if (ofused)
			{
				dwOverflow = cbFullSize;
			}
			else
			{
				memcpy_s(szOverflow, BUF_SIZE * 4, buf, cbFullSize);
				dwOverflow = cbFullSize;
			}
			cbFullSize = 0;
			break;
		}

		// Parse header
		memcpy(&dwRemoteThreadId, buf, sizeof(DWORD));
		memcpy(&dwRecvdLen, buf + sizeof(DWORD), sizeof(DWORD));
		BOOL bClose = (buf[8] == 0x01);

		if (bDebug) printf("[*] Processing packet: thread=%08X, len=%ld, close=%d\n", dwRemoteThreadId, dwRecvdLen, bClose);

		// Look up thread
		pta = LookupThread(dwRemoteThreadId);

		if (pta == NULL)
		{
			// New SOCKS connection request
			if (dwRecvdLen > 0)
			{
				// Create new thread entry (without socket yet)
				pta = AddThread(dwRemoteThreadId, INVALID_SOCKET, m_ptrChannel);
				if (pta)
				{
					if (HandleSocksRequest(buf + 9, dwRecvdLen, m_ptrChannel, dwRemoteThreadId, pta))
					{
						// If connected, start handler thread
						if (pta->connected && pta->s != INVALID_SOCKET)
						{
							HANDLE hThread = CreateThread(NULL, 0, &HandleConnection, pta, 0, NULL);
							if (hThread)
							{
								pta->hThread = hThread;
								CloseHandle(hThread);
							}
						}
					}
					else
					{
						// Failed to handle request, clean up
						DeleteThread(dwRemoteThreadId);
					}
				}
			}
		}
		else
		{
			// Existing connection
			if (bClose)
			{
				if (bDebug) printf("[*] Close signal received for thread %08X\n", dwRemoteThreadId);
				DeleteThread(dwRemoteThreadId);
			}
			else if (dwRecvdLen > 0)
			{
				if (!pta->connected)
				{
					// Continue handshake
					if (HandleSocksRequest(buf + 9, dwRecvdLen, m_ptrChannel, dwRemoteThreadId, pta))
					{
						// If now connected, start handler thread
						if (pta->connected && pta->s != INVALID_SOCKET && pta->hThread == NULL)
						{
							HANDLE hThread = CreateThread(NULL, 0, &HandleConnection, pta, 0, NULL);
							if (hThread)
							{
								pta->hThread = hThread;
								CloseHandle(hThread);
							}
						}
					}
				}
				else
				{
					// Forward data to target
					int sent = send(pta->s, buf + 9, dwRecvdLen, 0);
					if (sent == SOCKET_ERROR)
					{
						if (bDebug) printf("[-] Send failed for thread %08X, error: %d\n", dwRemoteThreadId, WSAGetLastError());
						DeleteThread(dwRemoteThreadId);
					}
					else if (bDebug)
					{
						printf("[+] Forwarded %d bytes to target for thread %08X\n", sent, dwRemoteThreadId);
					}
				}
			}
		}

		// Move to next packet
		DWORD packetSize = dwRecvdLen + 9;
		if (packetSize <= cbFullSize)
		{
			cbFullSize -= packetSize;
			buf += packetSize;
		}
		else
		{
			// Handle overflow
			if (ofused)
			{
				memmove_s(szOverflow, BUF_SIZE * 4, buf, cbFullSize);
				dwOverflow = cbFullSize;
			}
			else
			{
				memcpy_s(szOverflow, BUF_SIZE * 4, buf, cbFullSize);
				dwOverflow = cbFullSize;
			}
			cbFullSize = 0;
		}
	}

	return S_OK;
}

HRESULT STDMETHODCALLTYPE SocksOverRDPPlugin::OnClose()
{
	TerminateThreads();
	m_ptrChannel = NULL;
	WSACleanup();
	if (bDebug) printf("[*] Plugin closed\n");
	return S_OK;
}

OBJECT_ENTRY_AUTO(__uuidof(CompReg), SocksOverRDPPlugin)