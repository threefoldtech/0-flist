#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <time.h>
#include <fts.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/limits.h>
#include <sys/sysmacros.h>
#include <libgen.h>
#include "libflist.h"
#include "verbose.h"
#include "database.h"
#include "flist.capnp.h"
#include "flist_acl.h"
#include "flist_dirnode.h"
#include "flist_serial.h"
#include "flist_tools.h"
#include "zero_chunk.h"

#define discard __attribute__((cleanup(__cleanup_free)))

static void __cleanup_free(void *p) {
    free(* (void **) p);
}

inode_t *flist_inode_create(const char *name, size_t size, const char *fullpath) {
    inode_t *inode;

    if(!(inode = calloc(sizeof(inode_t), 1)))
        return NULL;

    inode->name = strdup(name);
    inode->fullpath = strdup(fullpath);
    inode->size = size;

    return inode;
}

void flist_inode_chunks_free(inode_t *inode) {
    if(!inode->chunks)
        return;

    for(size_t i = 0; i < inode->chunks->size; i += 1) {
        free(inode->chunks->list[i].entryid);
        free(inode->chunks->list[i].decipher);
    }

    free(inode->chunks->list);
    free(inode->chunks);
}

void flist_inode_free(inode_t *inode) {
    flist_acl_free(inode->acl);
    flist_inode_chunks_free(inode);

    free(inode->name);
    free(inode->fullpath);
    free(inode->sdata);
    free(inode->link);
    free(inode->subdirkey);
    free(inode);
}

inode_t *flist_inode_duplicate(inode_t *source) {
    inode_t *inode;

    if(!(inode = flist_inode_create(source->name, source->size, source->fullpath)))
        return NULL;

    inode->acl = flist_acl_duplicate(source->acl);
    inode->type = source->type;
    inode->modification = source->modification;
    inode->creation = source->creation;
    inode->stype = source->stype;

    inode->subdirkey = flist_strdup_safe(source->subdirkey);
    inode->sdata = flist_strdup_safe(source->sdata);
    inode->link = flist_strdup_safe(source->link);

    inode->chunks = flist_chunks_duplicate(source->chunks);

    return inode;
}

inode_t *flist_inode_search(dirnode_t *root, char *inodename) {
    // inodes list empty (no list already set)
    if(!root->inode_list)
        return NULL;

    // iterating over inodes
    for(inode_t *source = root->inode_list; source; source = source->next) {
        if(strcmp(source->name, inodename) == 0)
            return source;
    }

    // inode not found
    return NULL;
}


//
// remove an inode from a directory
//
dirnode_t *flist_directory_rm_inode(dirnode_t *root, inode_t *target) {
    // looking for the previous entry
    inode_t *prev = NULL;
    inode_t *inode = root->inode_list;

    while(inode && inode != target) {
        prev = inode;
        inode = inode->next;
    }

    // inode not found on that directory
    if(!inode)
        return NULL;

    // our item is the first item
    if(!prev) {
        root->inode_list = target->next;

    } else {
        // skipping our inode
        prev->next = target->next;
    }

    // updating contents counter
    root->inode_length -= 1;

    // our inode is the latest on the list
    // updating pointer
    if(root->inode_last == target) {
        // our inode was the single one
        // on that directory
        if(!prev) {
            root->inode_last = NULL;
            return root;
        }

        root->inode_last = prev;
    }

    return root;
}

// remove all subdirectories from a directory, and so on for all childs
// we assume dirnode was not fetch using _get_recursive for performance
int flist_directory_rm_recursively(flist_db_t *database, dirnode_t *dirnode) {
    for(dirnode_t *subdir = dirnode->dir_list; subdir; subdir = subdir->next) {
        debug("[+] libflist: rm: recursively: walking inside: %s\n", subdir->fullpath);
        libflist_directory_rm_recursively(database, subdir);

        // at this point, we know all subdirectories inside
        // this directory are removed already, we can safely remove
        // entry from database
        debug("[+] libflist: rm: recursively: removing %s [%s]\n", subdir->fullpath, subdir->hashkey);
        database->sdel(database, subdir->hashkey);
    }

    // removing self directory from database
    database->sdel(database, dirnode->hashkey);

    return 0;
}

