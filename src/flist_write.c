#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ftw.h>
#include <unistd.h>
#include <rocksdb/c.h>
#include <blake2.h>
#include <linux/limits.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <sys/sysmacros.h>
#include "flister.h"
#include "flist_write.h"
#include "flist.capnp.h"

#define KEYLENGTH 32

typedef struct acl_t {
    char *uname;
    char *gname;
    uint16_t mode;

} acl_t;

typedef enum inode_type_t {
    INODE_DIRECTORY,
    INODE_FILE,
    INODE_LINK,
    INODE_SPECIAL,

} inode_type_t;

const char *inode_type_str[] = {
    "INODE_DIRECTORY",
    "INODE_FILE",
    "INODE_LINK",
    "INODE_SPECIAL",
};

typedef struct inode_t {
    char *name;
    char *fullpath;
    size_t size;

    inode_type_t type;
    time_t modification;
    time_t creation;

    char *subdirkey; // sub key

    int stype;       // special type
    char *sdata;     // special metadata

    char *link;      // symlink target

    acl_t acl;

    struct inode_t *next;

} inode_t;

typedef struct directory_t {
    struct inode_t *inode_list;
    struct inode_t *inode_last;
    size_t inode_length;

    struct directory_t *dir_list;
    struct directory_t *dir_last;
    size_t dir_length;

    char *fullpath;
    char *name;
    // attrs

    time_t creation;
    time_t modification;
    acl_t acl;

    struct directory_t *next;

} directory_t;

static char *uidstr(struct passwd *passwd, uid_t uid) {
    // if username cannot be found
    // let use user id as username
    if(!passwd) {
        warnp("getpwuid");

        size_t len = snprintf(NULL, 0, "%d", uid);
        char *target = malloc(sizeof(char) * len);
        sprintf(target, "%d", uid);
    }

    return strdup(passwd->pw_name);
}

static char *gidstr(struct group *group, gid_t gid) {
    // if group name cannot be found
    // let use group id as groupname
    if(!group) {
        warnp("getgrpid");

        size_t len = snprintf(NULL, 0, "%d", gid);
        char *target = malloc(sizeof(char) * len);
        sprintf(target, "%d", gid);
    }

    return strdup(group->gr_name);
}

acl_t inode_acl(const struct stat *sb) {
    acl_t acl;

    acl.mode = sb->st_mode;
    acl.uname = uidstr(getpwuid(sb->st_uid), sb->st_uid);
    acl.gname = gidstr(getgrgid(sb->st_gid), sb->st_gid);

    return acl;
}

void inode_acl_free(acl_t *acl) {
    free(acl->uname);
    free(acl->gname);
}

static directory_t *rootdir = NULL;
static directory_t *currentdir = NULL;

static directory_t *directory_create(char *fullpath, char *name) {
    directory_t *directory;

    if(!(directory = calloc(sizeof(directory_t), 1)))
        return NULL;

    directory->fullpath = strdup(fullpath);
    directory->name = strdup(name);

    // some cleaning (remove trailing slash)
    size_t lf = strlen(directory->fullpath);
    if(directory->fullpath[lf - 1] == '/')
        directory->fullpath[lf - 1] = '\0';

    return directory;
}

void directory_free(directory_t *directory) {
    free(directory->fullpath);
    free(directory->name);
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
    inode_acl_free(&inode->acl);
    free(inode->name);
    free(inode->fullpath);
    free(inode->sdata);
    free(inode->link);
    free(inode);
}

void inode_dumps(inode_t *inode, directory_t *rootdir) {
    printf("[+] inode: rootdir: 0x%p\n", rootdir);
    printf("[+] inode: %s: %s/%s\n", inode_type_str[inode->type], rootdir->fullpath, inode->name);

    printf("[+] inode:   size: %lu bytes (%.2f MB)\n", inode->size, inode->size / (1024 * 1024.0));
    printf("[+] inode:   ctime: %lu, mtime: %lu\n", inode->creation, inode->modification);
    printf("[+] inode:   user: %s, group: %s\n", inode->acl.uname, inode->acl.gname);
    printf("[+] inode:   mode: %o\n", inode->acl.mode);

    if(inode->type == INODE_LINK)
        printf("[+] inode:   symlink: %s\n", inode->link);

    if(inode->type == INODE_SPECIAL)
        printf("[+] inode:   special: %s\n", inode->sdata);
}

