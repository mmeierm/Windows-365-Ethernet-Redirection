using System;
using System.Diagnostics;
using System.IO;
using System.Threading;
using System.Threading.Tasks;

namespace Windows_365_Ethernet_Redirection
{
    public class SocksServerWrapper
    {
        private Process? _socksProcess;
        private readonly string _executablePath;
        private bool _isRunning = false;
        private int _socksPort = 1080;
        private CancellationTokenSource? _monitoringCancellationTokenSource;

        public event Action<string>? OnLog;
        public event Action? OnServerStopped;
        public bool IsConnected => _isRunning && _socksProcess != null && !_socksProcess.HasExited;
        public int SocksPort
        {
            get => _socksPort;
            set => _socksPort = value;
        }

        public SocksServerWrapper()
        {
            // Look for the executable in the application directory
            string appDir = AppDomain.CurrentDomain.BaseDirectory;
            _executablePath = Path.Combine(appDir, "SocksOverRDP-Server.exe");
        }

        public bool Start()
        {
            if (_isRunning)
            {
                Log("SOCKS server is already running");
                return false;
            }

            if (!File.Exists(_executablePath))
            {
                Log($"ERROR: SocksOverRDP-Server.exe not found at: {_executablePath}");
                Log("Please ensure SocksOverRDP-Server.exe is in the application directory");
                return false;
            }

            try
            {
                Log($"Starting SocksOverRDP-Server.exe...");

                var startInfo = new ProcessStartInfo
                {
                    FileName = _executablePath,
                    Arguments = "",
                    UseShellExecute = false,
                    CreateNoWindow = true,
                    RedirectStandardOutput = true,
                    RedirectStandardError = true,
                    WorkingDirectory = Path.GetDirectoryName(_executablePath)
                };

                _socksProcess = new Process { StartInfo = startInfo };

                // Capture output for logging
                _socksProcess.OutputDataReceived += (sender, e) =>
                {
                    if (!string.IsNullOrEmpty(e.Data))
                        Log($"[SocksServer] {e.Data}");
                };

                _socksProcess.ErrorDataReceived += (sender, e) =>
                {
                    if (!string.IsNullOrEmpty(e.Data))
                        Log($"[SocksServer ERROR] {e.Data}");
                };

                _socksProcess.Exited += (sender, e) =>
                {
                    if (_isRunning)
                    {
                        Log("WARNING: SOCKS server process exited unexpectedly!");
                        HandleUnexpectedExit();
                    }
                };

                _socksProcess.EnableRaisingEvents = true;

                if (!_socksProcess.Start())
                {
                    Log("ERROR: Failed to start SOCKS server process");
                    return false;
                }

                _socksProcess.BeginOutputReadLine();
                _socksProcess.BeginErrorReadLine();

                _isRunning = true;
                Log($"SOCKS server started (PID: {_socksProcess.Id})");

                // Start monitoring thread
                _monitoringCancellationTokenSource = new CancellationTokenSource();
                Task.Run(() => MonitorProcessAsync(_monitoringCancellationTokenSource.Token));

                return true;
            }
            catch (Exception ex)
            {
                Log($"ERROR: Failed to start SOCKS server: {ex.Message}");
                _isRunning = false;
                return false;
            }
        }

        private async Task MonitorProcessAsync(CancellationToken cancellationToken)
        {
            try
            {
                while (!cancellationToken.IsCancellationRequested && _isRunning)
                {
                    await Task.Delay(1000, cancellationToken);

                    if (_socksProcess == null || _socksProcess.HasExited)
                    {
                        if (_isRunning)
                        {
                            Log("Process monitoring detected SOCKS server is no longer running");
                            HandleUnexpectedExit();
                        }
                        break;
                    }
                }
            }
            catch (OperationCanceledException)
            {
                // Normal cancellation, ignore
            }
            catch (Exception ex)
            {
                Log($"Error in process monitoring: {ex.Message}");
            }
        }

        private void HandleUnexpectedExit()
        {
            _isRunning = false;
            Log("Cleaning up after unexpected server exit...");
            
            // Notify UI that server has stopped (will trigger VPN cleanup)
            OnServerStopped?.Invoke();
        }

        public void Stop()
        {
            if (!_isRunning || _socksProcess == null)
                return;

            try
            {
                Log("Stopping SOCKS server...");

                // Cancel monitoring
                _monitoringCancellationTokenSource?.Cancel();

                if (!_socksProcess.HasExited)
                {
                    _socksProcess.Kill();
                    _socksProcess.WaitForExit(5000);
                }

                _socksProcess.Dispose();
                _socksProcess = null;
                _isRunning = false;

                _monitoringCancellationTokenSource?.Dispose();
                _monitoringCancellationTokenSource = null;

                Log("SOCKS server stopped");
            }
            catch (Exception ex)
            {
                Log($"ERROR: Failed to stop SOCKS server: {ex.Message}");
            }
        }

        private void Log(string message)
        {
            OnLog?.Invoke($"[{DateTime.Now:HH:mm:ss.fff}] {message}");
        }
    }
}
