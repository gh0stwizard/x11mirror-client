#ifndef X11MIRROR_IMGMAN_H
#define X11MIRROR_IMGMAN_H

#include <stdbool.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/shape.h>


#define METHOD(type, name, args)   type (*name) args


typedef struct imgman_struct * imgman_ptr;

typedef struct imgman_struct {
    METHOD(void, init, (imgman_ptr m, Display *dpy, Window window));
    METHOD(void, on_update, (imgman_ptr m));
    METHOD(XImage*, create_ximage, (imgman_ptr m));
    METHOD(void, destroy, (imgman_ptr m));
    Display *dpy;
    int screen;
    Window root;
    Window window;
    XWindowAttributes wa;
    XRenderPictureAttributes pa;
    Pixmap pixmap;
    unsigned int pixmap_width;
    unsigned int pixmap_height;
    Bool hasAlpha;
    struct {
        Picture window;
        Picture output;
    } picture;
    struct {
        XRenderPictFormat *window;
        XRenderPictFormat *output;
    } format;
    struct {
        Damage window;
        Damage output;
    } damage;
} imgman;


extern void
imgman_init(imgman_ptr m, Display *dpy, Window window);


extern XImage *
imgman_create_ximage(imgman_ptr m);


extern void
imgman_destroy(imgman_ptr m);


extern void
imgman_composite(imgman_ptr m);


extern void
imgman_export_ximage(XImage *image, const char *path, const char *type);


extern void
imgman_update_wa(imgman_ptr m);


extern void
imgman_refresh_pictures(imgman_ptr m);


extern bool
imgman_save_image(XImage *ximg, const char *path);


#endif /* X11MIRROR_IMGMAN_H */
