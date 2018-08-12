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

// dumps contents using kind of 'ls -al' view
// generate a list with type, permissions, size, blocks, name
int flist_ls(walker_t *walker, directory_t *root) {
    printf("/%s:\n", root->dir.location.str);
    // printf("[+] directory: /%s\n", dir.location.str);
    // printf("Parent: %s\n", dir.parent.str);
    // printf("Size: %lu\n", dir.size);

    Inode_ptr inodep;
    struct Inode inode;

    // iterating over the whole directory
    // contents
    for(int i = 0; i < capn_len(root->dir.contents); i++) {
        // reading the next entry
        inodep.p = capn_getp(root->dir.contents.p, i, 1);
        read_Inode(&inode, inodep);

        value_t *perms;

        // checking if this entry is another directory
        // (a subdirectory)
        if(inode.attributes_which == Inode_attributes_dir) {
            struct SubDir sub;
            read_SubDir(&sub, inode.attributes.dir);

            // if this is a subdirectory
            // we need to load that directory in order to
            // find it's acl (directory acl are defined on
            // the object itself)
            directory_t *subdir;
            char *keystr = (char *) sub.key.str;

            if(!(subdir = flist_directory_get(walker->database, keystr, inode.name.str)))
                return 0;

            // reading directory permissions
            keystr = (char *) subdir->dir.aclkey.str;
            perms = walker->database->sget(walker->database, keystr);
            if(!perms->data)
                dies("directory entry: cannot get acl from database");

            flist_directory_close(walker->database, subdir);

        } else {
            char *keystr = (char *) inode.aclkey.str;

            // reading file permissions
            perms = walker->database->sget(walker->database, keystr);
            if(!perms->data) {
                printf("%d, %s\n", inode.aclkey.len, inode.aclkey.str);
                dies("inode entry: cannot get acl from database");
            }
        }

        ACI_ptr acip;
        struct ACI aci;

        struct capn permsctx;
        if(capn_init_mem(&permsctx, (unsigned char *) perms->data, perms->length, 0)) {
            fprintf(stderr, "[-] capnp: init error\n");
        }

        acip.p = capn_getp(capn_root(&permsctx), 0, 1);
        read_ACI(&aci, acip);

        char modestr[16];
        flist_read_permstr(aci.mode, modestr, sizeof(modestr));

        const char *uname = (aci.uname.len) ? aci.uname.str : "????";
        const char *gname = (aci.gname.len) ? aci.gname.str : "????";

        // writing inode information
        switch(inode.attributes_which) {
            case Inode_attributes_dir:
                printf("d%s  %-8s %-8s ", modestr, uname, gname);

                struct SubDir sub;
                read_SubDir(&sub, inode.attributes.dir);

                printf("%8lu (       --- ) %-12s\n", inode.size, inode.name.str);
                break;

            case Inode_attributes_file: ;
                struct File file;
                read_File(&file, inode.attributes.file);

                printf("-%s  %-8s %-8s ", modestr, uname, gname);
                printf("%8lu (%4d blocks) %s\n", inode.size, capn_len(file.blocks), inode.name.str);
                break;

            case Inode_attributes_link:
                printf("lrwxrwxrwx  %-8s %-8s ", uname, gname);

                struct Link link;
                read_Link(&link, inode.attributes.link);

                printf("%8lu (       --- ) %s -> %s\n", inode.size, inode.name.str, link.target.str);
                break;

            case Inode_attributes_special:
                printf("b%s  %-8s %-8s ", modestr, uname, gname);
                printf("%8lu %s\n", inode.size, inode.name.str);
                break;
        }
    }

    // newline between directories
    printf("\n");

    // now, each entries (files) are displayed
    // walking over sub-directories
    for(int i = 0; i < capn_len(root->dir.contents); i++) {
        // reading again this entry
        inodep.p = capn_getp(root->dir.contents.p, i, 1);
        read_Inode(&inode, inodep);

        // if this is a directory, walking inside
        if(inode.attributes_which == Inode_attributes_dir) {
            struct SubDir sub;
            read_SubDir(&sub, inode.attributes.dir);

            // we don't need userptr
            flist_walk_directory(walker, sub.key.str, inode.name.str);
        }
    }

    return 0;
}


