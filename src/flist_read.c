#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <rocksdb/c.h>
#include <blake2.h>
#include "flister.h"
#include "flist_read.h"
#include "flist.capnp.h"

#define KEYLENGTH 32

static int flist_walk_dir(database_t *database, const char *key, int level);

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

//
// FIXME: ugly, use bitwise
//
static char *permsingle(char value) {
    if(value == '0') return "---";
    if(value == '1') return "--x";
    if(value == '2') return "-w-";
    if(value == '3') return "-wx";
    if(value == '4') return "r--";
    if(value == '5') return "r-x";
    if(value == '6') return "rw-";
    if(value == '7') return "rwx";
    return "???";
}

//
// FIXME: ugly, use bitwise
//
static char *permstr(unsigned int mode, char *modestr, size_t slen) {
    char octstr[16];

    if(slen < 12)
        return NULL;


    int length = snprintf(octstr, sizeof(octstr), "%o", mode);
    if(length < 3 && length > 6) {
        strcpy(modestr, "?????????");
        return NULL;
    }

    strcpy(modestr, permsingle(octstr[length - 3]));
    strcpy(modestr + 3, permsingle(octstr[length - 2]));
    strcpy(modestr + 6, permsingle(octstr[length - 1]));

    return modestr;
}

//
// listings
//
static void flist_tree(database_t *database, Dir_ptr dirp, int level) {
    struct Dir dir;
    read_Dir(&dir, dirp);

    for(int i = 0; i < level; i++)
        printf("    ");

    printf("| /%s\n", dir.location.str);
    // printf("Parent: %s\n", dir.parent.str);
    // printf("Size: %lu\n", dir.size);

    Inode_ptr inodep;
    struct Inode inode;

    for(int i = 0; i < capn_len(dir.contents); i++) {
        inodep.p = capn_getp(dir.contents.p, i, 1);
        read_Inode(&inode, inodep);

        for(int i = 0; i < level; i++)
            printf("    ");

        struct SubDir sub;
        struct Link link;

        // writing inode information
        switch(inode.attributes_which) {
            case Inode_attributes_dir:
                read_SubDir(&sub, inode.attributes.dir);

                printf("+-- %s\n", inode.name.str);
                flist_walk_dir(database, sub.key.str, level + 1);
                break;

            case Inode_attributes_link:
                read_Link(&link, inode.attributes.link);

                printf("| %s -> %s\n", inode.name.str, link.target.str);
                break;

            case Inode_attributes_special:
            case Inode_attributes_file:
                printf("| %s\n", inode.name.str);
                break;
        }
    }
}

static void flist_ls(database_t *database, Dir_ptr dirp, int level) {
    struct Dir dir;
    read_Dir(&dir, dirp);

    printf("/%s:\n", dir.location.str);
    // printf("[+] directory: /%s\n", dir.location.str);
    // printf("Parent: %s\n", dir.parent.str);
    // printf("Size: %lu\n", dir.size);

    Inode_ptr inodep;
    struct Inode inode;

    // dumping contents
    for(int i = 0; i < capn_len(dir.contents); i++) {
        inodep.p = capn_getp(dir.contents.p, i, 1);
        read_Inode(&inode, inodep);

        value_t *perms;

        // why reading root dir for acl ??
        if(inode.attributes_which == Inode_attributes_dir) {
            // reading permissions
            perms = database_get(database, dir.aclkey.str);
            if(!perms->data) {
                dies("cannot get acl from database");
            }

        } else {
            // reading permissions
            perms = database_get(database, inode.aclkey.str);
            if(!perms->data) {
                dies("cannot get acl from database");
            }


        }

        ACI_ptr acip;
        struct ACI aci;

        struct capn permsctx;
        if(capn_init_mem(&permsctx, (unsigned char *) perms->data, perms->length, 0)) {
            // fprintf(stderr, "[-] capnp: init error\n");
        }

        acip.p = capn_getp(capn_root(&permsctx), 0, 1);
        read_ACI(&aci, acip);

        char modestr[16];
        permstr(aci.mode, modestr, sizeof(modestr));

        const char *uname = (aci.uname.len) ? aci.uname.str : "????";
        const char *gname = (aci.gname.len) ? aci.gname.str : "????";

        // writing inode information
        switch(inode.attributes_which) {
            case Inode_attributes_dir:
                printf("d%s  %-5s %-5s ", modestr, uname, gname);

                struct SubDir sub;
                read_SubDir(&sub, inode.attributes.dir);

                // printf("%8lu  %-12s (%s)\n", inode.size, inode.name.str, sub.key.str);
                printf("%8lu (  no blocks) %-12s\n", inode.size, inode.name.str);
                // flist_walk_dir(database, sub.key.str, level + 1);

                break;

            case Inode_attributes_file: ;
                struct File file;
                read_File(&file, inode.attributes.file);

                printf("-%s  %-5s %-5s ", modestr, uname, gname);
                printf("%8lu (%4lu blocks) %s\n", inode.size, capn_len(file.blocks), inode.name.str);
                break;

            case Inode_attributes_link:
                printf("lrwxrwxrwx  %-5s %-5s ", uname, gname);

                struct Link link;
                read_Link(&link, inode.attributes.link);

                printf("%8lu (  no blocks) %s -> %s\n", inode.size, inode.name.str, link.target.str);
                break;

            case Inode_attributes_special:
                printf("b%s  %-5s %-5s ", modestr, uname, gname);
                printf("%8lu %s\n", inode.size, inode.name.str);
                break;
        }
    }

    printf("\n");

    // walking over directories
    for(int i = 0; i < capn_len(dir.contents); i++) {
        inodep.p = capn_getp(dir.contents.p, i, 1);
        read_Inode(&inode, inodep);

        if(inode.attributes_which == Inode_attributes_dir) {
            struct SubDir sub;
            read_SubDir(&sub, inode.attributes.dir);

            flist_walk_dir(database, sub.key.str, level + 1);
        }
    }
}

