# VPN Tunnel Setup

This application includes integration with SocksToVPN to automatically route all system traffic through the SOCKS proxy.

## Setup Instructions

1. Download the SocksToVPN executable for Windows x64 from:
   https://github.com/localtonet/SocksToVPN/releases

2. Create a folder structure:
   ```
   Windows 365 Ethernet Redirection/
   ??? Windows 365 Ethernet Redirection.exe
   ??? SocksToVPN/
       ??? windows_amd64.exe
   ```

3. Place the downloaded `windows_amd64.exe` file in the `SocksToVPN` subfolder

## Usage

- **With VPN Tunnel (Default)**: Check "Auto-start VPN Tunnel (All Traffic)" before connecting
  - All system traffic will be routed through the RDP connection
  - Works system-wide (no browser configuration needed)
  - Requires administrator privileges

- **Without VPN Tunnel**: Uncheck the VPN option
  - Only applications configured to use the SOCKS proxy will use the tunnel
  - Configure your browser/apps to use SOCKS5 proxy: `127.0.0.1:1080`
  - No administrator privileges required

## Notes

- The VPN tunnel option is enabled by default
- You can toggle the VPN option before connecting
- Once connected, the VPN setting cannot be changed until you disconnect
- The executable must be in the SocksToVPN subfolder for auto-detection

## Troubleshooting

If you see "ERROR: SocksToVPN executable not found":
1. Verify the file path: `<app directory>\SocksToVPN\windows_amd64.exe`
2. Ensure you downloaded the correct platform (windows_amd64)
3. Check that the filename is exactly `windows_amd64.exe`
