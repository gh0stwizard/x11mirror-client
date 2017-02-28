#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <stdarg.h>
#include <sys/select.h>
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
static void load_x11_extensions (Display *);
static void debug_window (Display *d, Window w, XWindowAttributes *wa);
static Window clone_window (Display *d, XWindowAttributes *source_wa);

static int      scrn;
static Window   root;

/* X11 extensions offsets */
static int composite_op, composite_ev, composite_error;
static int render_event, render_error;
static int xfixes_event, xfixes_error;
static int damage_event, damage_error;
static int xshape_event, xshape_error;


extern int
main (int argc, char *argv[])
{
    int         opt;
    Window      w = 0, clone = 0;
    Display     *dpy;
    char        *display = NULL;
    XEvent      ev;
    XWindowAttributes wa;
    Window      root_return, parent;
    Window      *children;
    unsigned int nchildren;
    long ev_mask = StructureNotifyMask | SubstructureNotifyMask |
        PropertyChangeMask |
        EnterWindowMask | LeaveWindowMask |
        ExposureMask |
        FocusChangeMask |
        PointerMotionMask;
    FILE        *out_file;

    (void)clone;

    if (!(out_file = fopen ("/tmp/x11mirror.xwd", "wb"))) {
        perror ("fopen");
        exit (EXIT_FAILURE);
    }

    while ( (opt = getopt (argc, argv, "d:w:")) != -1 ) {
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
        default: /* '?' */
            fprintf (stderr, "Usage: %s [-w window]\n", argv[0]);
            exit (EXIT_FAILURE);
        }
    }

    dpy = XOpenDisplay (display);
    if (!dpy)
        die ("Could not open the display.\n");

    load_x11_extensions (dpy);

    scrn = DefaultScreen (dpy);
    root = RootWindow (dpy, scrn);

    if (test_cm (dpy))
        printf ("compositor manager was not found.\n");
    else
        printf ("compositor manager has been found.\n");

    for (int i = 0; i < ScreenCount (dpy); i++) {
        XCompositeRedirectSubwindows (dpy, RootWindow (dpy, i),
            CompositeRedirectAutomatic);
    }

    if (w == 0) {
        printf ("no window has been specified, using root\n");
        w = root;
    }

    debug ("window id = 0x%lx\n", w);

    XGrabServer (dpy);
    XSelectInput (dpy, w, ev_mask);
    XShapeSelectInput (dpy, w, ShapeNotifyMask);
    XQueryTree (dpy, w, &root_return, &parent, &children, &nchildren);
    for (unsigned int i = 0; i < nchildren; i++) {
        XSelectInput (dpy, children[i], ev_mask);
        XShapeSelectInput (dpy, children[i], ShapeNotifyMask);
    }    
    XFree (children);
    XUngrabServer (dpy);

    debug ("retrieve window attributes\n");

    XGetWindowAttributes (dpy, w, &wa);
    debug_window (dpy, w, &wa);
//    clone = clone_window (dpy, &wa);
//    if (!clone)
//        die ("Failed to create cloned window\n");

    Pixmap clonePixmap = XCreatePixmap (dpy, w, wa.width, wa.height, wa.depth);

//    XStoreName (dpy, clone, "This is a clone!");

    XRenderPictFormat *format = XRenderFindVisualFormat (dpy, wa.visual);
    int op = (format->type == PictTypeDirect && format->direct.alphaMask)
        ? PictOpOver : PictOpSrc;
    op = PictOpSrc;
    XRenderPictureAttributes pa = { .subwindow_mode = IncludeInferiors };
    Picture picture = XRenderCreatePicture (dpy, w, format, CPSubwindowMode, &pa);

    format = XRenderFindVisualFormat (dpy, wa.visual);
    Picture clonePicture = XRenderCreatePicture (dpy, clonePixmap, format, 0, NULL);


    XRectangle r = { .x = 0, .y = 0, .width = wa.width, .height = wa.height };
