#include <assert.h>
#include <curl/curl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    curl_easy_setopt (curl, CURLOPT_POST, 1L);
    curl_easy_setopt (curl, CURLOPT_HEADER, 1L);
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
        fprintf (stderr, "curl: realloc: out of memory\n");
        return 0;
    }

    memcpy (&(mem->memory[mem->size]), data, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}


extern int
upload_file (FILE * fh)
{
    CURLcode res;
    long file_size;
    struct curl_httppost *form1 = NULL;
    struct curl_httppost *formend = NULL;
    struct curl_slist *header = NULL;
    struct MemoryStruct storage;

    assert (fh);

    fseek (fh, 0L, SEEK_END);
    file_size = ftell (fh);
    rewind (fh);

    storage.memory = malloc (1);
    storage.size = 0;

    assert (storage.memory);

    curl_formadd (&form1, &formend,
                  CURLFORM_COPYNAME, "file",
                  CURLFORM_FILENAME, "x11mirror.xwd.gz",
                  CURLFORM_STREAM, (void *) fh,
                  CURLFORM_CONTENTLEN, file_size,
                  CURLFORM_END);

    header = curl_slist_append (header, "Expect;");

    curl_easy_setopt (curl, CURLOPT_HTTPPOST, form1);
    curl_easy_setopt (curl, CURLOPT_POSTFIELDSIZE, file_size);
    curl_easy_setopt (curl, CURLOPT_HTTPHEADER, header);
    curl_easy_setopt (curl, CURLOPT_USERAGENT, "x11mirror-client/1.0");
    curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt (curl, CURLOPT_WRITEDATA, (void *) &storage);

    res = curl_easy_perform (curl);

    if (res != CURLE_OK)
        fprintf (stderr, "curl error: %s\n", curl_easy_strerror (res));
#if defined(_DEBUG)
    else
        fprintf (stderr, "curl response: %s\n", storage.memory);
#endif

    free (storage.memory);
    curl_formfree (form1);
    curl_slist_free_all (header);

    return (res == CURLE_OK) ? 0 : 1;
}
