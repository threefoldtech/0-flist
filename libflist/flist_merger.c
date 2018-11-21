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
// fulltree is a pointer of pointer, in case when the fulltree is NULL,
// we replace it with the new source, as first source
dirnode_t *libflist_merge(dirnode_t **fulltree, dirnode_t *source) {
    libflist_dirnode_dumps(source);

    // if fulltree is NULL, this is the first
    // merging, we just returns the original dirnode tree
    // as the source one
    if(*fulltree == NULL) {
        debug("[+] merger: first flist, just installing this one as tree\n");
        *fulltree = source;
        return 0;
    }

    // merging source into fulltree

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
