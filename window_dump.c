/*
Copyright 1987, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XWDFile.h>
#include "common.h"
#include "list.h"
#include "wsutils.h"
#include "multiVis.h"

static const int nobdrs = 0;
static Bool on_root = False;
static int format = ZPixmap;
static long add_pixel_value = 0;
static Bool use_installed = False;

extern void _swapshort(register char *, register unsigned);
extern void _swaplong(register char *, register unsigned);

extern int Image_Size(XImage *);
extern int Get_XColors(Display *, XWindowAttributes *, XColor **);
static int Get24bitDirectColors(XColor **);
static int ReadColors(Display *, Visual *, Colormap, XColor **);


void
Window_Dump (Display *dpy, int screen, Window window, FILE *out)
{
    unsigned long swaptest = 1;
    XColor *colors;
    unsigned buffer_size;
    size_t win_name_size;
    CARD32 header_size;
    int ncolors, i;
    char *win_name;
    char default_win_name[] = "xwdump";
    Bool got_win_name;
    XWindowAttributes win_info;
    XImage *image;
    int absx, absy, x, y;
    unsigned width, height;
    int dwidth, dheight;
    Window dummywin;
    XWDFileHeader header;
    XWDColor xwdcolor;

    int                 transparentOverlays , multiVis;
    int                 numVisuals;
    XVisualInfo         *pVisuals;
    int                 numOverlayVisuals;
    OverlayInfo         *pOverlayVisuals;
    int                 numImageVisuals;
    XVisualInfo         **pImageVisuals;
    list_ptr            vis_regions;    /* list of regions to read from */
    list_ptr            vis_image_regions;
    Visual              vis_h, *vis;
    int                 allImage = 0;

    /*
     * Get the parameters of the window being dumped.
     */
    debug ("xwd: Getting target window information.\n");
    if(!XGetWindowAttributes(dpy, window, &win_info))
      die ("Can't get target window attributes.");

    /* handle any frame window */
    if (!XTranslateCoordinates (dpy, window, RootWindow (dpy, screen), 0, 0,
				&absx, &absy, &dummywin)) {
	die ("unable to translate window coordinates (%d,%d)\n",
		 absx, absy);
    }
    win_info.x = absx;
    win_info.y = absy;
    width = win_info.width;
    height = win_info.height;

    if (!nobdrs) {
	absx -= win_info.border_width;
	absy -= win_info.border_width;
	width += (2 * win_info.border_width);
	height += (2 * win_info.border_width);
    }
    dwidth = DisplayWidth (dpy, screen);
    dheight = DisplayHeight (dpy, screen);


    /* clip to window */
    if (absx < 0) width += absx, absx = 0;
    if (absy < 0) height += absy, absy = 0;
    /* XXX: width & height could not be larger 65535 (ref. xlib manual) */
    if (absx + (int)width > dwidth) width = dwidth - absx;
    if (absy + (int)height > dheight) height = dheight - absy;

    XFetchName(dpy, window, &win_name);
    if (!win_name || !win_name[0]) {
	win_name = default_win_name;
	got_win_name = False;
    } else {
	got_win_name = True;
    }

    /* sizeof(char) is included for the null string terminator. */
    win_name_size = strlen(win_name) + sizeof(char);

    /*
     * Snarf the pixmap with XGetImage.
     */

    x = absx - win_info.x;
    y = absy - win_info.y;

    multiVis = GetMultiVisualRegions(dpy,RootWindow(dpy, screen),
               absx, absy,
	       width, height,&transparentOverlays,&numVisuals, &pVisuals,
               &numOverlayVisuals,&pOverlayVisuals,&numImageVisuals,
               &pImageVisuals,&vis_regions,&vis_image_regions,&allImage) ;
    if (on_root || multiVis)
    {
	if(!multiVis)
	    image = XGetImage (dpy, RootWindow(dpy, screen), absx, absy,
                    width, height, AllPlanes, format);
	else
	    image = ReadAreaToImage(dpy, RootWindow(dpy, screen), absx, absy,
                width, height,
    		numVisuals,pVisuals,numOverlayVisuals,pOverlayVisuals,
                numImageVisuals, pImageVisuals,vis_regions,
                vis_image_regions,format,allImage);
    }
    else
	image = XGetImage (dpy, window, x, y, width, height, AllPlanes, format);
    if (!image) {
	die ("unable to get image at %dx%d+%d+%d\n",
		 width, height, x, y);
    }

    if (add_pixel_value != 0) XAddPixel (image, add_pixel_value);

    /*
     * Determine the pixmap size.
     */
    buffer_size = Image_Size(image);

    debug ("xwd: Getting Colors.\n");

    if( !multiVis)
    {
       ncolors = Get_XColors(dpy, &win_info, &colors);
       vis = win_info.visual ;
    }
    else
    {
       ncolors = Get24bitDirectColors(&colors) ;
       initFakeVisual(&vis_h) ;
       vis = &vis_h ;
    }

    /*
     * Calculate header size.
     */
    debug ("xwd: Calculating header size.\n");
    header_size = SIZEOF(XWDheader) + (CARD32) win_name_size;

    /*
     * Write out header information.
     */
    debug ("xwd: Constructing and dumping file header.\n");
    header.header_size = (CARD32) header_size;
    header.file_version = (CARD32) XWD_FILE_VERSION;
    header.pixmap_format = (CARD32) format;
    header.pixmap_depth = (CARD32) image->depth;
    header.pixmap_width = (CARD32) image->width;
    header.pixmap_height = (CARD32) image->height;
    header.xoffset = (CARD32) image->xoffset;
    header.byte_order = (CARD32) image->byte_order;
    header.bitmap_unit = (CARD32) image->bitmap_unit;
    header.bitmap_bit_order = (CARD32) image->bitmap_bit_order;
    header.bitmap_pad = (CARD32) image->bitmap_pad;
    header.bits_per_pixel = (CARD32) image->bits_per_pixel;
    header.bytes_per_line = (CARD32) image->bytes_per_line;
    /****
    header.visual_class = (CARD32) win_info.visual->class;
    header.red_mask = (CARD32) win_info.visual->red_mask;
    header.green_mask = (CARD32) win_info.visual->green_mask;
    header.blue_mask = (CARD32) win_info.visual->blue_mask;
    header.bits_per_rgb = (CARD32) win_info.visual->bits_per_rgb;
    header.colormap_entries = (CARD32) win_info.visual->map_entries;
    *****/
    header.visual_class = (CARD32) vis->class;
    header.red_mask = (CARD32) vis->red_mask;
    header.green_mask = (CARD32) vis->green_mask;
    header.blue_mask = (CARD32) vis->blue_mask;
    header.bits_per_rgb = (CARD32) vis->bits_per_rgb;
    header.colormap_entries = (CARD32) vis->map_entries;

    header.ncolors = ncolors;
    header.window_width = (CARD32) win_info.width;
    header.window_height = (CARD32) win_info.height;
    header.window_x = absx;
    header.window_y = absy;
    header.window_bdrwidth = (CARD32) win_info.border_width;

    if (*(char *) &swaptest) {
	_swaplong((char *) &header, sizeof(header));
	for (i = 0; i < ncolors; i++) {
	    _swaplong((char *) &colors[i].pixel, sizeof(CARD32));
	    _swapshort((char *) &colors[i].red, 3 * sizeof(short));
	}
    }

    if (fwrite((char *)&header, SIZEOF(XWDheader), 1, out) != 1 ||
	fwrite(win_name, win_name_size, 1, out) != 1) {
	perror ("fwrite header");
	exit (EXIT_FAILURE);
    }

    /*
     * Write out the color maps, if any
     */

    debug ("xwd: Dumping %d colors.\n", ncolors);
    for (i = 0; i < ncolors; i++) {
	xwdcolor.pixel = colors[i].pixel;
	xwdcolor.red = colors[i].red;
	xwdcolor.green = colors[i].green;
	xwdcolor.blue = colors[i].blue;
	xwdcolor.flags = colors[i].flags;
	if (fwrite((char *) &xwdcolor, SIZEOF(XWDColor), 1, out) != 1) {
	    perror ("fwrite xwdcolor");
	    exit (EXIT_FAILURE);
	}
    }

    /*
     * Write out the buffer.
     */
    debug ("xwd: Dumping pixmap.  bufsize=%d\n",buffer_size);

    /*
     *    This copying of the bit stream (data) to a file is to be replaced
     *  by an Xlib call which hasn't been written yet.  It is not clear
     *  what other functions of xwd will be taken over by this (as yet)
     *  non-existant X function.
     */
    if (fwrite(image->data, (int) buffer_size, 1, out) != 1) {
	perror ("fwrite image data");
	exit (EXIT_FAILURE);
    }

    /*
     * free the color buffer.
     */

    if(ncolors > 0) debug ("xwd: Freeing colors.\n");
    if(ncolors > 0) free(colors);

    /*
     * Free window name string.
     */
    debug ("xwd: Freeing window name string.\n");
    if (got_win_name) XFree(win_name);

    /*
     * Free image
     */
    XDestroyImage(image);
}


