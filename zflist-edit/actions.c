#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <libgen.h>
#include <capnp_c.h>
#include "libflist.h"
#include "zflist-edit.h"
#include "filesystem.h"
#include "actions.h"

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

// FIXME
static capn_text chars_to_text(const char *chars) {
    return (capn_text) {
        .len = (int) strlen(chars),
        .str = chars,
        .seg = NULL,
    };
}

static flist_db_t *zf_init(char *mountpoint) {
    flist_db_t *database = libflist_db_sqlite_init(mountpoint);
    database->open(database);

    return database;
}


int zf_chmod(int argc, char *argv[], zfe_settings_t *settings) {
#if 0
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
        debug("[-] action: chmod: could not open the parent directory\n");
        return 1;
    }

    if(!(inode = libflist_inode_from_name(dirnode, filename))) {
        debug("[-] action: chmod: could not find requested file\n");
        return 1;
    }

    printf("[+] action: chmod: current mode: 0o%o\n", inode->acl.mode);

    // remove 9 last bits and set new last 9 bits
    uint32_t cleared = inode->acl.mode & 0xfffffe00;
    inode->acl.mode = cleared | newmode;

    printf("[+] action: chmod: new mode: 0o%o\n", inode->acl.mode);

    //
    // FIXME
    //
    {
        acl_t nacl;
        libflist_racl_to_acl(&nacl, acl);

        inode_acl_persist(database, &nacl);

        inode.aclkey = chars_to_text(nacl.key);
        set_Inode(&inode, dir->dir.contents, i);

        // commit into db
        unsigned char *buffer = malloc(8 * 512 * 1024); // FIXME
        write_Dir(&dir->dir, dir->dirp);

        if(capn_setp(capn_root(&dir->ctx), 0, dir->dirp.p))
            dies("capnp setp failed");

        int sz = capn_write_mem(&dir->ctx, buffer, 8 * 512 * 1024, 0); // FIXME

        // commit this object into the database
        debug("[+] removing old db key: %s\n", key);
        if(database->sdel(database, key))
            dies(libflist_strerror());

        debug("[+] writing into db: %s (%d bytes)\n", key, sz);
        if(database->sset(database, key, buffer, sz))
            dies(libflist_strerror());

        free(buffer);
        break;
    }

    database->close(database);
    free(dirpath);
#endif

    return 0;
}

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
    printf("[+] action: rm: files in the directory: %d\n", dirnode->inode_length);

    if(!libflist_directory_rm_inode(dirnode, inode)) {
        printf("[-] action: rm: something went wrong when removing the file\n");
        return 1;
    }

    printf("[+] action: rm: file removed\n");
    printf("[+] action: rm: files in the directory: %d\n", dirnode->inode_length);

    // FIXME: save

    database->close(database);
    free(dirpath);

    return 0;
}


