#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <blake2.h>
#include "flister.h"
#include "flist.capnp.h"
#include "flist_read.h"

// reading a flist -- iterating -- walking
//
// flist is a rocksdb database
// - each directory is one key-value object
// - each acl are stored and dedupe on the db
//
// - one directory is identified by blake2 hash of it's full path
// - the root directory is the blake2 hash of empty string
//

#define KEYLENGTH 16

//
// directory object reader
//
directory_t *flist_directory_get(database_t *database, const char *key) {
    directory_t *dir;

    if(!(dir = malloc(sizeof(directory_t))))
        diep("directory: malloc");

    // reading capnp message from database
    dir->value = database_get(database, key);

    if(!dir->value->data) {
        fprintf(stderr, "[-] directory: key [%s] not found\n", key);
        return NULL;
    }

    // build capn context
    if(capn_init_mem(&dir->ctx, (unsigned char *) dir->value->data, dir->value->length, 0)) {
        fprintf(stderr, "[-] directory: capnp: init error\n");
        database_value_free(dir->value);
        return NULL;
    }

    // populate dir struct from context
    // the contents is always a directory (one key per directory)
    // and the whole contents is on the content field
    dir->dirp.p = capn_getp(capn_root(&dir->ctx), 0, 1);
    read_Dir(&dir->dir, dir->dirp);

    return dir;
}

void flist_directory_close(directory_t *dir) {
    database_value_free(dir->value);
    capn_free(&dir->ctx);
    free(dir);
}

//
// helpers
//
const char *flist_pathkey(char *path) {
    uint8_t hash[KEYLENGTH];
    char *hexhash;

    if(blake2b(hash, path, "", KEYLENGTH, strlen(path), 0) < 0) {
        fprintf(stderr, "[-] blake2 error\n");
        exit(EXIT_FAILURE);
    }

    if(!(hexhash = malloc(sizeof(char) * ((KEYLENGTH * 2) + 1))))
        diep("malloc");

    for(int i = 0; i < KEYLENGTH; i++)
        sprintf(hexhash + (i * 2), "%02x", hash[i]);

    return (const char *) hexhash;
}

//
// FIXME: ugly, use bitwise
//
static char *permsingle(char value) {
    if(value == '0') return "---";
    if(value == '1') return "--x";
    if(value == '2') return "-w-";
    if(value == '3') return "-wx";
    if(value == '4') return "r--";
    if(value == '5') return "r-x";
    if(value == '6') return "rw-";
    if(value == '7') return "rwx";
    return "???";
}

//
// FIXME: ugly, use bitwise
//
char *flist_read_permstr(unsigned int mode, char *modestr, size_t slen) {
    char octstr[16];

    if(slen < 12)
        return NULL;


    int length = snprintf(octstr, sizeof(octstr), "%o", mode);
    if(length < 3 && length > 6) {
        strcpy(modestr, "?????????");
        return NULL;
    }

    strcpy(modestr, permsingle(octstr[length - 3]));
    strcpy(modestr + 3, permsingle(octstr[length - 2]));
    strcpy(modestr + 6, permsingle(octstr[length - 1]));

    return modestr;
}

char *flist_fullpath(directory_t *root, struct Inode *inode) {
    char *path;

    if(strlen(root->dir.location.str)) {
        // item under a directory
        if(asprintf(&path, "/%s/%s", root->dir.location.str, inode->name.str) < 0)
            diep("asprintf");

    } else {
        // item on the root
        if(asprintf(&path, "/%s", inode->name.str) < 0)
            diep("asprintf");
    }

    return path;
}

