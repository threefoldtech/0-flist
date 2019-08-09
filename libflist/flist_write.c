#define _DEFAULT_SOURCE
#define _GNU_SOURCE
#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <time.h>
#include <fts.h>
#include <ftw.h>
#include <unistd.h>
#include <blake2.h>
#include <linux/limits.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <sys/sysmacros.h>
#include <regex.h>
#include <libgen.h>
#include "libflist.h"
#include "verbose.h"
#include "database.h"
#include "flist.capnp.h"
#include "flist_write.h"

// #define FLIST_WRITE_FULLDUMP

// global excluders (regex of files or directory
// to exclude when creating an flist)
static excluders_t excluders = {
    .length = 0,
    .list = NULL,
};

int libflist_create_excluders_append(char *regex) {
    int errcode;
    char errstr[1024];

    // adding one entry on the list
    excluders.length += 1;

    // growing up the list
    if(!(excluders.list = realloc(excluders.list, sizeof(excluder_t) * excluders.length))) {
        libflist_errp("excluder: realloc");
        return 1;
    }

    excluder_t *excluder = &excluders.list[excluders.length - 1];

    // append string regex to list
    if(!(excluder->str = strdup(regex))) {
        libflist_errp("regex: strdup");
        return 1;
    }

    // append compiled regex to list
    if((errcode = regcomp(&excluder->regex, regex, REG_NOSUB | REG_EXTENDED))) {
        regerror(errcode, &excluder->regex, errstr, sizeof(errstr));
        libflist_set_error("regex", errstr);
        return 1;
    }

    return 0;
}

void libflist_create_excluders_free() {
    for(size_t i = 0; i < excluders.length; i++) {
        excluder_t *excluder = &excluders.list[i];
        regfree(&excluder->regex);
        free(excluder->str);
    }

    free(excluders.list);
    memset(&excluders, 0x00, sizeof(excluders_t));
}

int libflist_create_excluders_matches(const char *input) {
    // allows empty strings
    if(strlen(input) == 0)
        return 0;

    for(size_t i = 0; i < excluders.length; i++) {
        excluder_t *excluder = &excluders.list[i];

        if(regexec(&excluder->regex, input, 0, NULL, 0) != REG_NOMATCH)
            return 1;
    }

    return 0;
}

#define KEYLENGTH 16
#define ACLLENGTH 8

const char *inode_type_str[] = {
    "INODE_DIRECTORY",
    "INODE_FILE",
    "INODE_LINK",
    "INODE_SPECIAL",
};

//
//

flist_ctx_t *libflist_context_create(flist_db_t *db, flist_backend_t *backend) {
    flist_ctx_t *ctx;

    if(!(ctx = malloc(sizeof(flist_ctx_t))))
        diep("malloc");

    ctx->db = db;
    ctx->backend = backend;

    memset(&ctx->stats, 0x00, sizeof(flist_stats_t));

    return ctx;
}

void libflist_context_free(flist_ctx_t *ctx) {
    // FIXME: close database
    // FIXME: close backend
    free(ctx);
}

#if 0
typedef struct flist_write_global_t {
    char *root;
    flist_db_t *database;
    flist_backend_t *backend;
    dirnode_t *rootdir;
    dirnode_t *currentdir;
    flist_stats_t stats;

} flist_write_global_t;
#endif

#if 0
static flist_write_global_t globaldata = {
    .root = NULL,
    .database = NULL,
    .backend = NULL,
    .rootdir = NULL,
    .currentdir = NULL,
};
#endif

//
//

char *libflist_inode_acl_key(acl_t *acl) {
    char *key;
    char strmode[32];

    // re-implementing the python original version
    // the mode is created using 'oct(mode)[4:]'
    //
    // when doing oct(int), the returns is '0oXXXX'
    // so we fake the same behavior
    sprintf(strmode, "0o%o", acl->mode);

    // intermediate string key
    if(asprintf(&key, "user:%s\ngroup:%s\nmode:%s\n", acl->uname, acl->gname, strmode + 4) < 0)
        diep("asprintf");

    // hashing payload
    unsigned char hash[ACLLENGTH];

    if(blake2b(hash, key, "", ACLLENGTH, strlen(key), 0) < 0)
        dies("blake2 acl error");

    char *hashkey = libflist_hashhex(hash, ACLLENGTH);
    free(key);

    return hashkey;
}

