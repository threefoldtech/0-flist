#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <libgen.h>
#include <getopt.h>
#include <capnp_c.h>
#include "libflist.h"
#include "zflist.h"
#include "filesystem.h"
#include "tools.h"
#include "actions.h"
#include "actions_metadata.h"
#include "actions_hub.h"

//
// open
//
int zf_open(zf_callback_t *cb) {
    int value;

    // checking if arguments are set
    if(cb->argc != 2) {
        zf_error(cb, "open", "missing filename");
        return 1;
    }

    if((value = zf_open_file(cb, cb->argv[1], cb->settings->mnt)) == 0)
        debug("[+] action: open: flist opened\n");

    return value;
}

//
// init
//
int zf_init(zf_callback_t *cb) {
    char temp[2048];

    // creating mountpoint directory (if not exists)
    if(!dir_exists(cb->settings->mnt)) {
        debug("[+] action: init: creating mountpoint: <%s>\n", cb->settings->mnt);

        if(dir_create(cb->settings->mnt) < 0)
            zf_diep(cb, cb->settings->mnt);
    }

    // checking if the mountpoint doesn't contains already
    // an flist database
    snprintf(temp, sizeof(temp), "%s/flistdb.sqlite3", cb->settings->mnt);
    if(file_exists(temp)) {
        zf_error(cb, "init", "mountpoint already contains an open flist");
        return 1;
    }

    debug("[+] action: creating the flist database\n");
    flist_db_t *database = libflist_db_sqlite_init(cb->settings->mnt);
    database->open(database);

    flist_ctx_t *ctx = libflist_context_create(database, NULL);

    // initialize root directory
    dirnode_t *root = libflist_dirnode_create("", "");
    libflist_serial_dirnode_commit(root, ctx, root);
    libflist_dirnode_free(root);

    // commit changes
    database->close(database);
    libflist_context_free(ctx);

    debug("[+] action: init: flist initialized\n");
    return 0;
}


//
// commit
//
int zf_commit(zf_callback_t *cb) {
    if(cb->argc != 2) {
        zf_error(cb, "open", "missing filename");
        return 1;
    }

    char *filename = cb->argv[1];
    debug("[+] action: commit: creating <%s>\n", filename);

    // removing possible already existing db
    unlink(filename);

    // create flist
    if(!libflist_archive_create(filename, cb->settings->mnt)) {
        zf_error(cb, "commit", "could not create flist");
        return 1;
    }

    debug("[+] action: commit: file ready\n");
    return 0;
}

//
// close
//
int zf_close(zf_callback_t *cb) {
    if(zf_remove_database(cb, cb->settings->mnt))
        return 1;

    return 0;
}

//
// chmod
//
int zf_chmod(zf_callback_t *cb) {
    if(cb->argc != 3) {
        zf_error(cb, "chmod", "missing mode or filename");
        return 1;
    }

    debug("[+] action: chmod: setting mode %s on %s\n", cb->argv[1], cb->argv[2]);

    int newmode = strtol(cb->argv[1], NULL, 8);
    discard char *dirpath = dirname(strdup(cb->argv[2]));
    char *filename = basename(cb->argv[2]);

    dirnode_t *dirnode;
    inode_t *inode;

    if(!(dirnode = libflist_dirnode_get(cb->ctx->db, dirpath))) {
        zf_error(cb, "chmod", "no such parent directory");
        return 1;
    }

    if(!(inode = libflist_inode_from_name(dirnode, filename))) {
        zf_error(cb, "chmod", "no such file");
        return 1;
    }

    debug("[+] action: chmod: current mode: 0o%o\n", inode->acl->mode);

    // remove 9 last bits and set new last 9 bits
    uint32_t cleared = inode->acl->mode & 0xfffffe00;
    inode->acl->mode = cleared | newmode;
    libflist_acl_commit(inode->acl);

    debug("[+] action: chmod: new mode: 0o%o\n", inode->acl->mode);

    dirnode_t *parent = libflist_dirnode_get_parent(cb->ctx->db, dirnode);
    libflist_serial_dirnode_commit(dirnode, cb->ctx, parent);

    libflist_dirnode_free(parent);
    libflist_dirnode_free(dirnode);

    return 0;
}