void
_swapshort (register char *bp, register unsigned n)
{
    register char c;
    register char *ep = bp + n;

    while (bp < ep) {
	c = *bp;
	*bp = *(bp + 1);
	bp++;
	*bp++ = c;
    }
}

void
_swaplong (register char *bp, register unsigned n)
{
    register char c;
    register char *ep = bp + n;

    while (bp < ep) {
        c = bp[3];
        bp[3] = bp[0];
        bp[0] = c;
        c = bp[2];
        bp[2] = bp[1];
        bp[1] = c;
        bp += 4;
    }
}

static int
Get24bitDirectColors(XColor **colors)
{
    int i , ncolors = 256 ;
    XColor *tcol ;

    *colors = tcol = (XColor *)malloc(sizeof(XColor) * ncolors) ;

    for(i=0 ; i < ncolors ; i++)
    {
	tcol[i].pixel = i << 16 | i << 8 | i ;
	tcol[i].red = tcol[i].green = tcol[i].blue = i << 8   | i ;
    }

    return ncolors ;
}

/*
 * Determine the pixmap size.
 */

int Image_Size(XImage *image)
{
    if (image->format != ZPixmap)
      return(image->bytes_per_line * image->height * image->depth);

    return(image->bytes_per_line * image->height);
}

#define lowbit(x) ((x) & (~(x) + 1))

