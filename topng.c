#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "common.h"
#include "topng.h"

/*
 * credits to anonymous https://stackoverflow.com/a/38298349
 */

#define BPP 4


static void
initpng(png_image *pi, XImage *ximg)
{
    bzero(pi, sizeof(png_image));
    pi->version = PNG_IMAGE_VERSION;
    pi->width = ximg->width;
    pi->height = ximg->height;

    unsigned long
        rs = get_shift(ximg->red_mask),
        gs = get_shift(ximg->green_mask),
        bs = get_shift(ximg->blue_mask);

    if (ximg->byte_order == 0) {
        if (rs > gs && gs > bs)
            pi->format = PNG_FORMAT_BGRA;
        else if (bs > gs && gs > rs)
            pi->format = PNG_FORMAT_RGBA;
        else
            die("initpng: unable to choose PNG format for %lu %lu %lu byte_order = %i",
                rs, gs, bs, ximg->byte_order);
    }
    else { // XXX: not tested, might be wrong
        if (rs > gs && gs > bs)
            pi->format = PNG_FORMAT_ABGR;
        else if (bs > gs && gs > rs)
            pi->format = PNG_FORMAT_ARGB;
        else
            die("initpng: unable to choose PNG format for %lu %lu %lu byte_order = %i",
                rs, gs, bs, ximg->byte_order);
    }
}


extern bool
topng_save_ximage(XImage *ximg, const char *path)
{
    png_image pi;
    FILE *fh;


    if (strcmp(path, "-") == 0)
        fh = stdout;
    else
        fh = fopen(path, "w");


    if (!fh)
    {
        perror("topng_save_ximage");
        return false;
    }

    initpng(&pi, ximg);

    unsigned int scanline = pi.width * BPP; // XXX
    if (!png_image_write_to_stdio(&pi, fh, 0, ximg->data, scanline, NULL))
    {
        fclose(fh);
        warn("topng_save_ximage: could not save the png image\n");
        return false;
    }

    fclose(fh);

    return true;
}
