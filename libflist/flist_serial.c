#define _XOPEN_SOURCE 500
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <time.h>
#include <fts.h>
#include <unistd.h>
#include <regex.h>
#include "libflist.h"
#include "verbose.h"
#include "database.h"
#include "flist.capnp.h"
#include "flist_write.h"
#include "flist_read.h"
#include "flist_dirnode.h"

#define KEYLENGTH 16
#define ACLLENGTH 8


//
// capnp helpers
//
char *flist_inode_fullpath(struct Dir *dir, struct Inode *inode) {
    char *fullpath;

    if(strlen(dir->location.str) == 0)
        return strdup(inode->name.str);

    if(asprintf(&fullpath, "%s/%s", dir->location.str, inode->name.str) < 0)
        return NULL;

    return fullpath;
}

static capn_text chars_to_text(const char *chars) {
    return (capn_text) {
        .len = (int) strlen(chars),
        .str = chars,
        .seg = NULL,
    };
}

static capn_ptr capn_datatext(struct capn_segment *cs, char *payload) {
    size_t length = strlen(payload);

    capn_list8 data = capn_new_list8(cs, length);
    capn_setv8(data, 0, (uint8_t *) payload, length);

    return data.p;
}

static capn_ptr capn_databinary(struct capn_segment *cs, char *payload, size_t length) {
    capn_list8 data = capn_new_list8(cs, length);
    capn_setv8(data, 0, (uint8_t *) payload, length);

    return data.p;
}

//
// capnp deserializers
//
acl_t *libflist_get_acl(flist_db_t *database, const char *aclkey) {
    acl_t *acl;

    if(strlen(aclkey) == 0) {
        debug("[-] libflist: acl: get: empty key, cannot load it\n");
        return NULL;
    }

    value_t *rawdata = database->sget(database, (char *) aclkey);
    if(!rawdata->data) {
        debug("[-] libflist: acl: get: acl key <%s> not found\n", aclkey);
        return NULL;
    }

    if(!(acl = malloc(sizeof(acl_t)))) {
        warnp("acl: malloc");
        return NULL;
    }

    // load acl database object
    // into an acl readable entry
    ACI_ptr acip;
    struct ACI aci;

    struct capn permsctx;
    if(capn_init_mem(&permsctx, (unsigned char *) rawdata->data, rawdata->length, 0)) {
        debug("[-] libflist: acl: capnp: init error\n");
        return NULL;
    }

    acip.p = capn_getp(capn_root(&permsctx), 0, 1);
    read_ACI(&aci, acip);

    // now we can populate our public acl object
    // from the database entry
    acl->uname = strdup(aci.uname.str);
    acl->gname = strdup(aci.gname.str);
    acl->mode = aci.mode;
    acl->key = strdup(aclkey);

    capn_free(&permsctx);
    database->clean(rawdata);

    return acl;
}

//
// capnp serializers
//
void inode_acl_commit(flist_db_t *database, acl_t *acl) {
    if(database->sexists(database, acl->key))
        return;

    // create a capnp aci object
    struct ACI aci = {
        .uname = chars_to_text(acl->uname),
        .gname = chars_to_text(acl->gname),
        .mode = acl->mode,
        .id = 0,
    };

    // prepare a writer
    struct capn c;
    capn_init_malloc(&c);
    capn_ptr cr = capn_root(&c);
    struct capn_segment *cs = cr.seg;
    unsigned char buffer[4096];

    ACI_ptr ap = new_ACI(cs);
    write_ACI(&aci, ap);

    if(capn_setp(capn_root(&c), 0, ap.p))
        dies("acl capnp setp failed");

    int sz = capn_write_mem(&c, buffer, 4096, 0);
    capn_free(&c);

    debug("[+]   writing acl into db: %s\n", acl->key);
    if(database->sset(database, acl->key, buffer, sz))
        dies("acl database error");
}

