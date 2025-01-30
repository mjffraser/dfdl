# distributed file downloader

this needs a better name... \
A P2P file sharing software for decentralized file sharing. 

## REQUIRED:
> C++17 \
> CMake ver. >3.28 \
> SQLite \
> OpenSSL (libcrypto)

## SETUP:
```
$ mkdir build && cd build
$ cmake ..
$ make
```

Optionally, to install binary and add to system path:
```
$ ./dfd --install
```
Uninstall:
```
$ ./dfd --uninstall
```

## USAGE:

```
$ dfd --listen [PORT] --send [PORT]
```


## CONSOLE COMMANDS:

### File Sharing:
> \> help \
> \> index \[path_to_file\] \
> \> list \
> \> drop  \[id\] \
> \> grab  \[uuid\] \
> \> quit 

All of the above commands can also be invoked by their initial, so to get the help menu:
> \> h

```
index:
Makes a file available for peer requests. Provided path can be either relative or absolute.
```

```
list:
Lists a short form id for all actively indexed files.
```

```
drop:
Stops accepting peer requests for file associated with the provided id. The id should be obtained from the `list` command.
```

```
grab:
Download a file from a peer. Must provide the full unique id. 
```

### Configuration:
> \> bandwidth \[max bytes per sec] \
> \> destination \[path_to_file\]

```
bandwidth:
The max number of bytes that can be sent every second. `bandwidth 0` disables any rate-limiting.
```

```
destination:
Download destination. 
Default: `$XDG_DOWNLOAD_DIR/dfd`
Fallback: `~/dfd`
```
