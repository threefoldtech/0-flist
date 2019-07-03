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
#include "zflist-edit.h"
#include "filesystem.h"
#include "tools.h"
#include "actions.h"
#include "actions_metadata.h"

//
// open
//
int zf_open(zf_callback_t *cb) {
    char temp[2048];

    // checking if arguments are set
    if(cb->argc != 2) {
        fprintf(stderr, "[-] action: open: missing filename\n");
        return 1;
    }

    // creating mountpoint directory (if not exists)
    if(!dir_exists(cb->settings->mnt)) {
        debug("[+] action: open: creating mountpoint: <%s>\n", cb->settings->mnt);

        if(dir_create(cb->settings->mnt) < 0)
            diep(cb->settings->mnt);
    }

    // checking if the mountpoint doesn't contains already
    // an flist database
    snprintf(temp, sizeof(temp), "%s/flistdb.sqlite3", cb->settings->mnt);
    if(file_exists(temp)) {
        fprintf(stderr, "[-] action: open: mountpoint already contains an open flist\n");
        return 1;
    }

    char *filename = cb->argv[1];
    debug("[+] action: open: opening file <%s>\n", filename);

    if(!libflist_archive_extract(filename, cb->settings->mnt)) {
        warnp("libflist_archive_extract");
        return 1;
    }

    debug("[+] action: open: flist opened\n");
    return 0;
}

//
// commit
//
int zf_commit(zf_callback_t *cb) {
    if(cb->argc != 2) {
        fprintf(stderr, "[-] action: open: missing filename\n");
        return 1;
    }

    char *filename = cb->argv[1];
    debug("[+] action: commit: creating <%s>\n", filename);

    // removing possible already existing db
    unlink(filename);

    // create flist
    if(!libflist_archive_create(filename, cb->settings->mnt)) {
        fprintf(stderr, "[-] action: commit: could not create flist\n");
        return 1;
    }

    debug("[+] action: commit: file ready\n");
    return 0;
}

//
// chmod
//
int zf_chmod(zf_callback_t *cb) {
    if(cb->argc != 3) {
        fprintf(stderr, "[-] action: chmod: missing mode or filename\n");
        return 1;
    }

    debug("[+] action: chmod: setting mode %s on %s\n", cb->argv[1], cb->argv[2]);

    int newmode = strtol(cb->argv[1], NULL, 8);
    discard char *dirpath = dirname(strdup(cb->argv[2]));
    char *filename = basename(cb->argv[2]);

    dirnode_t *dirnode;
    inode_t *inode;

    if(!(dirnode = libflist_directory_get(cb->database, dirpath))) {
        fprintf(stderr, "[-] action: chmod: no such parent directory\n");
        return 1;
    }

    if(!(inode = libflist_inode_from_name(dirnode, filename))) {
        fprintf(stderr, "[-] action: chmod: no such file\n");
        return 1;
    }

    debug("[+] action: chmod: current mode: 0o%o\n", inode->acl.mode);

    // remove 9 last bits and set new last 9 bits
    uint32_t cleared = inode->acl.mode & 0xfffffe00;
    inode->acl.mode = cleared | newmode;
    libflist_inode_acl_commit(inode);

    debug("[+] action: chmod: new mode: 0o%o\n", inode->acl.mode);

    dirnode_t *parent = libflist_directory_get_parent(cb->database, dirnode);
    libflist_dirnode_commit(dirnode, cb->database, parent, NULL);

    return 0;
}

