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
#include "zflist.h"
#include "tools.h"

void __cleanup_free(void *p) {
    free(* (void **) p);
}

flist_ctx_t *zf_internal_init(char *mountpoint) {
    flist_ctx_t *ctx;
    flist_db_t *database = libflist_db_sqlite_init(mountpoint);

    debug("[+] database: opening the flist database\n");

    ctx = libflist_context_create(database, NULL);
    ctx->db->open(ctx->db);

    return ctx;
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
        printf("%c", (inode->acl->mode & mask) ? *layout : '-');
        layout += 1;
    }
}

int zf_stat_inode(inode_t *inode) {
    printf("  File: /%s\n", inode->fullpath[0] == '/' ? inode->fullpath + 1 : inode->fullpath);
    printf("  Size: %lu bytes\n", inode->size);

    printf("Access: (%o/%c", inode->acl->mode, zf_ls_inode_type(inode));
    zf_ls_inode_perm(inode);

    printf(")  UID: %s, GID: %s\n", inode->acl->uname, inode->acl->gname);

    printf("Access: %lu\n", inode->modification);
    printf("Create: %lu\n", inode->creation);

    if(inode->type == INODE_LINK)
        printf("Target: %s\n", inode->link);

    if(inode->type == INODE_SPECIAL)
        printf("Device: %s\n", inode->sdata);

    printf("Chunks: ");

    if(!inode->chunks || inode->chunks->size == 0) {
        printf("(empty set)\n");
        return 0;
    }

    for(size_t i = 0; i < inode->chunks->size; i++) {
        inode_chunk_t *ichunk = &inode->chunks->list[i];

        if(i > 0)
            printf("        ");

        discard char *hashstr = libflist_hashhex((unsigned char *) ichunk->entryid, ichunk->entrylen);
        discard char *keystr = libflist_hashhex((unsigned char *) ichunk->decipher, ichunk->decipherlen);

        printf("key: %s, decipher: %s\n", hashstr, keystr);

    }

    return 0;
}

// looking for global defined backend
// and set context if possible
flist_ctx_t *zf_backend_detect(flist_ctx_t *ctx) {
    flist_db_t *backdb = NULL;
    char *envbackend;

    if(!(envbackend = getenv("UPLOADBACKEND"))) {
        debug("[-] WARNING:\n");
        debug("[-] WARNING: upload backend is not set and is requested\n");
        debug("[-] WARNING: file won't be uploaded, but chunks\n");
        debug("[-] WARNING: will be computed and stored\n");
        debug("[-] WARNING:\n");
        return NULL;
   }

    if(!(backdb = libflist_metadata_backend_database_json(envbackend))) {
        fprintf(stderr, "[-] action: put: backend: %s\n", libflist_strerror());
        return NULL;
    }

    // updating context
    ctx->backend = libflist_backend_init(backdb, "/");

    return ctx;
}
