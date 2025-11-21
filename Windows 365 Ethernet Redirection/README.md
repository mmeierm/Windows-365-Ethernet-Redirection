# Windows 365 Ethernet Redirection - SOCKS over RDP

This is a .NET 8 Windows Forms application converted from the C++ SocksOverRDP-Server project. It provides a SOCKS proxy server that communicates over RDP virtual channels.

## Features

- **Connect/Disconnect Button**: Start and stop the SOCKS server with a single click
- **Debug Output Toggle**: Enable/disable debug logging via a checkbox
- **Debug Console**: View real-time debug output in a textbox when debug mode is enabled
- **RDP Virtual Channel Communication**: Uses Windows Terminal Services API to communicate over RDP virtual channels

## How It Works

The application uses the Windows Terminal Services (WTS) API to create a virtual channel named "SocksOverRDP-SocksOverRDP" that can be used to tunnel SOCKS proxy traffic over an RDP connection.

### Key Components

1. **Form1.cs**: Main UI form with connection controls and debug output
2. **SocksServer.cs**: Core SOCKS server implementation using WTS virtual channels

### Technical Details

- Uses `Wtsapi32.dll` P/Invoke calls for virtual channel operations
- Supports both SOCKS4 and SOCKS5 protocols (basic implementation)
- Runs channel reading on a background thread
- Thread-safe logging with UI marshaling

## Usage

1. Launch the application within an RDP session
2. Check "Enable Debug Output" to see connection details and traffic logs
3. Click "Connect" to start the SOCKS server
4. The server will listen on the RDP virtual channel for SOCKS proxy requests
5. Click "Disconnect" to stop the server

## Requirements

- .NET 8.0 or higher
- Windows operating system
- Must be running in an RDP session for virtual channel functionality

## Notes

- This application must be run within an RDP session to access virtual channels
- The virtual channel name must match the client-side component
- Debug output can be toggled on/off at any time without disconnecting

## Security Considerations

- This application creates a network proxy accessible through RDP
- Ensure proper firewall and RDP security settings are in place
- Only use in trusted environments

## Error Handling

The application includes comprehensive error handling:
- Failed channel creation displays an error message
- Connection errors are logged to the debug output
- Thread-safe cleanup on application close
