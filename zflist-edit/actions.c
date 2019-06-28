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
int zf_open(int argc, char *argv[], zfe_settings_t *settings) {
    char temp[2048];

    // checking if arguments are set
    if(argc != 2) {
        fprintf(stderr, "[-] action: open: missing filename\n");
        return 1;
    }

    // creating mountpoint directory (if not exists)
    if(!dir_exists(settings->mnt)) {
        debug("[+] action: open: creating mountpoint: <%s>\n", settings->mnt);

        if(dir_create(settings->mnt) < 0)
            diep(settings->mnt);
    }

    // checking if the mountpoint doesn't contains already
    // an flist database
    snprintf(temp, sizeof(temp), "%s/flistdb.sqlite3", settings->mnt);
    if(file_exists(temp)) {
        fprintf(stderr, "[-] action: open: mountpoint already contains an open flist\n");
        return 1;
    }

    char *filename = argv[1];
    printf("[+] action: open: opening file <%s>\n", filename);

    if(!libflist_archive_extract(filename, settings->mnt)) {
        warnp("libflist_archive_extract");
        return 1;
    }

    debug("[+] action: open: flist opened\n");
    return 0;
}

//
// commit
//
int zf_commit(int argc, char *argv[], zfe_settings_t *settings) {
    if(argc != 2) {
        fprintf(stderr, "[-] action: open: missing filename\n");
        return 1;
    }

    char *filename = argv[1];
    printf("[+] action: commit: creating <%s>\n", filename);

    // removing possible already existing db
    unlink(filename);

    // create flist
    if(!libflist_archive_create(filename, settings->mnt)) {
        fprintf(stderr, "[-] action: commit: could not create flist\n");
        return 1;
    }

    debug("[+] action: commit: file ready\n");
    return 0;
}

//
// chmod
//
int zf_chmod(int argc, char *argv[], zfe_settings_t *settings) {
    if(argc != 3) {
        fprintf(stderr, "[-] action: chmod: missing mode or filename\n");
        return 1;
    }

    debug("[+] action: chmod: setting mode %s on %s\n", argv[1], argv[2]);

    int newmode = strtol(argv[1], NULL, 8);
    char *dirpath = dirname(strdup(argv[2]));
    char *filename = basename(argv[2]);

    flist_db_t *database = zf_init(settings->mnt);
    dirnode_t *dirnode;
    inode_t *inode;

    if(!(dirnode = libflist_directory_get(database, dirpath))) {
        debug("[-] action: chmod: no such parent directory\n");
        return 1;
    }

    if(!(inode = libflist_inode_from_name(dirnode, filename))) {
        debug("[-] action: chmod: no such file\n");
        return 1;
    }

    printf("[+] action: chmod: current mode: 0o%o\n", inode->acl.mode);

    // remove 9 last bits and set new last 9 bits
    uint32_t cleared = inode->acl.mode & 0xfffffe00;
    inode->acl.mode = cleared | newmode;
    libflist_inode_acl_commit(inode);

    printf("[+] action: chmod: new mode: 0o%o\n", inode->acl.mode);

    dirnode_t *parent = libflist_directory_get_parent(database, dirnode);
    libflist_dirnode_commit(dirnode, database, parent, NULL);

    database->close(database);
    free(dirpath);

    return 0;
}

//
// rm
//
int zf_rm(int argc, char *argv[], zfe_settings_t *settings) {
    if(argc != 2) {
        fprintf(stderr, "[-] action: rm: missing filename\n");
        return 1;
    }

    char *dirpath = dirname(strdup(argv[1]));
    char *filename = basename(argv[1]);

    debug("[+] action: rm: removing <%s> from <%s>\n", filename, dirpath);

    flist_db_t *database = zf_init(settings->mnt);
    dirnode_t *dirnode;
    inode_t *inode;

    if(!(dirnode = libflist_directory_get(database, dirpath))) {
        debug("[-] action: rm: no such directory (file parent directory)\n");
        return 1;
    }

    if(!(inode = libflist_inode_from_name(dirnode, filename))) {
        debug("[-] action: rm: no such file\n");
        return 1;
    }

    printf("[+] action: rm: file found (size: %lu bytes)\n", inode->size);
    printf("[+] action: rm: files in the directory: %lu\n", dirnode->inode_length);

    if(!libflist_directory_rm_inode(dirnode, inode)) {
        printf("[-] action: rm: something went wrong when removing the file\n");
        return 1;
    }

    printf("[+] action: rm: file removed\n");
    printf("[+] action: rm: files in the directory: %lu\n", dirnode->inode_length);

    dirnode_t *parent = libflist_directory_get_parent(database, dirnode);
    libflist_dirnode_commit(dirnode, database, parent, NULL);

    database->close(database);
    free(dirpath);

    return 0;
}

//
// ls
//
int zf_ls(int argc, char *argv[], zfe_settings_t *settings) {
    if(argc != 2) {
        fprintf(stderr, "[-] action: ls: missing directory\n");
        return 1;
    }

    char *dirpath = argv[1];
    debug("[+] action: ls: listing <%s>\n", dirpath);

    flist_db_t *database = zf_init(settings->mnt);
    dirnode_t *dirnode;

    if(!(dirnode = libflist_directory_get(database, dirpath))) {
        debug("[-] action: ls: no such directory (file parent directory)\n");
        return 1;
    }

    for(inode_t *inode = dirnode->inode_list; inode; inode = inode->next) {
        printf("%c", zf_ls_inode_type(inode));
        zf_ls_inode_perm(inode);

        printf(" %-8s %-8s  ", inode->acl.uname, inode->acl.gname);
        printf(" %8lu ", inode->size);
        printf(" %s\n", inode->name);
    }

    database->close(database);

    return 0;
}

//
// metadata
//
int zf_metadata(int argc, char *argv[], zfe_settings_t *settings) {
    if(argc < 2) {
        fprintf(stderr, "[-] action: metadata: missing metadata name\n");
        return 1;
    }

    // fetching metadata from database
    if(argc == 2)
        return zf_metadata_get(argc, argv, settings);

    // setting metadata
    if(strcmp(argv[1], "backend") == 0)
        return zf_metadata_set_backend(argc, argv, settings);

    debug("[-] action: metadata: unknown metadata name\n");
    return 1;
}


