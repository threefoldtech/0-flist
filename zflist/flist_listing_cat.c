#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include "libflist.h"
#include "flist_walker.h"
#include "zflist.h"

typedef struct flist_cat_t {
    char *filename;
    char *output;
    int status;
    flist_backend_t *backend;

} flist_cat_t;

void *flist_cat_init(zflist_settings_t *settings) {
    flist_cat_t *cat;
    char *target = settings->targetfile;

    if(!(cat = malloc(sizeof(flist_cat_t))))
        diep("cat malloc");

    // skipping leading slash if any
    if(target[0] == '/')
        target += 1;

    cat->filename = strdup(target);
    cat->output = settings->outputfile;
    cat->status = 0;

    // FIXME: should not be here at all
    flist_db_t *backdb;

    if(!(backdb = libflist_db_redis_init_tcp(settings->backendhost, settings->backendport, "default", NULL))) {
        fprintf(stderr, "[-] cannot initialize backend\n");
        return NULL;
    }

    // initizlizing backend as requested
    if(!(cat->backend = libflist_backend_init(backdb, "/"))) {
        return NULL;
    }

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

        debug("[+] parsing: %s\n", fullpath);

        if(inode.attributes_which == Inode_attributes_file && strcmp(fullpath, cat->filename) == 0) {
            debug("[+] file found, downloading contents\n");

            FILE *fp = NULL;
            struct File file;
            read_File(&file, inode.attributes.file);

            FileBlock_ptr blockp;
            struct FileBlock block;

            if(cat->output) {
                if(!(fp = fopen(cat->output, "w")))
                    diep(cat->output);
            }

            for(int i = 0; i < capn_len(file.blocks); i++) {
                blockp.p = capn_getp(file.blocks.p, i, 1);
                read_FileBlock(&block, blockp);

                debug("[+] cat: data found, downloading block %d / %d\n", i + 1, capn_len(file.blocks));

                uint8_t *hash = libflist_bufdup(block.hash.p.data, block.hash.p.len);
                uint8_t *key = libflist_bufdup(block.key.p.data, block.key.p.len);

                // flist_chunk_t *chunk = libflist_chunk_new(hash, hashlen, key, keylen, NULL, 0);
                flist_chunk_t *chunk = libflist_chunk_new(hash, key, NULL, 0);

                if(!libflist_backend_download_chunk(cat->backend, chunk)) {
                    fprintf(stderr, "[-] could not download file: %s\n", libflist_strerror());
                    cat->status = 2;
                    return 1;
                }

                if(!cat->output) {
                    debug("[+] =======================================================\n");
                    printf("%.*s\n", (int) chunk->plain.length, chunk->plain.data);
                    debug("[+] =======================================================\n");
                }

                if(cat->output) {
                    if(fwrite(chunk->plain.data, chunk->plain.length, 1, fp) != 1) {
                        diep("fwrite");
                    }
                }

                // download_free(data);
                free(hash);
                free(key);

                cat->status = 1;
            }

            if(cat->output)
                fclose(fp);

            return 1;
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

void flist_cat_done(walker_t *walker) {
    flist_cat_t *cat = (flist_cat_t *) walker->userptr;

    if(cat->status == 1)
        debug("[+] file found !\n");

    free(cat->filename);
    free(cat);

    debug("[+] walking all done\n");
}
