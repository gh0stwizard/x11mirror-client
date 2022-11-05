#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <getopt.h>
#include <stdarg.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#ifdef USE_DELAY
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
#include "imgman.h"
#ifdef HAVE_CURL
    #include "upload.h"
#endif

/* is there compositor manager */
static Bool test_cm (Display *d);
/* load necessary x11's extensions */
static void load_x11_extensions (Display *);
/* select events for specified target window */
static void xselect_input (Display *d, Window w);
static inline void save_file (imgman_ptr m);
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

#define _STR(x) #x
#define STR(x) _STR(x)

#ifdef USE_DELAY
#ifndef _DELAY_NSEC
/* very low numbers may lead to lags */
#define _DELAY_NSEC 0 * 1000000
#endif
#ifndef _DELAY_SEC
#define _DELAY_SEC 1
#endif
#endif

#ifndef _OUTPUT_FILE
#ifdef HAVE_PNG
    #define _OUTPUT_FILE /tmp/x11mirror.png
#else
    #define _OUTPUT_FILE /tmp/x11mirror.xwd
#endif
#endif

#ifdef HAVE_CURL
#ifndef _UPLOAD_URL
#define _UPLOAD_URL "http://localhost:8888"
#endif
static Bool enable_upload = False;
#endif


static void
print_usage (const char *prog)
{
    printf ("Usage: %s [-w window] [OPTIONS]\n", prog);
    printf ("Options:\n");
#define desc(o,d) printf ("  %-16s  %s\n", o, d);
    desc ("--help, -h", "print this help");
    desc ("--version, -v", "print the program version");
    desc ("-d display", "connection string to X11");
    desc ("-o output", "output filename, default: " STR(_OUTPUT_FILE));
    desc ("-w window", "target window id, default: root");
    desc ("-S", "enable X11 synchronization, default: disabled");
#ifdef HAVE_CURL
    desc ("-u URL", "an URL to send data");
    desc ("-U", "enable uploading, default: disabled");
#endif
#ifdef USE_DELAY
    desc ("-D", "delay between making screenshots in milliseconds");
#endif
    desc ("--once, -O", "create a screenshot only once");
#undef desc
}


static void
print_version ()
{
    const char futures[] = {
#ifdef HAVE_CURL
        " +curl"
#endif
#ifdef HAVE_PNG
        " +png"
#endif
#ifdef _DEBUG
        " +debug"
#endif
#ifdef PRINT_ERRORS
        " +errors"
#endif
#ifndef USE_DELAY
        " -delay"
#endif
    };

    printf ("v%s%s\n", STR(APP_VERSION), futures);
}


