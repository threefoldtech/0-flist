#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <rocksdb/c.h>
#include <blake2.h>
#include "flister.h"
#include "flist.h"
#include "flist.capnp.h"

#define KEYLENGTH 32

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

static int flist_walk_dir(database_t *database, const char *key) {
    // reading capnp message from database
    value_t *value = database_get(database, key);

    if(!value->data) {
        fprintf(stderr, "[-] walk: key [%s] not found\n", key);
        exit(EXIT_FAILURE);
    }

    //
    // parsing capnp message and iterating over the contents
    //
    struct capn ctx;
    if(capn_init_mem(&ctx, (unsigned char *) value->data, value->length, 0))
        fprintf(stderr, "[-] capnp: init error\n");

    Dir_ptr dirp;
    struct Dir dir;
    dirp.p = capn_getp(capn_root(&ctx), 0, 1);
    read_Dir(&dir, dirp);

    printf("[+] directory: /%s\n", dir.location.str);
    // printf("Parent: %s\n", dir.parent.str);
    // printf("Size: %lu\n", dir.size);

    Inode_ptr inodep;
    struct Inode inode;

    for(int i = 0; i < capn_len(dir.contents); i++) {
        inodep.p = capn_getp(dir.contents.p, i, 1);

        read_Inode(&inode, inodep);

        switch(inode.attributes_which) {
            case Inode_attributes_dir:
                printf("d---------  nobody nobody ");

                struct SubDir sub;
                read_SubDir(&sub, inode.attributes.dir);

                printf("%8lu  %-12s (%s)\n", inode.size, inode.name.str, sub.key.str);
                flist_walk_dir(database, sub.key.str);

                break;

            case Inode_attributes_file:
                printf("----------  nobody nobody ");
                printf("%8lu  %s\n", inode.size, inode.name.str);
                break;

            case Inode_attributes_link:
                printf("lrwxrwxrwx  nobody nobody ");

                struct Link link;
                read_Link(&link, inode.attributes.link);

                printf("%8lu  %s -> %s\n", inode.size, inode.name.str, link.target.str);
                break;

            case Inode_attributes_special:
                printf("b---------  nobody nobody ");
                printf("%8lu  %s\n", inode.size, inode.name.str);
                break;
        }
    }

    // cleaning stuff
    database_value_free(value);
    capn_free(&ctx);

    return 0;
}

int flist_walk(database_t *database) {
    // root directory is an empty key
    const char *key = pathkey("");

    // walking starting from the root
    flist_walk_dir(database, key);

    // cleaning
    free((char *) key);

    return 0;
}
