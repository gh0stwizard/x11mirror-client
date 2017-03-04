#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <getopt.h>
#include <stdarg.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#ifndef _NO_DELAY
#include <time.h>
#include <math.h>
#endif
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/shape.h>

#include "common.h"
#include "window_dump.h"
#ifndef _NO_ZLIB
#include "compression.h"
#endif

/* is there compositor manager */
static Bool test_cm (Display *d);
/* load necessary x11's extensions */
static void load_x11_extensions (Display *);
static void debug_window (Display *d, Window w, XWindowAttributes *wa);
/* select events for specified target window */
static void xselect_input (Display *d, Window w);
static inline void
save_file (Display *d, int screen, Window w, Pixmap p);
/* by default X11 throws error and exit */
static int
xerror_handler (Display *dpy, XErrorEvent *ev);

/* X11 extensions offsets */
static int composite_opcode, composite_event, composite_error;
static int render_event, render_error;
static int xfixes_event, xfixes_error;
static int damage_event, damage_error;
/*static int xshape_event, xshape_error;*/

static char *out_file_name;
static FILE *out_file;

#ifndef _NO_DELAY
#ifndef _DELAY_NSEC
/* very low numbers may lead to lags */
#define _DELAY_NSEC 0 * 1000000
#endif
#ifndef _DELAY_SEC
#define _DELAY_SEC 1
#endif
#endif

#ifndef _OUTPUT_FILE
#define _OUTPUT_FILE "/tmp/x11mirror.xwd"
#endif

#ifndef _NO_ZLIB
static Bool use_zlib = False;
static unsigned zlevel = 7;
#endif