acl_t *libflist_inode_acl_commit(inode_t *inode) {
    inode->acl->key = libflist_inode_acl_key(inode->acl);
    return inode->acl;
}

static char *uidstr(struct passwd *passwd, uid_t uid) {
    char *target;

    // if username cannot be found
    // let use user id as username
    if(!passwd) {
        libflist_warnp("getpwuid");

        if(asprintf(&target, "%d", uid) < 0)
            diep("asprintf");

        return target;
    }

    return strdup(passwd->pw_name);
}

static char *gidstr(struct group *group, gid_t gid) {
    char *target;

    // if group name cannot be found
    // let use group id as groupname
    if(!group) {
        warnp("getgrpid");
        if(asprintf(&target, "%d", gid) < 0)
            diep("asprintf");

        return target;
    }

    return strdup(group->gr_name);
}

acl_t *inode_acl_new(char *uname, char *gname, int mode) {
    acl_t *acl;

    if(!(acl = malloc(sizeof(acl_t)))) {
        warnp("acl: new: malloc");
        return NULL;
    }

    acl->mode = mode;
    acl->uname = strdup(uname);
    acl->gname = strdup(gname);
    acl->key = libflist_inode_acl_key(acl);

    return acl;
}

acl_t *inode_acl(const struct stat *sb) {
    char *uname = uidstr(getpwuid(sb->st_uid), sb->st_uid);
    char *gname = gidstr(getgrgid(sb->st_gid), sb->st_gid);
    acl_t *acl = inode_acl_new(uname, gname, sb->st_mode);

    free(uname);
    free(gname);

    return acl;
}


void inode_acl_free(acl_t *acl) {
    free(acl->uname);
    free(acl->gname);
    free(acl->key);
    free(acl);
}

static dirnode_t *dirnode_create(char *fullpath, char *name) {
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

    directory->hashkey = libflist_path_key(directory->fullpath);
    directory->acl = inode_acl_new("root", "root", 0755);

    return directory;
}

static dirnode_t *dirnode_create_from_stat(dirnode_t *parent, const char *name, const struct stat *sb) {
    char *fullpath;
    dirnode_t *root;

    if(strlen(parent->fullpath) == 0) {
        if(!(fullpath = strdup(name)))
            diep("strdup");

    } else {
        if(asprintf(&fullpath, "%s/%s", parent->fullpath, name) < 0)
            diep("asprintf");
    }

    if(!(root = dirnode_create(fullpath, (char *) name)))
        return NULL;

    root->creation = sb->st_ctime;
    root->modification = sb->st_mtime;
    root->acl = inode_acl(sb);

    free(fullpath);

    return root;
}

dirnode_t *libflist_internal_dirnode_create(char *fullpath, char *name) {
    return dirnode_create(fullpath, name);
}

void dirnode_free(dirnode_t *directory) {
    inode_acl_free(directory->acl);

    free(directory->fullpath);
    free(directory->name);
    free(directory->hashkey);
    free(directory);
}

static inode_t *inode_create(const char *name, size_t size, const char *fullpath) {
    inode_t *inode;

    if(!(inode = calloc(sizeof(inode_t), 1)))
        return NULL;

    inode->name = strdup(name);
    inode->fullpath = strdup(fullpath);
    inode->size = size;

    return inode;
}

void inode_free(inode_t *inode) {
    inode_acl_free(inode->acl);
    free(inode->name);
    free(inode->fullpath);
    free(inode->sdata);
    free(inode->link);
    free(inode->subdirkey);
    free(inode);
}

dirnode_t *dirnode_lazy_appends_inode(dirnode_t *root, inode_t *inode) {
    if(!root->inode_list)
        root->inode_list = inode;

    if(root->inode_last)
        root->inode_last->next = inode;

    root->inode_last = inode;
    root->inode_length += 1;

    return root;
}

dirnode_t *dirnode_appends_inode(dirnode_t *root, inode_t *inode) {
    inode->next = NULL;
    return dirnode_lazy_appends_inode(root, inode);
}

