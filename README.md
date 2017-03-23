# x11mirror-client


## Description

This is an X11 program which retrieves an image (a screenshot)
of the specified window or a whole display, saves it to a file
and sends it to a remote server.

A video is worth a thousand words:
[video demonstration on YouTube](https://youtu.be/4Kz16FlFcjQ) ~7min.

The [x11mirror-server][1] is a HTTP server which demonstrated
in the video above. It is a complementary part for `x11mirror-client`.


## Features

* Don't create any windows for a work.
* Supports zlib to compress huge XWD format.
* Supports curl to upload files.


## Limitations

* The program must be run on the same machine where X11 xorg-server is
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


### General information

The *x11mirror-client* can be built in two different ways:

* Without CURL support
* Without ZLIB support

If the program is built with CURL support it will send
screenshots to a remote HTTP server (see case 3 below) in
automatic mode.

Without CURL support the program just saves the current screenshot to
a specified file.

Default values are:

* URL by default: http://localhost:8888/
* Output file by default: `/tmp/x11mirror.xwd`

In any case the program will store a screenshot to a file. The original
idea was to keep the screenshot into the program's memory.
May be I will implement it later.

Because of fact that the file will be frequently updated I advise you to
use `tmpfs`.


#### Why ZLIB?

The XWD format consumes a lot of disk space. To understand this better
look at these numbers:

```
-rw------- 1 xxx xxx 4.1M Mar 23 22:55 xmc.xwd
-rw------- 1 xxx xxx  74K Mar 23 22:41 xmc.xwd.gz
```

The files contain the same screenshot of the whole display with the
resolution 1366x768 pixels.


#### How do I convert XWD to PNG, JPEG?

Well, it is quite common question in Internet. You have to use `convert`
utility from ImageMagick package. For instance, to convert XWD to JPG
just type in console the next command:

```
shell> convert xwd:input_file.xwd jpg:output_file.jpg
```

The `convert` utility understands gzipped XWD files too, so you don't
need to `gunzip` them:

```
shell> convert xwd:input_file.xwd.gz jpg:output_file.jpg
```

One note to be mention: the ImageMagick on your system must be built
with X11 support. You may check if its ok by next command:

```
shell> convert -list format | grep XWD
      XWD* XWD       rw-   X Windows system window dump (color)
```


#### Window ID

Each X11's window has its own identifier (window id). The `xwininfo` 
utility may be useful for you.

To list all current windows type the next command:

```
shell> xwininfo -tree -root
```

You may also get the window id for a specific window. Just run
`xwininfo` without arguments and point to the window.

```
shell> xwininfo
```

To pass the window id for *x11mirror-client* you have to use the `-w`
option:

```
shell> ./x11mirror-client -w 0x1400006
```


### Case 1: save the current screenshot to a XWD file

In this case the program stores the screenshot of the window or the
whole display to one file. By default the outfile have the XWD format.
Use the `-o` option to specify a different location of the output file:

```
shell> ./x11mirror-client -o /tmp/x11mirror.xwd
```


### Case 2: save the current screenshot to a gzip file

Same as above but with a compression of the outfile to gzip format.
The example below have an additional the `-z` option.

```
shell> ./x11mirror-client -o /tmp/x11mirror.xwd -z
```

You may also specify a compression level via `-Z` option. For instance,
use the fastest compression:

```
shell> ./x11mirror-client -o /tmp/x11mirror.xwd -z -Z 1
```


### Case 3: send the current screenshot to a remote server by HTTP

You may combine the options above to get an acceptable result.
The *x11mirror-client* do a simple POST request to the remote server
using `multipart/form-data` format. To do so you have to specify
the `-u` option:

```
shell> ./x11mirror-client -o /tmp/x11mirror.xwd -u http://example.com/
```

The format of the request which will be sent to the server is simple:

```
form-data;name="file";filename="x11mirror.xwd.gz"
[data bytes]
```

You may look on the `upload.c` file for details.


### Delay option

The delay option `-D` was introduced to save CPU resources of the computer.
The value should be specified as milliseconds. For instance, to get
screenshots each 2 seconds you have to run *x11mirror-client* in such way:

```
shell> ./x11mirror-client -D 2000
```

If you would like to send screenshots without a delay you may re-build
the program with `-D_NO_DELAY` flag in CFLAGS. See `Makefile` for details.


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


## Dependecies

* X11 development headers
* zlib (optional)
* curl (optional)
* GNU make (to build)


## Build

Currently tested only on GNU/Linux. By default the program is building 
with CURL & ZLIB support.

```
shell> make
```


### Build without CURL

You have to modify the `Makefile` and specify the `-D_NO_CURL` in
CFLAGS, change the value of `IGN_SRC` to exclude `upload.c` and
change LDFLAGS to exclude CURL libs.


### Build without ZLIB

The workflow to do so is the same as described above for the CURL.
In that case you have to exclude `compression.c` file, see
variable `IGN_SRC` in `Makefile`. You have to specify `-D_NO_ZLIB`
in CFLAGS. And you have to change LDFLAGS too.


## See also

1. You may found the server to test the program here: [x11mirror-server][1]

2. [Keylogger][2] for X11


## Credits

Parts of the program contains a code provided by the [xwd][3] utility
wrote by The Open Group & Hewlett-Packard Co.


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