static directory_t *directory_appends_inode(directory_t *root, inode_t *inode) {
    inode->next = NULL;

    if(!root->inode_list)
        root->inode_list = inode;

    if(root->inode_last)
        root->inode_last->next = inode;

    root->inode_last = inode;
    root->inode_length += 1;

    return root;
}

static directory_t *directory_appends_directory(directory_t *root, directory_t *dir) {
    dir->next = NULL;

    if(!root->dir_list)
        root->dir_list = dir;

    if(root->dir_last)
        root->dir_last->next = dir;

    root->dir_last = dir;
    root->dir_length += 1;

    return root;
}

static directory_t *directory_search(directory_t *root, char *dirname) {
    // directory empty (no list already set)
    if(!root->dir_list)
        return NULL;

    // iterating over directories
    for(directory_t *source = root->dir_list; source; source = source->next) {
        if(strcmp(source->name, dirname) == 0)
            return source;
    }

    // directory not found
    return NULL;
}

static directory_t *directory_lookup_step(directory_t *root, char *token, char *incrpath) {
    directory_t *subdir = NULL;

    // incrpath can be safely strcat, the memory allocated
    // is the same length of the fullpath
    strcat(incrpath, token);
    strcat(incrpath, "/");

    // looking for that directory
    // if it doesn't exists, create a new one
    if(!(subdir = directory_search(root, token))) {
        if(!(subdir = directory_create(incrpath, token)))
            return NULL;

        directory_appends_directory(root, subdir);
    }

    // looking for newt token
    if(!(token = strtok(NULL, "/")))
        return subdir;

    return directory_lookup_step(subdir, token, incrpath);
}

static directory_t *directory_lookup(directory_t *root, char *fullpath) {
    char *token;
    char *incrpath;
    char *fulldup;

    printf("[+] directory lookup: %s\n", fullpath);

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
    if(!(incrpath = calloc(sizeof(char), strlen(fullpath) + 1)))
        diep("directory lookup: calloc");

    // iterate over the fullpath, looking for directories
    directory_t *target = directory_lookup_step(root, token, incrpath);
    free(fulldup);
    free(incrpath);

    return target;
}

static void directory_dumps(directory_t *root) {
    printf("[+] directory: %s [%s]\n", root->name, root->fullpath);
    printf("[+] contents: subdirectories: %lu\n", root->dir_length);
    printf("[+] contents: inodes: %lu\n", root->inode_length);

    for(directory_t *source = root->dir_list; source; source = source->next)
        directory_dumps(source);

    for(inode_t *inode = root->inode_list; inode; inode = inode->next)
        inode_dumps(inode, root);
}

static void directory_tree_free(directory_t *root) {
    for(directory_t *source = root->dir_list; source; ) {
        directory_t *next = source->next;

        directory_tree_free(source);
        source = next;
    }

    for(inode_t *inode = root->inode_list; inode; ) {
        inode_t *next = inode->next;

        inode_free(inode);
        inode = next;
    }

    directory_free(root);
}

//
// WARNING: this code uses 'ftw', which is by design, not thread safe
//          all functions used here need to be concidered as non-thread-safe
//          be careful if you want to add threads on the layer
//

