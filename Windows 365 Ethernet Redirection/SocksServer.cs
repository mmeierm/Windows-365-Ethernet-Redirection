using System;
using System.Collections.Concurrent;
using System.Net;
using System.Net.Sockets;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;

namespace Windows_365_Ethernet_Redirection
{
    public class SocksServer
    {
        private const string CHANNEL_NAME = "SocksChannel";
        private const int BUFFER_SIZE = 4096;
        private const int WTS_CURRENT_SESSION = -1;
        private const uint WTS_CHANNEL_OPTION_DYNAMIC = 0x00000001;
        private const uint ERROR_IO_PENDING = 997;
        private const int DEFAULT_SOCKS_PORT = 1080;

        private IntPtr _channelHandle = IntPtr.Zero;
        private IntPtr _wtsHandle = IntPtr.Zero;
        private bool _isRunning = false;
        private Thread? _channelThread;
        private Thread? _socksServerThread;
        private CancellationTokenSource? _cancellationTokenSource;
        private TcpListener? _socksListener;
        private int _socksPort = DEFAULT_SOCKS_PORT;
        
        private ConcurrentDictionary<uint, TcpClient> _socksClients = new ConcurrentDictionary<uint, TcpClient>();
        private uint _nextThreadId = 1000;

        public event Action<string>? OnLog;
        public bool IsConnected => _channelHandle != IntPtr.Zero && _isRunning;
        public int SocksPort
        {
            get => _socksPort;
            set => _socksPort = value;
        }

        [DllImport("Wtsapi32.dll", SetLastError = true, CharSet = CharSet.Ansi)]
        private static extern IntPtr WTSVirtualChannelOpenEx(int sessionId, string pVirtualName, uint flags);

        [DllImport("Wtsapi32.dll", SetLastError = true)]
        private static extern bool WTSVirtualChannelQuery(IntPtr hChannelHandle, WTS_VIRTUAL_CLASS WtsVirtualClass, out IntPtr ppBuffer, out uint pBytesReturned);

        [DllImport("Wtsapi32.dll", SetLastError = true)]
        private static extern void WTSFreeMemory(IntPtr pMemory);

        [DllImport("Wtsapi32.dll", SetLastError = true)]
        private static extern bool WTSVirtualChannelClose(IntPtr hChannelHandle);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool ReadFile(IntPtr hFile, byte[] lpBuffer, uint nNumberOfBytesToRead, out uint lpNumberOfBytesRead, ref OVERLAPPED lpOverlapped);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool WriteFile(IntPtr hFile, byte[] lpBuffer, uint nNumberOfBytesToWrite, out uint lpNumberOfBytesWritten, ref OVERLAPPED lpOverlapped);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool GetOverlappedResult(IntPtr hFile, ref OVERLAPPED lpOverlapped, out uint lpNumberOfBytesTransferred, bool bWait);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern IntPtr CreateEvent(IntPtr lpEventAttributes, bool bManualReset, bool bInitialState, string? lpName);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool CloseHandle(IntPtr hObject);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern uint WaitForSingleObject(IntPtr hHandle, uint dwMilliseconds);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool DuplicateHandle(IntPtr hSourceProcessHandle, IntPtr hSourceHandle, IntPtr hTargetProcessHandle, out IntPtr lpTargetHandle, uint dwDesiredAccess, bool bInheritHandle, uint dwOptions);

        [DllImport("kernel32.dll")]
        private static extern IntPtr GetCurrentProcess();

        [DllImport("kernel32.dll")]
        private static extern uint GetLastError();

        [StructLayout(LayoutKind.Sequential)]
        private struct OVERLAPPED
        {
            public IntPtr Internal;
            public IntPtr InternalHigh;
            public uint Offset;
            public uint OffsetHigh;
            public IntPtr hEvent;
        }

        private enum WTS_VIRTUAL_CLASS
        {
            WTSVirtualClientData,
            WTSVirtualFileHandle
        }

        private const uint DUPLICATE_SAME_ACCESS = 0x00000002;
        private const uint WAIT_OBJECT_0 = 0;
        private const uint INFINITE = 0xFFFFFFFF;
        private const uint CHANNEL_FLAG_FIRST = 0x01;
        private const uint CHANNEL_FLAG_LAST = 0x02;