extern int
main (int argc, char *argv[])
{
    int         opt;                /* options of  getopt */
    Window      w = 0;              /* target window */
    int         w_screen = -1;      /* number of screen of target window */
    Display     *dpy;
    char        *display = NULL;    /* display string from args */
    XEvent      ev;
    XWindowAttributes wa;
    Pixmap      w_pixmap = 0;
    XRenderPictFormat *format;
    Picture     w_picture, pixmap_picture;
    XRenderPictureAttributes w_pa = { .subwindow_mode = IncludeInferiors };
    Damage      w_damage, pixmap_damage;
    Bool        synchronize = False;
    unsigned    my_events;
#ifndef _NO_DELAY
    time_t      delay_sec = _DELAY_SEC;
    long        delay_nsec = _DELAY_NSEC;
#endif


    while ( (opt = getopt (argc, argv, "d:o:w:zD:SZ:")) != -1 ) {
        switch (opt) {
        case 'd':
            display = optarg;
            break;
#ifndef _NO_ZLIB
        case 'Z':
            sscanf (optarg, "%u", &zlevel);
            if (zlevel <= 0 || zlevel >= 10)
                die ("Invalid zlib compression level: %s.", optarg);
            break;
#endif
        case 'o':
            out_file_name = optarg;
            break;
        case 'w':
            sscanf (optarg, "0x%lx", &w);
            if (w == 0)
                sscanf (optarg, "%lu", &w);
            if (w == 0)
                die ("Invalid window id: %s.", optarg);
            break;
#ifndef _NO_DELAY
        case 'D': {
            long delay = 0;
            sscanf (optarg, "%li", &delay);
            if (delay < 0)
                die ("Delay must be greater than zero.");
            else if (delay >= 1000) {
                delay_sec = lround ((delay - (delay % 1000)) / 1000);
                delay_nsec = (delay % 1000) * 1000000;
            }
            else {
                delay_sec = 0;
                delay_nsec = delay * 1000000;
            }
        }   break;
#endif
        case 'S':
            synchronize = True;
            break;
#ifndef _NO_ZLIB
        case 'z':
            use_zlib = True;
            break;
#endif
        default: /* '?' */
            fprintf (stderr, "Usage: %s [-w window] [OPTIONS]\n", argv[0]);
            fprintf (stderr, "Options:\n");
#define desc(o,d) fprintf (stderr, "  %-16s  %s\n", o, d);
            desc ("-d display", "connection string to X11");
            desc ("-o output", "output filename, default: " _OUTPUT_FILE);
            desc ("-w window", "target window id, default is root");
#ifndef _NO_ZLIB
            desc ("-z", "enable gzip (zlib) compression, by default disabled");
#endif
#ifndef _NO_DELAY
            desc ("-D", "delay after taking a screenshot in milliseconds");
#endif
            desc ("-S", "enable X11 synchronization, by default disabled");
#ifndef _NO_ZLIB
            desc ("-Z", "zlib compression level (1-9), default 7");
#endif
#undef desc
            exit (EXIT_FAILURE);
        }
    }

    if (out_file_name == NULL) {
        out_file_name = _OUTPUT_FILE;
    }
    else {
        int len = strlen (out_file_name);
        if (len <= 0 && len >= PATH_MAX)
            die ("invalid length of output filename.");
    }

    debug ("output file: %s\n", out_file_name);
#ifndef _NO_ZLIB
    debug ("output file format: %s\n", (use_zlib) ? "zlib" : "xwd");
#endif

    if (!(out_file = fopen (out_file_name, "wb"))) {
        perror ("fopen output filename");
        exit (EXIT_FAILURE);
    }

#ifndef _NO_DELAY
    debug ("delay between screenshots: %lu.%lu second(s)\n",
            delay_sec, lround (delay_nsec / 1000000));
#endif

    dpy = XOpenDisplay (display);
    if (!dpy)
        die ("Could not open the display.");

    if (synchronize) {
        XSynchronize (dpy, synchronize);
        debug ("WARNING: X11 synchronization enabled\n");
    }

    XSetErrorHandler (xerror_handler);

    load_x11_extensions (dpy);
    const int damageEventType = damage_event + XDamageNotify;

    if (w == 0) {
        w = RootWindow (dpy, DefaultScreen (dpy));
        debug ("using root window as target one.\n");
    }

    if (test_cm (dpy))
        debug ("compositor manager was not found.\n");
    else
        debug ("compositor manager has been found.\n");

    for (int i = 0; i < ScreenCount (dpy); i++) {
        XCompositeRedirectSubwindows (dpy, RootWindow (dpy, i),
            CompositeRedirectAutomatic);
    }

    debug ("retrieving attributes for 0x%lx\n", w);
    XGetWindowAttributes (dpy, w, &wa);
    debug_window (dpy, w, &wa);

    for (int i = 0; i < ScreenCount (dpy); i++) {
        if (ScreenOfDisplay (dpy, i) == wa.screen) {
            w_screen = i;
            debug ("default screen = %d, target window screen = %d\n",
                DefaultScreen (dpy), i);
            break;
        }
    }

    if (w_screen < 0)
        die ("failed to find out a screen number of the window.");

    /* select events for the window */
    xselect_input (dpy, w);

    format = XRenderFindVisualFormat (dpy, wa.visual);
    w_picture = XRenderCreatePicture (dpy, w, format, CPSubwindowMode, &w_pa);
    w_pixmap = XCreatePixmap (dpy, w, wa.width, wa.height, wa.depth);
    pixmap_picture = XRenderCreatePicture (dpy, w_pixmap, format, 0, NULL);

    w_damage = XDamageCreate (dpy, w, XDamageReportRawRectangles);
    pixmap_damage = XDamageCreate (dpy, w_pixmap, XDamageReportRawRectangles);

    XFlush (dpy);

#define COMPOSITE() XRenderComposite (dpy, \
    PictOpSrc, w_picture, None, pixmap_picture, \
    0, 0, 0, 0, \
    0, 0, wa.width, wa.height)


    while (1) {
        my_events = 0;
        do {
            XNextEvent (dpy, &ev);
            debug ("event for 0x%08lx type: %4d", ev.xany.window, ev.type);

            if (ev.type == damageEventType) {
                Damage d = ((XDamageNotifyEvent *)&ev)->damage;

                if (d == w_damage) {
                    debug (" damage event for the window\n");
                    my_events |= 1 << 0;
                }
                else if (d == pixmap_damage) {
                    debug (" damage event for the pixmap\n");
                    my_events |= 1 << 1;
                }

                continue;
            }
            else
                debug ("\n");

            switch (ev.type) {
            case ConfigureNotify:
            case MapNotify: {
                if (ev.xconfigure.window == w) {
                    debug ("updating window attributes.\n");
                    XGetWindowAttributes (dpy, w, &wa);
                    format = XRenderFindVisualFormat (dpy, wa.visual);
                    XFreePixmap (dpy, w_pixmap);
                    XRenderFreePicture (dpy, pixmap_picture);
                    XRenderFreePicture (dpy, w_picture);
                    XDamageDestroy (dpy, pixmap_damage);
                    w_picture =
                        XRenderCreatePicture (dpy, w, format, CPSubwindowMode, &w_pa);
                    w_pixmap = 
                        XCreatePixmap (dpy, w, wa.width, wa.height, wa.depth);
                    pixmap_picture = 
                        XRenderCreatePicture (dpy, w_pixmap, format, 0, NULL);
                    pixmap_damage = 
                        XDamageCreate (dpy, w_pixmap, XDamageReportRawRectangles);
                }
            }   break;
            case DestroyNotify:
                if (ev.xunmap.window == w)
                    goto done;
                break;
            }
        } while (QLength (dpy));


        if (my_events == 0) {
            int fd = ConnectionNumber (dpy);
            fd_set fdset;
            FD_ZERO (&fdset);
            FD_SET (fd, &fdset);
            struct timeval wtime = { 0, 500000 };
            select (fd + 1, &fdset, NULL, NULL, &wtime);
        }
        else if (my_events & (1 << 1)) {
            save_file (dpy, w_screen, w, w_pixmap);
#ifndef _NO_DELAY
            struct timespec wtime = {
                .tv_sec = delay_sec,
                .tv_nsec = delay_nsec
            };
            nanosleep (&wtime, NULL);
#endif
        }
        else if (my_events & (1 << 0)) {
            COMPOSITE ();
        }
    } /* while (1) */

#undef COMPOSITE