static inode_t *flist_process_file(const char *iname, const struct stat *sb, const char *realpath, dirnode_t *parent, flist_ctx_t *ctx) {
    inode_t *inode;

    char vpath[PATH_MAX];
    snprintf(vpath, sizeof(vpath), "%s/%s", parent->fullpath, iname);

    // override virtual path with only the
    // inode name, if parent have no path
    //
    // if parent have no path, the parent is the root
    // directory, and concatenate with it will ends
    // with a leading slash
    if(strlen(parent->fullpath) == 0)
        sprintf(vpath, "%s", iname);

    inode = flist_inode_create(iname, sb->st_size, vpath);

    inode->creation = sb->st_ctime;
    inode->modification = sb->st_mtime;
    inode->acl = flist_acl_from_stat(sb);

    // special stuff related to different
    // type of inode
    if(S_ISDIR(sb->st_mode)) {
        inode->type = INODE_DIRECTORY;
        inode->subdirkey = libflist_path_key(vpath);

        // create entry on the database
        debug("[+] libflist: process file: creating new directory entry\n");
        dirnode_t *newdir = flist_dirnode_create_from_stat(parent, iname, sb);
        flist_dirnode_appends_dirnode(parent, newdir);
        flist_serial_commit_dirnode(newdir, ctx, parent);
        // flist_dirnode_free(newdir); // FIXME ?
    }

    if(S_ISCHR(sb->st_mode) || S_ISBLK(sb->st_mode)) {
        inode->type = INODE_SPECIAL;
        inode->sdata = calloc(sizeof(char), 32);

        sprintf(inode->sdata, "%d,%d", major(sb->st_rdev), minor(sb->st_rdev));
    }

    if(S_ISFIFO(sb->st_mode) || S_ISSOCK(sb->st_mode)) {
        inode->type = INODE_SPECIAL;
        inode->sdata = strdup("(nothing)");
    }

    if(S_ISLNK(sb->st_mode)) {
        inode->type = INODE_LINK;
        inode->link = calloc(sizeof(char), sb->st_size + 1);

        if(readlink(realpath, inode->link, sb->st_size + 1) < 0)
            warnp("readlink");
    }

    if(S_ISREG(sb->st_mode)) {
        inode->type = INODE_FILE;

        // computing chunks
        inode->chunks = libflist_chunks_proceed((char *) realpath, ctx);
    }

    return inode;
}

inode_t *flist_inode_mkdir(char *name, dirnode_t *parent) {
    inode_t *inode;
    char vpath[PATH_MAX];

    sprintf(vpath, "%s", name);

    // support no-parent (mkdir /) or when parent is /
    if(parent && strlen(parent->fullpath) > 0)
        snprintf(vpath, sizeof(vpath), "%s/%s", parent->fullpath, name);

    if(!(inode = flist_inode_create(name, 4096, vpath)))
        return NULL;

    inode->creation = time(NULL);
    inode->modification = time(NULL);
    inode->type = INODE_DIRECTORY;
    inode->subdirkey = libflist_path_key(vpath);
    inode->acl = flist_acl_new("root", "root", 0755);

    return inode;
}

inode_t *flist_inode_rename(inode_t *inode, char *name) {
    free(inode->name);
    inode->name = strdup(name);

    return inode;
}

inode_t *flist_inode_from_localfile(char *localpath, dirnode_t *parent, flist_ctx_t *ctx) {
    struct stat sb;
    char *localdup = NULL;
    inode_t *inode = NULL;

    if(stat(localpath, &sb) < 0) {
        warnp(localpath);
        return NULL;
    }

    if(!(localdup = strdup(localpath)))
        diep("strdup");

    char *filename = basename(localdup);

    if(!(inode = flist_process_file(filename, &sb, localpath, parent, ctx)))
        return NULL;

    free(localdup);

    return inode;
}

static int fts_compare(const FTSENT **one, const FTSENT **two) {
    return (strcmp((*one)->fts_name, (*two)->fts_name));
}

