#ifndef X11MIRROR_TOPNG_H
#define X11MIRROR_TOPNG_H

#include <stdbool.h>
#include <png.h>
#include <X11/Xlib.h>


extern bool
topng_save_ximage(XImage *ximg, const char *filename);


#endif  /* X11MIRROR_TOPNG_H */