extern int
main (int argc, char *argv[])
{
    int         opt;                /* options of  getopt */
    Window      window = 0;         /* target window */
    Display     *dpy;
    char        *display = NULL;    /* display string from args */
    XEvent      ev;
    Bool        synchronize = False;
    unsigned    my_events;
#ifdef USE_DELAY
    time_t      delay_sec = _DELAY_SEC;
    long        delay_nsec = _DELAY_NSEC;
#endif
#ifdef HAVE_CURL
    char        *url = NULL;
#endif
    Bool        do_once = False;

    /* our little image manager */
    imgman      m = {
        .init = imgman_init,
        .on_update = imgman_update_wa,
        .create_ximage = imgman_create_ximage,
        .export_ximage = imgman_export_ximage,
        .destroy = imgman_destroy
    };

    static struct option opts[] = {
        { "help",       no_argument,        0, 'h' },
        { "version",    no_argument,        0, 'v' },
        { "display",    required_argument,  0, 'd' },
        { "output",     required_argument,  0, 'o' },
        { "window",     required_argument,  0, 'w' },
        { "sync-x11",   no_argument,        0, 'S' },
        { "once",       no_argument,        0, 'O' },
#ifdef HAVE_CURL
        { "url",        required_argument,  0, 'u' },
        { "upload",     no_argument,        0, 'U' },
#endif
#ifdef USE_DELAY
        { "delay",      required_argument,  0, 'D' },
#endif
        { 0, 0, 0, 0 }
    };

    while (1) {
        int index = 0;
        opt = getopt_long (argc, argv, "hvOd:o:w:Su:UzZ:D:", opts, &index);
        if (opt == -1) break;
        switch (opt) {
        case 'O':
            do_once = True;
            break;
        case 'd':
            display = optarg;
            break;
        case 'o':
            out_file_name = optarg;
            break;
#ifdef HAVE_CURL
        case 'u':
            url = optarg;
            break;
        case 'U':
            enable_upload = True;
            break;
#endif
        case 'w':
            sscanf (optarg, "0x%lx", &window);
            if (window == 0)
                sscanf (optarg, "%lu", &window);
            if (window == 0)
                die ("Invalid window id: %s.", optarg);
            break;
#ifdef USE_DELAY
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
        case 'h':
            print_usage (argv[0]);
            exit (EXIT_SUCCESS);
        case 'v':
            print_version ();
            exit (EXIT_SUCCESS);
        default: /* '?' */
            print_usage (argv[0]);
            exit (EXIT_FAILURE);
        }
    }

    if (out_file_name == NULL) {
        out_file_name = STR(_OUTPUT_FILE);
    }
    else {
        int len = strlen (out_file_name);

        if (len <= 0 && len >= PATH_MAX)
            die ("invalid length of output filename.");
    }

    debug ("output file: %s\n", out_file_name);

#ifdef HAVE_CURL
    /* initialize curl first */
    if (url == NULL)
        url = _UPLOAD_URL;

    if (enable_upload) {
        debug ("uploading to %s\n", url);
        init_uploader (url);
    }
#endif

#ifdef USE_DELAY
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

    if (window == 0) {
        window = RootWindow (dpy, DefaultScreen (dpy));
        debug ("using root window as target one.\n");
    }

    if (test_cm (dpy))
        debug ("!!! compositor manager was not found\n");
    else
        debug (">>> compositor manager has been found\n");

    for (int i = 0; i < ScreenCount (dpy); i++)
        XCompositeRedirectSubwindows
            (dpy, RootWindow (dpy, i), CompositeRedirectAutomatic);


    m.init(&m, dpy, window);

    /* select events for the window */
    xselect_input (dpy, window);

/*
    int absx, absy, x, y;
    int dwidth, dheight;
    unsigned width, height;
    int nobdrs = 0;
    XWindowAttributes win_info;
    Window dummywin;
    XGetWindowAttributes (dpy, window, &win_info);
    XTranslateCoordinates (dpy, window, root, 0, 0, &absx, &absy, &dummywin);

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

    // clip to window
    if (absx < 0) width += absx, absx = 0;
    if (absy < 0) height += absy, absy = 0;
    // XXX: width & height could not be larger 65535 (ref. xlib manual)
    if (absx + (int)width > dwidth) width = dwidth - absx;
    if (absy + (int)height > dheight) height = dheight - absy;

    x = absx - win_info.x;
    y = absy - win_info.y;

    debug ("x = %i (%i) y = %i (%i) w = %i (%i) h = %i (%i)\n",
            x, win_info.x, y, win_info.y,
            width, wa.width, height, wa.height);
*/

    XFlush (dpy);

    debug ("waiting for events ...\n");

    while (1) {
        my_events = 0;

        do {
            XNextEvent (dpy, &ev);

            debug ("event for 0x%08lx type: %4d", ev.xany.window, ev.type);

            if (ev.type == damageEventType) {
                Damage d = ((XDamageNotifyEvent *)&ev)->damage;

                if (d == m.damage.window) {
                    debug (" damage event for the window\n");
                    my_events |= 1 << 0;
                }
                else if (d == m.damage.output) {
                    debug (" damage event for the output\n");
                    my_events |= 1 << 1;
                }

                continue;
            }
            else
                debug ("\n");

            switch (ev.type) {
            case ConfigureNotify:
            case MapNotify: {
                if (ev.xconfigure.window == window) {
                    my_events |= 1 << 2;
                }
            }   break;
            case UnmapNotify: { // 18
                /* generated when a window is minimized or closed */
                if (ev.xconfigure.window == window) {
                    my_events |= 1 << 3;
                }
            }   break;
            case DestroyNotify: // 17
                if (ev.xunmap.window == window)
                    goto done;
                break;
            } // switch (ev.type) {
        } while (QLength (dpy));


        if (my_events == 0) {
            int fd = ConnectionNumber (dpy);
            fd_set fdset;
            FD_ZERO (&fdset);
            FD_SET (fd, &fdset);
            struct timeval wtime = { 0, 500000 };
            select (fd + 1, &fdset, NULL, NULL, &wtime);
        }
        else if (my_events & (1 <<  3)) {
            m.on_update(&m);
        }
        else if (my_events & (1 << 2)) {
            debug ("  ***** refresh *****\n");
            m.on_update(&m);
            imgman_refresh_pictures(&m);
            imgman_composite(&m);
        }
        else if (my_events & (1 << 1)) {
            debug ("  ----- ready -----\n");
            save_file (&m);
#ifdef HAVE_CURL
            if (enable_upload)
                upload_file (out_file_name);
#endif
            if (do_once)
                break;
#ifdef USE_DELAY
            struct timespec wtime = {
                .tv_sec = delay_sec,
                .tv_nsec = delay_nsec
            };
            nanosleep (&wtime, NULL);
#endif
        }
        else if (my_events & (1 << 0)) {
            imgman_composite(&m);
        }
    } /* while (1) */

#undef COMPOSITE

done:
    m.destroy(&m);
    XSync (dpy, False);
    XCloseDisplay (dpy);
#ifdef HAVE_CURL
    free_uploader ();
#endif

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
save_file (imgman_ptr m)
{
    XImage *image = m->create_ximage(m);
    m->export_ximage(m, image, out_file_name);
    XDestroyImage(image);
}


static int
xerror_handler (Display *dpy, XErrorEvent *ev)
{
#if defined(_NO_ERRORS)
    (void) dpy;
    (void) ev;
#else
    int         o;
    const char  *name = NULL;
    static char buffer[256];

    o = ev->error_code - xfixes_error;
    switch (o) {
    case BadRegion: name = "BadRegion"; break;
    default: break;
    }

    o = ev->error_code - damage_error;
    switch (o) {
    case BadDamage: name = "BadDamage"; break;
    default: break;
    }

    o = ev->error_code - render_error;
    switch (o) {
    case BadPictFormat: name ="BadPictFormat";  break;
    case BadPicture:    name ="BadPicture";     break;
    case BadPictOp:     name ="BadPictOp";      break;
    case BadGlyphSet:   name ="BadGlyphSet";    break;
    case BadGlyph:      name ="BadGlyph";       break;
    default: break;
    }

    if (name == NULL) {
        buffer[0] = '\0';
        XGetErrorText (dpy, ev->error_code, buffer, sizeof (buffer));
        name = buffer;
    }

    warn("error %d: %s request %d minor %d serial %lu\n",
        ev->error_code, (strlen (name) > 0) ? name : "unknown",
        ev->request_code, ev->minor_code, ev->serial);
#endif

    return 0;
}