//
// rm
//
int zf_rm(zf_callback_t *cb) {
    if(cb->argc != 2) {
        zf_error(cb, "rm", "missing filename");
        return 1;
    }

    discard char *dirpath = dirname(strdup(cb->argv[1]));
    char *filename = basename(cb->argv[1]);

    debug("[+] action: rm: removing <%s> from <%s>\n", filename, dirpath);

    dirnode_t *dirnode;
    inode_t *inode;

    if(!(dirnode = libflist_dirnode_get(cb->ctx->db, dirpath))) {
        zf_error(cb, "rm", "no such directory (file parent directory)");
        return 1;
    }

    if(!(inode = libflist_inode_from_name(dirnode, filename))) {
        zf_error(cb, "rm", "no such file");
        return 1;
    }

    debug("[+] action: rm: file found (size: %lu bytes)\n", inode->size);
    debug("[+] action: rm: files in the directory: %lu\n", dirnode->inode_length);

    if(!libflist_directory_rm_inode(dirnode, inode)) {
        zf_error(cb, "rm", "something went wrong when removing the file");
        return 1;
    }

    libflist_inode_free(inode);

    debug("[+] action: rm: file removed\n");
    debug("[+] action: rm: files in the directory: %lu\n", dirnode->inode_length);

    dirnode_t *parent = libflist_dirnode_get_parent(cb->ctx->db, dirnode);
    libflist_serial_dirnode_commit(dirnode, cb->ctx, parent);

    libflist_dirnode_free(parent);
    libflist_dirnode_free(dirnode);

    return 0;
}

//
// rmdir
// warning: this remove directory and all subdirectories (recursive)
//
int zf_rmdir(zf_callback_t *cb) {
    if(cb->argc != 2) {
        zf_error(cb, "rmdir", "missing target directory");
        return 1;
    }

    char *dirpath = cb->argv[1];

    if(strcmp(dirpath, "/") == 0) {
        zf_error(cb, "rmdir", "cannot remove root directory");
        return 1;
    }

    debug("[+] action: rmdir: removing recursively <%s>\n", dirpath);

    dirnode_t *dirnode;

    if(!(dirnode = libflist_dirnode_get_recursive(cb->ctx->db, dirpath))) {
        zf_error(cb, "rmdir", "no such directory");
        return 1;
    }

    // fetching parent of this directory
    dirnode_t *parent = libflist_dirnode_get_parent(cb->ctx->db, dirnode);
    debug("[+] action: rmdir: directory found, parent: %s\n", parent->fullpath);

    // removing all subdirectories
    if(libflist_directory_rm_recursively(cb->ctx->db, dirnode) != 0) {
        zf_error(cb, "rmdir", "could not remove directories: %s", libflist_strerror());
        return 1;
    }

    // all subdirectories removed
    // looking for directory inode inside the parent now
    debug("[+] action: rmdir: all subdirectories removed, removing directory from parent\n");
    inode_t *inode = libflist_inode_search(parent, basename(dirnode->fullpath));

    // removing inode from the parent directory
    if(!libflist_directory_rm_inode(parent, inode)) {
        zf_error(cb, "rmdir", "something went wrong when removing the file");
        return 1;
    }

    libflist_inode_free(inode);

    // commit changes in the parent (and parent of the parent)
    dirnode_t *pparent = libflist_dirnode_get_parent(cb->ctx->db, parent);
    libflist_serial_dirnode_commit(parent, cb->ctx, pparent);

    libflist_dirnode_free(parent);
    libflist_dirnode_free(pparent);
    libflist_dirnode_free_recursive(dirnode);

    return 0;
}

//
// mkdir
//
int zf_mkdir(zf_callback_t *cb) {
    if(cb->argc != 2) {
        zf_error(cb, "mkdir", "missing directory name");
        return 1;
    }

    dirnode_t *dirnode;
    inode_t *newdir;

    char *dirpath = cb->argv[1];
    discard char *dirpathcpy = strdup(cb->argv[1]);
    char *parent = dirname(dirpathcpy);
    char *dirname = basename(dirpath);

    if((dirnode = libflist_dirnode_get(cb->ctx->db, dirpath))) {
        zf_error(cb, "mkdir", "cannot create directory: file exists");
        return 1;
    }

    if(!(dirnode = libflist_dirnode_get(cb->ctx->db, parent))) {
        zf_error(cb, "mkdir", "parent directory doesn't exists");
        return 1;
    }

    debug("[+] action: mkdir: creating <%s> inside <%s>\n", basename(dirpath), parent);

    if(!(newdir = libflist_directory_create(dirnode, dirname))) {
        zf_error(cb, "mkdir", "could not create new directory");
        return 1;
    }

    // commit changes in the parent
    dirnode_t *dparent = libflist_dirnode_get_parent(cb->ctx->db, dirnode);
    libflist_serial_dirnode_commit(dirnode, cb->ctx, dparent);

    libflist_dirnode_free(dirnode);
    libflist_dirnode_free(dparent);

    return 0;
}

