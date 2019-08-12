#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <blake2.h>
#include <libgen.h>
#include "libflist.h"
#include "verbose.h"
#include "flist_serial.h"
#include "flist_dirnode.h"

//
// flist helpers
//
char *flist_path_key(char *path) {
    uint8_t hash[FLIST_ENTRY_KEY_LENGTH];

    if(blake2b(hash, path, "", FLIST_ENTRY_KEY_LENGTH, strlen(path), 0) < 0) {
        debug("[-] libflist: blake2 error\n");
        return NULL;
    }

    return libflist_hashhex(hash, FLIST_ENTRY_KEY_LENGTH);
}

char *flist_clean_path(char *path) {
    size_t offset, length;

    offset = 0;
    length = strlen(path);

    if(length == 0)
        return calloc(1, 1);

    // remove lead slash
    if(path[0] == '/')
        offset = 1;

    // remove trailing slash
    if(path[length - 1] == '/')
        length -= 1;

    return strndup(path + offset, length);
}

flist_ctx_t *flist_context_create(flist_db_t *db, flist_backend_t *backend) {
    flist_ctx_t *ctx;

    if(!(ctx = malloc(sizeof(flist_ctx_t))))
        diep("malloc");

    ctx->db = db;
    ctx->backend = backend;

    memset(&ctx->stats, 0x00, sizeof(flist_stats_t));

    return ctx;
}

void flist_context_free(flist_ctx_t *ctx) {
    free(ctx);
}


//
// public interface
//
char *libflist_path_key(char *path) {
    return flist_path_key(path);
}

flist_ctx_t *libflist_context_create(flist_db_t *db, flist_backend_t *backend) {
    return flist_context_create(db, backend);
}

void libflist_context_free(flist_ctx_t *ctx) {
    flist_context_free(ctx);
}
