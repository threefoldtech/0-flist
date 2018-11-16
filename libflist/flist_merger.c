#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include "libflist.h"
#include "database.h"

// database is target output archive
int libflist_merge(flist_db_t *database, flist_merge_t *merge) {
    (void) database;

    for(size_t i = 0; i < merge->length; i++) {
        debug("[+] merging source: %s\n", merge->sources[i]);
    }

    return 0;
}

int libflist_merge_list_init(flist_merge_t *merge) {
    merge->length = 0;
    merge->sources = NULL;

    return 0;
}

int libflist_merge_list_append(flist_merge_t *merge, char *path) {
    merge->length += 1;

    if(!(merge->sources = realloc(merge->sources, sizeof(char *) * merge->length))) {
        libflist_errp("realloc");
        return 1;
    }

    if(!(merge->sources[merge->length - 1] = strdup(path))) {
        libflist_errp("strdup");
        return 1;
    }

    return 0;
}

int libflist_merge_list_free(flist_merge_t *merge) {
    for(size_t i = 0; i < merge->length; i++)
        free(merge->sources[i]);

    free(merge->sources);

    return 0;
}
