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
#include "libflist.h"
#include "verbose.h"

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
        return libflist_errp(filename);

    if(asprintf(&destination, "%s/%s.tar.gz", target, basename(filename)) < 0)
        return libflist_errp("asprintf");

    if(!(dst = fopen(destination, "w")))
        return libflist_errp(destination);

    while((len = gzread(gsrc, buffer, sizeof(buffer))))
        fwrite(buffer, 1, len, dst);

    fclose(dst);
    gzclose(gsrc);

    return destination;
}

char *libflist_archive_extract(char *filename, char *target) {
    TAR *th = NULL;
    struct stat st;
    char *destination;

    // checking if file is reachable
    if(stat(filename, &st))
        return libflist_errp(filename);

    // uncompression tar file
    debug("[+] libflist: archive: uncompressing: %s\n", filename);
    debug("[+] libflist: archive: target: %s\n", target);
    if(!(destination = archive_prepare(filename, target)))
        return libflist_errp("archive_prepare");

    // loading tar and extracting contents
    debug("[+] libflist: archive: loading: %s\n", destination);
    if(tar_open(&th, destination, NULL, O_RDONLY, 0644, TAR_GNU))
        return libflist_errp("tar_open");

    // TODO: ensure security on files

    debug("[+] libflist: archive: extracting archive\n");
    if(tar_extract_all(th, target)) {
        libflist_errp("tar_extract_all");
        filename = NULL;
    }

    tar_close(th);

    if(unlink(destination) < 0)
        return libflist_errp("unlink");

    free(destination);

    return filename;
}

static char *archive_compress(char *source, char *destination) {
    char buffer[4096];
    ssize_t len;
    FILE *src;
    gzFile gdst;

    if(!(src = fopen(source, "r")))
        return libflist_errp(source);

    if(!(gdst = gzopen(destination, "w")))
        return libflist_errp(destination);

    while((len = fread(buffer, 1, sizeof(buffer), src)))
        gzwrite(gdst, buffer, len);

    fclose(src);
    gzclose(gdst);

    return destination;
}

char *libflist_archive_create(char *filename, char *source) {
    TAR *th = NULL;
    char *tempfile;

    if(asprintf(&tempfile, "%s.uncompressed", filename) < 0)
        return libflist_errp("asprintf");

    debug("[+] building uncompressed archive: %s\n", tempfile);
    if(tar_open(&th, tempfile, NULL, O_WRONLY | O_CREAT, 0644, TAR_GNU))
        return libflist_errp("tar_open");

    if(tar_append_tree(th, source, ".")) {
        libflist_errp("tar_append_tree");
        tar_close(th);
        return NULL;
    }

    tar_close(th);

    // compressing
    debug("[+] compressing file: %s > %s\n", tempfile, filename);
    char *retval = archive_compress(tempfile, filename);

    // cleaning
    unlink(tempfile);
    free(tempfile);

    return retval;
}
