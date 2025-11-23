using System;
using System.Diagnostics;
using System.IO;
using System.Security.Principal;
using System.Threading.Tasks;

namespace Windows_365_Ethernet_Redirection
{
    public class SocksToVpnManager
    {
        private Process? _vpnProcess;
        private readonly string _executablePath;
        private bool _isRunning = false;

        public event Action<string>? OnLog;
        public bool IsRunning => _isRunning && _vpnProcess != null && !_vpnProcess.HasExited;

        private string _subnet = "0.0.0.0/0";

        public SocksToVpnManager()
        {
            // Look for the executable in the application directory
            string appDir = AppDomain.CurrentDomain.BaseDirectory;
            _executablePath = Path.Combine(appDir, "SocksToVPN", "tun2socks.exe");
        }

        public async Task<bool> StartAsync(string socksAddress, int socksPort, string subnet = "0.0.0.0/0")
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

            // Check for administrator privileges
            if (!IsAdministrator())
            {
                Log("ERROR: Administrator privileges required to configure network interfaces");
                Log("Please run the application as administrator");
                return false;
            }

            try
            {
                // Store subnet for cleanup later
                _subnet = subnet;
                
                Log("Starting SOCKS to VPN tunnel...");
                Log($"Routing subnet: {_subnet}");

                var startInfo = new ProcessStartInfo
                {
                    FileName = _executablePath,
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
                        Log($"[tun2socks] {e.Data}");
                };
                
                _vpnProcess.ErrorDataReceived += (sender, e) =>
                {
                    if (!string.IsNullOrEmpty(e.Data))
                        Log($"[tun2socks ERROR] {e.Data}");
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
                
                // Wait for the interface to be created by tun2socks
                Log("Waiting for wintun interface to be created...");
                await Task.Delay(5000);
                
                // Configure the network interface
                await ConfigureNetworkInterfaceAsync();

                return true;
            }
            catch (Exception ex)
            {
                Log($"ERROR: Failed to start VPN tunnel: {ex.Message}");
                _isRunning = false;
                return false;
            }
        }

        private async Task ConfigureNetworkInterfaceAsync()
        {
            try
            {
                // Configure IP address for the wintun interface
                Log("Configuring wintun interface IP address...");
                ExecuteCommand("netsh", "interface ipv4 set address name=\"wintun\" source=static addr=192.168.123.1 mask=255.255.255.0");
                
                await Task.Delay(1000); // Brief delay between commands
                
                // Set DNS for the interface
                Log("Configuring DNS settings...");
                ExecuteCommand("netsh", "interface ipv4 set dnsservers name=\"wintun\" static address=8.8.8.8 register=none validate=no");
                
                await Task.Delay(1000);
                
                // Configure routing to redirect traffic through the TUN interface
                Log($"Configuring routing for {_subnet}...");
                ExecuteCommand("netsh", $"interface ipv4 add route {_subnet} \"wintun\" 192.168.123.1 metric=1");
                
                Log("Network interface configuration complete");
                
                if (_subnet == "0.0.0.0/0")
                {
                    Log("All network traffic will now be routed through the SOCKS proxy");
                }
                else
                {
                    Log($"Traffic to {_subnet} will be routed through the SOCKS proxy");
                }
            }
            catch (Exception ex)
            {
                Log($"ERROR: Failed to configure network interface: {ex.Message}");
            }
        }

        private void ExecuteCommand(string command, string arguments)
        {
            try
            {
                var process = new Process
                {
                    StartInfo = new ProcessStartInfo
                    {
                        FileName = command,
                        Arguments = arguments,
                        RedirectStandardOutput = true,
                        RedirectStandardError = true,
                        UseShellExecute = false,
                        CreateNoWindow = true
                    }
                };

                process.Start();
                string output = process.StandardOutput.ReadToEnd();
                string error = process.StandardError.ReadToEnd();
                process.WaitForExit();

                if (!string.IsNullOrEmpty(output))
                    Log($"[netsh] {output.Trim()}");
                
                if (!string.IsNullOrEmpty(error) && !error.Contains("Ok."))
                    Log($"[netsh ERROR] {error.Trim()}");
                
                Log($"Executed: {command} {arguments}");
            }
            catch (Exception ex)
            {
                Log($"ERROR executing command: {ex.Message}");
            }
        }

        private bool IsAdministrator()
        {
            try
            {
                using var identity = WindowsIdentity.GetCurrent();
                var principal = new WindowsPrincipal(identity);
                return principal.IsInRole(WindowsBuiltInRole.Administrator);
            }
            catch
            {
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
                
                // Remove the route before stopping
                try
                {
                    Log("Removing routing configuration...");
                    ExecuteCommand("netsh", $"interface ipv4 delete route {_subnet} \"wintun\"");
                }
                catch (Exception ex)
                {
                    Log($"Warning: Failed to remove route: {ex.Message}");
                }
                
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
