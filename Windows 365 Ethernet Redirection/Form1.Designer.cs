namespace Windows_365_Ethernet_Redirection
{
    partial class Form1
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
            btnConnect.Location = new Point(12, 12);
            btnConnect.Name = "btnConnect";
            btnConnect.Size = new Size(120, 30);
            btnConnect.TabIndex = 0;
            btnConnect.Text = "Connect";
            btnConnect.UseVisualStyleBackColor = true;
            btnConnect.Click += btnConnect_Click;
            // 
            // chkDebugOutput
            // 
            chkDebugOutput.AutoSize = true;
            chkDebugOutput.Location = new Point(150, 17);
            chkDebugOutput.Name = "chkDebugOutput";
            chkDebugOutput.Size = new Size(140, 19);
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
            chkEnableVpn.Location = new Point(310, 17);
            chkEnableVpn.Name = "chkEnableVpn";
            chkEnableVpn.Size = new Size(150, 19);
            chkEnableVpn.TabIndex = 4;
            chkEnableVpn.Text = "Auto-start VPN Tunnel";
            chkEnableVpn.UseVisualStyleBackColor = true;
            // 
            // lblSubnet
            // 
            lblSubnet.AutoSize = true;
            lblSubnet.Location = new Point(470, 18);
            lblSubnet.Name = "lblSubnet";
            lblSubnet.Size = new Size(47, 15);
            lblSubnet.TabIndex = 5;
            lblSubnet.Text = "Subnet:";
            // 
            // txtSubnet
            // 
            txtSubnet.Location = new Point(523, 15);
            txtSubnet.Name = "txtSubnet";
            txtSubnet.PlaceholderText = "0.0.0.0/0 (all traffic)";
            txtSubnet.Size = new Size(130, 23);
            txtSubnet.TabIndex = 6;
            txtSubnet.Text = "0.0.0.0/0";
            // 
            // txtDebugOutput
            // 
            txtDebugOutput.Anchor = AnchorStyles.Top | AnchorStyles.Bottom | AnchorStyles.Left | AnchorStyles.Right;
            txtDebugOutput.BackColor = SystemColors.Window;
            txtDebugOutput.Font = new Font("Consolas", 9F);
            txtDebugOutput.Location = new Point(12, 75);
            txtDebugOutput.Multiline = true;
            txtDebugOutput.Name = "txtDebugOutput";
            txtDebugOutput.ReadOnly = true;
            txtDebugOutput.ScrollBars = ScrollBars.Both;
            txtDebugOutput.Size = new Size(639, 244);
            txtDebugOutput.TabIndex = 2;
            txtDebugOutput.WordWrap = false;
            // 
            // lblStatus
            // 
            lblStatus.AutoSize = true;
            lblStatus.Location = new Point(12, 50);
            lblStatus.Name = "lblStatus";
            lblStatus.Size = new Size(77, 15);
            lblStatus.TabIndex = 3;
            lblStatus.Text = "Status: Ready";
            // 
            // Form1
            // 
            AutoScaleDimensions = new SizeF(7F, 15F);
            AutoScaleMode = AutoScaleMode.Font;
            ClientSize = new Size(663, 331);
            Controls.Add(txtSubnet);
            Controls.Add(lblSubnet);
            Controls.Add(chkEnableVpn);
            Controls.Add(lblStatus);
            Controls.Add(txtDebugOutput);
            Controls.Add(chkDebugOutput);
            Controls.Add(btnConnect);
            Name = "Form1";
            Text = "Windows 365 Ethernet Redirection - SOCKS over RDP";
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
