#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ftw.h>
#include <unistd.h>
#include <rocksdb/c.h>
#include <blake2.h>
#include <uthash.h>
#include <linux/limits.h>
#include "flister.h"
#include "flist_write.h"
#include "flist.capnp.h"

#define KEYLENGTH 32

typedef struct {
    const char *key;
    int size;
    int current;

    // Dir_ptr

    UT_hash_handle hh;

} dir_entry_t;

dir_entry_t *entryhash = NULL;

//
// WARNING: this code uses 'ftw', which is by design, not thread safe
//          all functions used here need to be concidered as non-thread-safe
//          be careful if you want to add threads on the layer
//

static const char *pathkey(char *path) {
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

static int flist_create_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    ssize_t length = ftwbuf->base - settings.rootlen - 1;
    char *relpath;

    if(typeflag == FTW_DP) {
        printf("DIRECTORY, ALL SUBS TREATED\n");
    }

    if(ftwbuf->level == 0) {
        printf("ROOT PATH, WE ARE DONE\n");
    }

    // checking if it's a directory deep or root directory
    if(length > 0 && ftwbuf->level > 1) {
         relpath = strndup(fpath + settings.rootlen, ftwbuf->base - settings.rootlen - 1);

    } else relpath = strdup("");

    char *itemname = fpath + ftwbuf->base;
    printf("Relative: <%s>, Item: <%s> -- %s (%lu)\n", relpath, itemname, fpath, sb->st_size);

    dir_entry_t *found = NULL;
    HASH_FIND_STR(entryhash, relpath, found);

    if(!found)
        dies("relative path not found on hashlist, directory is not stable");

    printf("-> Filled: %d/%d\n", found->current, found->size);

    free(relpath);

    return 0;
}

static int flist_prepare_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    char relpath[PATH_MAX];
    ssize_t length = ftwbuf->base - settings.rootlen - 1;

    // checking if it's a directory deep or root directory
    if(length > 0 && ftwbuf->level > 1) {
        // extracting the relative path
        strncpy(relpath, fpath + settings.rootlen, length);
        relpath[length] = '\0';

    } else sprintf(relpath, "");

    // checking if path is already on the hashlist or not
    dir_entry_t *found = NULL;
    HASH_FIND_STR(entryhash, relpath, found);

    if(!found) {
        // insert item into the hashlist
        found = malloc(sizeof(dir_entry_t));
        found->key = strdup(relpath);
        found->size = 1;
        found->current = 0;

        HASH_ADD_KEYPTR(hh, entryhash, found->key, strlen(found->key), found);

    } else found->size += 1;

    return 0;
}

static void flist_memory_clean() {
    dir_entry_t *entry, *tmp;

    HASH_ITER(hh, entryhash, entry, tmp) {
        HASH_DEL(entryhash, entry);
        free(entry->key);
        free(entry);
    }
}

int flist_create(database_t *database, char *root) {
    printf("[+] preparing flist for: %s\n", root);

    printf("[+] analyzing directory\n");
    if(nftw(root, flist_prepare_cb, 512, FTW_DEPTH | FTW_PHYS))
        diep("nftw");

    printf("[+] building database\n");

    if(nftw(root, flist_create_cb, 512, FTW_DEPTH | FTW_PHYS))
        diep("nftw");

    flist_memory_clean();

    return 0;
}
