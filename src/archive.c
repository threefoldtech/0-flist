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
#include "flister.h"

//
// uncompress gzip archive to the temporary directory
//
static char *archive_prepare(char *filename, char *target) {
    char *destination;
    char buffer[4096];
    FILE *dst;
    gzFile gsrc;

    if(!(gsrc = gzopen(filename, "r")))
        diep(filename);

    if(asprintf(&destination, "%s/%s", target, basename(filename)) < 0)
        diep("asprintf");

    if(!(dst = fopen(destination, "w")))
        diep(destination);

    while(gzread(gsrc, buffer, sizeof(buffer)))
        fwrite(buffer, sizeof(buffer), 1, dst);

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
    verbose("[+] uncompressing archive: %s\n", filename);
    if(!(destination = archive_prepare(filename, target)))
        return NULL;

    // loading tar and extracting contents
    verbose("[+] loading archive: %s\n", destination);
    if(tar_open(&th, destination, NULL, O_RDONLY, 0644, TAR_GNU))
        return NULL;

    // TODO: ensure security on files

    verbose("[+] extracting archive\n");
    if(tar_extract_all(th, target))
        filename = NULL;

    tar_close(th);
    free(destination);

    return filename;
}
/*
char *archive_create(char *filename, char *source) {

}
*/
