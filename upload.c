#include <assert.h>
#include <curl/curl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"

/* curl_mime was added since 7.56.0 */
#if LIBCURL_VERSION_NUM < 0x073800
    #include <b64/cencode.h>
#endif


static CURL *curl = NULL;


struct MemoryStruct
{
    char *memory;
    size_t size;
};


extern void
init_uploader (const char *url)
{
    assert (url);

    curl_global_init (CURL_GLOBAL_ALL);
    curl = curl_easy_init ();

    assert (curl);

    curl_easy_setopt (curl, CURLOPT_URL, url);
#if LIBCURL_VERSION_NUM < 0x073800
    curl_easy_setopt (curl, CURLOPT_POST, 1L);
    curl_easy_setopt (curl, CURLOPT_HEADER, 1L);
#endif
#if defined(_DEBUG)
    curl_easy_setopt (curl, CURLOPT_VERBOSE, 1L);
#else
    curl_easy_setopt (curl, CURLOPT_VERBOSE, 0L);
#endif
}


extern void
free_uploader (void)
{
    if (curl)
        curl_easy_cleanup (curl);
}


static size_t
write_cb (char *data, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *) userp;

    mem->memory = realloc (mem->memory, mem->size + realsize + 1);

    if (mem->memory == NULL) {
        warn("!!! curl write_cb: realloc: out of memory\n");
        return 0;
    }

    memcpy (&(mem->memory[mem->size]), data, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}


#if LIBCURL_VERSION_NUM >= 0x073800
extern int
upload_file (const char *path)
{
    CURLcode res;
    struct MemoryStruct storage;
    curl_mime *mime = NULL;
    curl_mimepart *part = NULL;


    storage.memory = malloc (1);
    storage.size = 0;
    assert (storage.memory);

    /* create the form */
    mime = curl_mime_init (curl);
    part = curl_mime_addpart (mime);
    curl_mime_name (part, "file");
    curl_mime_filedata (part, path);
    curl_mime_encoder (part, "base64");

    /* set curl options */
    curl_easy_setopt (curl, CURLOPT_MIMEPOST, mime);
    curl_easy_setopt (curl, CURLOPT_USERAGENT, "x11mirror-client/1.0");
    curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt (curl, CURLOPT_WRITEDATA, (void *) &storage);

    res = curl_easy_perform (curl);

    if (res != CURLE_OK)
        warn("curl error: %s\n", curl_easy_strerror (res));
#if defined(_DEBUG)
    else
        warn("*** curl response: %s\n", storage.memory);
#endif

    free (storage.memory);
    curl_mime_free (mime);

    return (res == CURLE_OK) ? 0 : 1;
}
#else
/*
 * libcurl < 7.56.0 + libb64
 */

struct FileStruct {
    FILE *fh;
    size_t size;
    char *buf;
    size_t bufsize;
    base64_encodestate b64_state;
};


static size_t
read_cb_base64 (char *buf, size_t size, size_t nitems, void *userp)
{
    struct FileStruct *up = (struct FileStruct *)userp;
    size_t buf_size = size * nitems;
    size_t read = 0;
    int r = 0;


    if (buf_size > 2 * up->bufsize || up->bufsize >= buf_size) {
        up->bufsize = buf_size / 2;
        up->buf = realloc (up->buf, up->bufsize);
        if (!up->buf) {
            warn ("!!! read_cb_base64 realloc: out of memory\n");
            return 0;
        }
    }

    read = fread (up->buf, size, up->bufsize, up->fh);
    if (read > 0)
        r = base64_encode_block (up->buf, read, buf, &(up->b64_state));
    else if (feof(up->fh))
        return 0;

//    debug ("*** size = %zu read = %zu r = %i\n", up->size, read, r);

    up->size -= read;
    if (up->size == 0)
        r += base64_encode_blockend(buf+r, &(up->b64_state));

    return r;
}


extern int
upload_file (const char *path)
{
    CURLcode res;
    long file_size;
    struct curl_httppost *form1 = NULL;
    struct curl_httppost *formend = NULL;
    struct curl_slist *chunk = NULL;
    struct FileStruct rddata;
    struct MemoryStruct wrdata;
    FILE *fh;
    char *filename;


    wrdata.memory = malloc (1);
    wrdata.size = 0;
    assert (wrdata.memory);

    // XXX: no critic
    filename = strrchr(path, '/');
    if (filename)
        filename++;
    else
        filename = (char*)path;


    fh = fopen (path, "rb");
    assert (fh);
    fseek (fh, 0L, SEEK_END);
    file_size = ftell (fh);
    rewind (fh);

    rddata.fh = fh;
    rddata.bufsize = sizeof(unsigned char) * 1024;
    rddata.buf = malloc (rddata.bufsize);
    rddata.size = file_size;
    base64_init_encodestate(&(rddata.b64_state));

    chunk = curl_slist_append(chunk, "Transfer-Encoding: chunked");
    res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

    curl_formadd (&form1, &formend,
                  CURLFORM_COPYNAME, "file",
                  CURLFORM_FILENAME, filename,
                  CURLFORM_STREAM, (void *) &rddata,
                  CURLFORM_END);

    curl_easy_setopt (curl, CURLOPT_HTTPPOST, form1);
    curl_easy_setopt (curl, CURLOPT_USERAGENT, "x11mirror-client/1.0");
    curl_easy_setopt (curl, CURLOPT_READFUNCTION, read_cb_base64);
    curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt (curl, CURLOPT_WRITEDATA, (void *) &wrdata);

    res = curl_easy_perform (curl);

    if (res != CURLE_OK)
        warn("curl error: %s\n", curl_easy_strerror (res));
#if defined(_DEBUG)
    else
        warn("*** curl response: %s\n", wrdata.memory);
#endif

    rddata.fh = NULL;
    fclose (fh);
    free (rddata.buf);
    free (wrdata.memory);
    curl_formfree (form1);
    curl_slist_free_all (chunk);

    return (res == CURLE_OK) ? 0 : 1;
}
#endif
