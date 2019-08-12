#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>
#include "libflist.h"
#include "verbose.h"
#include "database.h"
#include "flist.capnp.h"
#include "flist_acl.h"
#include "flist_serial.h"
#include "flist_tools.h"

#define discard __attribute__((cleanup(__cleanup_free)))

static void __cleanup_free(void *p) {
    free(* (void **) p);
}

//
// dirnode
//
dirnode_t *flist_dirnode_create(char *fullpath, char *name) {
    dirnode_t *directory;

    if(!(directory = calloc(sizeof(dirnode_t), 1)))
        return NULL;

    directory->fullpath = strdup(fullpath);
    directory->name = strdup(name);

    // some cleaning (remove trailing slash)
    // but ignoring empty fullpath
    size_t lf = strlen(directory->fullpath);
    if(lf && directory->fullpath[lf - 1] == '/')
        directory->fullpath[lf - 1] = '\0';

    directory->hashkey = flist_path_key(directory->fullpath);
    directory->acl = flist_acl_new("root", "root", 0755);

    return directory;
}

dirnode_t *flist_dirnode_create_from_stat(dirnode_t *parent, const char *name, const struct stat *sb) {
    char *fullpath;
    dirnode_t *root;

    if(strlen(parent->fullpath) == 0) {
        if(!(fullpath = strdup(name)))
            diep("strdup");

    } else {
        if(asprintf(&fullpath, "%s/%s", parent->fullpath, name) < 0)
            diep("asprintf");
    }

    if(!(root = flist_dirnode_create(fullpath, (char *) name)))
        return NULL;

    root->creation = sb->st_ctime;
    root->modification = sb->st_mtime;

    if(root->acl)
        flist_acl_free(root->acl);

    root->acl = flist_acl_from_stat(sb);

    free(fullpath);

    return root;
}

void flist_dirnode_free(dirnode_t *dirnode) {
    flist_acl_free(dirnode->acl);

    for(inode_t *inode = dirnode->inode_list; inode; ) {
        inode_t *next = inode->next;
        libflist_inode_free(inode);
        inode = next;
    }

    free(dirnode->fullpath);
    free(dirnode->name);
    free(dirnode->hashkey);
    free(dirnode);
}

void flist_dirnode_free_recursive(dirnode_t *dirnode) {
    for(dirnode_t *subdir = dirnode->dir_list; subdir; ) {
        dirnode_t *temp = subdir->next;
        flist_dirnode_free_recursive(subdir);
        subdir = temp;
    }

    flist_dirnode_free(dirnode);
}

dirnode_t *flist_dirnode_lazy_appends_inode(dirnode_t *root, inode_t *inode) {
    if(!root->inode_list)
        root->inode_list = inode;

    if(root->inode_last)
        root->inode_last->next = inode;

    root->inode_last = inode;
    root->inode_length += 1;

    return root;
}

dirnode_t *flist_dirnode_appends_inode(dirnode_t *root, inode_t *inode) {
    inode->next = NULL;
    return flist_dirnode_lazy_appends_inode(root, inode);
}

dirnode_t *flist_dirnode_lazy_appends_dirnode(dirnode_t *root, dirnode_t *dir) {
    if(!root->dir_list)
        root->dir_list = dir;

    if(root->dir_last)
        root->dir_last->next = dir;

    root->dir_last = dir;
    root->dir_length += 1;

    return root;
}

/*
dirnode_t dirnode_lazy_appends_new_dirnode(dirnode_t *root, dirnode_t *dir) {

    return dirnode_lazy_appends_dirnode(root, dir);
}
*/

dirnode_t *flist_dirnode_appends_dirnode(dirnode_t *root, dirnode_t *dir) {
    dir->next = NULL;
    return flist_dirnode_lazy_appends_dirnode(root, dir);
}

dirnode_t *flist_dirnode_search(dirnode_t *root, char *dirname) {
    // directory empty (no list already set)
    if(!root->dir_list)
        return NULL;

    // iterating over directories
    for(dirnode_t *source = root->dir_list; source; source = source->next) {
        if(strcmp(source->name, dirname) == 0)
            return source;
    }

    // directory not found
    return NULL;
}

dirnode_t *flist_dirnode_duplicate(dirnode_t *source) {
    dirnode_t *dirnode;

    if(!(dirnode = calloc(sizeof(dirnode_t), 1)))
        return libflist_diep("malloc");

    dirnode->fullpath = strdup(source->fullpath);
    dirnode->name = strdup(source->name);
    dirnode->hashkey = strdup(source->hashkey);
    dirnode->creation = source->creation;
    dirnode->modification = source->modification;
    dirnode->acl = flist_acl_duplicate(source->acl);

    return dirnode;
}

int flist_dirnode_is_root(dirnode_t *dirnode) {
    return (strlen(dirnode->fullpath) == 0);
}

