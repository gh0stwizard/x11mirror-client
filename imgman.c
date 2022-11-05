#include "common.h"
#include "imgman.h"
#ifdef HAVE_PNG
    #include "topng.h"
#endif


extern void
imgman_update_wa(imgman_ptr m)
{
    debug (">>> retrieving attributes for 0x%lx\n", m->window);
    XGetWindowAttributes (m->dpy, m->window, &(m->wa));
    debug_window (m->dpy, m->window, &(m->wa));

    int i;
    for (i = 0; i < ScreenCount (m->dpy); i++) {
        if (ScreenOfDisplay (m->dpy, i) == m->wa.screen) {
            m->screen = i;
            debug ("=== default screen = %d, target window screen = %d\n",
                    DefaultScreen (m->dpy), i);
            break;
        }
    }

    if (m->screen < 0)
        die ("failed to find out a screen number of the window.");

    m->root = RootWindow (m->dpy, m->screen);
}


static void
imgman_create_pictures(imgman_ptr m)
{
    Display *dpy = m->dpy;
    Window window = m->window, root = m->root;
    unsigned int w = m->wa.width, h = m->wa.height;

    m->picture.window = XRenderCreatePicture
        (dpy, window, m->format.window, CPSubwindowMode, &(m->pa));

    m->pixmap = XCreatePixmap(dpy, root, w, h, 32);
    m->picture.output = XRenderCreatePicture
        (dpy, m->pixmap, m->format.output, 0, NULL);

    XRenderColor c = {
        .red = 0x0000, .green = 0x0000, .blue = 0x0000, .alpha = 0x0000
    };
    XRenderFillRectangle(dpy, PictOpSrc, m->picture.output, &c, 0, 0, w, h);

    m->damage.output = XDamageCreate (dpy, m->pixmap, XDamageReportRawRectangles);

    m->pixmap_width = w;
    m->pixmap_height = h;
}


static void
imgman_destroy_pictures(imgman_ptr m)
{
    Display *dpy = m->dpy;
    XRenderFreePicture(dpy, m->picture.window);
    XRenderFreePicture(dpy, m->picture.output);
    XDamageDestroy(dpy, m->damage.output);
    XFreePixmap(dpy, m->pixmap);
}


extern void
imgman_init(imgman_ptr m, Display *dpy, Window window)
{
    XRenderPictFormat *format;
    XRenderPictFormat *out_format;
    Bool hasAlpha;


    if (window == 0) {
        window = RootWindow (dpy, DefaultScreen (dpy));
        debug (">>> using root window as target one.\n");
    }

    m->dpy = dpy;
    m->window = window;
    m->pa.subwindow_mode = IncludeInferiors;

    imgman_update_wa(m);

#define check_alpha(f) (f->type == PictTypeDirect && f->direct.alphaMask)
    format = XRenderFindVisualFormat (m->dpy, m->wa.visual);
    hasAlpha = check_alpha(format);
    debug ("=== window format: alpha = %s depth = %d id = %lu type = %d\n",
            hasAlpha ? "ON" : "OFF",
            format->depth, format->id, format->type);

    out_format = XRenderFindStandardFormat(dpy, PictStandardARGB32);
    debug ("=== output format: alpha = %s depth = %d id = %lu type = %d\n",
            check_alpha(out_format) ? "ON" : "OFF",
            out_format->depth, out_format->id, out_format->type);
#undef check_alpha

    m->format.window = format;
    m->format.output = out_format;
    m->hasAlpha = hasAlpha;

    imgman_create_pictures(m);
    m->damage.window = XDamageCreate (dpy, window, XDamageReportRawRectangles);
}


extern XImage *
imgman_create_ximage(imgman_ptr m)
{
    XRenderPictFormat *out_format = m->format.output;

    XImage *image = XGetImage
        (m->dpy, m->pixmap, 0, 0, m->wa.width, m->wa.height, AllPlanes, ZPixmap);
    image->red_mask = out_format->direct.redMask << out_format->direct.red;
    image->green_mask = out_format->direct.greenMask << out_format->direct.green;
    image->blue_mask = out_format->direct.blueMask << out_format->direct.blue;
    image->depth = out_format->depth;
    debug ("*** created ximage\n");
    return image;
}


extern void
imgman_destroy(imgman_ptr m)
{
    imgman_destroy_pictures(m);
    XDamageDestroy(m->dpy, m->damage.window);
}


extern void
imgman_composite(imgman_ptr m)
{
    XRenderComposite(m->dpy, m->hasAlpha ? PictOpOver : PictOpSrc,
        m->picture.window, None, m->picture.output,
        0, 0, /* x, y */
        0, 0, 0, 0, m->pixmap_width, m->pixmap_height);
}


extern void
imgman_refresh_pictures(imgman_ptr m)
{
    imgman_destroy_pictures(m);
    imgman_create_pictures(m);
}


extern void
imgman_export_ximage(imgman_ptr m, XImage *image, const char *filename)
{
    (void)m;
    debug(">>> exporting ximage:\n"
        "  format = %i\n"
        "  byte_order = %i\n"
        "  bitmap_unit = %i\n"
        "  bitmap_bit_order = %i\n"
        "  bitmap_pad = %i\n"
        "  depth = %i\n"
        "  bits_per_pixel = %i\n"
        "  red_mask = %lu green_mask = %lu blue_mask = %lu\n"
        "  red_shift = %lu green_shift = %lu blue_shift = %lu\n",
        image->format,
        image->byte_order,
        image->bitmap_unit,
        image->bitmap_bit_order,
        image->bitmap_pad,
        image->depth,
        image->bits_per_pixel,
        image->red_mask, image->green_mask, image->blue_mask,
        get_shift(image->red_mask), get_shift(image->green_mask), get_shift(image->blue_mask)
    );

#ifdef HAVE_PNG
    topng_save_ximage(image, filename);
#else
    (void)m;
    (void)image;
    (void)filename;
    die("imgman_save not implemented yet");
#endif
}
