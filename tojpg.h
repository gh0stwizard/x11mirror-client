#ifndef X11MIRROR_TOJPG_H
#define X11MIRROR_TOJPG_H

#include <stdbool.h>
#include <X11/Xlib.h>


#ifndef JPG_QUALITY
#define JPG_QUALITY 85
#endif


extern bool
tojpg_save_ximage(XImage *ximg, const char *path);


#endif /* X11MIRROR_TOJPG_H */