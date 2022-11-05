#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <jpeglib.h>
#include "common.h"
#include "tojpg.h"

/*
 * FIXME: output quality is really bad & size of png files lower ...
 */


#define BPP 4


static void
initjpg(j_compress_ptr cinfo, XImage *ximg)
{
    cinfo->image_width = ximg->width;
    cinfo->image_height = ximg->height;
    cinfo->input_components = BPP;

    unsigned long
        rs = get_shift(ximg->red_mask),
        gs = get_shift(ximg->green_mask),
        bs = get_shift(ximg->blue_mask);

    if (ximg->byte_order == 0) {
        if (rs > gs && gs > bs)
            cinfo->in_color_space = JCS_EXT_BGRA;
        else if (bs > gs && gs > rs)
            cinfo->in_color_space = JCS_EXT_RGBA;
        else
            die("initjpg: unable to choose JPG format for %lu %lu %lu byte_order = %i",
                rs, gs, bs, ximg->byte_order);
    }
    else { // XXX: not tested, might be wrong
        if (rs > gs && gs > bs)
            cinfo->in_color_space = JCS_EXT_ABGR;
        else if (bs > gs && gs > rs)
            cinfo->in_color_space = JCS_EXT_ARGB;
        else
            die("initjpg: unable to choose JPG format for %lu %lu %lu byte_order = %i",
                rs, gs, bs, ximg->byte_order);
    }
}


extern bool
tojpg_save_ximage(XImage *ximg, const char *path)
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row_pointer[1];
    int row_stride;
    bool status = true;
    FILE *fh;


    if (strcmp(path, "-") == 0)
        fh = stdout;
    else
        fh = fopen(path, "wb");


    if (!fh)
    {
        perror("tojpg_save_ximage");
        return false;
    }


    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, fh);

    initjpg(&cinfo, ximg);

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, JPG_QUALITY, TRUE);
    jpeg_start_compress(&cinfo, TRUE);

    row_stride = ximg->width * BPP;
    while (cinfo.next_scanline < cinfo.image_height) {
        row_pointer[0] = (unsigned char*)&ximg->data[cinfo.next_scanline * row_stride];
#ifdef _DEBUG
        if (!jpeg_write_scanlines(&cinfo, row_pointer, 1)) {
            debug("!!! jpeg_write_scanlines failed\n");
            status = false;
            break;
        }
#else
        (void)jpeg_write_scanlines(&cinfo, row_pointer, 1);
#endif
    }

    jpeg_finish_compress(&cinfo);
    fclose(fh);
    jpeg_destroy_compress(&cinfo);

    return status;
}
