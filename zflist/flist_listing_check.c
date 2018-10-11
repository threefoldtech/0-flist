#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <jansson.h>
#include "libflist.h"
#include "flist_walker.h"
#include "zflist.h"

extern zflist_settings_t settings;

typedef struct flist_check_t {
    flist_backend_t *backend;
    flist_stats_t stats;

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
    if(!(checker->backend = libflist_backend_init(backdb, "/"))) {
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

                debug("[+] check: downloading block %d / %d\n", i + 1, capn_len(file.blocks));

                uint8_t *hash = libflist_bufdup(block.hash.p.data, block.hash.p.len);
                uint8_t *key = libflist_bufdup(block.key.p.data, block.key.p.len);

                flist_chunk_t *chunk = libflist_chunk_new(hash, key, NULL, 0);

                // integrity check is done inside the download function
                if(!libflist_backend_download_chunk(checker->backend, chunk)) {
                    debug("[-] could not download file: %s\n", libflist_strerror());
                    checker->stats.failure += 1;
                }

                checker->stats.regular += 1;
                checker->stats.size += chunk->plain.length;

                // this call free the contents of
                // hash and key duplicated
                libflist_chunk_free(chunk);
            }

        }

        if(inode.attributes_which == Inode_attributes_link)
            checker->stats.symlink += 1;

        if(inode.attributes_which == Inode_attributes_special)
            checker->stats.special += 1;

        // walking over internal directories
        if(inode.attributes_which == Inode_attributes_dir) {
            checker->stats.directory += 1;

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
    flist_stats_t *stats = &checker->stats;

    if(settings.json) {
        json_t *root = json_object();

        json_object_set_new(root, "regular", json_integer(stats->regular));
        json_object_set_new(root, "symlink", json_integer(stats->symlink));
        json_object_set_new(root, "directory", json_integer(stats->directory));
        json_object_set_new(root, "special", json_integer(stats->special));
        json_object_set_new(root, "failure", json_integer(stats->failure));


        char *output = json_dumps(root, 0);
        json_decref(root);

        puts(output);
        free(output);

    } else {
        printf("[+]\n");
        printf("[+] flist integrity check summary:\n");
        printf("[+]   flist: regular  : %lu\n", stats->regular);
        printf("[+]   flist: symlink  : %lu\n", stats->symlink);
        printf("[+]   flist: directory: %lu\n", stats->directory);
        printf("[+]   flist: special  : %lu\n", stats->special);
        printf("[+]   flist: failure  : %lu\n", stats->failure);
        printf("[+]   flist: size read: %.2f MB\n", (stats->size / (1024 * 1024.0)));
        printf("[+]\n");
    }

    libflist_backend_free(checker->backend);
    free(checker);
}