static const char *pathkey(const char *path) {
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
// capnp dumper
//
void directory_tree_capn(directory_t *root) {
    /*
    struct capn c;
    capn_init_malloc(&c);
    capn_ptr cr = capn_root(&c);
    struct capn_segment *cs = cr.seg;

    struct
    */
}

//
// walker
//
static inode_t *flist_process_file(const char *iname, const struct stat *sb, const char *realpath, directory_t *parent) {
    inode_t *inode;

    char vpath[PATH_MAX];
    sprintf(vpath, "%s/%s", parent->fullpath, iname);

    inode = inode_create(iname, sb->st_size, vpath);

    inode->creation = sb->st_ctime;
    inode->modification = sb->st_mtime;
    inode->acl = inode_acl(sb);

    // special stuff related to different
    // type of inode
    if(S_ISDIR(sb->st_mode)) {
        inode->type = INODE_DIRECTORY;
    }

    if(S_ISCHR(sb->st_mode) || S_ISBLK(sb->st_mode)) {
        inode->type = INODE_SPECIAL;
        inode->sdata = calloc(sizeof(char), 32);

        sprintf(inode->sdata, "%d,%d", major(sb->st_rdev), minor(sb->st_rdev));
    }

    if(S_ISFIFO(sb->st_mode) || S_ISSOCK(sb->st_mode)) {
        inode->type = INODE_SPECIAL;
        inode->sdata = "(nothing)";
    }

    if(S_ISLNK(sb->st_mode)) {
        inode->type = INODE_LINK;
        inode->link = calloc(sizeof(char), sb->st_size + 1);

        if(readlink(realpath, inode->link, sb->st_size + 1) < 0)
            warnp("readlink");
    }

    if(S_ISREG(sb->st_mode)) {
        inode->type = INODE_FILE;
    }

    return inode;
}

static int flist_create_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    ssize_t length = ftwbuf->base - settings.rootlen - 1;
    char *relpath;

    // flag FTW_DP is set when all subdirectories was
    // parsed, now we know the remaining contents of this directory
    // are only files, let reset currentdir, this will be looked up again
    // later, we can't ensure the currentdir is still the relevant directory
    // since it can be one level up
    if(typeflag == FTW_DP) {
        printf("DIRECTORY, ALL SUBS TREATED\n");
        currentdir = NULL;
    }

    // if we reach level 0, we are processing the root directory
    // this directory should not be proceed since we have a virtual root
    // if we are here, we are all done with the walking process
    if(ftwbuf->level == 0) {
        printf("======== ROOT PATH, WE ARE DONE ========\n");
        return 0;
    }

    // building current path (called 'relative path')
    // this relative path is relative to the root filesystem tree
    // but is kind of absolute for our virtual root we are building
    //
    // this relative path won't start with a slash, to simplify some process
    // this mean when processing files or directory on the virtual root, the relative
    // path will be an empty string
    if(length > 0 && ftwbuf->level > 1) {
         relpath = strndup(fpath + settings.rootlen, ftwbuf->base - settings.rootlen - 1);

    } else relpath = strdup("");

    // if the global current directory is not set
    // we can be in the first call or in a call relative to a subdirectory
    // change, let's lookup (again) this relative directory to set the global
    // currentdir correctly
    if(!currentdir) {
        if(!(currentdir = directory_lookup(rootdir, relpath)))
            return 1;
    }

    printf("[+] current directory: %s (%s)\n", currentdir->name, currentdir->fullpath);

    // processing the real entry
    // this can be a file, a directory, anything
    //
    // at this point we don't know what this is, we will use the stat struct
    // provided by nftw to parse this entry
    //
    // since directory tree is already ensured by previous call (the currentdir
    // lookup, basicly) we don't need to care about directories existance, etc.
    //
    // we can just parse the stat struct, fill-in our inode object and add it to the
    // currentdir object
    const char *itemname = fpath + ftwbuf->base;
    printf("[+] processing: %s [%s] (%lu)\n", itemname, fpath, sb->st_size);

    inode_t *inode = flist_process_file(itemname, sb, fpath, currentdir);
    directory_appends_inode(currentdir, inode);

    // this is maybe an empty directory, we don't know
    // in doubt, let's call lookup in order to create
    // entry if it doesn't exists
    if(inode->type == INODE_DIRECTORY)
        directory_lookup(rootdir, inode->fullpath);

    // if relpath is empty, we are doing some stuff
    // on the virtual root, let's reset the currentdir
    // since next call can be a deep directory somewhere else
    //
    // we set nftw to parse directories first, which mean we always goes
    // as deep as possible on each call
    if(strlen(relpath) == 0) {
        printf("[+] touching virtual root directory, reseting current directory\n");
        currentdir = NULL;
    }

    free(relpath);

    return 0;
}

int flist_create(database_t *database, const char *root) {
    printf("[+] preparing flist for: %s\n", root);

    if(!(rootdir = directory_create("", "")))
        return 1;

    printf("[+] building database\n");
    if(nftw(root, flist_create_cb, 512, FTW_DEPTH | FTW_PHYS))
        diep("nftw");

    printf("===================================\n");
    directory_dumps(rootdir);

    printf("===================================\n");
    printf("[+] building capnp from memory tree\n");
    directory_tree_capn(rootdir);

    printf("[+] recursivly freeing directory tree\n");
    directory_tree_free(rootdir);

    return 0;
}
