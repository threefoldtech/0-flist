#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "flister.h"
#include "debug.h"
#include "flist.capnp.h"
#include "flist_read.h"
#include "flist_walker.h"

typedef struct tree_obj_t {
    int level;

} tree_obj_t;

void *flist_tree_init() {
    void *target;

    if(!(target = calloc(sizeof(tree_obj_t), 1)))
        diep("calloc");

    return target;
}

// generate a tree dump of all entries
int flist_tree(walker_t *walker, directory_t *root) {
    tree_obj_t *obj = (tree_obj_t *) walker->userptr;

    for(int i = 0; i < obj->level; i++)
        printf("    ");

    printf("| /%s\n", root->dir.location.str);

    Inode_ptr inodep;
    struct Inode inode;

    for(int i = 0; i < capn_len(root->dir.contents); i++) {
        inodep.p = capn_getp(root->dir.contents.p, i, 1);
        read_Inode(&inode, inodep);

        for(int i = 0; i < obj->level; i++)
            printf("    ");

        // writing inode information
        switch(inode.attributes_which) {
            case Inode_attributes_dir: {
                struct SubDir sub;
                read_SubDir(&sub, inode.attributes.dir);

                printf("+-- %s\n", inode.name.str);

                obj->level += 1;
                flist_walk_directory(walker, sub.key.str, inode.name.str);
                obj->level -= 1;

                break;
            }

            case Inode_attributes_link: {
                struct Link link;
                read_Link(&link, inode.attributes.link);

                printf("| %s -> %s\n", inode.name.str, link.target.str);
                break;
            }

            case Inode_attributes_special:
            case Inode_attributes_file:
                printf("| %s\n", inode.name.str);
                break;
        }
    }

    return 0;
}


