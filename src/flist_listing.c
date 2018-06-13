#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "flister.h"
#include "flist.capnp.h"
#include "flist_read.h"
#include "flist_walker.h"
#include "flist_listing_dump.h"
#include "flist_listing_json.h"
#include "flist_listing_ls.h"
#include "flist_listing_tree.h"
#include "flist_listing_check.h"

// walking entry point
int flist_listing(database_t *database) {
    walker_t walker = {
        .database = database,
        .userptr = NULL,
        .callback = flist_ls,
        .postproc = NULL,
    };

    if(settings.list == LIST_TREE) {
        walker.callback = flist_tree;
        walker.userptr = flist_tree_init();
    }

    if(settings.list == LIST_DUMP)
        walker.callback = flist_dump;

    if(settings.list == LIST_BLOCKS)
        walker.callback = flist_blocks;

    if(settings.list == LIST_JSON) {
        walker.callback = flist_json;
        walker.postproc = flist_json_dump;
        walker.userptr = flist_json_init();
    }

    if(settings.list == LIST_CHECK) {
        if(!(settings.backendhost)) {
            fprintf(stderr, "[-] missing backend for integrity check\n");
            return 0;
        }

        walker.callback = flist_check;
        walker.postproc = flist_check_done;
        walker.userptr = flist_check_init();
    }

    // root directory is an empty key
    const char *key = flist_pathkey("");

    // walking starting from the root
    flist_walk_directory(&walker, key);

    if(walker.postproc)
        walker.postproc(&walker);

    // cleaning
    free((char *) key);

    return 0;
}
