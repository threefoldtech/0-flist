#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include "libflist.h"
#include "verbose.h"
#include "database.h"

// database is target output archive
dirnode_t *libflist_merge(dirnode_t *fulltree, dirnode_t *source) {
    (void) fulltree;

    libflist_dirnode_dumps(source);
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
