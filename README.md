# distributed file downloader

A P2P file sharing software for decentralized file sharing. 

## REQUIRED:
> C++17 \
> CMake ver. >3.28 \
> OpenSSL (libcrypto)

## SETUP:
```
$ mkdir build && cd build
$ cmake ..
$ make
```

### Options
| option   | switch | args           | client desc.              | server desc.                    | used by         | required?                                    |
| ------   | ------ | ----           | ------------              | ------------                    | -----------     | ---------                                    |
| --server | -s     | n/a            | n/a                       | open as server                  | SERVER          | yes                                          |
| --client | -c     | n/a            | n/a                       | open as client                  | CLIENT          | yes                                          |
| --port   | none   | \<port #\>[^1] | server port to connect to | port to open server listener on | CLIENT & SERVER | required by server. optional for client.[^2] |
| --ip     | none   | \<IPv4 addr\>  | server ip to connect to   | n/a                             | CLIENT          | optional for client.[^2]                     |
| --download | none | \<path\> | set a directory to download to[^3] | n/a | CLIENT | optional |
| --listen | none | \<IPv4 addr\> | interface to listen on[^4] | n/a | CLIENT | yes |
| --connect | none | \<ip\> \<port\> | n/a | server to register with on startup | SERVER | no[^5] |


[^1]: Ports in the range 0..1023 are disallowed to avoid conflicts. 
[^2]: Required by client for first-time connection. If a `hosts` file has been created by a past session this is optional.
[^3]: Default is `$XDG_DOWNLOAD_DIR/dfd` if `$XDG_DOWNLOAD_DIR` env variable is set. Fallback is `~/dfd`. Further fallback is cwd.
[^4]: IP that will be shared with the server for peers to connect to. Allows for internal listening on `192.168.*.*` and `localhost` if desired. Otherwise a public IP is best used. Ensure the port is open to connections in firewall.
[^5]: This option is used to form a network of synchronized servers. If not provided the server starts and forms its own separate network. Other servers can form a network with a lone server by specifying `--connect`.

## CLIENT CONSOLE COMMANDS:

### File Sharing:
> \> help \
> \> index \[path_to_file\] \
> \> drop  \[id\] \
> \> download  \[uuid\] \
> \> quit 

```
index:
Makes a file available for peer requests. Provided path can be either relative or absolute.
```

```
drop:
Stops accepting peer requests for file associated with the provided id. The id should be obtained from the `list` command.
```

```
download:
Download a file from a peer. Must provide the full unique id. 
```