//
// ls
//
static int zf_ls_json(zf_callback_t *cb, dirnode_t *dirnode) {
    json_t *list = json_array();

    for(inode_t *inode = dirnode->inode_list; inode; inode = inode->next) {
        json_t *entry = json_object();

        json_object_set(entry, "name", json_string(inode->name));
        json_object_set(entry, "type", json_string(zf_inode_typename(inode->type, inode->stype)));
        json_object_set(entry, "size", json_integer(inode->size));
        json_object_set(entry, "user", json_string(inode->acl->uname));
        json_object_set(entry, "group", json_string(inode->acl->gname));

        json_array_append(list, entry);
    }

    json_object_set_new(cb->jout, "response", list);
    libflist_dirnode_free(dirnode);

    return 0;
}

int zf_ls(zf_callback_t *cb) {
    char *dirpath = (cb->argc < 2) ? "/" : cb->argv[1];
    debug("[+] action: ls: listing <%s>\n", dirpath);

    dirnode_t *dirnode;

    if(!(dirnode = libflist_dirnode_get(cb->ctx->db, dirpath))) {
        zf_error(cb, "ls", "no such directory (file parent directory)");
        return 1;
    }

    if(cb->jout)
        return zf_ls_json(cb, dirnode);

    for(inode_t *inode = dirnode->inode_list; inode; inode = inode->next) {
        printf("%c", zf_ls_inode_type(inode));
        zf_ls_inode_perm(inode);

        printf(" %-8s %-8s  ", inode->acl->uname, inode->acl->gname);
        printf(" %8lu ", inode->size);
        printf(" %s\n", inode->name);
    }

    libflist_dirnode_free(dirnode);

    return 0;
}

//
// find
//
int zf_find(zf_callback_t *cb) {
    dirnode_t *dirnode;

    if(!(dirnode = libflist_dirnode_get(cb->ctx->db, "/"))) {
        zf_error(cb, "find", "no such root directory");
        return 1;
    }

    zf_find_recursive(cb, dirnode);
    zf_find_finalize(cb);

    libflist_dirnode_free(dirnode);

    return 0;
}

//
// stat
//
int zf_stat(zf_callback_t *cb) {
    if(cb->argc != 2) {
        zf_error(cb, "stat", "missing filename or directory");
        return 1;
    }

    char *target = cb->argv[1];
    discard char *targetcpy = strdup(cb->argv[1]);
    char *parentdir = dirname(targetcpy);
    char *filename = basename(target);

    dirnode_t *dirnode;
    inode_t *inode;

    // let's fetch parent directory and looking if inode exists inside
    if(!(dirnode = libflist_dirnode_get(cb->ctx->db, parentdir))) {
        zf_error(cb, "stat", "no parent directory found");
        return 1;
    }

    if(!(inode = libflist_inode_from_name(dirnode, filename))) {
        zf_error(cb, "stat", "no such file or directory");
        return 1;
    }

    // we found the inode
    int value = zf_stat_inode(cb, inode);
    libflist_dirnode_free(dirnode);

    return value;
}

//
// metadata
//
int zf_metadata(zf_callback_t *cb) {
    if(cb->argc < 2) {
        zf_error(cb, "metadata", "missing metadata name");
        return 1;
    }

    // fetching metadata from database
    if(cb->argc == 2)
        return zf_metadata_get(cb);

    // skipping first argument
    cb->argc -= 1;
    cb->argv += 1;

    // setting metadata
    if(strcmp(cb->argv[0], "backend") == 0)
        return zf_metadata_set_backend(cb);

    else if(strcmp(cb->argv[0], "entrypoint") == 0)
        return zf_metadata_set_entry(cb);

    else if(strcmp(cb->argv[0], "environ") == 0)
        return zf_metadata_set_environ(cb);

    else if(strcmp(cb->argv[0], "port") == 0)
        return zf_metadata_set_port(cb);

    else if(strcmp(cb->argv[0], "volume") == 0)
        return zf_metadata_set_volume(cb);

    else if(strcmp(cb->argv[0], "readme") == 0)
        return zf_metadata_set_readme(cb);

    zf_error(cb, "metadata", "unknown metadata name");
    return 1;
}

