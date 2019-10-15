#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <libgen.h>
#include <getopt.h>
#include <curl/curl.h>
#include "libflist.h"
#include "zflist.h"
#include "tools.h"

#define ZFLIST_HUB_BASEURL "https://hub.grid.tf"

// /api/flist/me/upload
#define ZFLIST_HUB_UPLOAD    ZFLIST_HUB_BASEURL "/api/flist/me/upload"

// /api/flist/me/upload-flist
#define ZFLIST_HUB_UPLOADFL  ZFLIST_HUB_BASEURL "/api/flist/me/upload-flist"

// /api/flist/me/promote/<sourcerepo>/<sourcefile>/<localname>
#define ZFLIST_HUB_PROMOTE   ZFLIST_HUB_BASEURL "/api/flist/me/promote/%s/%s/%s"

// /api/flist/me/<source>/link/<linkname>
#define ZFLIST_HUB_SYMLINK   ZFLIST_HUB_BASEURL "/api/flist/me/%s/link/%s"


//
// internal curl handling
//
typedef struct curl_t {
    CURL *handler;
    CURLcode code;
    char *body;
    size_t length;

} curl_t;

static size_t zf_curl_body(char *ptr, size_t size, size_t nmemb, void *userdata) {
	curl_t *curl = (curl_t *) userdata;
	size_t prev = curl->length;

    curl->length += (size * nmemb);
	curl->body = (char *) realloc(curl->body, (curl->length + 1));
    curl->body[curl->length] = '\0';

	memcpy(curl->body + prev, ptr, size * nmemb);

	return size * nmemb;
}

static int zf_hub_curl(zf_callback_t *cb, char *url, char *filename) {
    char *cookies = NULL;
    long httpcode;

    curl_t curl = {
        .handler = NULL,
        .code = 0,
        .body = NULL,
        .length = 0,
    };

    curl.handler = curl_easy_init();

    // building cookies
    if(!(cookies = calloc(sizeof(char), strlen(cb->settings->token) + 14)))
        zf_diep(cb, "cookies: calloc");

    strcat(cookies, "caddyoauth=");
    strcat(cookies, cb->settings->token);
    strcat(cookies, "; ");

    if(cb->settings->user) {
        if(!(cookies = realloc(cookies, strlen(cookies) + strlen(cb->settings->user) + 14)))
            zf_diep(cb, "cookies: realloc");

        strcat(cookies, "active-user=");
        strcat(cookies, cb->settings->user);
        strcat(cookies, ";");
    }

    debug("[+] hub: target: %s\n", url);

	if(filename) {
        debug("[+] hub: sending file: %s\n", filename);

        curl_mime *form = curl_mime_init(curl.handler);
        curl_mimepart *field = field = curl_mime_addpart(form);

        curl_mime_name(field, "file");
        curl_mime_filedata(field, filename);

        curl_easy_setopt(curl.handler, CURLOPT_MIMEPOST, form);
    }

    curl_easy_setopt(curl.handler, CURLOPT_URL, url);
    curl_easy_setopt(curl.handler, CURLOPT_WRITEDATA, &curl);
    curl_easy_setopt(curl.handler, CURLOPT_WRITEFUNCTION, zf_curl_body);
    curl_easy_setopt(curl.handler, CURLOPT_USERAGENT, "zflist/" ZFLIST_VERSION);
    curl_easy_setopt(curl.handler, CURLOPT_COOKIE, cookies);

    #ifdef FLIST_DEBUG
    // curl_easy_setopt(curl.handler, CURLOPT_VERBOSE, 1);
    #endif

    debug("[+] hub: sending request\n");

    curl.code = curl_easy_perform(curl.handler);
    curl_easy_getinfo(curl.handler, CURLINFO_RESPONSE_CODE, &httpcode);

    printf("[+] response [%ld]: %s", httpcode, curl.body);

    // cleaning
    curl_easy_cleanup(curl.handler);
    free(curl.body);
    free(cookies);

    return 0;
}

static char *zf_extension(char *str) {
    char *value;

    if((value = strrchr(str, '.')))
        return value;

    return "";
}

//
// subcommands callback
//
int zf_hub_upload(zf_callback_t *cb) {
    if(cb->argc != 2) {
        zf_error(cb, "hub", "missing arguments: source filename");
        return 1;
    }

    char *filename = cb->argv[1];

    if(strcmp(zf_extension(filename), ".flist") == 0)
        zf_hub_curl(cb, ZFLIST_HUB_UPLOADFL, filename);

    if(strcmp(zf_extension(filename), ".gz") == 0)
        zf_hub_curl(cb, ZFLIST_HUB_UPLOAD, filename);

    return 0;
}

int zf_hub_promote(zf_callback_t *cb) {
    if(cb->argc != 3) {
        zf_error(cb, "hub", "missing arguments: repo/file target");
        return 1;
    }

    if(strchr(cb->argv[1], '/') == NULL) {
        zf_error(cb, "hub", "malformed source argument, should be: repository/filename");
        return 1;
    }

    char *sourcerepo = dirname(strdup(cb->argv[1]));
    char *sourcefile = strrchr(cb->argv[1], '/') + 1;
    char *localname = cb->argv[2];
    char endpoint[1024];

    debug("[+] hub: promote: %s/%s -> %s\n", sourcerepo, sourcefile, localname);

    snprintf(endpoint, sizeof(endpoint), ZFLIST_HUB_PROMOTE, sourcerepo, sourcefile, localname);
    zf_hub_curl(cb, endpoint, NULL);

    free(sourcerepo);

    return 0;
}

int zf_hub_symlink(zf_callback_t *cb) {
    if(cb->argc != 3) {
        zf_error(cb, "hub", "missing arguments: source linkname");
        return 1;
    }

    char *source = cb->argv[1];
    char *linkname = cb->argv[2];
    char endpoint[1024];

    debug("[+] hub: symlink: you/%s -> %s\n", source, linkname);

    snprintf(endpoint, sizeof(endpoint), ZFLIST_HUB_SYMLINK, source, linkname);
    zf_hub_curl(cb, endpoint, NULL);

    return 0;
}

