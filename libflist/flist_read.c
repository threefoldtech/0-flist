#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <blake2.h>
#include "libflist.h"
#include "verbose.h"

#define KEYLENGTH 16

//
// directory object reader
//
directory_t *flist_directory_get(flist_db_t *database, char *key, char *fullpath) {
    directory_t *dir;

    if(!(dir = malloc(sizeof(directory_t))))
        diep("directory: malloc");

    // reading capnp message from database
    dir->value = database->sget(database, key);

    if(!dir->value->data) {
        debug("[-] directory: key [%s, %s] not found\n", key, fullpath);
        return NULL;
    }

    // build capn context
    if(capn_init_mem(&dir->ctx, (unsigned char *) dir->value->data, dir->value->length, 0)) {
        debug("[-] directory: capnp: init error\n");
        database->clean(dir->value);
        return NULL;
    }

    // populate dir struct from context
    // the contents is always a directory (one key per directory)
    // and the whole contents is on the content field
    dir->dirp.p = capn_getp(capn_root(&dir->ctx), 0, 1);
    read_Dir(&dir->dir, dir->dirp);

    return dir;
}

void flist_directory_close(flist_db_t *database, directory_t *dir) {
    database->clean(dir->value);
    capn_free(&dir->ctx);
    free(dir);
}

static char *flist_inode_fullpath(directory_t *directory, struct Inode *inode) {
    char *fullpath;

    if(asprintf(&fullpath, "%s/%s", directory->dir.location.str, inode->name.str) < 0)
        return NULL;

    return fullpath;
}

//
// helpers
//
char *libflist_path_key(char *path) {
    uint8_t hash[KEYLENGTH];

    if(blake2b(hash, path, "", KEYLENGTH, strlen(path), 0) < 0) {
        debug("[-] blake2 error\n");
        return NULL;
    }

    return libflist_hashhex(hash, KEYLENGTH);
}

static char *flist_clean_path(char *path) {
    size_t offset, length;

    offset = 0;
    length = strlen(path);

    // remove lead slash
    if(path[0] == '/')
        offset = 1;

    // remove trailing slash
    if(path[length - 1] == '/')
        length -= 1;

    return strndup(path + offset, length);
}

static inode_chunks_t *capnp_inode_to_chunks(struct Inode *inode) {
    inode_chunks_t *blocks;

    struct File file;
    read_File(&file, inode->attributes.file);

    // allocate empty blocks
    if(!(blocks = calloc(sizeof(inode_chunks_t), 1)))
        return NULL;

    blocks->size = capn_len(file.blocks);
    blocks->blocksize = 0;

    if(!(blocks->list = (inode_chunk_t *) malloc(sizeof(inode_chunk_t) * blocks->size)))
        return NULL;

    for(size_t i = 0; i < blocks->size; i++) {
        FileBlock_ptr blockp;
        struct FileBlock block;

        blockp.p = capn_getp(file.blocks.p, i, 1);
        read_FileBlock(&block, blockp);

        blocks->list[i].entryid = libflist_bufdup(block.hash.p.data, block.hash.p.len);
        blocks->list[i].entrylen = block.hash.p.len;

        blocks->list[i].decipher = libflist_bufdup(block.key.p.data, block.key.p.len);
        blocks->list[i].decipherlen = block.key.p.len;
    }

    return blocks;
}

void inode_chunks_free(inode_t *inode) {
    if(!inode->chunks)
        return;

    for(size_t i = 0; i < inode->chunks->size; i += 1) {
        free(inode->chunks->list[i].entryid);
        free(inode->chunks->list[i].decipher);
    }

    free(inode->chunks->list);
    free(inode->chunks);
}

//
// public helpers
//
flist_acl_t *libflist_get_permissions(flist_db_t *database, const char *aclkey) {
    flist_acl_t *acl;

    if(strlen(aclkey) == 0) {
        debug("[-] get_permissions: empty acl key, cannot load it\n");
        return NULL;
    }

    value_t *rawdata = database->sget(database, (char *) aclkey);
    if(!rawdata->data) {
        debug("[-] get_permissions: acl key <%s> not found\n", aclkey);
        return NULL;
    }

    if(!(acl = malloc(sizeof(flist_acl_t))))
        return NULL;

    // load acl database object
    // into an acl readable entry
    ACI_ptr acip;
    struct ACI aci;

    struct capn permsctx;
    if(capn_init_mem(&permsctx, (unsigned char *) rawdata->data, rawdata->length, 0)) {
        debug("[-] capnp: init error\n");
        return NULL;
    }

    acip.p = capn_getp(capn_root(&permsctx), 0, 1);
    read_ACI(&aci, acip);

    // now we can populate our public acl object
    // from the database entry
    acl->uname = strdup(aci.uname.str);
    acl->gname = strdup(aci.gname.str);
    acl->mode = aci.mode;

    return acl;
}

int flist_fileindex_from_name(directory_t *direntry, char *filename) {
    Inode_ptr inodep;
    struct Inode inode;

    for(int i = 0; i < capn_len(direntry->dir.contents); i++) {
        inodep.p = capn_getp(direntry->dir.contents.p, i, 1);
        read_Inode(&inode, inodep);

        if(strcmp(inode.name.str, filename) == 0)
            return i;
    }

    return -1;
}

