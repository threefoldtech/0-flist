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


int zf_chmod(int argc, char *argv[], zfe_settings_t *settings) {
    if(argc != 3) {
        fprintf(stderr, "[-] action: open: missing mode or filename\n");
        return 1;
    }

    debug("[+] action: chmod: setting mode %s on %s\n", argv[1], argv[2]);

    int newmode = strtol(argv[1], NULL, 8);
    char *dirpath = strdup(argv[2]);
    int isdir = 1;
    directory_t *dir;
    flist_db_t *database = libflist_db_sqlite_init(settings->mnt);
    database->open(database);

    char *key = libflist_path_key(dirpath);
    debug("[+] action: chmod: trying to open: %s\n", dirpath);

    if(!(dir = flist_directory_get(database, key, dirpath))) {
        debug("[-] action: chmod: could not open the directory, trying parent\n");

        dirpath = dirname(dirpath);
        key = libflist_path_key(dirpath);

        // target is maybe a file, not a directory
        isdir = 0;

        if(!(dir = flist_directory_get(database, key, dirpath))) {
            fprintf(stderr, "[-] action: chmod: no such file or directory\n");
            return 1;
        }
    }

    debug("[+] action: chmod: we found something (isdir: %d)\n", isdir);

    // FIXME
    if(isdir == 1) {
        fprintf(stderr, "not supported yet\n");
        return 1;
    }

    Inode_ptr inodep;
    struct Inode inode;

    // iterating over the whole directory
    // contents
    for(int i = 0; i < capn_len(dir->dir.contents); i++) {
        // reading the next entry
        inodep.p = capn_getp(dir->dir.contents.p, i, 1);
        read_Inode(&inode, inodep);

        char temp[2048];
        snprintf(temp, sizeof(temp), "%s/%s", dirpath, inode.name.str);

        if(strcmp(temp, argv[2]) != 0) {
            printf("skipping file %s\n", temp);
            continue;
        }

        printf("[+] action: chmod: we found the file\n");

        // FIXME: SUPPORT DIRECTORY
        flist_acl_t *acl = libflist_get_permissions(database, inode.aclkey.str);

        printf("[+] action: chmod: current mode: 0o%o\n", acl->mode);

        // remove 9 last bits and set new last 9 bits
        uint32_t cleared = acl->mode & 0xfffffe00;
        acl->mode = cleared | newmode;

        printf("[+] action: chmod: new mode: 0o%o\n", acl->mode);

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

    return 0;
}