        public bool Start()
        {
            if (_isRunning) return false;

            try
            {
                Log($"Opening dynamic virtual channel '{CHANNEL_NAME}'...");
                uint rc = OpenDynamicChannel(CHANNEL_NAME, out _channelHandle);
                
                if (rc != 0)
                {
                    Log($"Could not open Dynamic Virtual Channel. Error: {rc}");
                    return false;
                }

                Log("Channel opened - CLIENT PLUGIN IS CONNECTED!");
                _isRunning = true;
                _cancellationTokenSource = new CancellationTokenSource();

                _channelThread = new Thread(() => ChannelReaderThread(_cancellationTokenSource.Token)) { IsBackground = true };
                _channelThread.Start();

                _socksServerThread = new Thread(() => SocksServerThread(_cancellationTokenSource.Token)) { IsBackground = true };
                _socksServerThread.Start();

                return true;
            }
            catch (Exception ex)
            {
                Log($"Error starting: {ex.Message}");
                Stop();
                return false;
            }
        }

        private uint OpenDynamicChannel(string szChannelName, out IntPtr phFile)
        {
            phFile = IntPtr.Zero;
            IntPtr vcFileHandlePtr = IntPtr.Zero;
            
            try
            {
                _wtsHandle = WTSVirtualChannelOpenEx(WTS_CURRENT_SESSION, szChannelName, WTS_CHANNEL_OPTION_DYNAMIC | 4);
                if (_wtsHandle == IntPtr.Zero) return GetLastError();

                if (!WTSVirtualChannelQuery(_wtsHandle, WTS_VIRTUAL_CLASS.WTSVirtualFileHandle, out vcFileHandlePtr, out uint len))
                    return GetLastError();

                IntPtr hWTSFileHandle = Marshal.ReadIntPtr(vcFileHandlePtr);
                
                if (!DuplicateHandle(GetCurrentProcess(), hWTSFileHandle, GetCurrentProcess(), out phFile, 0, false, DUPLICATE_SAME_ACCESS))
                    return GetLastError();

                return 0;
            }
            finally
            {
                if (vcFileHandlePtr != IntPtr.Zero) WTSFreeMemory(vcFileHandlePtr);
                if (_wtsHandle != IntPtr.Zero) { WTSVirtualChannelClose(_wtsHandle); _wtsHandle = IntPtr.Zero; }
            }
        }