/*
    XserverRegion cloneRegion = XFixesCreateRegion (dpy, &r, 1);
    XFixesSetPictureClipRegion (dpy, clonePicture, 0, 0, cloneRegion);
    XFixesDestroyRegion (dpy, cloneRegion);
*/

    r.x = wa.x;
    r.y = wa.y;
    XserverRegion region = XFixesCreateRegion (dpy, &r, 1);
    XFixesSetPictureClipRegion (dpy, picture, 0, 0, region);
    XFixesDestroyRegion (dpy, region);

    Damage w_damage = XDamageCreate (dpy, w, XDamageReportNonEmpty);
    (void)w_damage;
/*    Damage clone_damage = XDamageCreate (dpy, clone, XDamageReportNonEmpty);
    (void)clone_damage; */

//    XMapWindow (dpy, clone);
    XFlush (dpy);

    const int damageEventType = damage_event + XDamageNotify;
    const int shapeEventType = xshape_event + ShapeNotify;

    while (1) {
        do {
            XNextEvent (dpy, &ev);
            debug ("event for 0x%lx, type = %d\n", ev.xany.window, ev.type);

            if (ev.type == damageEventType) {
                debug ("damage event for 0x%lx\n", ev.xany.window);
//                if (ev.xany.window == clone) {
//                    Window_Dump (dpy, scrn, clone, out_file);
//                    fclose (out_file);
//                    goto done;
//                }
                continue;
            }
            else if (ev.type == shapeEventType) {
                debug ("shape event for 0x%lx\n", ev.xany.window);
                continue;
            }

            switch (ev.type) {
            case Expose:
                break;
            case ConfigureNotify:
//                if (ev.xconfigure.window == w)
//                    XGetWindowAttributes (dpy, w, &wa);
                break;
            case MapNotify: {
//                if (ev.xmap.window == clone)
//                    XUnmapWindow (dpy, clone);
            }   break;
            case UnmapNotify:
                if (ev.xunmap.window == w)
                    goto done;
                break;
            default:
                break;
            }

//            if (ev.xany.window != clone) {
                XRenderComposite (dpy, op, picture, None, clonePicture,
                    0, 0, 0, 0, 0, 0, wa.width, wa.height);
//            }
        } while (XPending (dpy));

        /* A dirty hack to avoid implementing own timer.
         * There is a delay between the moment when we got an event
         * from the target window AND the moment when X-server is
         * actually drawn (updated) a picture of the window.
         * So, in the moment of the event the target window contains
         * an old image and because of that we have to wait 
         * a small period of a time to get/draw a correct image.
         */
        int x11_fd = ConnectionNumber (dpy);
        fd_set fdset;
        FD_ZERO (&fdset);
        FD_SET (x11_fd, &fdset);
        struct timeval small_time = { 0, 500000 };
        if (select (x11_fd + 1, &fdset, NULL, NULL, &small_time) == 0) {
            XRenderComposite (dpy, op, picture, None, clonePicture,
                0, 0, 0, 0, 0, 0, wa.width, wa.height);
            //Window_Dump (dpy, scrn, w, out_file);
            Pixmap_Dump (dpy, scrn, w, clonePixmap, out_file);
            fclose (out_file);
            goto done;
        }

    }

done:
    XCloseDisplay (dpy);
    return 0;
}


static void
load_x11_extensions (Display *dpy)
{
    int major_version, minor_version;

    if (!XQueryExtension (dpy, COMPOSITE_NAME, 
        &composite_op, &composite_ev, &composite_error))
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


static Window
clone_window (Display *d, XWindowAttributes *source_wa)
{
    Window          w;
    unsigned long   attr_mask = CWBackingStore | CWBackPixel | CWEventMask;
    XSetWindowAttributes wa = { 0 };

    wa.background_pixel = WhitePixel (d, DefaultScreen (d));
    wa.event_mask = StructureNotifyMask | ExposureMask;
    wa.backing_store = Always;

    w = XCreateWindow (d, RootWindow (d, DefaultScreen (d)),
        0, 0,
        source_wa->width, source_wa->height,
        source_wa->border_width,
        CopyFromParent,
        InputOutput, CopyFromParent, attr_mask, &wa);

    XClearWindow (d, w);

    return w;
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
    if (window_name != NULL)
        debug ("name: %s\n", window_name);
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

    snprintf (name, sizeof (name), "_NET_WM_CM_S%d", scrn);
    if (None == XGetSelectionOwner (dpy, XInternAtom (dpy, name, False)))
        return True;
    else
        return False;
}
