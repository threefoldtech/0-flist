#define _DEFAULT_SOURCE
#define _GNU_SOURCE
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
#include "filesystem.h"
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

void zf_internal_cleanup(flist_ctx_t *ctx) {
    ctx->db->close(ctx->db);
    libflist_context_free(ctx);
}

void zf_internal_json_init(zf_callback_t *cb) {
    // initialize json response object
    cb->jout = json_object();

    json_object_set(cb->jout, "success", json_true());
    json_object_set(cb->jout, "error", json_null());
    json_object_set(cb->jout, "response", json_object());
}

void zf_internal_json_finalize(zf_callback_t *cb) {
    char *json;

    // dump json response object
    if(!(json = json_dumps(cb->jout, 0))) {
        fprintf(stderr, "zflist: json: could not dumps message\n");
        return;
    }

    puts(json);
    free(json);
}

int zf_remove_database(zf_callback_t *cb, char *mountpoint) {
    char dbfile[2048];

    snprintf(dbfile, sizeof(dbfile), "%s/flistdb.sqlite3", mountpoint);
    if(!file_exists(dbfile)) {
        zf_error(cb, "remove", "no database opened found");
        return 1;
    }

    debug("[+] action: remove: unlink database: %s\n", dbfile);
    if(unlink(dbfile) < 0) {
        zf_warnp(cb, dbfile);
        return 1;
    }

    return 0;
}