        private void ChannelReaderThread(CancellationToken cancellationToken)
        {
            byte[] buffer = new byte[BUFFER_SIZE * 2];
            byte[] messageBuffer = new byte[BUFFER_SIZE * 8];
            int messageSize = 0;
            bool inFragment = false;
            
            IntPtr hEvent = CreateEvent(IntPtr.Zero, false, false, null);
            if (hEvent == IntPtr.Zero) return;

            Log("Channel reader started");

            try
            {
                while (_isRunning && !cancellationToken.IsCancellationRequested)
                {
                    OVERLAPPED overlapped = new OVERLAPPED { hEvent = hEvent };
                    
                    if (!ReadFile(_channelHandle, buffer, BUFFER_SIZE, out uint bytesRead, ref overlapped))
                    {
                        uint error = GetLastError();
                        if (error == ERROR_IO_PENDING)
                        {
                            if (WaitForSingleObject(overlapped.hEvent, 1000) != WAIT_OBJECT_0) continue;
                            if (!GetOverlappedResult(_channelHandle, ref overlapped, out bytesRead, false)) break;
                        }
                        else break;
                    }

                    if (bytesRead < 8) continue;

                    Log($"Read {bytesRead} bytes from channel");

                    // Parse PDU header
                    uint pduLength = BitConverter.ToUInt32(buffer, 0);
                    uint pduFlags = BitConverter.ToUInt32(buffer, 4);
                    bool isFirst = (pduFlags & CHANNEL_FLAG_FIRST) != 0;
                    bool isLast = (pduFlags & CHANNEL_FLAG_LAST) != 0;
                    
                    // CRITICAL: The pduLength field is the TOTAL message length for fragmentation
                    // NOT the payload size of THIS PDU!
                    // The actual payload size is: bytesRead - 8
                    int actualPayloadSize = (int)bytesRead - 8;

                    Log($"PDU: totalMsgLen={pduLength}, flags=0x{pduFlags:X} (FIRST={isFirst}, LAST={isLast}), actualPayload={actualPayloadSize}");

                    // Handle fragmentation
                    if (isFirst && isLast)
                    {
                        // Complete message in one PDU
                        Log($"Complete message: {actualPayloadSize} bytes");
                        ProcessMessage(buffer, 8, actualPayloadSize);
                    }
                    else if (isFirst)
                    {
                        // Start new fragment
                        if (actualPayloadSize > messageBuffer.Length)
                        {
                            Log($"ERROR: Fragment too large ({actualPayloadSize} bytes), skipping");
                            inFragment = false;
                            continue;
                        }
                        messageSize = 0;
                        Buffer.BlockCopy(buffer, 8, messageBuffer, 0, actualPayloadSize);
                        messageSize = actualPayloadSize;
                        inFragment = true;
                        Log($"Fragment START: {messageSize} bytes (total will be ~{pduLength})");
                    }
                    else if (isLast && inFragment)
                    {
                        // Complete fragment
                        if (messageSize + actualPayloadSize > messageBuffer.Length)
                        {
                            Log($"ERROR: Fragment overflow, discarding");
                            inFragment = false;
                            messageSize = 0;
                            continue;
                        }
                        Buffer.BlockCopy(buffer, 8, messageBuffer, messageSize, actualPayloadSize);
                        messageSize += actualPayloadSize;
                        Log($"Fragment END: total {messageSize} bytes assembled");
                        ProcessMessage(messageBuffer, 0, messageSize);
                        inFragment = false;
                        messageSize = 0;
                    }
                    else if (inFragment)
                    {
                        // Middle fragment
                        if (messageSize + actualPayloadSize > messageBuffer.Length)
                        {
                            Log($"ERROR: Fragment overflow, discarding");
                            inFragment = false;
                            messageSize = 0;
                            continue;
                        }
                        Buffer.BlockCopy(buffer, 8, messageBuffer, messageSize, actualPayloadSize);
                        messageSize += actualPayloadSize;
                        Log($"Fragment MIDDLE: {messageSize} bytes so far (total will be ~{pduLength})");
                    }
                    else
                    {
                        // Orphaned fragment
                        Log($"WARNING: Orphaned fragment (FIRST={isFirst}, LAST={isLast}), skipping {actualPayloadSize} bytes");
                        // Reset fragment state if we're orphaned
                        inFragment = false;
                        messageSize = 0;
                    }
                }
            }
            catch (Exception ex)
            {
                Log($"Channel reader error: {ex.Message}");
                Log($"Stack trace: {ex.StackTrace}");
            }
            finally
            {
                CloseHandle(hEvent);
                Log("Channel reader stopped");
            }
        }

        private void ProcessMessage(byte[] data, int offset, int length)
        {
            int pos = offset;
            int end = offset + length;
            int packetCount = 0;

            while (pos + 9 <= end)
            {
                uint threadId = BitConverter.ToUInt32(data, pos);
                uint dataLen = BitConverter.ToUInt32(data, pos + 4);
                byte closeFlag = data[pos + 8];

                int packetSize = 9 + (int)dataLen;

                // Sanity check: packet size must be reasonable
                if (dataLen > 1000000)
                {
                    Log($"Invalid packet size {dataLen}, stopping");
                    break;
                }

                // Check if we have the complete packet
                if (pos + packetSize > end)
                {
                    Log($"Incomplete packet: need {packetSize}, have {end - pos}");
                    break;
                }

                packetCount++;
                Log($"Packet {packetCount}: thread={threadId:X8}, len={dataLen}, close={closeFlag}");

                if (_socksClients.TryGetValue(threadId, out TcpClient? client) && client?.Connected == true)
                {
                    try
                    {
                        if (dataLen > 0)
                        {
                            client.GetStream().Write(data, pos + 9, (int)dataLen);
                            client.GetStream().Flush();
                            Log($"Forwarded {dataLen} bytes to client {threadId:X8}");
                        }

                        if (closeFlag == 0x01)
                        {
                            Log($"Closing client {threadId:X8}");
                            _socksClients.TryRemove(threadId, out _);
                            client.Close();
                        }
                    }
                    catch (Exception ex)
                    {
                        Log($"Error forwarding to {threadId:X8}: {ex.Message}");
                        _socksClients.TryRemove(threadId, out _);
                        client?.Close();
                    }
                }
                else
                {
                    Log($"No client for thread {threadId:X8}");
                }

                pos += packetSize;
            }
        }

