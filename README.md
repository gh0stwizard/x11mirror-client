# x11mirror-client


## Description

This is an X11 program which retrieve an image of specified window
or a whole display.

[Video demonstration on YouTube](https://youtu.be/4Kz16FlFcjQ)


## Features

* Don't create any windows for a work. The Image structure is attached 
  to target window (display).
* Supports zlib to compress huge XWD format.
* Supports curl to upload files.


## Limitations

* The program must be runned on the same machine where is X11 server is
  running.
* X11 server must support next extentions: composite, RENDER, FIXES, DAMAGE.
  On modern GNU/Linux distributives they are all supported and available.


## Usage

```
shell> ./x11mirror-client -?
Usage: ./x11mirror-client [-w window] [OPTIONS]
Options:
  -d display        connection string to X11
  -o output         output filename, default: /tmp/x11mirror.xwd
  -u URL            an URL to send data
  -w window         target window id, default is root
  -z                enable gzip (zlib) compression, by default disabled
  -D                delay after taking a screenshot in milliseconds
  -S                enable X11 synchronization, by default disabled
  -Z                zlib compression level (1-9), default 7
```

## Why?

Just for fun. Also, because of this project I'd learned X11 xlib.


## Dependecies

* X11 development headers
* zlib (optional)
* curl (optional)
* GNU make (to build)


## Build

Currently tested only on GNU/Linux.

```
shell> make
```


## See also

1. You may found the server to test the program here: [x11mirror-server][]

2. [Keylogger][] for X11


[x11mirror-server]: https://github.com/gh0stwizard/x11mirror-server
[Keylogger]: https://github.com/xdevelnet/x11k