// populate an acl object from a racl pointer
flist_acl_t *libflist_racl_to_acl(acl_t *dst, flist_acl_t *src) {
    dst->uname = src->uname;
    dst->gname = src->gname;
    dst->mode = src->mode;
    dst->key = libflist_inode_acl_key(dst);

    return src;
}

inode_t *flist_itementry_to_inode(flist_db_t *database, directory_t *direntry, int fileindex) {
    inode_t *target;
    Inode_ptr inodep;
    struct Inode inode;

    // pointing to the right item
    // on the contents list
    inodep.p = capn_getp(direntry->dir.contents.p, fileindex, 1);
    read_Inode(&inode, inodep);

    // allocate a new inode empty object
    if(!(target = calloc(sizeof(inode_t), 1)))
        return NULL;

    // fill in default information
    target->name = strdup(inode.name.str);
    target->size = inode.size;
    target->fullpath = flist_inode_fullpath(direntry, &inode);
    target->creation = inode.creationTime;
    target->modification = inode.modificationTime;

    if(!(target->racl = libflist_get_permissions(database, inode.aclkey.str))) {
        inode_free(target);
        return NULL;
    }

    if(!(libflist_racl_to_acl(&target->acl, target->racl))) {
        inode_free(target);
        return NULL;
    }

    // fill in specific information dependent of
    // the type of the entry
    switch(inode.attributes_which) {
        case Inode_attributes_dir: ;
            struct SubDir sub;
            read_SubDir(&sub, inode.attributes.dir);

            target->type = INODE_DIRECTORY;
            target->subdirkey = strdup(sub.key.str);
            break;

        case Inode_attributes_file: ;
            target->type = INODE_FILE;
            target->chunks = capnp_inode_to_chunks(&inode);
            break;

        case Inode_attributes_link: ;
            struct Link link;
            read_Link(&link, inode.attributes.link);

            target->type = INODE_LINK;
            target->link = strdup(link.target.str);
            break;

        case Inode_attributes_special: ;
            struct Special special;
            read_Special(&special, inode.attributes.special);

            target->type = INODE_SPECIAL;
            target->stype = special.type;

            capn_data capdata = capn_get_data(special.data.p, 0);
            target->sdata = strndup(capdata.p.data, capdata.p.len);
            break;
    }

    return target;
}

dirnode_t *flist_directory_to_dirnode(flist_db_t *database, directory_t *direntry) {
    dirnode_t *dirnode;

    if(!(dirnode = calloc(sizeof(dirnode_t), 1)))
        return NULL;

    // setting directory metadata
    dirnode->fullpath = strdup(direntry->dir.location.str);
    dirnode->name = strdup(direntry->dir.name.str);
    dirnode->hashkey = libflist_path_key(dirnode->fullpath);
    dirnode->creation = direntry->dir.creationTime;
    dirnode->modification = direntry->dir.modificationTime;
    dirnode->racl = libflist_get_permissions(database, direntry->dir.aclkey.str);

    libflist_racl_to_acl(&dirnode->acl, dirnode->racl);

    // iterating over the full contents
    // and add each inode to the inode list of this directory
    for(int i = 0; i < capn_len(direntry->dir.contents); i++) {
        inode_t *inode;

        if((inode = flist_itementry_to_inode(database, direntry, i)))
            dirnode_appends_inode(dirnode, inode);
    }

    return dirnode;
}

// convert an internal capnp object
// directory into a public directory object
dirnode_t *libflist_directory_get(flist_db_t *database, char *path) {
    char *cleanpath = NULL;

    // we use strict convention to store
    // entry on the database since the id is a hash of
    // the path, the path needs to match exactly
    //
    // all the keys don't have leading slash and never have
    // trailing slash, the root directory is an empty string
    //
    // to make this function working for mostly everybody, let's
    // clean the path to ensure it should reflect exactly how
    // it's stored on the database
    if(!(cleanpath = flist_clean_path(path)))
        return NULL;

    debug("[+] directory get: clean path: <%s> -> <%s>\n", path, cleanpath);

    // converting this directory string into a directory
    // hash by the internal way used everywhere, this will
    // give the key required to find entry on the database
    char *key = libflist_path_key(cleanpath);
    debug("[+] directory get: entry key: <%s>\n", key);

    // requesting the directory object from the database
    // the object in the database is packed, this function will
    // return us something decoded and ready to use
    directory_t *direntry;

    if(!(direntry = flist_directory_get(database, key, cleanpath)))
        return NULL;

    // we now have the full directory contents into memory
    // because the internal object contains everything, but in
    // an internal format, it's time to convert this format
    // into our public interface
    dirnode_t *contents;

    if(!(contents = flist_directory_to_dirnode(database, direntry)))
        return NULL;

    // cleaning temporary string allocated
    // by internal functions and not needed objects anymore
    flist_directory_close(database, direntry);
    free(cleanpath);
    free(key);

    return contents;
}

dirnode_t *libflist_directory_get_recursive(flist_db_t *database, char *path) {
    dirnode_t *root = NULL;

    // fetching root directory
    if(!(root = libflist_directory_get(database, path)))
        return NULL;

    for(inode_t *inode = root->inode_list; inode; inode = inode->next) {
        // ignoring non-directories
        if(inode->type != INODE_DIRECTORY)
            continue;

        // if it's a directory, loading it's contents
        // and adding it to the directory lists
        dirnode_t *subdir;
        if(!(subdir = libflist_directory_get_recursive(database, inode->fullpath)))
            return NULL;

        dirnode_appends_dirnode(root, subdir);
    }

    return root;
}
