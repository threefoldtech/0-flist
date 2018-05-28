#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "flister.h"
#include "flist.capnp.h"
#include "flist_read.h"
#include "flist_walker.h"

void flist_blocks(walker_t *walker, directory_t *root) {
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

                printf("  hash: %.*s\n", block.hash.p.len, block.hash.p.data);
                printf("  key : %.*s\n", block.key.p.len, block.key.p.data);
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
}

// dumps (in a 'find' way) file contents
// simple print one line per entries, with full path
void flist_dump(walker_t *walker, directory_t *root) {
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
            flist_walk_directory(walker, sub.key.str);
        }
    }
}



