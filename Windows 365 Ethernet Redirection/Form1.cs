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
            }
            else
            {
                // Connect
                bool success = _socksServer.Start();
                UpdateUI(success);
                
                if (!success)
                {
                    MessageBox.Show(
                        "Failed to start the SOCKS server. Make sure you are running this application in an RDP session.",
                        "Connection Error",
                        MessageBoxButtons.OK,
                        MessageBoxIcon.Error);
                }
            }
        }

        private void chkDebugOutput_CheckedChanged(object sender, EventArgs e)
        {
            _debugEnabled = chkDebugOutput.Checked;
            
            if (!_debugEnabled)
            {
                txtDebugOutput.Clear();
            }
            
            LogMessage($"Debug output {(_debugEnabled ? "enabled" : "disabled")}");
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
            LogMessage(message);
        }

        private void LogMessage(string message)
        {
            if (!_debugEnabled)
                return;

            if (InvokeRequired)
            {
                Invoke(() => LogMessage(message));
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
