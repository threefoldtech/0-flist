#define _GNU_SOURCE
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <curl/curl.h>
#include "libflist.h"
#include "zflist.h"

size_t write_data(void *buffer, size_t size, size_t nmemb, void *userp) {
    (void) buffer;
    (void) userp;

    return size * nmemb;
}

int flist_upload(char *file, char *url, char *token) {
    CURL *curl;
    CURLcode res;
    struct curl_httppost* post = NULL;
    struct curl_httppost* last = NULL;
    char *cookie;
    int value = 0;

    if(asprintf(&cookie, "caddyoauth=%s;", token) < 0)
        diep("asprintf");

    if(!(curl = curl_easy_init()))
        return 1;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_formadd(
        &post, &last,
        CURLFORM_COPYNAME, "file",
        CURLFORM_FILE, file,
        CURLFORM_END
    );

    curl_easy_setopt(curl, CURLOPT_HTTPPOST, post);
    curl_easy_setopt(curl, CURLOPT_COOKIE, cookie);

    #ifdef FLIST_DEBUG
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    #else
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    #endif

    if((res = curl_easy_perform(curl)) != CURLE_OK) {
        fprintf(stderr, "[-] upload error: %s\n", curl_easy_strerror(res));
        value = 1;
    }

    curl_easy_cleanup(curl);

    return value;
}