char *flist_dirnode_virtual_path(dirnode_t *parent, char *target) {
    char *vpath;

    if(flist_dirnode_is_root(parent)) {
        if(asprintf(&vpath, "%s", target) < 0)
            return NULL;

        return vpath;
    }

    if(asprintf(&vpath, "/%s%s", parent->fullpath, target) < 0)
        return NULL;

    return vpath;
}

dirnode_t *flist_dirnode_from_inode(inode_t *inode) {
    dirnode_t *dirnode;

    if(!(dirnode = calloc(sizeof(dirnode_t), 1)))
        return NULL;

    // setting directory metadata
    dirnode->fullpath = strdup(inode->fullpath);
    dirnode->name = strdup(inode->name);
    dirnode->hashkey = flist_path_key(inode->fullpath);
    dirnode->creation = inode->creation;
    dirnode->modification = inode->modification;
    dirnode->acl = inode->acl;

    return dirnode;
}

//
// fetch a directory object from the database
//
dirnode_t *flist_dirnode_get(flist_db_t *database, char *path) {
    discard char *cleanpath = NULL;

    // we use strict convention to store
    // entry on the database since the id is a hash of
    // the path, the path needs to match exactly
    //
    // all the keys are without leading slash and never have
    // trailing slash, the root directory is an empty string
    //
    //   eg:  /        -> ""
    //        /home    -> "home"
    //        /var/log -> "var/log"
    //
    // to make this function working for mostly everybody, let's
    // clean the path to ensure it should reflect exactly how
    // it's stored on the database
    if(!(cleanpath = flist_clean_path(path)))
        return NULL;

    debug("[+] libflist: dirnode: get: clean path: <%s> -> <%s>\n", path, cleanpath);

    // converting this directory string into a directory
    // hash by the internal way used everywhere, this will
    // give the key required to find entry on the database
    discard char *key = flist_path_key(cleanpath);
    debug("[+] libflist: dirnode: get: entry key: <%s>\n", key);

    // requesting the directory object from the database
    // the object in the database is packed, this function will
    // return us something decoded and ready to use
    dirnode_t *direntry;
    if(!(direntry = flist_serial_get_dirnode(database, key, cleanpath)))
        return NULL;

    // cleaning temporary string allocated
    // by internal functions and not needed objects anymore
    // flist_directory_close(database, direntry);

    return direntry;
}

dirnode_t *flist_dirnode_get_recursive(flist_db_t *database, char *path) {
    dirnode_t *root = NULL;

    // fetching root directory
    if(!(root = flist_dirnode_get(database, path)))
        return NULL;

    for(inode_t *inode = root->inode_list; inode; inode = inode->next) {
        // ignoring non-directories
        if(inode->type != INODE_DIRECTORY)
            continue;

        // if it's a directory, loading it's contents
        // and adding it to the directory lists
        dirnode_t *subdir;
        if(!(subdir = flist_dirnode_get_recursive(database, inode->fullpath)))
            return NULL;

        flist_dirnode_appends_dirnode(root, subdir);
    }

    return root;
}

dirnode_t *flist_dirnode_get_parent(flist_db_t *database, dirnode_t *root) {
    discard char *copypath = strdup(root->fullpath);
    char *parent = dirname(copypath);

    // no parent
    if(strcmp(parent, root->fullpath) == 0)
        return root;

    return flist_dirnode_get(database, copypath);
}

dirnode_t *flist_dirnode_lookup_dirnode(dirnode_t *root, const char *dirname) {
    for(dirnode_t *dir = root->dir_list; dir; dir = dir->next)
        if(strcmp(dir->name, dirname) == 0)
            return dir;

    return NULL;
}

//
// public interface
//
dirnode_t *libflist_dirnode_create(char *fullpath, char *name) {
    return flist_dirnode_create(fullpath, name);
}

void libflist_dirnode_free(dirnode_t *dirnode) {
    flist_dirnode_free(dirnode);
}

void libflist_dirnode_free_recursive(dirnode_t *dirnode) {
    flist_dirnode_free_recursive(dirnode);
}

dirnode_t *libflist_dirnode_search(dirnode_t *root, char *dirname) {
    return flist_dirnode_search(root, dirname);
}

dirnode_t *libflist_dirnode_get(flist_db_t *database, char *path) {
    return flist_dirnode_get(database, path);
}

dirnode_t *libflist_dirnode_get_recursive(flist_db_t *database, char *path) {
    return flist_dirnode_get_recursive(database, path);
}

dirnode_t *libflist_dirnode_appends_inode(dirnode_t *root, inode_t *inode) {
    return flist_dirnode_appends_inode(root, inode);
}

dirnode_t *libflist_dirnode_get_parent(flist_db_t *database, dirnode_t *root) {
    return flist_dirnode_get_parent(database, root);
}

dirnode_t *libflist_dirnode_lookup_dirnode(dirnode_t *root, const char *dirname) {
    return flist_dirnode_lookup_dirnode(root, dirname);
}