//
// cat
//
int zf_cat(zf_callback_t *cb) {
    if(cb->argc < 2) {
        zf_error(cb, "cat", "missing filename");
        return 1;
    }

    flist_db_t *backdb;

    if(!(backdb = libflist_metadata_backend_database(cb->ctx->db))) {
        zf_error(cb, "cat", "backend: %s", libflist_strerror());
        return 1;
    }

    flist_backend_t *backend = libflist_backend_init(backdb, "/");

    discard char *dirpath = dirname(strdup(cb->argv[1]));
    char *filename = basename(cb->argv[1]);

    debug("[+] action: cat: looking for: %s in %s\n", filename, dirpath);

    dirnode_t *dirnode;
    inode_t *inode;

    // there is no backend and dirnode free
    // in case of error, it's okay

    if(!(dirnode = libflist_dirnode_get(cb->ctx->db, dirpath))) {
        zf_error(cb, "cat", "no such parent directory");
        return 1;
    }

    if(!(inode = libflist_inode_from_name(dirnode, filename))) {
        zf_error(cb, "cat", "no such file");
        return 1;
    }

    for(size_t i = 0; i < inode->chunks->size; i++) {
        inode_chunk_t *ichunk = &inode->chunks->list[i];
        flist_chunk_t *chunk = libflist_chunk_new(ichunk->entryid, ichunk->decipher, NULL, 0);

        if(!libflist_backend_download_chunk(backend, chunk)) {
            zf_error(cb, "cat", "could not download file: %s", libflist_strerror());
            return 1;
        }

        printf("%.*s\n", (int) chunk->plain.length, chunk->plain.data);
        libflist_chunk_free(chunk);
    }

    libflist_dirnode_free(dirnode);
    libflist_backend_free(backend);

    return 0;
}

//
// put
//
int zf_put(zf_callback_t *cb) {
    if(cb->argc < 3) {
        zf_error(cb, "put", "missing host file or target destination");
        return 1;
    }

    // looking for backend
    if(zf_backend_detect())
        if(!(zf_backend_extract(cb->ctx)))
            return 1;

    // building directories
    char *localfile = cb->argv[1];
    char *filename = basename(localfile);
    discard char *dirpath = dirname(strdup(cb->argv[2]));
    char *targetname = basename(cb->argv[2]);

    // avoid root directory and directory name
    if(strcmp(targetname, "/") == 0 || strcmp(targetname, dirpath) == 0)
        targetname = filename;

    dirnode_t *dirnode;
    inode_t *inode;

    debug("[+] action: put: looking for directory: %s\n", cb->argv[2]);

    if(!(dirnode = libflist_dirnode_get(cb->ctx->db, cb->argv[2]))) {
        debug("[+] action: put: looking for directory: %s\n", dirpath);

        if(!(dirnode = libflist_dirnode_get(cb->ctx->db, dirpath))) {
            zf_error(cb, "put", "no such parent directory");
            return 1;
        }
    }

    // if user specified a directory as destination, forcing filename
    if(strcmp(targetname, dirnode->fullpath) == 0)
        targetname = filename;

    debug("[+] action: put: will put file in directory: %s\n", dirnode->fullpath);

    if((inode = libflist_inode_from_name(dirnode, targetname))) {
        debug("[+] action: put: requested filename (%s) already exists, overwriting\n", targetname);
        if(!libflist_directory_rm_inode(dirnode, inode)) {
            zf_error(cb, "put", "could not overwrite existing inode");
            return 1;
        }

        libflist_inode_free(inode);
    }

    if(!(inode = libflist_inode_from_localfile(localfile, dirnode, cb->ctx))) {
        zf_error(cb, "put", "could not load local file");
        return 1;
    }

    // rename inode to target file
    libflist_inode_rename(inode, targetname);

    // append inode to that directory
    libflist_dirnode_appends_inode(dirnode, inode);

    // commit
    dirnode_t *parent = libflist_dirnode_get_parent(cb->ctx->db, dirnode);
    libflist_serial_dirnode_commit(dirnode, cb->ctx, parent);

    libflist_dirnode_free(parent);
    libflist_dirnode_free(dirnode);

    return 0;
}

