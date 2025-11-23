# VPN Tunnel Setup

This application includes integration with tun2socks to automatically route all system traffic (or specific subnets) through the SOCKS proxy.

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

## Subnet Routing Configuration

The subnet field allows you to control which traffic gets routed through the VPN tunnel:

### Examples:

- **`0.0.0.0/0`** (default): Routes ALL traffic through the tunnel
  - Use this for complete VPN functionality
  - All applications will use the tunnel

- **`192.168.1.0/24`**: Only routes traffic to the 192.168.1.x subnet
  - Use this to access specific remote networks
  - Prevents routing loops
  - Local traffic stays local

- **`10.0.0.0/8`**: Routes all 10.x.x.x traffic
  - Useful for corporate networks

- **`172.16.0.0/12`**: Routes 172.16.x.x to 172.31.x.x

### Preventing Routing Loops

If you experience connectivity issues or routing loops when using `0.0.0.0/0`, try:

1. Specify only the subnet you need to access remotely
2. Example: If the remote network is `192.168.100.0/24`, enter that instead of `0.0.0.0/0`
3. This prevents routing your local traffic through the tunnel unnecessarily

## How It Works

When VPN tunnel is enabled, the application will:

1. Start tun2socks with a Wintun virtual network adapter
2. Configure the virtual interface with IP address `192.168.123.1/24`
3. Set DNS to `8.8.8.8` for the virtual interface
4. Add a route for your specified subnet through the virtual interface with metric 1
5. Traffic matching the subnet is now routed through the SOCKS proxy over RDP

## Usage

- **With VPN Tunnel (Default)**: 
  - Check "Auto-start VPN Tunnel"
  - Enter the subnet to route (or leave as `0.0.0.0/0` for all traffic)
  - Click Connect
  - All traffic matching the subnet will be routed through the RDP connection
  - Works system-wide (no browser configuration needed)
  - Requires administrator privileges
  - Creates a "wintun" network interface

- **Without VPN Tunnel**: 
  - Uncheck the VPN option
  - Only applications configured to use the SOCKS proxy will use the tunnel
  - Configure your browser/apps to use SOCKS5 proxy: `127.0.0.1:1080`
  - No administrator privileges required

## Notes

- The VPN tunnel option is enabled by default
- The subnet field defaults to `0.0.0.0/0` (all traffic)
- You can change the subnet before connecting
- Once connected, the subnet setting cannot be changed until you disconnect
- The executable must be in the SocksToVPN subfolder for auto-detection
- Routes are automatically cleaned up when disconnecting

## Network Configuration Details

The VPN tunnel configures:
- **Interface**: wintun (virtual TUN device)
- **IP Address**: 192.168.123.1/24
- **DNS Server**: 8.8.8.8
- **Route**: {your subnet} via wintun with metric 1

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
5. Try using a specific subnet instead of `0.0.0.0/0`

### Cannot connect after disconnecting
1. Routes may not have been cleaned up properly
2. Manually remove: `netsh interface ipv4 delete route {your subnet} "wintun"`
3. Restart the application

### Routing loop / No connectivity with 0.0.0.0/0
1. Change the subnet to only the network you need
2. Example: Enter `192.168.1.0/24` instead of `0.0.0.0/0`
3. This prevents routing your local/RDP traffic through the tunnel
4. Only traffic to the specified subnet goes through the tunnel

### Invalid subnet format warning
1. Ensure format is: `x.x.x.x/y` (e.g., `192.168.1.0/24`)
2. IP address must be valid (each octet 0-255)
3. Prefix must be 0-32
4. The application will fall back to `0.0.0.0/0` if invalid
