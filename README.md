# Windows 365 Ethernet Redirection #
This Tool is based on the Socks Proxy through RDP from https://github.com/nccgroup/SocksOverRDP and the vpn tunnel from https://github.com/xjasonlyu/tun2socks. This Tool allows you to access the network of the RDP Client from your RDP Server eg. Windows 365 Cloud PC.
It uses Dynamic Virtual Channel that enables us to communicate over an open RDP connection without the need to open a new socket, connection or a port on a firewall. It creates a internal socks5 proxy on Port 1080 that will then be used by the tun2socks tunnel.

### How does this work? ###
If the DLL is properly registered, it will be loaded by the mstsc.exe (Remote Desktop Client) every time it is started. When the server executable runs on the server side, it connects back to the DLL on a dynamic virtual channel, which is a feature of the Remote Desktop Protocol. After the channel is set up, a SOCKS Proxy will spin up on the server,  on 127.0.0.1:1080. This service will then be used by tun2socks to create a VPN tunnel back to the client.


### Installation Client side ###
The *.dll* needs to be placed on the client computer in any directory (for long-term use, it is recommended to copy it into the %SYSROOT%\\system32\\ or %SYSROOT%\\SysWoW64\\) and install it with the following command as an elevated user (a.k.a Administrator): 

`regsvr32.exe SocksOverRDP-Plugin.dll`

If you wish to remove it: 

`regsvr32.exe /u SocksOverRDP-Plugin.dll`

**Every time you connect to an RDP server from now on, this plugin will be loaded and will allow you to run the server to connect to the client network**

### Installation Server side ###
The *SocksOverRDPServer.exe* needs to be copied to the server in the same Folder as the *Windows 365 Ethernet Redirection.exe*. For the VPN Part we need the tun2socks.exe from https://github.com/xjasonlyu/tun2socks placed in a SubFolder called "SocksToVPN". We also need the matching version of the wintun driver from here in the same folder https://www.wintun.net/
It then can be executed by the user, for the VPN Part to work it needs to be started as admin. If you use the VPN type in the IP Subnet in the top right box which should be routed through that tunnel. Don't use the default 0.0.0.0/0 as it also contains the 127.0.0.1:1080 address where the socks proxy is running, creating a routing loop