void flist_dirnode_commit(dirnode_t *root, flist_ctx_t *ctx, dirnode_t *parent) {
    struct capn c;
    capn_init_malloc(&c);
    capn_ptr cr = capn_root(&c);
    struct capn_segment *cs = cr.seg;

    debug("[+] populating directory: </%s>\n", root->fullpath);

    // creating this directory entry
    struct Dir dir = {
        .name = chars_to_text(root->name),
        .location = chars_to_text(root->fullpath),
        .contents = new_Inode_list(cs, root->inode_length),
        .parent = chars_to_text(parent->hashkey),
        .size = 4096,
        .aclkey = chars_to_text(root->acl->key),
        .modificationTime = root->modification,
        .creationTime = root->creation,
    };

    inode_acl_commit(ctx->db, root->acl);

    // populating contents
    int index = 0;
    for(inode_t *inode = root->inode_list; inode; inode = inode->next, index += 1) {
        struct Inode target;

        debug("[+]   populate inode: <%s>\n", inode->name);

        target.name = chars_to_text(inode->name);
        target.size = inode->size;
        target.attributes_which = inode->type;
        target.aclkey = chars_to_text(inode->acl->key);
        target.modificationTime = inode->modification;
        target.creationTime = inode->creation;

        if(inode->type == INODE_DIRECTORY) {
            struct SubDir sd = {
                .key = chars_to_text(inode->subdirkey),
            };

            target.attributes.dir = new_SubDir(cs);
            write_SubDir(&sd, target.attributes.dir);

            ctx->stats.directory += 1;
        }

        if(inode->type == INODE_LINK) {
            struct Link l = {
                .target = chars_to_text(inode->link),
            };

            target.attributes.link = new_Link(cs);
            write_Link(&l, target.attributes.link);

            ctx->stats.symlink += 1;
        }

        if(inode->type == INODE_SPECIAL) {
            struct Special sp = {
                .type = inode->stype,
            };

            // see: https://github.com/opensourcerouting/c-capnproto/blob/master/lib/capnp_c.h#L196
            sp.data.p = capn_datatext(cs, inode->sdata);

            // capn_list8 data = capn_new_list8(cs, 1);
            // capn_setv8(data, 0, (uint8_t *) inode->sdata, strlen(inode->sdata));
            // sp.data.p = data.p;

            target.attributes.special = new_Special(cs);
            write_Special(&sp, target.attributes.special);

            ctx->stats.special += 1;
        }

        if(inode->type == INODE_FILE) {
            struct File f = {
                .blockSize = 128, // ignored, hardcoded value
            };

            // upload non-empty files
            if(inode->size && (ctx->backend || inode->chunks)) {
                // chunks filled by read functions
                if(inode->chunks) {
                    f.blocks = new_FileBlock_list(cs, inode->chunks->size);

                    for(size_t i = 0; i < inode->chunks->size; i++) {
                        struct FileBlock block;
                        inode_chunk_t *chk = &inode->chunks->list[i];

                        block.hash.p = capn_databinary(cs, (char *) chk->entryid, chk->entrylen);
                        block.key.p = capn_databinary(cs, (char *) chk->decipher, chk->decipherlen);

                        set_FileBlock(&block, f.blocks, i);
                    }
                }
            }

            target.attributes.file = new_File(cs);
            write_File(&f, target.attributes.file);

            ctx->stats.regular += 1;
            ctx->stats.size += inode->size;
        }

        set_Inode(&target, dir.contents, index);
        inode_acl_commit(ctx->db, inode->acl);
    }

    // commit capnp object
    unsigned char *buffer = malloc(8 * 512 * 1024); // FIXME

    Dir_ptr dp = new_Dir(cs);
    write_Dir(&dir, dp);


    if(capn_setp(capn_root(&c), 0, dp.p))
        dies("capnp setp failed");

    int sz = capn_write_mem(&c, buffer, 8 * 512 * 1024, 0); // FIXME
    capn_free(&c);

    // commit this object into the database
    debug("[+] writing into db: %s\n", root->hashkey);
    if(ctx->db->sexists(ctx->db, root->hashkey)) {
        if(ctx->db->sdel(ctx->db, root->hashkey))
            dies("key exists, deleting: database error");
    }

    if(ctx->db->sset(ctx->db, root->hashkey, buffer, sz))
        dies("database error");

    free(buffer);

    // walking over the sub-directories
    for(dirnode_t *subdir = root->dir_list; subdir; subdir = subdir->next)
        flist_dirnode_commit(subdir, ctx, root);
}

dirnode_t *flist_dir_to_dirnode(flist_db_t *database, struct Dir *dir) {
    dirnode_t *dirnode;

    if(!(dirnode = calloc(sizeof(dirnode_t), 1)))
        return NULL;

    // setting directory metadata
    dirnode->fullpath = strdup(dir->location.str);
    dirnode->name = strdup(dir->name.str);
    dirnode->hashkey = libflist_path_key(dirnode->fullpath);
    dirnode->creation = dir->creationTime;
    dirnode->modification = dir->modificationTime;

    dirnode->acl = libflist_get_acl(database, dir->aclkey.str);

    // iterating over the full contents
    // and add each inode to the inode list of this directory
    for(int i = 0; i < capn_len(dir->contents); i++) {
        inode_t *inode;

        if((inode = flist_itementry_to_inode(database, dir, i)))
            flist_dirnode_appends_inode(dirnode, inode);
    }

    return dirnode;
}

dirnode_t *xxx_flist_dirnode_get(flist_db_t *database, char *key, char *fullpath) {
    value_t *value;
    struct capn capctx;
    Dir_ptr dirp;
    struct Dir dir;

    // reading capnp message from database
    value = database->sget(database, key);

    // FIXME: memory leak
    if(!value->data) {
        debug("[-] libflist: dirnode: key [%s - %s] not found\n", key, fullpath);
        database->clean(value);
        return NULL;
    }

    // build capn context
    if(capn_init_mem(&capctx, (unsigned char *) value->data, value->length, 0)) {
        debug("[-] libflist: dirnode: capnp: init error\n");
        database->clean(value);
        // FIXME: memory leak
        return NULL;
    }

    // populate dir struct from context
    // the contents is always a directory (one key per directory)
    // and the whole contents is on the content field
    dirp.p = capn_getp(capn_root(&capctx), 0, 1);
    read_Dir(&dir, dirp);

    dirnode_t *dirnode = flist_dir_to_dirnode(database, &dir);

    // cleanup capnp
    capn_free(&capctx);
    database->clean(value);

    return dirnode;
}

//
// public interface
//
void libflist_dirnode_commit(dirnode_t *root, flist_ctx_t *ctx, dirnode_t *parent) {
    return flist_dirnode_commit(root, ctx, parent);
}