static int
ReadColors(Display *dpy, Visual *vis, Colormap cmap, XColor **colors)
{
    int i,ncolors ;

    ncolors = vis->map_entries;

    if (!(*colors = (XColor *) malloc (sizeof(XColor) * ncolors)))
      die ("Out of memory!");

    if (vis->class == DirectColor ||
	vis->class == TrueColor) {
	Pixel red, green, blue, red1, green1, blue1;

	red = green = blue = 0;
	red1 = lowbit(vis->red_mask);
	green1 = lowbit(vis->green_mask);
	blue1 = lowbit(vis->blue_mask);
	for (i=0; i<ncolors; i++) {
	  (*colors)[i].pixel = red|green|blue;
	  (*colors)[i].pad = 0;
	  red += red1;
	  if (red > vis->red_mask)
	    red = 0;
	  green += green1;
	  if (green > vis->green_mask)
	    green = 0;
	  blue += blue1;
	  if (blue > vis->blue_mask)
	    blue = 0;
	}
    } else {
	for (i=0; i<ncolors; i++) {
	  (*colors)[i].pixel = i;
	  (*colors)[i].pad = 0;
	}
    }

    XQueryColors(dpy, cmap, *colors, ncolors);

    return(ncolors);
}

/*
 * Get the XColors of all pixels in image - returns # of colors
 */
int Get_XColors(Display *dpy, XWindowAttributes *win_info, XColor **colors)
{
    int i, ncolors;
    Colormap cmap = win_info->colormap;

    if (use_installed)
	/* assume the visual will be OK ... */
	cmap = XListInstalledColormaps(dpy, win_info->root, &i)[0];
    if (!cmap)
	return(0);
    ncolors = ReadColors(dpy, win_info->visual,cmap,colors) ;
    return ncolors ;
}


