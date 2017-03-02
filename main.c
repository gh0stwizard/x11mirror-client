#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <stdarg.h>
#include <sys/select.h>
#ifndef _NO_DELAY
#include <time.h>
#endif
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/shape.h>

#include "common.h"
#include "window_dump.h"

/* is there compositor manager */
static Bool test_cm (Display *d);
/* load necessary x11's extensions */
static void load_x11_extensions (Display *);
static void debug_window (Display *d, Window w, XWindowAttributes *wa);
/* select events for specified target window */
static void xselect_input (Display *d, Window w);
static inline void
save_file (Display *d, int screen, Window w, Pixmap p);
static int
xerror_handler (Display *dpy, XErrorEvent *ev);

/* X11 extensions offsets */
static int composite_opcode, composite_event, composite_error;
static int render_event, render_error;
static int xfixes_event, xfixes_error;
static int damage_event, damage_error;
static int xshape_event, xshape_error;

static FILE *out_file;

#ifndef _NO_DELAY
#ifndef _DELAY_NSEC
/* very low numbers may lead to lags */
#define _DELAY_NSEC 1000 * 1000000
#endif
#ifndef _DELAY_SEC
#define _DELAY_SEC 0
#endif
static struct timespec tp_old, tp_current;
static inline Bool is_ready (void);
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

    while ( (opt = getopt (argc, argv, "d:w:S")) != -1 ) {
        switch (opt) {
        case 'd':
            display = optarg;
            break;
        case 'w':
            sscanf (optarg, "0x%lx", &w);
            if (w == 0)
                sscanf (optarg, "%lu", &w);
            if (w == 0)
                die ("Invalid window id: %s.", optarg);
            break;
        case 'S':
            synchronize = True;
            break;
        default: /* '?' */
            fprintf (stderr, "Usage: %s [-w window] [OPTIONS]\n", argv[0]);
            fprintf (stderr, "Options:\n");
#define desc(o,d) fprintf (stderr, "\t%-16s\t%s\n", o, d);
            desc ("-d display", "connection string to X11");
            desc ("-w window", "target window id");
            desc ("-S", "enable X11 synchronization");
#undef desc
            exit (EXIT_FAILURE);
        }
    }

    dpy = XOpenDisplay (display);
    if (!dpy)
        die ("Could not open the display.\n");

    XSynchronize (dpy, synchronize);
    XSetErrorHandler (xerror_handler);

    load_x11_extensions (dpy);
    const int damageEventType = damage_event + XDamageNotify;
    const int shapeEventType = xshape_event + ShapeNotify;

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

    debug ("retrieving attributes for window = 0x%lx\n", w);
    XGetWindowAttributes (dpy, w, &wa);
    debug_window (dpy, w, &wa);

    for (int i = 0; i < ScreenCount (dpy); i++) {
        if (ScreenOfDisplay (dpy, i) == wa.screen) {
            w_screen = i;
            debug ("default screen = %d, window screen = %d\n",
                DefaultScreen (dpy), i);
            break;
        }
    }

    if (w_screen < 0)
        die ("failed to find out a screen number of the window.\n");

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

#ifndef _NO_DELAY
    if (clock_gettime (CLOCK_MONOTONIC_COARSE, &tp_old) != 0)
        die ("clock_gettime has fail");
#endif

    while (1) {
        do {
            XNextEvent (dpy, &ev);
            debug ("event for 0x%lx, type = %d\n", ev.xany.window, ev.type);


            if (ev.type == damageEventType) {
                Damage d = ((XDamageNotifyEvent *)&ev)->damage;
                if (d == w_damage) {
                    debug ("  damage event for the window\n");
#ifndef _NO_DELAY
                    if (is_ready ())
#endif
                    COMPOSITE();
                }
                else if (d == pixmap_damage) {
                    debug ("  damage event for the pixmap\n");
                    save_file (dpy, w_screen, w, w_pixmap);
                }
                continue;
            }
            else if (ev.type == shapeEventType) {
                debug ("  shape event\n");
                continue;
            }

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
            default:
#ifndef _NO_DELAY
                if (is_ready ())
#endif
                COMPOSITE();
                break;
            }
        } while (XPending (dpy));
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
    return 0;
}


