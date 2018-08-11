#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libtar.h>
#include <zlib.h>
#include <libgen.h>
#include "archive.h"
#include "debug.h"
#include "flister.h"

//
// uncompress gzip archive to the temporary directory
//
static char *archive_prepare(char *filename, char *target) {
    char *destination;
    char buffer[4096];
    ssize_t len;
    FILE *dst;
    gzFile gsrc;

    if(!(gsrc = gzopen(filename, "r")))
        diep(filename);

    if(asprintf(&destination, "%s/%s", target, basename(filename)) < 0)
        diep("asprintf");

    if(!(dst = fopen(destination, "w")))
        diep(destination);

    while((len = gzread(gsrc, buffer, sizeof(buffer))))
        fwrite(buffer, 1, len, dst);

    fclose(dst);
    gzclose(gsrc);

    return destination;
}

char *archive_extract(char *filename, char *target) {
    TAR *th = NULL;
    struct stat st;
    char *destination;

    // checking if file is reachable
    if(stat(filename, &st))
        return NULL;

    // uncompression tar file to our ramdisk
    debug("[+] uncompressing archive: %s\n", filename);
    if(!(destination = archive_prepare(filename, target)))
        return NULL;

    // loading tar and extracting contents
    debug("[+] loading archive: %s\n", destination);
    if(tar_open(&th, destination, NULL, O_RDONLY, 0644, TAR_GNU))
        return NULL;

    // TODO: ensure security on files

    debug("[+] extracting archive\n");
    if(tar_extract_all(th, target))
        filename = NULL;

    tar_close(th);
    free(destination);

    return filename;
}

static int archive_compress(char *source, char *destination) {
    char buffer[4096];
    ssize_t len;
    FILE *src;
    gzFile gdst;

    if(!(src = fopen(source, "r")))
        diep(source);

    if(!(gdst = gzopen(destination, "w")))
        diep(destination);

    while((len = fread(buffer, 1, sizeof(buffer), src)))
        gzwrite(gdst, buffer, len);

    fclose(src);
    gzclose(gdst);

    return 0;
}

int archive_create(char *filename, char *source) {
    TAR *th = NULL;
    char *tempfile;

    if(asprintf(&tempfile, "%s.uncompressed", filename) < 0)
        diep("asprintf");

    debug("[+] building uncompressed archive: %s\n", tempfile);
    if(tar_open(&th, tempfile, NULL, O_WRONLY | O_CREAT, 0644, TAR_GNU))
        return 1;

    if(tar_append_tree(th, source, ".")) {
        warnp("tar_append_tree");
        tar_close(th);
        return 1;
    }

    tar_close(th);

    // compressing
    debug("[+] compressing file: %s > %s\n", tempfile, filename);
    int retval = archive_compress(tempfile, filename);

    // cleaning
    unlink(tempfile);
    free(tempfile);

    return retval;
}
