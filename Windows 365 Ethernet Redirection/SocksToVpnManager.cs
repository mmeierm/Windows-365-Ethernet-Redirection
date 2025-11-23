using System;
using System.Diagnostics;
using System.IO;

namespace Windows_365_Ethernet_Redirection
{
    public class SocksToVpnManager
    {
        private Process? _vpnProcess;
        private readonly string _executablePath;
        private bool _isRunning = false;

        public event Action<string>? OnLog;
        public bool IsRunning => _isRunning && _vpnProcess != null && !_vpnProcess.HasExited;

        public SocksToVpnManager()
        {
            // Look for the executable in the application directory
            string appDir = AppDomain.CurrentDomain.BaseDirectory;
            _executablePath = Path.Combine(appDir, "SocksToVPN", "tun2socks.exe");
        }

        public bool Start(string socksAddress, int socksPort)
        {
            if (_isRunning)
            {
                Log("VPN tunnel is already running");
                return false;
            }

            if (!File.Exists(_executablePath))
            {
                Log($"ERROR: SocksToVPN executable not found at: {_executablePath}");
                Log("Please ensure tun2socks.exe is in the SocksToVPN subfolder");
                return false;
            }

            try
            {
                Log("Starting SOCKS to VPN tunnel...");

                var startInfo = new ProcessStartInfo
                {
                    FileName = _executablePath,
                    //Arguments = $"--socks-addr {socksAddress}:{socksPort}",
                    Arguments = $"-device wintun -proxy socks5://{socksAddress}:{socksPort}",
                    UseShellExecute = false,
                    CreateNoWindow = true,
                    RedirectStandardOutput = true,
                    RedirectStandardError = true,
                    WorkingDirectory = Path.GetDirectoryName(_executablePath)
                };

                _vpnProcess = new Process { StartInfo = startInfo };
                
                // Capture output for logging
                _vpnProcess.OutputDataReceived += (sender, e) =>
                {
                    if (!string.IsNullOrEmpty(e.Data))
                        Log($"[VPN] {e.Data}");
                };
                
                _vpnProcess.ErrorDataReceived += (sender, e) =>
                {
                    if (!string.IsNullOrEmpty(e.Data))
                        Log($"[VPN ERROR] {e.Data}");
                };

                _vpnProcess.Exited += (sender, e) =>
                {
                    _isRunning = false;
                    Log("VPN tunnel process exited");
                };

                _vpnProcess.EnableRaisingEvents = true;

                if (!_vpnProcess.Start())
                {
                    Log("ERROR: Failed to start VPN tunnel process");
                    return false;
                }

                _vpnProcess.BeginOutputReadLine();
                _vpnProcess.BeginErrorReadLine();

                _isRunning = true;
                Log($"VPN tunnel started (PID: {_vpnProcess.Id})");
                Log("All network traffic will now be routed through the SOCKS proxy");
                return true;
            }
            catch (Exception ex)
            {
                Log($"ERROR: Failed to start VPN tunnel: {ex.Message}");
                _isRunning = false;
                return false;
            }
        }

        public void Stop()
        {
            if (!_isRunning || _vpnProcess == null)
                return;

            try
            {
                Log("Stopping VPN tunnel...");
                
                if (!_vpnProcess.HasExited)
                {
                    _vpnProcess.Kill();
                    _vpnProcess.WaitForExit(5000);
                }

                _vpnProcess.Dispose();
                _vpnProcess = null;
                _isRunning = false;

                Log("VPN tunnel stopped");
            }
            catch (Exception ex)
            {
                Log($"ERROR: Failed to stop VPN tunnel: {ex.Message}");
            }
        }

        private void Log(string message)
        {
            OnLog?.Invoke($"[{DateTime.Now:HH:mm:ss.fff}] {message}");
        }
    }
}