inode_t *flist_inode_from_localdir(char *localdir, dirnode_t *parent, flist_ctx_t *ctx) {
    struct stat sb;
    size_t localen = strlen(localdir);

    // removing any training slashes on the localdir
    while(localdir[localen - 1] == '/') {
        localdir[localen - 1] = '\0';
        localen -= 1;
    }

    debug("[+] libflist: adding <%s> into </%s>\n", localdir, parent->fullpath);
    if(stat(localdir, &sb) < 0) {
        warnp(localdir);
        return NULL;
    }

    if(!S_ISDIR(sb.st_mode)) {
        debug("[-] libflist: localdir: local path is not a directory\n");
        // FIXME: set lib error str
        return NULL;
    }

    // recursively add file and directories
    FTS* fs = NULL;
    FTSENT *fentry = NULL;
    char *ftsargv[2] = {localdir, NULL};
    char *tmpsrc = dirname(strdup(localdir));
    inode_t *inode = NULL;
    dirnode_t *workingdir = parent;

    //
    // first pass:
    //   creating all directories hierarchy
    //
    debug("[+] libflist: localdir: ---\n");
    debug("[+] libflist: localdir: processing pass one\n");
    debug("[+] libflist: localdir: ---\n");

    if(!(fs = fts_open(ftsargv, FTS_NOCHDIR | FTS_NOSTAT | FTS_PHYSICAL, &fts_compare)))
        diep(localdir);

    while((fentry = fts_read(fs))) {
        if(fentry->fts_info != FTS_D)
            continue;

        discard char *vpath = flist_dirnode_virtual_path(workingdir, fentry->fts_path + strlen(tmpsrc));
        discard char *parentpath = dirname(strdup(vpath));

        debug("[+] libflist: local directory: adding: %s [%s]\n", fentry->fts_name, parentpath);

        // fetching parent directory
        dirnode_t *localparent = flist_dirnode_get(ctx->db, parentpath);

        // adding this new directory
        if(!(inode = libflist_inode_from_localfile(fentry->fts_path, localparent, ctx))) {
            fprintf(stderr, "[-] libflist: local directory: could not create inode\n");
            return NULL;
        }

        // saving changes
        flist_dirnode_appends_inode(localparent, inode);
        flist_serial_commit_dirnode(localparent, ctx, localparent);

        flist_dirnode_free(localparent);
    }

    fts_close(fs);

    //
    // second pass:
    //   processing all files
    //
    debug("[+] libflist: localdir: ---\n");
    debug("[+] libflist: localdir: processing pass two\n");
    debug("[+] libflist: localdir: ---\n");

    if(!(fs = fts_open(ftsargv, FTS_NOCHDIR | FTS_NOSTAT | FTS_PHYSICAL, &fts_compare)))
        diep(localdir);

    // reset working directory
    workingdir = parent;

    while((fentry = fts_read(fs))) {
        discard char *target = flist_dirnode_virtual_path(parent, fentry->fts_path + strlen(tmpsrc));

        debug("[+] libflist: processing: %s -> %s\n", fentry->fts_path, target);

        if(fentry->fts_info == FTS_D) {
            // pre-order directory, let's load the new directory
            // and keep track of the previous (insde 'next' field)
            debug("[+] libflist: switching to virtual directory: %s\n", target);

            dirnode_t *newdir = flist_dirnode_get(ctx->db, target);
            newdir->next = workingdir;
            workingdir = newdir;
            continue;
        }

        if(fentry->fts_info == FTS_DP) {
            // post-order directory, let's commit it's contents
            // and reload previous directory
            debug("[+] libflist: commiting: %s\n", workingdir->fullpath);

            flist_serial_commit_dirnode(workingdir, ctx, workingdir->next);
            dirnode_t *next = workingdir->next;

            flist_dirnode_free(workingdir);
            workingdir = next;
            continue;
        }

        if(!(inode = libflist_inode_from_localfile(fentry->fts_path, workingdir, ctx))) {
            fprintf(stderr, "[-] libflist: local directory: could not create inode\n");
            return NULL;
        }

        flist_dirnode_appends_inode(workingdir, inode);
    }

    fts_close(fs);

    // cleaning
    free(tmpsrc);

    return inode;
}

inode_t *flist_inode_from_dirnode(dirnode_t *dirnode) {
    inode_t *inode;

    if(!(inode = flist_inode_create(dirnode->name, 4096, dirnode->fullpath)))
        return NULL;

    inode->type = INODE_DIRECTORY;
    inode->modification = dirnode->modification;
    inode->creation = dirnode->creation;
    inode->subdirkey = strdup(dirnode->hashkey);
    inode->acl = flist_acl_duplicate(dirnode->acl);

    return inode;
}

inode_t *flist_directory_create(dirnode_t *parent, char *name) {
    inode_t *inode = libflist_inode_mkdir(name, parent);
    flist_dirnode_appends_inode(parent, inode);

    dirnode_t *dirnode = flist_dirnode_from_inode(inode);
    flist_dirnode_appends_dirnode(parent, dirnode);

    return inode;
}

inode_t *flist_inode_from_name(dirnode_t *root, char *filename) {
    for(inode_t *inode = root->inode_list; inode; inode = inode->next) {
        if(strcmp(inode->name, filename) == 0)
            return inode;
    }

    return NULL;
}




//
// public interface
//
void libflist_inode_free(inode_t *inode) {
    flist_inode_free(inode);
}

inode_t *libflist_inode_mkdir(char *name, dirnode_t *parent) {
    return flist_inode_mkdir(name, parent);
}

inode_t *libflist_inode_rename(inode_t *inode, char *name) {
    return flist_inode_rename(inode, name);
}

inode_t *libflist_inode_from_localfile(char *localpath, dirnode_t *parent, flist_ctx_t *ctx) {
    return flist_inode_from_localfile(localpath, parent, ctx);
}

inode_t *libflist_inode_from_localdir(char *localdir, dirnode_t *parent, flist_ctx_t *ctx) {
    return flist_inode_from_localdir(localdir, parent, ctx);
}

dirnode_t *libflist_directory_rm_inode(dirnode_t *root, inode_t *target) {
    return flist_directory_rm_inode(root, target);
}

int libflist_directory_rm_recursively(flist_db_t *database, dirnode_t *dirnode) {
    return flist_directory_rm_recursively(database, dirnode);
}

inode_t *libflist_inode_search(dirnode_t *root, char *inodename) {
    return flist_inode_search(root, inodename);
}

inode_t *libflist_directory_create(dirnode_t *parent, char *name) {
    return flist_directory_create(parent, name);
}

inode_t *libflist_inode_from_name(dirnode_t *root, char *filename) {
    return flist_inode_from_name(root, filename);
}
