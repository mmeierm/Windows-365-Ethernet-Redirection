namespace Windows_365_Ethernet_Redirection
{
    partial class UI
    {
        /// <summary>
        ///  Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        ///  Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        ///  Required method for Designer support - do not modify
        ///  the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            btnConnect = new Button();
            chkDebugOutput = new CheckBox();
            chkEnableVpn = new CheckBox();
            txtSubnet = new TextBox();
            lblSubnet = new Label();
            txtDebugOutput = new TextBox();
            lblStatus = new Label();
            SuspendLayout();
            // 
            // btnConnect
            // 
            btnConnect.Location = new Point(17, 20);
            btnConnect.Margin = new Padding(4, 5, 4, 5);
            btnConnect.Name = "btnConnect";
            btnConnect.Size = new Size(171, 50);
            btnConnect.TabIndex = 0;
            btnConnect.Text = "Connect";
            btnConnect.UseVisualStyleBackColor = true;
            btnConnect.Click += btnConnect_Click;
            // 
            // chkDebugOutput
            // 
            chkDebugOutput.AutoSize = true;
            chkDebugOutput.Location = new Point(214, 28);
            chkDebugOutput.Margin = new Padding(4, 5, 4, 5);
            chkDebugOutput.Name = "chkDebugOutput";
            chkDebugOutput.Size = new Size(211, 29);
            chkDebugOutput.TabIndex = 1;
            chkDebugOutput.Text = "Enable Debug Output";
            chkDebugOutput.UseVisualStyleBackColor = true;
            chkDebugOutput.CheckedChanged += chkDebugOutput_CheckedChanged;
            // 
            // chkEnableVpn
            // 
            chkEnableVpn.AutoSize = true;
            chkEnableVpn.Checked = true;
            chkEnableVpn.CheckState = CheckState.Checked;
            chkEnableVpn.Location = new Point(443, 28);
            chkEnableVpn.Margin = new Padding(4, 5, 4, 5);
            chkEnableVpn.Name = "chkEnableVpn";
            chkEnableVpn.Size = new Size(215, 29);
            chkEnableVpn.TabIndex = 4;
            chkEnableVpn.Text = "Auto-start VPN Tunnel";
            chkEnableVpn.UseVisualStyleBackColor = true;
            // 
            // txtSubnet
            // 
            txtSubnet.Location = new Point(747, 25);
            txtSubnet.Margin = new Padding(4, 5, 4, 5);
            txtSubnet.Name = "txtSubnet";
            txtSubnet.PlaceholderText = "0.0.0.0/0 (all traffic)";
            txtSubnet.Size = new Size(184, 31);
            txtSubnet.TabIndex = 6;
            txtSubnet.Text = "0.0.0.0/0";
            // 
            // lblSubnet
            // 
            lblSubnet.AutoSize = true;
            lblSubnet.Location = new Point(671, 30);
            lblSubnet.Margin = new Padding(4, 0, 4, 0);
            lblSubnet.Name = "lblSubnet";
            lblSubnet.Size = new Size(72, 25);
            lblSubnet.TabIndex = 5;
            lblSubnet.Text = "Subnet:";
            // 
            // txtDebugOutput
            // 
            txtDebugOutput.Anchor = AnchorStyles.Top | AnchorStyles.Bottom | AnchorStyles.Left | AnchorStyles.Right;
            txtDebugOutput.BackColor = SystemColors.Window;
            txtDebugOutput.Font = new Font("Consolas", 9F);
            txtDebugOutput.Location = new Point(17, 125);
            txtDebugOutput.Margin = new Padding(4, 5, 4, 5);
            txtDebugOutput.Multiline = true;
            txtDebugOutput.Name = "txtDebugOutput";
            txtDebugOutput.ReadOnly = true;
            txtDebugOutput.ScrollBars = ScrollBars.Both;
            txtDebugOutput.Size = new Size(911, 404);
            txtDebugOutput.TabIndex = 2;
            txtDebugOutput.WordWrap = false;
            // 
            // lblStatus
            // 
            lblStatus.AutoSize = true;
            lblStatus.Location = new Point(17, 83);
            lblStatus.Margin = new Padding(4, 0, 4, 0);
            lblStatus.Name = "lblStatus";
            lblStatus.Size = new Size(117, 25);
            lblStatus.TabIndex = 3;
            lblStatus.Text = "Status: Ready";
            // 
            // UI
            // 
            AutoScaleDimensions = new SizeF(10F, 25F);
            AutoScaleMode = AutoScaleMode.Font;
            ClientSize = new Size(947, 552);
            Controls.Add(txtSubnet);
            Controls.Add(lblSubnet);
            Controls.Add(chkEnableVpn);
            Controls.Add(lblStatus);
            Controls.Add(txtDebugOutput);
            Controls.Add(chkDebugOutput);
            Controls.Add(btnConnect);
            Margin = new Padding(4, 5, 4, 5);
            Name = "UI";
            Text = "Windows 365 Ethernet Redirection";
            FormClosing += Form1_FormClosing;
            ResumeLayout(false);
            PerformLayout();
        }

        #endregion

        private Button btnConnect;
        private CheckBox chkDebugOutput;
        private CheckBox chkEnableVpn;
        private TextBox txtSubnet;
        private Label lblSubnet;
        private TextBox txtDebugOutput;
        private Label lblStatus;
    }
}