        private void SocksServerThread(CancellationToken cancellationToken)
        {
            try
            {
                _socksListener = new TcpListener(IPAddress.Loopback, _socksPort);
                _socksListener.Start();
                Log($"SOCKS proxy: 127.0.0.1:{_socksPort}");

                while (_isRunning && !cancellationToken.IsCancellationRequested)
                {
                    if (_socksListener.Pending())
                    {
                        TcpClient client = _socksListener.AcceptTcpClient();
                        uint threadId = _nextThreadId++;
                        _socksClients.TryAdd(threadId, client);
                        Log($"SOCKS client connected: {threadId:X8}");
                        Task.Run(() => HandleSocksClient(client, threadId, cancellationToken), cancellationToken);
                    }
                    else
                    {
                        Thread.Sleep(100);
                    }
                }
            }
            catch (Exception ex)
            {
                Log($"SOCKS server error: {ex.Message}");
            }
            finally
            {
                _socksListener?.Stop();
                Log("SOCKS server stopped");
            }
        }

        private async Task HandleSocksClient(TcpClient client, uint threadId, CancellationToken cancellationToken)
        {
            try
            {
                NetworkStream stream = client.GetStream();
                byte[] buffer = new byte[BUFFER_SIZE];
                byte[] sendBuffer = new byte[BUFFER_SIZE + 9];

                while (_isRunning && !cancellationToken.IsCancellationRequested && client.Connected)
                {
                    int bytesRead = await stream.ReadAsync(buffer, 0, BUFFER_SIZE, cancellationToken);
                    if (bytesRead <= 0) break;

                    // Prepare packet: threadId (4) + length (4) + closeFlag (1) + data
                    Buffer.BlockCopy(BitConverter.GetBytes(threadId), 0, sendBuffer, 0, 4);
                    Buffer.BlockCopy(BitConverter.GetBytes((uint)bytesRead), 0, sendBuffer, 4, 4);
                    sendBuffer[8] = 0x00;
                    Buffer.BlockCopy(buffer, 0, sendBuffer, 9, bytesRead);

                    if (!WriteToChannel(sendBuffer, 9 + bytesRead)) break;
                }

                // Send close
                Buffer.BlockCopy(BitConverter.GetBytes(threadId), 0, sendBuffer, 0, 4);
                Buffer.BlockCopy(BitConverter.GetBytes(0u), 0, sendBuffer, 4, 4);
                sendBuffer[8] = 0x01;
                WriteToChannel(sendBuffer, 9);

                _socksClients.TryRemove(threadId, out _);
                client.Close();
            }
            catch
            {
                _socksClients.TryRemove(threadId, out _);
                client?.Close();
            }
        }

        private bool WriteToChannel(byte[] buffer, int length)
        {
            IntPtr hEvent = CreateEvent(IntPtr.Zero, false, false, null);
            if (hEvent == IntPtr.Zero) return false;

            try
            {
                OVERLAPPED overlapped = new OVERLAPPED { hEvent = hEvent };
                
                if (!WriteFile(_channelHandle, buffer, (uint)length, out uint bytesWritten, ref overlapped))
                {
                    if (GetLastError() == ERROR_IO_PENDING)
                    {
                        WaitForSingleObject(overlapped.hEvent, INFINITE);
                        return GetOverlappedResult(_channelHandle, ref overlapped, out bytesWritten, true);
                    }
                    return false;
                }
                return true;
            }
            finally
            {
                CloseHandle(hEvent);
            }
        }

        public void Stop()
        {
            if (!_isRunning) return;

            _isRunning = false;
            _cancellationTokenSource?.Cancel();

            foreach (var kvp in _socksClients)
            {
                try { kvp.Value.Close(); } catch { }
            }
            _socksClients.Clear();

            _socksListener?.Stop();

            if (_channelHandle != IntPtr.Zero)
            {
                CloseHandle(_channelHandle);
                _channelHandle = IntPtr.Zero;
            }

            _channelThread?.Join(5000);
            _socksServerThread?.Join(5000);
            _cancellationTokenSource?.Dispose();
        }

        private void Log(string message)
        {
            OnLog?.Invoke($"[{DateTime.Now:HH:mm:ss.fff}] {message}");
        }
    }
}
