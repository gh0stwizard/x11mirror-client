#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <curl/curl.h>


static CURL *curl;


extern void
init_uploader (const char *url)
{
    assert(url);

    curl_global_init (CURL_GLOBAL_ALL);
    curl = curl_easy_init ();

    assert(curl);

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
    curl_easy_cleanup (curl);
}


#if 0
static size_t
read_cb (char *buffer, size_t size, size_t nmemb, void *stream)
{
    curl_off_t nread;
    size_t retcode;


    errno = 0;
    retcode = fread (buffer, size, nmemb, (FILE *)stream);
    nread = (curl_off_t)retcode;

    fprintf (stderr,
            "*** We read %" CURL_FORMAT_CURL_OFF_T " bytes from file\n",
            nread);

    if (errno)
        fprintf (stderr, "!!! fread: %s\n", strerror (errno));

    return retcode;
}
#endif


extern int
upload_file (FILE *fh)
{
    CURLcode res;
    long file_size;
    struct curl_httppost *form1 = NULL;
    struct curl_httppost *formend = NULL;
    struct curl_slist *header = NULL;


    assert(fh);

    fseek (fh, 0L, SEEK_END);
    file_size = ftell (fh);
    rewind (fh);

    curl_formadd (
        &form1,
        &formend,
        CURLFORM_COPYNAME, "file",
        CURLFORM_FILENAME, "x11mirror.xwd.gz",
        CURLFORM_STREAM, (void *)fh,
        CURLFORM_CONTENTLEN, file_size,
        CURLFORM_END);

    header = curl_slist_append (header, "Expect;");

    curl_easy_setopt (curl, CURLOPT_HTTPPOST, form1);
    curl_easy_setopt (curl, CURLOPT_POSTFIELDSIZE, file_size);
    curl_easy_setopt (curl, CURLOPT_HTTPHEADER, header);
#if 0
    curl_easy_setopt (curl, CURLOPT_READFUNCTION, &read_cb);
#endif

    res = curl_easy_perform (curl);

    if (res != CURLE_OK) {
        fprintf (stderr, "curl: %s\n", curl_easy_strerror (res));
        return 1;
    }

    curl_formfree(form1);
    curl_slist_free_all (header);

    return 0;
}
