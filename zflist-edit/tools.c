#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <libgen.h>
#include <getopt.h>
#include "libflist.h"
#include "zflist-edit.h"
#include "tools.h"

void __cleanup_free(void *p) {
    free(* (void **) p);
}

flist_db_t *zf_init(char *mountpoint) {
    debug("[+] database: opening the flist database\n");

    flist_db_t *database = libflist_db_sqlite_init(mountpoint);
    database->open(database);

    return database;
}

char zf_ls_inode_type(inode_t *inode) {
    char *slayout = "sbcf?";
    char *rlayout = "d-l.";

    // FIXME: overflow possible
    if(inode->type == INODE_SPECIAL)
        return slayout[inode->stype];

    return rlayout[inode->type];
}

void zf_ls_inode_perm(inode_t *inode) {
    char *layout = "rwxrwxrwx";

    // foreach permissions bits, checking
    for(int mask = 1 << 8; mask; mask >>= 1) {
        printf("%c", (inode->acl.mode & mask) ? *layout : '-');
        layout += 1;
    }
}

