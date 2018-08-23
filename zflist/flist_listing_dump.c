#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "libflist.h"
#include "flist_walker.h"
#include "zflist.h"

int flist_blocks(walker_t *walker, directory_t *root) {
    Inode_ptr inodep;
    struct Inode inode;

    for(int i = 0; i < capn_len(root->dir.contents); i++) {
        inodep.p = capn_getp(root->dir.contents.p, i, 1);
        read_Inode(&inode, inodep);

        // print full path
        if(root->dir.location.len) {
            printf("/%s/%s\n", root->dir.location.str, inode.name.str);

        } else printf("/%s\n", inode.name.str);

        if(inode.attributes_which == Inode_attributes_file) {
            struct File file;
            read_File(&file, inode.attributes.file);

            FileBlock_ptr blockp;
            struct FileBlock block;

            for(int i = 0; i < capn_len(file.blocks); i++) {
                blockp.p = capn_getp(file.blocks.p, i, 1);
                read_FileBlock(&block, blockp);

                char *hashstr = libflist_hashhex(block.hash.p.data, block.hash.p.len);
                char *keystr = libflist_hashhex(block.key.p.data, block.key.p.len);

                printf("  hash: %s\n", hashstr);
                printf("  key : %s\n", keystr);

                free(hashstr);
                free(keystr);
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

// dumps (in a 'find' way) file contents
// simple print one line per entries, with full path
int flist_dump(walker_t *walker, directory_t *root) {
    Inode_ptr inodep;
    struct Inode inode;

    for(int i = 0; i < capn_len(root->dir.contents); i++) {
        inodep.p = capn_getp(root->dir.contents.p, i, 1);
        read_Inode(&inode, inodep);

        // print full path
        if(root->dir.location.len) {
            printf("/%s/%s\n", root->dir.location.str, inode.name.str);

        } else printf("/%s\n", inode.name.str);

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



