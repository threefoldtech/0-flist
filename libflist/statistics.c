#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include "libflist.h"

size_t libflist_stats_regular_add(flist_ctx_t *ctx, size_t amount) {
    ctx->stats.regular += amount;
    return ctx->stats.regular;
}

size_t libflist_stats_directory_add(flist_ctx_t *ctx, size_t amount) {
    ctx->stats.directory += amount;
    return ctx->stats.directory;
}

size_t libflist_stats_symlink_add(flist_ctx_t *ctx, size_t amount) {
    ctx->stats.symlink += amount;
    return ctx->stats.symlink;
}

size_t libflist_stats_special_add(flist_ctx_t *ctx, size_t amount) {
    ctx->stats.special += amount;
    return ctx->stats.special;
}

size_t libflist_stats_size_add(flist_ctx_t *ctx, size_t amount) {
    ctx->stats.size += amount;
    return ctx->stats.size;
}

flist_stats_t *libflist_stats_get(flist_ctx_t *ctx) {
    return &ctx->stats;
}
