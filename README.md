# x11mirror-client

## Description

This is an X11 program which retrieves an image (a screenshot)
of the specified window or a whole display, saves it to a file
and, possibly, sends it to a remote server.

A video is worth a thousand words:
[video demonstration on YouTube](https://youtu.be/4Kz16FlFcjQ) ~7min.

The [x11mirror-server][1] is a HTTP server which demonstrated
in the video above. It was a complementary part of **x11mirror-client**.
The current version does not need it anymore.

An example configuration of nginx could be found inside of the
[misc/nginx](/misc/nginx) directory.


## Features

* Don't create any windows for a work.
* Supports PNG or JPEG output formats.
* Supports curl to upload files.


## Dependecies

* X11 development headers (X11, Xcomposite, Xrender, Xdamage)
* [libpng][18] (optional)
* [libjpeg-turbo][19] (optional)
* curl (optional)
* GNU make (to build)
* C99 compiler (gcc, clang, etc)


## Build

Currently tested only on GNU/Linux. By default the program is building 
with CURL & PNG support. Use `make help` to see general options:

```
% make help
WITH_CURL=YES|NO - enable/disable curl support, default: YES
WITH_PNG=YES|NO - enable/disable libpng support, default: YES
WITH_JPG=YES|NO - enable/disable libjpeg-turbo support, default: YES
ENABLE_DELAY=YES|NO - use delay between screenshots, default: YES
ENABLE_DEBUG=YES|NO - be verbose and print debug information, default: NO
ENABLE_ERRORS=YES|NO - print errors to STDERR, default: YES
```

Additional options that should be passed via `CFLAGS`:

* `_DELAY_SEC`, `_DELAY_NSEC` - default is `1` and `0` respectively.
* `_UPLOAD_URL` - default is `"http://localhost:8888/"`.
* `_OUTPUT_TMPL` - default is `/tmp/x11mirror`.
* `_OUTPUT_TYPE` - default is `png`.
* `_OUTPUT_FILE` - default is `_OUTPUT_TMPL._OUTPUT_TYPE`.
* `_JPG_QUALITY` - default is 85.

Note that the default value is `CFLAGS=-O2`. Please, see [main.c](/main.c)
and [Makefile](/Makefile) for details.

An example how to change the uploading URL:

```
% make CFLAGS="-Os -D_UPLOAD_URL=\\\"http://127.0.0.1:8888\\\""
```


### Build dependencies

To build the application you have to install development packages on your
system. The list below shows packages to install on Void Linux:

* libX11-devel
* libXcomposite-devel
* libXrender-devel
* libXdamage-devel

Note that on your system the package names could differ from the list above.

Optionally you may install curl, libpng, libturbojpeg libriries too:
* libcurl-devel
* libpng-devel
* libjpeg-turbo-devel


## Usage

### Limitations

* The program must be run on the same machine where X11 xorg-server is
  running.
* X11 server must support next extentions: composite, RENDER, FIXES, DAMAGE.
  On modern GNU/Linux distributives they are all supported and available.


### General usage

Please see options below to understand basics:

```
% ./x11mirror-client --help
Usage: ./x11mirror-client [-w window] [OPTIONS]
Options:
  -h, --help        print this help
  -v, --version     print the program version
  -d <display>      connection string to X11
  -o <output>       output filename, default: /tmp/x11mirror.png
  -f <format>       output format (png, jpg, xwd), default: png
  -w <window>       target window id, default: root
  -S                enable X11 synchronization, default: disabled
  -u <URL>          an URL to send data
  -U                enable uploading, default: disabled
  -D <ms>           delay between taking screenshots in milliseconds
  -O, --once        create a screenshot only once
```

Quickstart command:

```
% ./x11mirror-client
```

By default the latest screenshot file could been found in `/tmp/x11mirror.png`
(not compressed) and it will be updated every 1 second.

You can stop the program at any time by pressing `CTRL+C` on keyboard.


### Supported modes

The *x11mirror-client* can be built in different ways:

* Without CURL support
* Without PNG support
* Without JPEG support

If the program was built with CURL support it can send
screenshots to a remote HTTP server in automatic mode.

Without CURL support the program just saves a screenshot to
the specified file.

Default values are:

* Remote server URL: `http://localhost:8888/`
* Output file: `/tmp/x11mirror.png`

The program will print out the output data on the screen,
if specified output file is equal to `-`.

Because of fact that the file will be frequently updated I advise you to
use `tmpfs`.


#### Output formats (PNG, JPG, etc)

The default preference of output format is:
* PNG (good quality, small output file size)
* JPG (fast, bad quality)
* XWD (unsupported at the moment)

You may change the output format by using `--format`, `-f` command
line option:

```
% ./x11mirror-client -f jpg
```

Acceptable format values:
* `png`
* `jpg`
* `xwd`


#### Window ID

Each X11's window has its own identifier (window id). The `xwininfo` 
utility may be useful for you.

To list all current windows type the next command:

```
% xwininfo -tree -root
```

You may also get the window id for a specific window. Just run
`xwininfo` without arguments and point to the window.

```
% xwininfo
```

To pass the window id for *x11mirror-client* you have to use the `-w`
option, for instance:

```
% ./x11mirror-client -w 0x1400006
```

By default `x11mirror-client` uses the root window (whole screen).


### Case 1: save the current screenshot to a file

In this case the program stores the screenshot of the window or the
whole display to one file. By default the outfile have the PNG format.
Use the `-o` option to specify a different location of the output file:

```
% ./x11mirror-client -o /path/to/x11mirror.png
```

The default output filepath is `/tmp/x11mirror.png` (when built with
PNG support).

To save as a JPEG file:

```
% ./x11mirror-client -o /path/to/x11mirror.jpg -f jpg
```

or to save to default location as `/tmp/x11mirror.jpg`:

```
% ./x11mirror-client -f jpg
```


### Case 2: send the current screenshot to a remote server via HTTP

You may combine the options above to get an acceptable result.
The *x11mirror-client* do a simple POST request to the remote server
using `multipart/form-data` format. To do so you have to specify
the `-U` option:

```
% ./x11mirror-client -U
```

To change the remote server URL you may use `-u` option, as show below:

```
% ./x11mirror-client -U -u http://example.net/
```

The format of the request which will be sent to the server is simple:

```
form-data;name="file";filename="x11mirror.xwd.gz"
[data bytes]
```

You may look at the [upload.c](/upload.c) file for details.


### Delay option

The delay option `-D` was introduced to save CPU resources of the computer.
The value should be specified as milliseconds. For instance, to get
screenshots each 2 seconds you have to run *x11mirror-client* in such way:

```
% ./x11mirror-client -D 2000
```

The default delay value is the 1 second (e.g. `-D 1000`).

You may disable the delay by passing `-D 0`. Either you may re-build
the program with passing `ENABLE_DELAY=NO` to `make`.
See also [Makefile](/Makefile) for details.


## Why?

Just for fun.

To be more precise, when I had read the X11 documentation I have wanted to 
implement something unusual. Also such exercises helps remember what you 
had read about.


## How does it work?


### Introduction

The X11 or better to say the `xorg-server` is a complicated as hell thing.
It is trivial to retrieve a screenshot of the whole display. 
You may look at [xwd][3] utility to understand this yourself.

The things becomes complicated when you want to take a screenshot
of a minimized window. Or more common task is take a screenshot of
the window which is covered by other windows. That's why this 
`x11mirror` project was born.


### X11 concepts

Well, it is hard to explain all details in this README. I would say
that the most important part of X11 which is related to this project
is next: the X11 uses only one buffer to display content of windows.

That's right, there is no individual buffers for each window. When
the content of the window has draw by `xorg-server`, the last one
calculates the size of the window, its position, its intersections
with other windows (if they present) and then you see "a picture"
of the window.

In the case of intersections with other windows, the `xorg-server`
will not try to draw things that are covered by other windows.
You may imagine that `xorg-server` is operates under some
sort of a patchwork, where each part is either belongs to
the target window or does not.

That's why it is not possible to just get a screenshot
of the whole window if the last one is covered by other windows.

See links to docs below in the **References** section for details.


### X11 extentions

As was described above to take the screenshot of the whole window is
not possible by [xwd][3] if such a window is covered by other ones.

The *x11mirror-client* uses next X11 extentions:

* [COMPOSITE][4] - compose multiple windows into a one image
* [RENDER][5] - provides modern interface to draw pictures
* [DAMAGE][6] - monitor changes of windows


#### COMPOSITE

The role of COMPOSITE extention is to compose the output
images of the windows. It is also provides support of images
with alpha channel.

So, basically, if you want to draw images (content) of windows 
on the display in some kind of predefined order you would like to see, 
then you have to use this extention. For instance, if you wish 
to add true transparency to windows you have to use this extention 
too. There is no other way to get true transparency under 
X11 at the moment.

In `x11mirror-client` the COMPOSITE plays a role which
does the major job: it allows to get a content of whole window
even if it is covered by other ones.


#### RENDER

There is one issue which I did not mentioned before. Under
X11 to get an image (picture, content, you name it) of the
window you have care that Visual Format of the window
and the output image are the same. I will not provide
details about that here, you may look at them in **References**
section below.

So, the first thing is that RENDER extention will take care for
us about Visual Format of the window and the output image too.

Secondly, this extention provides an interface to for modern 
Picture format. The `x11mirror-client` uses it as an output buffer.


#### DAMAGE

This extention plays a helpful role. The X11 uses the event-driven model,
so instead of using polling method, the `x11mirror-client` asks 
the `xorg-server` send to it events when something is changed inside 
of the window. For instance, if a user is typing something in a browser 
(and new letters are drawn), the DAMAGE extention will tell about this.


#### Conclusions

In the end the overall logic of the program may put in this algorithm:

Input:

* Using common call XCreatePixmap() we create the Pixmap of the window.
* Using RENDER we create the intermediate Picture structure for the Pixmap of the window
  above.

Output:

* Using RENDER we create the output Picture structure.

Copying input to output:

* DAMAGE gets events when the content of the window is changed.
* When this is happen we copy the content of the window using RENDER to
  output Picture structure.
* DAMAGE is also looking at the content of output Picture. And when the
  copying is finished it sends another event about such changes.
* On event when copying is finished we store an image into XWD file.


One thing I forgot to tell. The X11 client program by design don't
have the content of the window in its own memory. To get the real
bytes of the image you have to use standard call to `XGetImage()`.
The function asks the `xorg-server` to transfer real bytes
to X11 client program. And later, the last one may do anything 
with them. In our case, save bytes to a file.

This function is cranky, it will not work properly if you pass
incorrect Visual Format to it. That's why RENDER is so helpful
to us, as I mentioned before.

That's all.


## See also

1. You may found the server to test the program here: [x11mirror-server][1]

2. [Keylogger][2] for X11

3. [scrot][20] - depends on [imlib2][22]

4. [maim][21] - a small screenshot utility written in C++


## Credits

Parts of the program contains a code provided by the [xwd][3] utility
wrote by The Open Group & Hewlett-Packard Co.

[libb64][23] written by Chris Venter


## References

The documentation for X11 is scattered over the Internet. Especially
in case of X11 extentions: the README included in these projects
provides a little details about how they work. So, I will try
to give you full information which may be interested in X11 development:


1. [The X New Developer’s Guide][7] - I would say that it provides an introduction to X11 world.

2. [Xlib - C Language X Interface][16] - xlib documentation, the main
document of X11 developer.

3. [XCB API][17] - if you'll plan to create X11 applications from a scratch, the xcb
provides a modern interface to X11 than xlib. You have to look on it.

4. [fredrik's X11 Composite Tutorial][8] - a good tutorial for practice
purposes.

5. [A New Rendering Model for X][11] by Keith Packard - a good article which explains why and
how RENDER extention was created.

6. [All articles by Keith Packard][12] - you have to read some of them too.
A lot of details were told on Usenix Conferences and probably you will not
find a place in Internet which explains X11 extentions in a such simple way.

7. [DAMAGE][9] - a document describes its API.

8. [RENDER][10] - a document describes its API.

9. `man xcomposite` - yep, manpage that describes its API.

10. [Everything curl][13] (pdf) - a wonderful book about CURL, also available
[online][14].

11. [zlib Manual][15] - explains itself.


[1]: https://github.com/gh0stwizard/x11mirror-server
[2]: https://github.com/xdevelnet/x11k
[3]: https://cgit.freedesktop.org/xorg/app/xwd/
[4]: https://cgit.freedesktop.org/xorg/lib/libXcomposite/
[5]: https://cgit.freedesktop.org/xorg/lib/libXrender/
[6]: http://cgit.freedesktop.org/xorg/proto/damageproto/
[7]: https://www.x.org/wiki/guide/
[8]: http://www.talisman.org/~erlkonig/misc/x11-composite-tutorial/
[9]: https://cgit.freedesktop.org/xorg/proto/damageproto/tree/damageproto.txt
[10]: https://cgit.freedesktop.org/xorg/lib/libXrender/tree/doc/libXrender.txt
[11]: https://keithp.com/~keithp/talks/usenix2000/render.html
[12]: https://keithp.com/~keithp/talks/
[13]: https://www.gitbook.com/download/pdf/book/bagder/everything-curl
[14]: https://ec.haxx.se/
[15]: http://www.zlib.net/manual.html
[16]: https://www.x.org/releases/current/doc/libX11/libX11/libX11.html
[17]: https://xcb.freedesktop.org/XcbApi/
[18]: http://www.libpng.org/
[19]: https://libjpeg-turbo.org/
[20]: https://github.com/resurrecting-open-source-projects/scrot/
[21]: https://github.com/naelstrof/maim/
[22]: https://sourceforge.net/projects/enlightenment/files/imlib2-src/
[23]: https://libb64.sourceforge.net/
