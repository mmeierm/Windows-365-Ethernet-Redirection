# VPN Tunnel Setup

This application includes integration with tun2socks to automatically route all system traffic through the SOCKS proxy.

## Setup Instructions

1. Download the tun2socks executable for Windows from:
   https://github.com/xjasonlyu/tun2socks/releases

2. Create a folder structure:
   ```
   Windows 365 Ethernet Redirection/
   ??? Windows 365 Ethernet Redirection.exe
   ??? SocksToVPN/
       ??? tun2socks.exe
   ```

3. Place the downloaded `tun2socks.exe` file in the `SocksToVPN` subfolder

## Requirements

- **Administrator Privileges**: The application MUST be run as administrator when using VPN tunnel mode
  - Right-click on the executable and select "Run as administrator"
  - This is required to configure network interfaces and routing

- **Wintun Driver**: The tun2socks application will automatically install the Wintun driver on first run

## How It Works

When VPN tunnel is enabled, the application will:

1. Start tun2socks with a Wintun virtual network adapter
2. Configure the virtual interface with IP address `192.168.123.1/24`
3. Set DNS to `8.8.8.8` for the virtual interface
4. Add a default route through the virtual interface with metric 1
5. All system traffic is now routed through the SOCKS proxy over RDP

## Usage

- **With VPN Tunnel (Default)**: Check "Auto-start VPN Tunnel (All Traffic)" before connecting
  - All system traffic will be routed through the RDP connection
  - Works system-wide (no browser configuration needed)
  - Requires administrator privileges
  - Creates a "wintun" network interface

- **Without VPN Tunnel**: Uncheck the VPN option
  - Only applications configured to use the SOCKS proxy will use the tunnel
  - Configure your browser/apps to use SOCKS5 proxy: `127.0.0.1:1080`
  - No administrator privileges required

## Notes

- The VPN tunnel option is enabled by default
- You can toggle the VPN option before connecting
- Once connected, the VPN setting cannot be changed until you disconnect
- The executable must be in the SocksToVPN subfolder for auto-detection
- Network routes are automatically cleaned up when disconnecting

## Network Configuration Details

The VPN tunnel configures:
- **Interface**: wintun (virtual TUN device)
- **IP Address**: 192.168.123.1/24
- **DNS Server**: 8.8.8.8
- **Default Route**: 0.0.0.0/0 via wintun with metric 1

## Troubleshooting

### "ERROR: SocksToVPN executable not found"
1. Verify the file path: `<app directory>\SocksToVPN\tun2socks.exe`
2. Ensure you downloaded the Windows version
3. Check that the filename is exactly `tun2socks.exe`

### "ERROR: Administrator privileges required"
1. Close the application
2. Right-click on the executable
3. Select "Run as administrator"
4. Try connecting again

### VPN tunnel starts but no internet
1. Check the debug output for configuration errors
2. Verify the wintun interface was created: `ipconfig /all`
3. Check routes: `route print`
4. Ensure the SOCKS proxy is working (test without VPN first)

### Cannot connect after disconnecting
1. Routes may not have been cleaned up properly
2. Manually remove: `netsh interface ipv4 delete route 0.0.0.0/0 "wintun"`
3. Restart the application
