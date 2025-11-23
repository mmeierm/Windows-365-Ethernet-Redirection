namespace Windows_365_Ethernet_Redirection
{
    public partial class Form1 : Form
    {
        private SocksServer? _socksServer;
        private bool _debugEnabled = false;

        public Form1()
        {
            InitializeComponent();
            InitializeServer();
        }

        private void InitializeServer()
        {
            _socksServer = new SocksServer();
            _socksServer.OnLog += SocksServer_OnLog;
        }

        private void btnConnect_Click(object sender, EventArgs e)
        {
            if (_socksServer == null)
                return;

            if (_socksServer.IsConnected)
            {
                // Disconnect
                _socksServer.Stop();
                UpdateUI(false);
                LogMessage("Disconnected", alwaysShow: true);
            }
            else
            {
                // Connect
                bool success = _socksServer.Start();
                UpdateUI(success);

                if (!success)
                {
                    LogMessage("Failed to start SOCKS server", alwaysShow: true);
                    MessageBox.Show(
                        "Failed to start the SOCKS server. Make sure you are running this application in an RDP session.",
                        "Connection Error",
                        MessageBoxButtons.OK,
                        MessageBoxIcon.Error);
                }
                else
                {
                    LogMessage("Connected!", alwaysShow: true);
                    LogMessage("Starting VPN Connection", alwaysShow: true);
                }
            }
        }

        private void chkDebugOutput_CheckedChanged(object sender, EventArgs e)
        {
            _debugEnabled = chkDebugOutput.Checked;

            if (!_debugEnabled)
            {
                txtDebugOutput.Clear();
                LogMessage("Debug output disabled - only showing important messages", alwaysShow: true);
            }
            else
            {
                LogMessage("Debug output enabled - showing all messages", alwaysShow: true);
            }
        }

        private void UpdateUI(bool connected)
        {
            if (InvokeRequired)
            {
                Invoke(() => UpdateUI(connected));
                return;
            }

            btnConnect.Text = connected ? "Disconnect" : "Connect";
            lblStatus.Text = connected ? "Status: Connected" : "Status: Disconnected";
            lblStatus.ForeColor = connected ? Color.Green : Color.Black;
        }

        private void SocksServer_OnLog(string message)
        {
            // Determine if this is an important message that should always be shown
            bool isImportant = message.Contains("connected", StringComparison.OrdinalIgnoreCase) ||
                              message.Contains("CLIENT PLUGIN IS CONNECTED", StringComparison.OrdinalIgnoreCase) ||
                              message.Contains("started", StringComparison.OrdinalIgnoreCase) ||
                              message.Contains("stopped", StringComparison.OrdinalIgnoreCase) ||
                              message.Contains("error", StringComparison.OrdinalIgnoreCase) ||
                              message.Contains("failed", StringComparison.OrdinalIgnoreCase);

            LogMessage(message, alwaysShow: isImportant);
        }

        private void LogMessage(string message, bool alwaysShow = false)
        {
            if (!_debugEnabled && !alwaysShow)
                return;

            if (InvokeRequired)
            {
                Invoke(() => LogMessage(message, alwaysShow));
                return;
            }

            txtDebugOutput.AppendText(message + Environment.NewLine);

            // Auto-scroll to bottom
            txtDebugOutput.SelectionStart = txtDebugOutput.Text.Length;
            txtDebugOutput.ScrollToCaret();
        }

        private void Form1_FormClosing(object sender, FormClosingEventArgs e)
        {
            if (_socksServer?.IsConnected == true)
            {
                _socksServer.Stop();
            }
        }

    }
}
