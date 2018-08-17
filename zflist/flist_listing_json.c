#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <jansson.h>
#include "zflist.h"
#include "libflist.h"
#include "flist_listing.h"
#include "flist_walker.h"

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


void *flist_json_init() {
    flist_json_t *obj;

    // create initial json root object
    if(!(obj = (flist_json_t *) calloc(sizeof(flist_json_t), 1)))
        diep("malloc");

    obj->root = json_object();
    obj->content = json_array();

    return obj;
}

void flist_json_dump(walker_t *walker) {
    flist_json_t *json = (flist_json_t *) walker->userptr;

    json_object_set_new(json->root, "regular", json_integer(json->regular));
    json_object_set_new(json->root, "symlink", json_integer(json->symlink));
    json_object_set_new(json->root, "directory", json_integer(json->directory));
    json_object_set_new(json->root, "special", json_integer(json->special));
    json_object_set_new(json->root, "content", json->content);

    // char *output = json_dumps(json->root, JSON_INDENT(2));
    char *output = json_dumps(json->root, 0);
    json_decref(json->root);

    puts(output);
    free(output);
    free(json);
}

int flist_json(walker_t *walker, directory_t *root) {
    flist_json_t *json = (flist_json_t *) walker->userptr;

    Inode_ptr inodep;
    struct Inode inode;

    for(int i = 0; i < capn_len(root->dir.contents); i++) {
        inodep.p = capn_getp(root->dir.contents.p, i, 1);
        read_Inode(&inode, inodep);

        char *path = flist_fullpath(root, &inode);

        json_t *this = json_object();
        json_object_set_new(this, "path", json_string(path));
        json_object_set_new(this, "size", json_integer(0));

        if(inode.attributes_which == Inode_attributes_dir) {
            // recursive walking
            struct SubDir sub;
            read_SubDir(&sub, inode.attributes.dir);
            flist_walk_directory(walker, sub.key.str, inode.name.str);

            json->directory += 1;
        }

        if(inode.attributes_which == Inode_attributes_file) {
            json->regular += 1;
            json_object_set_new(this, "size", json_integer(inode.size));
        }

        if(inode.attributes_which == Inode_attributes_link) {
            struct Link link;
            read_Link(&link, inode.attributes.link);

            json->symlink += 1;
            json_object_set_new(this, "target", json_string(link.target.str));
        }

        if(inode.attributes_which == Inode_attributes_special) {
            json->special += 1;
        }

        json_array_append_new(json->content, this);
        free(path);
    }

    return 0;
}