done:
    if (w_pixmap)
        XFreePixmap (dpy, w_pixmap);
    XDamageDestroy (dpy, w_damage);
    XDamageDestroy (dpy, pixmap_damage);
    XRenderFreePicture (dpy, pixmap_picture);
    XRenderFreePicture (dpy, w_picture);
    XSync (dpy, False);
    XCloseDisplay (dpy);
    fclose (out_file);
    return 0;
}


static void
load_x11_extensions (Display *dpy)
{
    int major_version, minor_version;

    if (!XQueryExtension (dpy, COMPOSITE_NAME, 
        &composite_opcode, &composite_event, &composite_error))
        die ("COMPOSITE extension is not installed.");
    XCompositeQueryVersion (dpy, &major_version, &minor_version);
    if (major_version == 0 && minor_version < 4)
        die ("libXcomposite 0.4 or better is required.");
    debug ("using COMPOSITE %d.%d\n", major_version, minor_version);

    if (!XRenderQueryExtension (dpy, &render_event, &render_error))
        die ("RENDER extension is not installed.");
    XRenderQueryVersion (dpy, &major_version, &minor_version);
    debug ("using RENDER %d.%d\n", major_version, minor_version);

    if (!XFixesQueryExtension (dpy, &xfixes_event, &xfixes_error))
        die ("FIXES extension is not installed.");
    XFixesQueryVersion (dpy, &major_version, &minor_version);
    debug ("using FIXES %d.%d\n", major_version, minor_version);

    if (!XDamageQueryExtension (dpy, &damage_event, &damage_error))
        die ("DAMAGE extension is not installed.");
    XDamageQueryVersion (dpy, &major_version, &minor_version);
    debug ("using DAMAGE %d.%d\n", major_version, minor_version);
}


static void
xselect_input (Display *d, Window w)
{
    Window      root, parent, *children;
    unsigned int nchildren;
    long ev_mask = StructureNotifyMask | SubstructureNotifyMask | ExposureMask;

    XGrabServer (d);
    XSelectInput (d, w, ev_mask);
    XQueryTree (d, w, &root, &parent, &children, &nchildren);
    for (unsigned int i = 0; i < nchildren; i++) {
        XSelectInput (d, children[i], ev_mask);
    }
    XFree (children);
    XUngrabServer (d);
}


static void
debug_window (Display *d, Window w, XWindowAttributes *wa)
{
#define BACKSTORE(i) ( (i == NotUseful) \
    ? ("Not Useful") \
    : (i == WhenMapped) ? ("When Mapped") : ("Always") )

    (void)wa;
    char *window_name = NULL;

/* TODO: _NET_WM_NAME */
    XFetchName (d, w, &window_name);
    if (window_name != NULL) {
        debug ("  name: %s\n", window_name);
        XFree (window_name);
    }
    debug ("  x: %d y: %d\n", wa->x, wa->y);
    debug ("  width: %d height: %d\n", wa->width, wa->height);
    debug ("  root: 0x%lx\n", wa->root);
    debug ("  backing store: %s\n", BACKSTORE(wa->backing_store));
#undef BACKSTORE
}


/*
 * test_cm: returns True if there is no any Compositor Manager.
 */
static Bool
test_cm (Display *dpy)
{
    static char name[] = "_NET_WM_CM_Sxx";

    snprintf (name, sizeof (name), "_NET_WM_CM_S%d", DefaultScreen (dpy));
    if (None == XGetSelectionOwner (dpy, XInternAtom (dpy, name, False)))
        return True;
    else
        return False;
}


static inline void
save_file (Display *d, int screen, Window w, Pixmap p)
{
    static mirrorDump *dump;

    if ((dump = Pixmap_Dump (d, screen, w, p)) != NULL) {
        rewind (out_file);
#ifndef _NO_ZLIB
        if (use_zlib)
            save_gzip_file (dump, out_file, zlevel);
        else
#endif
            Save_Dump (dump, out_file);
    }

    Free_Dump (dump);
    fflush (out_file);
}


static int
xerror_handler (Display *dpy, XErrorEvent *ev)
{
#ifndef _NO_ERRORS
    int         o;
    const char  *name = NULL;
    static char buffer[256];

    o = ev->error_code - xfixes_error;
    switch (o) {
    case BadRegion: name = "BadRegion";	break;
    default: break;
    }

    o = ev->error_code - damage_error;
    switch (o) {
    case BadDamage: name = "BadDamage";	break;
    default: break;
    }

    o = ev->error_code - render_error;
    switch (o) {
    case BadPictFormat: name ="BadPictFormat"; break;
    case BadPicture: name ="BadPicture"; break;
    case BadPictOp: name ="BadPictOp"; break;
    case BadGlyphSet: name ="BadGlyphSet"; break;
    case BadGlyph: name ="BadGlyph"; break;
    default: break;
    }

    if (name == NULL) {
	    buffer[0] = '\0';
	    XGetErrorText (dpy, ev->error_code, buffer, sizeof (buffer));
	    name = buffer;
    }

    fprintf (stderr, "error %d: %s request %d minor %d serial %lu\n",
	     ev->error_code, (strlen (name) > 0) ? name : "unknown",
	     ev->request_code, ev->minor_code, ev->serial);
#endif
    return 0;
}