//
// rm
//
int zf_rm(zf_callback_t *cb) {
    if(cb->argc != 2) {
        fprintf(stderr, "[-] action: rm: missing filename\n");
        return 1;
    }

    discard char *dirpath = dirname(strdup(cb->argv[1]));
    char *filename = basename(cb->argv[1]);

    debug("[+] action: rm: removing <%s> from <%s>\n", filename, dirpath);

    dirnode_t *dirnode;
    inode_t *inode;

    if(!(dirnode = libflist_directory_get(cb->database, dirpath))) {
        fprintf(stderr, "[-] action: rm: no such directory (file parent directory)\n");
        return 1;
    }

    if(!(inode = libflist_inode_from_name(dirnode, filename))) {
        fprintf(stderr, "[-] action: rm: no such file\n");
        return 1;
    }

    debug("[+] action: rm: file found (size: %lu bytes)\n", inode->size);
    debug("[+] action: rm: files in the directory: %lu\n", dirnode->inode_length);

    if(!libflist_directory_rm_inode(dirnode, inode)) {
        fprintf(stderr, "[-] action: rm: something went wrong when removing the file\n");
        return 1;
    }

    debug("[+] action: rm: file removed\n");
    debug("[+] action: rm: files in the directory: %lu\n", dirnode->inode_length);

    dirnode_t *parent = libflist_directory_get_parent(cb->database, dirnode);
    libflist_dirnode_commit(dirnode, cb->database, parent, NULL);

    return 0;
}

//
// ls
//
int zf_ls(zf_callback_t *cb) {
    if(cb->argc != 2) {
        fprintf(stderr, "[-] action: ls: missing directory\n");
        return 1;
    }

    char *dirpath = cb->argv[1];
    debug("[+] action: ls: listing <%s>\n", dirpath);

    dirnode_t *dirnode;

    if(!(dirnode = libflist_directory_get(cb->database, dirpath))) {
        fprintf(stderr, "[-] action: ls: no such directory (file parent directory)\n");
        return 1;
    }

    for(inode_t *inode = dirnode->inode_list; inode; inode = inode->next) {
        printf("%c", zf_ls_inode_type(inode));
        zf_ls_inode_perm(inode);

        printf(" %-8s %-8s  ", inode->acl.uname, inode->acl.gname);
        printf(" %8lu ", inode->size);
        printf(" %s\n", inode->name);
    }

    return 0;
}

//
// metadata
//
int zf_metadata(zf_callback_t *cb) {
    if(cb->argc < 2) {
        fprintf(stderr, "[-] action: metadata: missing metadata name\n");
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

    fprintf(stderr, "[-] action: metadata: unknown metadata name\n");
    return 1;
}

//
// cat
//
int zf_cat(zf_callback_t *cb) {
    if(cb->argc < 2) {
        fprintf(stderr, "[-] action: cat: missing filename\n");
        return 1;
    }

    flist_db_t *backdb;

    if(!(backdb = libflist_metadata_backend_database(cb->database))) {
        fprintf(stderr, "[-] action: cat: backend: %s\n", libflist_strerror());
        return 1;
    }

    flist_backend_t *backend = libflist_backend_init(backdb, "/");

    discard char *dirpath = dirname(strdup(cb->argv[1]));
    char *filename = basename(cb->argv[1]);

    debug("[+] action: cat: looking for: %s in %s\n", filename, dirpath);

    dirnode_t *dirnode;
    inode_t *inode;

    if(!(dirnode = libflist_directory_get(cb->database, dirpath))) {
        fprintf(stderr, "[-] action: cat: no such parent directory\n");
        return 1;
    }

    if(!(inode = libflist_inode_from_name(dirnode, filename))) {
        fprintf(stderr, "[-] action: cat: no such file\n");
        return 1;
    }

    for(size_t i = 0; i < inode->chunks->size; i++) {
        inode_chunk_t *ichunk = &inode->chunks->list[i];
        flist_chunk_t *chunk = libflist_chunk_new(ichunk->entryid, ichunk->decipher, NULL, 0);

        if(!libflist_backend_download_chunk(backend, chunk)) {
            fprintf(stderr, "[-] could not download file: %s\n", libflist_strerror());
            return 1;
        }

        printf("%.*s\n", (int) chunk->plain.length, chunk->plain.data);
        libflist_chunk_free(chunk);
    }

    return 0;
}