dirnode_t *dirnode_lazy_appends_dirnode(dirnode_t *root, dirnode_t *dir) {
    if(!root->dir_list)
        root->dir_list = dir;

    if(root->dir_last)
        root->dir_last->next = dir;

    root->dir_last = dir;
    root->dir_length += 1;

    return root;
}

dirnode_t *dirnode_appends_dirnode(dirnode_t *root, dirnode_t *dir) {
    dir->next = NULL;
    return dirnode_lazy_appends_dirnode(root, dir);
}

inode_t *libflist_inode_search(dirnode_t *root, char *inodename) {
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

dirnode_t *libflist_dirnode_search(dirnode_t *root, char *dirname) {
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

#if 0
static dirnode_t *dirnode_lookup_step(dirnode_t *root, char *token, char *incrpath) {
    dirnode_t *subdir = NULL;

    // incrpath can be safely strcat, the memory allocated
    // is the same length of the fullpath
    strcat(incrpath, token);
    strcat(incrpath, "/");

    // looking for that directory
    // if it doesn't exists, create a new one
    if(!(subdir = libflist_dirnode_search(root, token))) {
        if(!(subdir = dirnode_create(incrpath, token)))
            return NULL;

        dirnode_appends_dirnode(root, subdir);
    }

    // looking for new token
    if(!(token = strtok(NULL, "/")))
        return subdir;

    return dirnode_lookup_step(subdir, token, incrpath);
}
#endif

#if 0
static dirnode_t *dirnode_lookup(dirnode_t *root, const char *fullpath) {
    char *token;
    char *incrpath;
    char *fulldup;

    debug("[+] directory lookup: %s\n", fullpath);

    // duplicate fullpath
    // strtok change the string, we don't want to
    // modify string returns by nftw callback
    if(!(fulldup = strdup(fullpath)))
        diep("directory lookup: strdup");

    // creating token
    if(!(token = strtok(fulldup, "/"))) {
        free(fulldup);
        return root;
    }

    // incremental memory to build relative path step by step
    // using length + trailing slash + end of string
    if(!(incrpath = calloc(sizeof(char), strlen(fullpath) + 2)))
        diep("directory lookup: calloc");

    // iterate over the fullpath, looking for directories
    dirnode_t *target = dirnode_lookup_step(root, token, incrpath);
    free(fulldup);
    free(incrpath);

    return target;
}

static void dirnode_tree_free(dirnode_t *root) {
    for(dirnode_t *source = root->dir_list; source; ) {
        dirnode_t *next = source->next;

        dirnode_tree_free(source);
        source = next;
    }

    for(inode_t *inode = root->inode_list; inode; ) {
        inode_t *next = inode->next;

        inode_free(inode);
        inode = next;
    }

    dirnode_free(root);
}
#endif


// WARNING: FIXME - duplication just copy everything and cut next pointer
//                  THIS IS NOT A REAL DUPLICATION
dirnode_t *dirnode_lazy_duplicate(dirnode_t *source) {
    dirnode_t *copy;

    if(!(copy = malloc(sizeof(dirnode_t))))
        return libflist_diep("malloc");

    memcpy(copy, source, sizeof(dirnode_t));
    copy->next = NULL;

    return copy;
}

inode_t *inode_lazy_duplicate(inode_t *source) {
    inode_t *copy;

    if(!(copy = malloc(sizeof(inode_t))))
        return libflist_diep("malloc");

    memcpy(copy, source, sizeof(inode_t));
    copy->next = NULL;

    return copy;
}

//
// remove an inode from a directory
//
dirnode_t *libflist_directory_rm_inode(dirnode_t *root, inode_t *target) {
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
int libflist_directory_rm_recursively(flist_db_t *database, dirnode_t *dirnode) {
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

    inode = inode_create(iname, sb->st_size, vpath);

    inode->creation = sb->st_ctime;
    inode->modification = sb->st_mtime;
    inode->acl = inode_acl(sb);

    // special stuff related to different
    // type of inode
    if(S_ISDIR(sb->st_mode)) {
        inode->type = INODE_DIRECTORY;
        inode->subdirkey = libflist_path_key(vpath);

        // create entry on the database
        debug("[+] libflist: process file: creating new directory entry\n");
        dirnode_t *newdir = dirnode_create_from_stat(parent, iname, sb);
        dirnode_appends_dirnode(parent, newdir);
        libflist_dirnode_commit(newdir, ctx, parent);
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

inode_t *libflist_inode_mkdir(char *name, dirnode_t *parent) {
    inode_t *inode;
    char vpath[PATH_MAX];

    sprintf(vpath, "%s", name);

    // support no-parent (mkdir /) or when parent is /
    if(parent && strlen(parent->fullpath) > 0)
        snprintf(vpath, sizeof(vpath), "%s/%s", parent->fullpath, name);

    if(!(inode = inode_create(name, 4096, vpath)))
        return NULL;

    inode->creation = time(NULL);
    inode->modification = time(NULL);
    inode->type = INODE_DIRECTORY;
    inode->subdirkey = libflist_path_key(vpath);
    inode->acl = inode_acl_new("root", "root", 0755);

    return inode;
}

inode_t *libflist_inode_rename(inode_t *inode, char *name) {
    free(inode->name);
    inode->name = strdup(name);

    return inode;
}

inode_t *libflist_inode_from_localfile(char *localpath, dirnode_t *parent, flist_ctx_t *ctx) {
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

static int dirnode_is_root(dirnode_t *dirnode) {
    return (strlen(dirnode->fullpath) == 0);
}

static char *dirnode_virtual_path(dirnode_t *parent, char *target) {
    char *vpath;

    if(dirnode_is_root(parent)) {
        if(asprintf(&vpath, "%s", target) < 0)
            return NULL;

        return vpath;
    }

    if(asprintf(&vpath, "/%s%s", parent->fullpath, target) < 0)
        return NULL;

    return vpath;
}

inode_t *libflist_inode_from_localdir(char *localdir, dirnode_t *parent, flist_ctx_t *ctx) {
    struct stat sb;

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

        char *vpath = dirnode_virtual_path(workingdir, fentry->fts_path + strlen(tmpsrc));
        char *parentpath = dirname(strdup(vpath));

        debug("[+] libflist: local directory: adding: %s [%s]\n", fentry->fts_name, parentpath);

        // fetching parent directory
        dirnode_t *localparent = libflist_dirnode_get(ctx->db, parentpath);

        // adding this new directory
        if(!(inode = libflist_inode_from_localfile(fentry->fts_path, localparent, ctx))) {
            fprintf(stderr, "[-] libflist: local directory: could not create inode\n");
            return NULL;
        }

        // saving changes
        dirnode_appends_inode(localparent, inode);
        libflist_dirnode_commit(localparent, ctx, localparent);

        free(vpath);
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
        char *target = dirnode_virtual_path(parent, fentry->fts_path + strlen(tmpsrc));

        debug("[+] libflist: processing: %s -> %s\n", fentry->fts_path, target);

        if(fentry->fts_info == FTS_D) {
            // pre-order directory, let's load the new directory
            // and keep track of the previous (insde 'next' field)
            debug("[+] libflist: switching to virtual directory: %s\n", target);

            dirnode_t *newdir = libflist_dirnode_get(ctx->db, target);
            newdir->next = workingdir;
            workingdir = newdir;
            continue;
        }

        if(fentry->fts_info == FTS_DP) {
            // post-order directory, let's commit it's contents
            // and reload previous directory
            debug("[+] libflist: commiting: %s\n", workingdir->fullpath);

            libflist_dirnode_commit(workingdir, ctx, workingdir->next);
            workingdir = workingdir->next;
            continue;
        }

        if(!(inode = libflist_inode_from_localfile(fentry->fts_path, workingdir, ctx))) {
            fprintf(stderr, "[-] libflist: local directory: could not create inode\n");
            return NULL;
        }

        dirnode_appends_inode(workingdir, inode);
    }

    fts_close(fs);

    // cleaning
    free(tmpsrc);

    return inode;
}

#if 0
static int flist_dirnode_metadata(dirnode_t *root, const struct stat *sb) {
    debug("[+] fixing root directory: %s\n", root->fullpath);

    root->creation = sb->st_ctime;
    root->modification = sb->st_mtime;

    // skipping already set acl
    if(!root->acl->key)
        root->acl = inode_acl(sb);

    return 0;
}
#endif

#if 0
// FIX TODO:
//  - always lookup relative directory from fpath
//  - when FTW_DP && fpath == / -> we are done
//  - recursive create directories when lookup
static char *relative_path(const char *fpath, int typeflag, const char *rootpath) {
    // building general relative path
    //
    //      real: /realroot/somewhere/target/hello/world   # current path
    //    create: ++++++++++++++++++++++++++^              # create root path provided
    //  relative: ........................../hello/world   # relative path
    //
    const char *relative = fpath + strlen(rootpath);

    // we are on the root, nothing more to check
    if(strlen(relative) == 0)
        return strdup("");

    // removing leading slash
    relative += 1;

    // flag FTW_D and FTW_DP are set when fpath is a directory
    // or a directory and moreover, everthing below was proceed
    if(typeflag == FTW_D || typeflag == FTW_DP) {
        // since it's already a directory, we can return it as it
        return strdup(relative);
    }

    // if we are here, we are handling a file, let's finding it's
    // root directory
    size_t length = strlen(relative);

    while(*(relative + length - 1) != '/' && length)
        length -= 1;

    // this should not happens, anyway let's ensure this
    if(length == 0)
        return strdup("");

    // removing trailing slash
    return strndup(relative, length - 1);
}

// returns the parent directory dirnode of a relative path
static char *relative_path_parent(char *relative) {
    size_t length = strlen(relative);

    while(length && relative[length - 1] != '/')
        length -= 1;

    if(length == 0)
        return strdup("");

    // we have the parent directory
    return strndup(relative, length - 1);
}
#endif

#if 0
static int flist_create_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    char *relpath = NULL;
    char *parent = NULL;

    // checking if entry is rejected by exclude filter
    // if exclude matches, we don't parse this entry at all
    if(libflist_create_excluders_matches(fpath)) {
        debug("[+] skipping [%s] from exclude filters\n", fpath);
        return 0;
    }

    // building relative directory, which is the directory
    // where the file (or the directory itself if target is a directory)
    // is located
    if(!(relpath = relative_path(fpath, typeflag, globaldata.root)))
        goto something_wrong;

    debug("[+] relative directory: </%s>\n", relpath);

    // setting the global current directory pointer to this directory
    if(!(globaldata.currentdir = dirnode_lookup(globaldata.rootdir, relpath)))
        goto something_wrong;

    // if the relative path is empty, we are on the root directory
    // and if the FTW_DP flag is set, we know all subdirectories and file
    // in that directory are proceed, that simply mean we are done, everything
    // was proceed
    if(strlen(relpath) == 0 && typeflag == FTW_DP) {
        debug("[+] the complete rootpath has been proceed, mapping done\n");

        // setting up permissions etc. on the root directory
        // and we are done
        flist_dirnode_metadata(globaldata.currentdir, sb);

        // goto next file, even if there are nothing more
        // we know it's the end of the walk
        goto next_file;
    }

    // we hit a directory, let's set it's metadata and ensure we add it
    // this will be done for each directories, except root one (we just did it
    // right before)
    if(typeflag == FTW_DP) {
        debug("[+] processing directory\n");

        dirnode_t *parentnode;
        if(!(parent = relative_path_parent(relpath)))
            goto something_wrong;

        // append this inode to the root directory
        if(!(parentnode = dirnode_lookup(globaldata.rootdir, parent)))
            goto something_wrong;

        // building the inode of this directory
        const char *itemname = fpath + ftwbuf->base;
        inode_t *inode = flist_process_file(itemname, sb, fpath, parentnode, NULL);
        dirnode_appends_inode(parentnode, inode);

        // setting metadata of that directory
        flist_dirnode_metadata(globaldata.currentdir, sb);

        // nothing more to do on this directory
        goto next_file;
    }

    /*
    // if the global current directory is not set
    // we can be in the first call or in a call relative to a subdirectory
    // change, let's lookup (again) this relative directory to set the global
    // currentdir correctly
    // if(!currentdir) {
    //
    // FIX: force to reset currentdir each time
    //      there are some bug when trusting this safe
    if(!(globaldata.currentdir = dirnode_lookup(globaldata.rootdir, relpath)))
        return 1;

    // } else printf("current dir set\n");
    */

    debug("[+] current directory: %s (%s)\n", globaldata.currentdir->name, globaldata.currentdir->fullpath);

    // processing the real entry
    // this can be everything, except a directory
    //
    // we can just parse the stat struct, fill-in our inode object and add it to the
    // currentdir object
    const char *itemname = fpath + ftwbuf->base;
    debug("[+] processing: %s [%s] (%lu)\n", itemname, fpath, sb->st_size);

    inode_t *inode = flist_process_file(itemname, sb, fpath, globaldata.currentdir, NULL);
    dirnode_appends_inode(globaldata.currentdir, inode);

    /*
    // this is maybe an empty directory, we don't know
    // in doubt, let's call lookup in order to create
    // entry if it doesn't exists
    if(inode->type == INODE_DIRECTORY) {
        dirnode_t *check = dirnode_lookup(globaldata.rootdir, inode->fullpath);

        // if the directory is never reached anyway
        // metadata won't be set anywhere else
        flist_dirnode_metadata(check, sb);
    }
    */

    /*
    // if relpath is empty, we are doing some stuff
    // on the virtual root, let's reset the currentdir
    // since next call can be a deep directory somewhere else
    //
    // we set nftw to parse directories first, which mean we always goes
    // as deep as possible on each call
    if(strlen(relpath) == 0) {
        debug("[+] touching virtual root directory, reseting current directory\n");
        globaldata.currentdir = NULL;
    }
    */

next_file:
    free(relpath);
    free(parent);
    return 0;

something_wrong:
    free(relpath);
    free(parent);
    return 1;
}

// ensure the root path is well formatted
char *flist_rootpath_clean(const char *input) {
    size_t length = strlen(input);
    char *fixed = strdup(input);

    if(!fixed)
        return NULL;

    // forcing rootpath to not have trailing slash
    while(fixed[length - 1] == '/') {
        fixed[length - 1] = '\0';
        length -= 1;
    }

    return fixed;
}
#endif

#if 0
flist_stats_t *libflist_create(flist_db_t *database, const char *root, flist_backend_t *backend) {
    flist_stats_t *stats = NULL;
    struct stat st;

    debug("[+] preparing flist for: %s\n", root);

    // does that directory exists, before doing
    // anything on it
    if(stat(root, &st))
        return libflist_errp(root);

    if(!S_ISDIR(st.st_mode))
        return libflist_set_error("source path is not a directory");

    // initialize excluders
    libflist_create_excluders_append("\\.pyc$");
    libflist_create_excluders_append(".*__pycache__");
    libflist_create_excluders_append("/dev");

    if(!(globaldata.rootdir = dirnode_create("", "")))
        return NULL;

    // FIXME: thread safe ? add one global lock ?

    globaldata.root = flist_rootpath_clean(root);
    globaldata.database = database;
    globaldata.backend = backend;

    memset(&globaldata.stats, 0x00, sizeof(flist_stats_t));

    debug("[+] building database\n");
    if(nftw(root, flist_create_cb, 512, FTW_DEPTH | FTW_PHYS)) {
        libflist_errp("nftw");
        return NULL;
    }

    // FIXME: release the global lock ?

#ifdef FLIST_WRITE_FULLDUMP
    debug("===================================\n");
    libflist_dirnode_dumps(globaldata.rootdir);
#endif

    debug("[+] =========================================\n");
    debug("[+] building capnp from memory tree\n");
    flist_ctx_t *ctx = libflist_context_create(database, backend);
    libflist_dirnode_commit(globaldata.rootdir, ctx, globaldata.rootdir);

    debug("[+] recursivly freeing directory tree\n");
    dirnode_tree_free(globaldata.rootdir);
    // upload_inode_flush(); // FIXME

    if(backend)
        libflist_backend_free(backend);

    // cleaning
    free(globaldata.root);
    libflist_create_excluders_free();

    // convert local stats
    stats = libflist_bufdup(&globaldata.stats, sizeof(flist_stats_t));

    return stats;
}
#endif
