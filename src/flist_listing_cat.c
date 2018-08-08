#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include "flister.h"
#include "database.h"
#include "backend.h"
#include "flist.capnp.h"
#include "flist_read.h"
#include "flist_walker.h"

typedef struct flist_cat_t {
    char *filename;
    int status;

} flist_cat_t;

void *flist_cat_init() {
    flist_cat_t *cat;
    char *target = settings.targetfile;

    if(!(cat = malloc(sizeof(flist_cat_t))))
        diep("cat malloc");

    // skipping leading slash if any
    if(target[0] == '/')
        target += 1;

    cat->filename = strdup(target);
    cat->status = 0;

    debug("[+] cat: looking for: %s\n", cat->filename);

    return cat;
}

int flist_cat(walker_t *walker, directory_t *root) {
    Inode_ptr inodep;
    struct Inode inode;
    flist_cat_t *cat = (flist_cat_t *) walker->userptr;
    char *fullpath;

    for(int i = 0; i < capn_len(root->dir.contents); i++) {
        inodep.p = capn_getp(root->dir.contents.p, i, 1);
        read_Inode(&inode, inodep);

        // print full path
        if(root->dir.location.len) {
            if(asprintf(&fullpath, "%s/%s", root->dir.location.str, inode.name.str) < 0)
                diep("asprintf");

        } else {
            // maybe strdup
            if(asprintf(&fullpath, "%s", inode.name.str) < 0)
                diep("asprintf");
        }

        // debug("[+] parsing: %s\n", fullpath);

        if(inode.attributes_which == Inode_attributes_file && strcmp(fullpath, cat->filename) == 0) {
            struct File file;
            read_File(&file, inode.attributes.file);

            FileBlock_ptr blockp;
            struct FileBlock block;

            for(int i = 0; i < capn_len(file.blocks); i++) {
                blockp.p = capn_getp(file.blocks.p, i, 1);
                read_FileBlock(&block, blockp);

                uint8_t *hash = bufdup(block.hash.p.data, block.hash.p.len);
                size_t hashlen = block.hash.p.len;

                uint8_t *key = bufdup(block.key.p.data, block.key.p.len);
                size_t keylen = block.key.p.len;

                backend_data_t *data;

                if(!(data = download_block(hash, hashlen, key, keylen))) {
                    fprintf(stderr, "[-] could not download file\n");
                    cat->status = 1;
                    return 1;
                }

                debug("[+] cat: data found\n");
                debug("[+] =======================================================\n");
                printf("%.*s\n", (int) data->length, data->payload);
                debug("[+] =======================================================\n");

                download_free(data);
                free(hash);
                free(key);

                // we are done
                return 1;
            }
        }

        // walking over internal directories
        if(inode.attributes_which == Inode_attributes_dir) {
            struct SubDir sub;
            read_SubDir(&sub, inode.attributes.dir);

            // recursive list contents
            flist_walk_directory(walker, sub.key.str);
        }
    }

    return 0;
}

void flist_cat_done(walker_t *walker) {
    flist_cat_t *cat = (flist_cat_t *) walker->userptr;

    free(cat->filename);
    free(cat);

    debug("[+] walking all done\n");
}