static void flist_blocks(database_t *database, Dir_ptr dirp, int level) {
    struct Dir dir;
    read_Dir(&dir, dirp);

    Inode_ptr inodep;
    struct Inode inode;

    for(int i = 0; i < capn_len(dir.contents); i++) {
        inodep.p = capn_getp(dir.contents.p, i, 1);
        read_Inode(&inode, inodep);

        // print full path
        if(dir.location.len) {
            printf("/%s/%s\n", dir.location.str, inode.name.str);

        } else printf("/%s\n", inode.name.str);

        if(inode.attributes_which == Inode_attributes_file) {
            struct File file;
            read_File(&file, inode.attributes.file);

            FileBlock_ptr blockp;
            struct FileBlock block;

            for(int i = 0; i < capn_len(file.blocks); i++) {
                blockp.p = capn_getp(file.blocks.p, i, 1);
                read_FileBlock(&block, blockp);

                //
            }
        }

        // walking over internal directories
        if(inode.attributes_which == Inode_attributes_dir) {
            struct SubDir sub;
            read_SubDir(&sub, inode.attributes.dir);

            // recursive list contents
            flist_walk_dir(database, sub.key.str, level + 1);
        }
    }
}

static void flist_dump(database_t *database, Dir_ptr dirp, int level) {
    struct Dir dir;
    read_Dir(&dir, dirp);

    Inode_ptr inodep;
    struct Inode inode;

    for(int i = 0; i < capn_len(dir.contents); i++) {
        inodep.p = capn_getp(dir.contents.p, i, 1);
        read_Inode(&inode, inodep);

        // print full path
        if(dir.location.len) {
            printf("/%s/%s\n", dir.location.str, inode.name.str);

        } else printf("/%s\n", inode.name.str);

        // walking over internal directories
        if(inode.attributes_which == Inode_attributes_dir) {
            struct SubDir sub;
            read_SubDir(&sub, inode.attributes.dir);

            // recursive list contents
            flist_walk_dir(database, sub.key.str, level + 1);
        }
    }
}

//
// walker
//
static int flist_walk_dir(database_t *database, const char *key, int level) {
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
    dirp.p = capn_getp(capn_root(&ctx), 0, 1);

    //
    // printing 'ls' format the directory
    //
    flist_ls(database, dirp, level);
    // flist_blocks(database, dirp, level);

    //
    // printing 'tree' format the directory
    //
    // flist_tree(database, dirp, level);

    //
    // simple files dump
    //
    // flist_dump(database, dirp, level);

    // cleaning stuff
    database_value_free(value);
    capn_free(&ctx);

    return 0;
}

int flist_walk(database_t *database) {
    // root directory is an empty key
    const char *key = pathkey("");

    // walking starting from the root
    flist_walk_dir(database, key, 0);

    // cleaning
    free((char *) key);

    return 0;
}
