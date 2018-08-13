#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include "libflist.h"
#include "flist_walker.h"
#include "zflist.h"

typedef struct flist_check_t {
    flist_backend_t *backend;
    size_t files;
    size_t size;
    int status;

} flist_check_t;

void *flist_check_init(zflist_settings_t *settings) {
    flist_check_t *checker;

    if(!(checker = calloc(sizeof(flist_check_t), 1)))
        diep("checker malloc");

    // FIXME: should not be here at all
    flist_db_t *backdb;

    if(!(backdb = libflist_db_redis_init_tcp(settings->backendhost, settings->backendport, "default", NULL))) {
        fprintf(stderr, "[-] cannot initialize backend\n");
        return NULL;
    }

    // initizlizing backend as requested
    if(!(checker->backend = backend_init(backdb, "/"))) {
        return NULL;
    }

    return checker;
}

int flist_check(walker_t *walker, directory_t *root) {
    Inode_ptr inodep;
    struct Inode inode;
    flist_check_t *checker = (flist_check_t *) walker->userptr;

    for(int i = 0; i < capn_len(root->dir.contents); i++) {
        inodep.p = capn_getp(root->dir.contents.p, i, 1);
        read_Inode(&inode, inodep);

        // print full path
        if(root->dir.location.len) {
            debug("[+] checking: /%s/%s\n", root->dir.location.str, inode.name.str);

        } else debug("[+] checking: /%s\n", inode.name.str);

        if(inode.attributes_which == Inode_attributes_file) {
            struct File file;
            read_File(&file, inode.attributes.file);

            FileBlock_ptr blockp;
            struct FileBlock block;

            for(int i = 0; i < capn_len(file.blocks); i++) {
                blockp.p = capn_getp(file.blocks.p, i, 1);
                read_FileBlock(&block, blockp);

                uint8_t *hash = libflist_bufdup(block.hash.p.data, block.hash.p.len);
                size_t hashlen = block.hash.p.len;

                uint8_t *key = libflist_bufdup(block.key.p.data, block.key.p.len);
                size_t keylen = block.key.p.len;

                flist_backend_data_t *data;

                if(!(data = download_block(checker->backend, hash, hashlen, key, keylen))) {
                    checker->status = 1;
                    return 1;
                }



                checker->files += 1;
                checker->size += data->length;

                download_free(data);
                free(hash);
                free(key);
            }
        }

        // walking over internal directories
        if(inode.attributes_which == Inode_attributes_dir) {
            struct SubDir sub;
            read_SubDir(&sub, inode.attributes.dir);

            // recursive list contents
            flist_walk_directory(walker, sub.key.str, inode.name.str);
        }
    }

    return 0;
}

void flist_check_done(walker_t *walker) {
    flist_check_t *checker = (flist_check_t *) walker->userptr;

    if(checker->status) {
        debug("[-] integrity failed, missing chunk detected\n");
        return;
    }

    debug("[+] integrity check completed, no error found\n");

    debug("[+] files checked: %lu\n", checker->files);
    debug("[+] fullsize checked: %.2f MB\n", (checker->size / (1024.0 * 1024)));
}