static void
load_x11_extensions (Display *dpy)
{
    int major_version, minor_version;

    if (!XQueryExtension (dpy, COMPOSITE_NAME, 
        &composite_opcode, &composite_event, &composite_error))
        die ("COMPOSITE extension is not installed.\n");
    XCompositeQueryVersion (dpy, &major_version, &minor_version);
    if (major_version == 0 && minor_version < 4)
        die ("libXcomposite 0.4 or better is required.\n");
    debug ("using COMPOSITE %d.%d\n", major_version, minor_version);

    if (!XRenderQueryExtension (dpy, &render_event, &render_error))
        die ("RENDER extension is not installed.\n");
    XRenderQueryVersion (dpy, &major_version, &minor_version);
    debug ("using RENDER %d.%d\n", major_version, minor_version);

    if (!XFixesQueryExtension (dpy, &xfixes_event, &xfixes_error))
        die ("FIXES extension is not installed.\n");
    XFixesQueryVersion (dpy, &major_version, &minor_version);
    debug ("using FIXES %d.%d\n", major_version, minor_version);

    if (!XDamageQueryExtension (dpy, &damage_event, &damage_error))
        die ("DAMAGE extension is not installed.\n");
    XDamageQueryVersion (dpy, &major_version, &minor_version);
    debug ("using DAMAGE %d.%d\n", major_version, minor_version);

    if (!XShapeQueryExtension (dpy, &xshape_event, &xshape_error))
        die ("SHAPE extension is not installed.\n");
    XShapeQueryVersion (dpy, &major_version, &minor_version);
    debug ("using SHAPE %d.%d\n", major_version, minor_version);
}


static void
xselect_input (Display *d, Window w)
{
    Window      root, parent, *children;
    unsigned int nchildren;
    long ev_mask = StructureNotifyMask | 
        SubstructureNotifyMask |
/*
        PointerMotionMask |
        EnterWindowMask | LeaveWindowMask |
        FocusChangeMask |
        PropertyChangeMask |
*/
        ExposureMask;

    XGrabServer (d);
    XSelectInput (d, w, ev_mask);
    XShapeSelectInput (d, w, ShapeNotifyMask);
    XQueryTree (d, w, &root, &parent, &children, &nchildren);
    for (unsigned int i = 0; i < nchildren; i++) {
        XSelectInput (d, children[i], ev_mask);
        XShapeSelectInput (d, children[i], ShapeNotifyMask);
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
        debug ("name: %s\n", window_name);
        XFree (window_name);
    }
    debug ("x: %d y: %d\n", wa->x, wa->y);
    debug ("width: %d height: %d\n", wa->width, wa->height);
    debug ("root: 0x%lx\n", wa->root);
    debug ("backing store: %s\n", BACKSTORE(wa->backing_store));
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
#define OUTFILE "/run/user/1000/x11mirror.xwd"
    if (!(out_file = fopen (OUTFILE "~", "wb"))) {
        perror ("fopen");
        exit (EXIT_FAILURE);
    }
    mirrorDump *dump = Pixmap_Dump (d, screen, w, p);
    if (dump)
        Save_Dump (dump, out_file);
    Free_Dump (dump);
    fclose (out_file);
    rename (OUTFILE "~", OUTFILE);
#undef OUTFILE
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


#ifndef _NO_DELAY
static inline Bool
is_ready (void)
{
    if (clock_gettime (CLOCK_MONOTONIC_COARSE, &tp_current) != 0)
        die ("clock_gettime has fail");

    if (((tp_current.tv_sec - tp_old.tv_sec) > _DELAY_SEC) ||
        ((tp_current.tv_nsec - tp_old.tv_nsec) > _DELAY_NSEC))
    {
        if (clock_gettime (CLOCK_MONOTONIC_COARSE, &tp_old) != 0)
            die ("clock_gettime has fail");
        return True;
    }

    return False;
}
#endif