int zf_open_file(zf_callback_t *cb, char *filename, char *endpoint) {
    char temp[2048];

    // creating mountpoint directory (if not exists)
    if(!dir_exists(endpoint)) {
        debug("[+] action: open file: creating mountpoint: <%s>\n", endpoint);

        if(dir_create(endpoint) < 0)
            zf_diep(cb, endpoint);
    }

    // checking if the mountpoint doesn't contains already
    // an flist database
    snprintf(temp, sizeof(temp), "%s/flistdb.sqlite3", endpoint);
    if(file_exists(temp)) {
        zf_error(cb, "open file", "mountpoint already contains an open flist");
        return 1;
    }

    debug("[+] action: open file: opening file <%s>\n", filename);

    if(!libflist_archive_extract(filename, endpoint)) {
        zf_warnp(cb, "libflist_archive_extract");
        return 1;
    }

    return 0;
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

char *zf_inode_typename(inode_type_t type, inode_special_t special) {
    char *types[] = {"directory", "regular file", "symlink"};
    char *specials[] = {"unix socket", "block device", "character device", "fifo", "unknown"};

    if(type == INODE_SPECIAL)
        return specials[special];

    return types[type];
}

//
// stat implementation
//
int zf_stat_inode_json(zf_callback_t *cb, inode_t *inode) {
    json_t *response = json_object_get(cb->jout, "response");

    char filename[2048];
    sprintf(filename, "/%s", inode->fullpath[0] == '/' ? inode->fullpath + 1 : inode->fullpath);

    json_object_set_new(response, "file", json_string(filename));
    json_object_set_new(response, "size", json_integer(inode->size));
    json_object_set_new(response, "mode", json_integer(inode->acl->mode));
    json_object_set_new(response, "user", json_string(inode->acl->uname));
    json_object_set_new(response, "group", json_string(inode->acl->gname));
    json_object_set_new(response, "created", json_integer(inode->creation));
    json_object_set_new(response, "modified", json_integer(inode->modification));

    if(inode->type == INODE_LINK)
        json_object_set_new(response, "linkto", json_string(inode->link));

    if(inode->type == INODE_SPECIAL)
        json_object_set_new(response, "device", json_string(inode->sdata));

    json_t *chunks = json_array();

    for(size_t i = 0; inode->chunks && i < inode->chunks->size; i++) {
        inode_chunk_t *ichunk = &inode->chunks->list[i];
        json_t *chunk = json_object();

        discard char *hashstr = libflist_hashhex((unsigned char *) ichunk->entryid, ichunk->entrylen);
        discard char *keystr = libflist_hashhex((unsigned char *) ichunk->decipher, ichunk->decipherlen);

        json_object_set_new(chunk, "key", json_string(hashstr));
        json_object_set_new(chunk, "decipher", json_string(keystr));
        json_array_append(chunks, chunk);
    }

    json_object_set_new(response, "chunks", chunks);

    return 0;
}

static int zf_stat_inode_text(inode_t *inode) {
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

int zf_stat_inode(zf_callback_t *cb, inode_t *inode) {
    if(cb->jout)
        return zf_stat_inode_json(cb, inode);

    return zf_stat_inode_text(inode);
}

// looking for global defined backend
// and set context if possible
flist_ctx_t *zf_backend_detect(flist_ctx_t *ctx) {
    flist_db_t *backdb = NULL;
    char *envbackend;

    if(!(envbackend = getenv("ZFLIST_BACKEND"))) {
        debug("[-] WARNING:\n");
        debug("[-] WARNING: upload backend is not set and is requested\n");
        debug("[-] WARNING: file won't be uploaded, but chunks\n");
        debug("[-] WARNING: will be computed and stored\n");
        debug("[-] WARNING:\n");
        return NULL;
    }

    debug("[+] backend: detecting backend settings\n");

    if(!(backdb = libflist_metadata_backend_database_json(envbackend))) {
        fprintf(stderr, "[-] action: put: backend: %s\n", libflist_strerror());
        return NULL;
    }

    // updating context
    ctx->backend = libflist_backend_init(backdb, "/");

    debug("[+] backend: connected and attached to context\n");

    return ctx;
}

//
// find implementaion
//
static int zf_find_recursive_text(zf_callback_t *cb, dirnode_t *dirnode) {
    dirnode_t *subnode = NULL;

    for(inode_t *inode = dirnode->inode_list; inode; inode = inode->next) {
        // print filename
        printf("/%s\n", inode->fullpath);

        // if it's a directory, let's walk inside
        if(inode->type == INODE_DIRECTORY) {
            libflist_stats_directory_add(cb->ctx, 1);

            if(!(subnode = libflist_dirnode_get(cb->ctx->db, inode->fullpath))) {
                zf_error(cb, "find", "recursive directory not found");
                return 1;
            }

            zf_find_recursive_text(cb, subnode);
            libflist_dirnode_free(subnode);
        }

        // updating statistics
        if(inode->type == INODE_FILE) {
            libflist_stats_regular_add(cb->ctx, 1);
            libflist_stats_size_add(cb->ctx, inode->size);
        }

        if(inode->type == INODE_SPECIAL)
            libflist_stats_special_add(cb->ctx, 1);

        if(inode->type == INODE_LINK)
            libflist_stats_symlink_add(cb->ctx, 1);
    }

    return 0;
}

static int zf_find_recursive_json(zf_callback_t *cb, dirnode_t *dirnode) {
    dirnode_t *subnode = NULL;
    char buffer[2048];

    json_t *response = json_object_get(cb->jout, "response");
    json_t *content = json_object_get(response, "content");

    for(inode_t *inode = dirnode->inode_list; inode; inode = inode->next) {
        json_t *entry = json_object();

        snprintf(buffer, sizeof(buffer), "/%s", inode->fullpath);

        json_object_set_new(entry, "size", json_integer(inode->size));
        json_object_set_new(entry, "path", json_string(buffer));
        json_array_append_new(content, entry);

        // if it's a directory, let's walk inside
        if(inode->type == INODE_DIRECTORY) {
            libflist_stats_directory_add(cb->ctx, 1);

            if(!(subnode = libflist_dirnode_get(cb->ctx->db, inode->fullpath))) {
                zf_error(cb, "find", "recursive directory not found");
                return 1;
            }

            zf_find_recursive_json(cb, subnode);
            libflist_dirnode_free(subnode);
        }

        // updating statistics
        if(inode->type == INODE_FILE) {
            libflist_stats_regular_add(cb->ctx, 1);
            libflist_stats_size_add(cb->ctx, inode->size);
        }

        if(inode->type == INODE_SPECIAL)
            libflist_stats_special_add(cb->ctx, 1);

        if(inode->type == INODE_LINK)
            libflist_stats_symlink_add(cb->ctx, 1);
    }

    return 0;
}

int zf_find_recursive(zf_callback_t *cb, dirnode_t *dirnode) {
    if(cb->jout) {
        json_t *response = json_object_get(cb->jout, "response");
        json_t *content = json_array();

        json_object_set(response, "content", content);
        cb->userptr = content;

        return zf_find_recursive_json(cb, dirnode);
    }

    return zf_find_recursive_text(cb, dirnode);
}

static int zf_find_finalize_text(zf_callback_t *cb) {
    flist_stats_t *stats = libflist_stats_get(cb->ctx);

    printf("\nContent summary:\n");
    printf("  Regular files  : %zu\n", stats->regular);
    printf("  Symbolic links : %lu\n", stats->symlink);
    printf("  Directories    : %lu\n", stats->directory);
    printf("  Special files  : %lu\n", stats->special);
    printf("  Total size     : %.2f MB\n\n", (stats->size / (1024.0 * 1024)));

    return 0;
}

static int zf_find_finalize_json(zf_callback_t *cb) {
    flist_stats_t *stats = libflist_stats_get(cb->ctx);
    json_t *response = json_object_get(cb->jout, "response");

    json_object_set_new(response, "regular", json_integer(stats->regular));
    json_object_set_new(response, "symlink", json_integer(stats->symlink));
    json_object_set_new(response, "directory", json_integer(stats->directory));
    json_object_set_new(response, "special", json_integer(stats->special));
    json_object_set_new(response, "fullsize", json_integer(stats->size));

    return 0;
}

int zf_find_finalize(zf_callback_t *cb) {
    if(cb->jout)
        return zf_find_finalize_json(cb);

    return zf_find_finalize_text(cb);
}

//
// error handling
//
static void zf_error_json(zf_callback_t *cb, char *function, char *message, va_list argp) {
    char *str = NULL;
    json_t *error = json_object();

    if(vasprintf(&str, message, argp) < 0)
        zf_diep(cb, "vasprintf");

    json_object_set(error, "function", json_string(function));
    json_object_set(error, "message", json_string(str));

    json_object_set(cb->jout, "success", json_false());
    json_object_set(cb->jout, "error", error);

    free(str);
}

static void zf_error_stderr(zf_callback_t *cb, char *function, char *message, va_list argp) {
    (void) cb;

    fprintf(stderr, "zflist: %s: ", function);
    vfprintf(stderr, message, argp);
    fprintf(stderr, "\n");
}

void zf_error(zf_callback_t *cb, char *function, char *message, ...) {
    va_list args;

    va_start(args, message);

    if(cb->jout)
        zf_error_json(cb, function, message, args);
    else
        zf_error_stderr(cb, function, message, args);

    va_end(args);
}

void zf_warnp(zf_callback_t *cb, const char *str) {
    zf_error(cb, "warning", "%s: %s", str, strerror(errno));
}

void zf_diep(zf_callback_t *cb, const char *str) {
    zf_warnp(cb, str);
    exit(EXIT_FAILURE);
}

void zf_dies(zf_callback_t *cb, const char *str) {
    zf_error(cb, "critical", "%s", str);
    exit(EXIT_FAILURE);
}