//
// putdir
//
int zf_putdir(zf_callback_t *cb) {
    if(cb->argc < 3) {
        zf_error(cb, "putdir", "missing host directory or target destination");
        return 1;
    }

    // looking for backend
    if(zf_backend_detect())
        if(!(zf_backend_extract(cb->ctx)))
            return 1;

    // building directories
    char *localdir = cb->argv[1];
    char *destdir = cb->argv[2];

    if(strcmp(localdir, "/") == 0) {
        zf_error(cb, "putdir", "adding your own root directory is not allowed");
        return 1;
    }

    debug("[+] action: putdir: looking for directory: %s\n", destdir);

    dirnode_t *dirnode;
    inode_t *inode;

    if(!(dirnode = libflist_dirnode_get(cb->ctx->db, destdir))) {
        zf_error(cb, "putdir", "no such parent directory");
        return 1;
    }

    if(!(inode = libflist_inode_from_localdir(localdir, dirnode, cb->ctx))) {
        zf_error(cb, "putdir", "could not load local directory");
        return 1;
    }

    libflist_stats_dump(&cb->ctx->stats);
    libflist_dirnode_free(dirnode);

    return 0;
}

//
// merge
//
int zf_merge(zf_callback_t *cb) {
    int value = 0;

    if(cb->argc < 2) {
        zf_error(cb, "merge", "missing flist argument to merge");
        return 1;
    }

    // create temporary workspace for the merging flist
    char dname[2048];

    snprintf(dname, sizeof(dname), "%s/mergeXXXXXX", cb->settings->mnt);
    debug("[+] action: merge: creating temporary directory\n");

    if(!mkdtemp(dname))
        zf_diep(cb, "dname");

    debug("[+] action: merge: temporary directory: %s\n", dname);

    // open the target flist into that directory
    if(zf_open_file(cb, cb->argv[1], dname)) {
        debug("[-] action: merge: could not extract merging flist\n");
        return 1;
    }

    // merging the flist with the current database
    flist_ctx_t *ctx = zf_internal_init(dname);

    // do the merge
    dirnode_t *merged;

    if(!(merged = libflist_merge(cb->ctx, ctx))) {
        zf_error(cb, "merge", "error: %s", libflist_strerror());
        value = 1;
    }

    libflist_serial_dirnode_commit(merged, cb->ctx, merged);
    libflist_dirnode_free_recursive(merged);

    // cleanup target context
    zf_internal_cleanup(ctx);

    // cleaning workspace
    if(zf_remove_database(cb, dname)) {
        zf_error(cb, "merge", "could not remove database file");
        return 1;
    }

    debug("[+] action: merge: removing temporary directory: %s\n", dname);
    if(rmdir(dname) < 0)
        zf_diep(cb, "dname");

    return value;
}

//
// put et putdir (memory leak)
//

//
// hub
//
int zf_hub(zf_callback_t *cb) {
    if(cb->argc < 2) {
        zf_error(cb, "hub", "missing hub subcommand");
        return 1;
    }

    // checking if token is specified
    cb->settings->token = getenv("ZFLIST_HUB_TOKEN");

    if(cb->settings->token == NULL || strlen(cb->settings->token) == 0) {
        zf_error(cb, "hub", "missing hub token");
        return 1;
    }

    // checking if a special user is specified
    cb->settings->user = getenv("ZFLIST_HUB_USER");

    // skipping first argument
    cb->argc -= 1;
    cb->argv += 1;

    // setting metadata
    if(strcmp(cb->argv[0], "upload") == 0)
        return zf_hub_upload(cb);

    else if(strcmp(cb->argv[0], "promote") == 0)
        return zf_hub_promote(cb);

    else if(strcmp(cb->argv[0], "symlink") == 0)
        return zf_hub_symlink(cb);

    else if(strcmp(cb->argv[0], "delete") == 0)
        return zf_hub_delete(cb);

    else if(strcmp(cb->argv[0], "readlink") == 0)
        return zf_hub_readlink(cb);

    else if(strcmp(cb->argv[0], "login") == 0)
        return zf_hub_login(cb);

    else if(strcmp(cb->argv[0], "refresh") == 0)
        return zf_hub_refresh(cb);

    zf_error(cb, "hub", "unknown hub subcommand");
    return 1;
}

