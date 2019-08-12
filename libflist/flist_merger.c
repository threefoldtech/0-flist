#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <libgen.h>
#include "libflist.h"
#include "verbose.h"
#include "database.h"
#include "flist_dirnode.h"
#include "flist_inode.h"
#include "flist_serial.h"

static int flist_merge_sync_directories(dirnode_t *local, dirnode_t *target, flist_ctx_t *targetctx) {
    for(dirnode_t *subdir = target->dir_list; subdir; subdir = subdir->next) {
        dirnode_t *lookup;

        if((lookup = flist_dirnode_search(local, subdir->name))) {
            debug("[+] libflist: dirsync: directory <%s> already exists, skipping\n", subdir->name);
            continue;
        }

        debug("[+] libflist: dirsync: appending target: %s\n", subdir->name);

        dirnode_t *newdir = flist_dirnode_get_recursive(targetctx->db, subdir->fullpath);
        inode_t *inode = flist_inode_from_dirnode(newdir);

        flist_dirnode_appends_dirnode(local, newdir);
        flist_dirnode_appends_inode(local, inode);
    }

    return 0;
}

static int flist_merge_sync_inodes(dirnode_t *local, dirnode_t *target) {
    for(inode_t *inode = target->inode_list; inode; inode = inode->next) {
        inode_t *lookup;

        if((lookup = flist_inode_search(local, inode->name))) {
            debug("[+] libflist: syncnode: file <%s> already exists, skipping\n", inode->name);
            continue;
        }

        debug("[+] libflist: syncnode: file <%s> not found, duplicating\n", inode->name);
        lookup = flist_inode_duplicate(inode);
        flist_dirnode_appends_inode(local, lookup);
    }

    for(dirnode_t *subdir = local->dir_list; subdir; subdir = subdir->next) {
        dirnode_t *lookup;

        if(!(lookup = flist_dirnode_search(target, subdir->name))) {
            debug("[+] libflist: syncnode: subdirectory <%s> doesn't exists on target\n", subdir->name);
            continue;
        }

        debug("[+] libflist: syncnode: entering subdirectory: %s\n", subdir->fullpath);
        flist_merge_sync_inodes(subdir, lookup);
    }

    return 0;
}


// merge two context together
// let's do two pass like we do for the localdir insertion
// first pass will sync directories and second pass will
// sync all inodes
dirnode_t *libflist_merge(flist_ctx_t *source, flist_ctx_t *target) {
    dirnode_t *targetroot = NULL;
    dirnode_t *localroot = NULL;

    debug("[+] libflist: merging: starting merge process\n");

    if(!(targetroot = flist_dirnode_get_recursive(target->db, ""))) {
        libflist_set_error("could not load root directory from target flist");
        return NULL;
    }

    if(!(localroot = flist_dirnode_get_recursive(source->db, ""))) {
        libflist_set_error("could not load local root directory");
        return NULL;
    }

    // first pass: directories
    flist_merge_sync_directories(localroot, targetroot, target);

    // second pass: all inodes
    flist_merge_sync_inodes(localroot, targetroot);

    debug("[+] libflist: merging: cleaning target root tree\n");
    libflist_dirnode_free_recursive(targetroot);

    debug("[+] libflist: merging: nodes merged\n");

    return localroot;
}

