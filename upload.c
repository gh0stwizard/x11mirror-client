#include <assert.h>
#include <curl/curl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"


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


extern int
upload_file (const char *path)
{
    CURLcode res;
    long file_size;
    struct MemoryStruct storage;
    char *filename;
    curl_mime *mime = NULL;
    curl_mimepart *part = NULL;

    // XXX: no critic
    filename = strrchr(path, '/');
    if (filename)
        filename++;
    else
        filename = (char*)path;

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

    return (res == CURLE_OK) ? 0 : 1;
}