void
Pixmap_Dump (Display *dpy, int screen, Window window, Pixmap pixmap, FILE *out)
{
    unsigned long swaptest = 1;
    XColor *colors;
    unsigned buffer_size;
    size_t win_name_size;
    CARD32 header_size;
    int ncolors, i;
    char *win_name;
    char default_win_name[] = "xwdump";
    Bool got_win_name;
    XWindowAttributes win_info;
    XImage *image;
    int absx, absy, x, y;
    unsigned width, height;
    int dwidth, dheight;
    Window dummywin;
    XWDFileHeader header;
    XWDColor xwdcolor;

    int                 transparentOverlays , multiVis;
    int                 numVisuals;
    XVisualInfo         *pVisuals;
    int                 numOverlayVisuals;
    OverlayInfo         *pOverlayVisuals;
    int                 numImageVisuals;
    XVisualInfo         **pImageVisuals;
    list_ptr            vis_regions;    /* list of regions to read from */
    list_ptr            vis_image_regions;
    Visual              vis_h, *vis;
    int                 allImage = 0;


    /*
     * Get the parameters of the window being dumped.
     */
    debug ("xwd: Getting target window information.\n");
    if(!XGetWindowAttributes(dpy, window, &win_info))
      die ("Can't get target window attributes.");

    /* handle any frame window */
    if (!XTranslateCoordinates (dpy, window, RootWindow (dpy, screen),
        0, 0, &absx, &absy, &dummywin))
    {
	    die ("unable to translate window coordinates (%d,%d)\n",
        absx, absy);
    }

    win_info.x = absx;
    win_info.y = absy;
    width = win_info.width;
    height = win_info.height;

    if (!nobdrs) {
	absx -= win_info.border_width;
	absy -= win_info.border_width;
	width += (2 * win_info.border_width);
	height += (2 * win_info.border_width);
    }
    dwidth = DisplayWidth (dpy, screen);
    dheight = DisplayHeight (dpy, screen);


    /* clip to window */
    if (absx < 0) width += absx, absx = 0;
    if (absy < 0) height += absy, absy = 0;
    /* XXX: width & height could not be larger 65535 (ref. xlib manual) */
    if (absx + (int)width > dwidth) width = dwidth - absx;
    if (absy + (int)height > dheight) height = dheight - absy;

    XFetchName(dpy, window, &win_name);
    if (!win_name || !win_name[0]) {
	win_name = default_win_name;
	got_win_name = False;
    } else {
	got_win_name = True;
    }

    /* sizeof(char) is included for the null string terminator. */
    win_name_size = strlen(win_name) + sizeof(char);

    /*
     * Snarf the pixmap with XGetImage.
     */

    x = absx - win_info.x;
    y = absy - win_info.y;

    multiVis = GetMultiVisualRegions (
        dpy, RootWindow(dpy, screen),
        absx, absy,
        width, height, &transparentOverlays, &numVisuals, &pVisuals,
        &numOverlayVisuals, &pOverlayVisuals, &numImageVisuals,
        &pImageVisuals, &vis_regions, &vis_image_regions, &allImage);

    debug ("xwd: GetImage of the pixmap\n");
    image = XGetImage (dpy, pixmap, x, y, width, height, AllPlanes, format);
    if (!image)
        die ("unable to get image at %dx%d+%d+%d\n", width, height, x, y);

    if (add_pixel_value != 0) XAddPixel (image, add_pixel_value);

    /*
     * Determine the pixmap size.
     */
    buffer_size = Image_Size(image);

    debug ("xwd: Getting Colors.\n");

    if (!multiVis) {
        ncolors = Get_XColors (dpy, &win_info, &colors);
        vis = win_info.visual;
    }
    else {
        ncolors = Get24bitDirectColors (&colors);
        initFakeVisual (&vis_h);
        vis = &vis_h;
        debug ("xwd: Using fake visual colors.\n");
    }

    /*
     * Calculate header size.
     */
    debug ("xwd: Calculating header size.\n");
    header_size = SIZEOF(XWDheader) + (CARD32) win_name_size;

    /*
     * Write out header information.
     */
    debug ("xwd: Constructing and dumping file header.\n");
    header.header_size = (CARD32) header_size;
    header.file_version = (CARD32) XWD_FILE_VERSION;
    header.pixmap_format = (CARD32) format;
    header.pixmap_depth = (CARD32) image->depth;
    header.pixmap_width = (CARD32) image->width;
    header.pixmap_height = (CARD32) image->height;
    header.xoffset = (CARD32) image->xoffset;
    header.byte_order = (CARD32) image->byte_order;
    header.bitmap_unit = (CARD32) image->bitmap_unit;
    header.bitmap_bit_order = (CARD32) image->bitmap_bit_order;
    header.bitmap_pad = (CARD32) image->bitmap_pad;
    header.bits_per_pixel = (CARD32) image->bits_per_pixel;
    header.bytes_per_line = (CARD32) image->bytes_per_line;
    /****
    header.visual_class = (CARD32) win_info.visual->class;
    header.red_mask = (CARD32) win_info.visual->red_mask;
    header.green_mask = (CARD32) win_info.visual->green_mask;
    header.blue_mask = (CARD32) win_info.visual->blue_mask;
    header.bits_per_rgb = (CARD32) win_info.visual->bits_per_rgb;
    header.colormap_entries = (CARD32) win_info.visual->map_entries;
    *****/
    header.visual_class = (CARD32) vis->class;
    header.red_mask = (CARD32) vis->red_mask;
    header.green_mask = (CARD32) vis->green_mask;
    header.blue_mask = (CARD32) vis->blue_mask;
    header.bits_per_rgb = (CARD32) vis->bits_per_rgb;
    header.colormap_entries = (CARD32) vis->map_entries;

    header.ncolors = ncolors;
    header.window_width = (CARD32) win_info.width;
    header.window_height = (CARD32) win_info.height;
    header.window_x = absx;
    header.window_y = absy;
    header.window_bdrwidth = (CARD32) win_info.border_width;

    if (*(char *) &swaptest) {
	_swaplong((char *) &header, sizeof(header));
	for (i = 0; i < ncolors; i++) {
	    _swaplong((char *) &colors[i].pixel, sizeof(CARD32));
	    _swapshort((char *) &colors[i].red, 3 * sizeof(short));
	}
    }

    if (fwrite((char *)&header, SIZEOF(XWDheader), 1, out) != 1 ||
	fwrite(win_name, win_name_size, 1, out) != 1) {
	perror ("fwrite header");
	exit (EXIT_FAILURE);
    }

    /*
     * Write out the color maps, if any
     */

    debug ("xwd: Dumping %d colors.\n", ncolors);
    for (i = 0; i < ncolors; i++) {
	xwdcolor.pixel = colors[i].pixel;
	xwdcolor.red = colors[i].red;
	xwdcolor.green = colors[i].green;
	xwdcolor.blue = colors[i].blue;
	xwdcolor.flags = colors[i].flags;
	if (fwrite((char *) &xwdcolor, SIZEOF(XWDColor), 1, out) != 1) {
	    perror ("fwrite xwdcolor");
	    exit (EXIT_FAILURE);
	}
    }

    /*
     * Write out the buffer.
     */
    debug ("xwd: Dumping pixmap.  bufsize=%d\n",buffer_size);

    /*
     *    This copying of the bit stream (data) to a file is to be replaced
     *  by an Xlib call which hasn't been written yet.  It is not clear
     *  what other functions of xwd will be taken over by this (as yet)
     *  non-existant X function.
     */
    if (fwrite(image->data, (int) buffer_size, 1, out) != 1) {
	perror ("fwrite image data");
	exit (EXIT_FAILURE);
    }

    /*
     * free the color buffer.
     */

    if(ncolors > 0) debug ("xwd: Freeing colors.\n");
    if(ncolors > 0) free(colors);

    /*
     * Free window name string.
     */
    debug ("xwd: Freeing window name string.\n");
    if (got_win_name) XFree(win_name);

    /*
     * Free image
     */
    XDestroyImage(image);
}
