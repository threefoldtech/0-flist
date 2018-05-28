#define _DEFAULT_SOURCE
#define _GNU_SOURCE
#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <ftw.h>
#include <unistd.h>
#include <rocksdb/c.h>
#include <blake2.h>
#include <linux/limits.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <sys/sysmacros.h>
#include <openssl/md5.h>
#include "flister.h"
#include "flist_write.h"
#include "flist.capnp.h"

#define KEYLENGTH 32

typedef struct acl_t {
    char *uname;     // username (user id if not found)
    char *gname;     // group name (group id if not found)
    uint16_t mode;   // integer file mode
    char *key;       // hash of the payload (dedupe in db)

} acl_t;

// link internal type to capnp type directly
// this make things easier later
typedef enum inode_type_t {
    INODE_DIRECTORY = Inode_attributes_dir,
    INODE_FILE = Inode_attributes_file,
    INODE_LINK = Inode_attributes_link,
    INODE_SPECIAL = Inode_attributes_special,

} inode_type_t;

typedef enum inode_special_t {
    SOCKET = Special_Type_socket,
    BLOCK = Special_Type_block,
    CHARDEV = Special_Type_chardev,
    FIFOPIPE = Special_Type_fifopipe,
    UNKNOWN = Special_Type_unknown,

} inode_special_t;

const char *inode_type_str[] = {
    "INODE_DIRECTORY",
    "INODE_FILE",
    "INODE_LINK",
    "INODE_SPECIAL",
};

typedef struct inode_t {
    char *name;       // relative inode name (filename)
    char *fullpath;   // full relative path
    size_t size;      // size in byte
    acl_t acl;        // access control

    inode_type_t type;     // internal file type
    time_t modification;   // modification time
    time_t creation;       // creation time

    char *subdirkey;       // for directory: directory target key
    inode_special_t stype; // for special: special type
    char *sdata;           // for special: special metadata
    char *link;            // for symlink: symlink target

    struct inode_t *next;

} inode_t;

typedef struct directory_t {
    struct inode_t *inode_list;
    struct inode_t *inode_last;
    size_t inode_length;

    struct directory_t *dir_list;
    struct directory_t *dir_last;
    size_t dir_length;

    char *fullpath;        // virtual full path
    char *name;            // directory name
    char *hashkey;         // internal hash (for db)

    acl_t acl;             // access control
    time_t creation;       // creation time
    time_t modification;   // modification time

    struct directory_t *next;

} directory_t;

//
// md5 helper
//
static char __hex[] = "0123456789abcdef";

static unsigned char *md5(const char *buffer, size_t length) {
    unsigned char *hash;
    MD5_CTX md5;

    if(!(hash = calloc(MD5_DIGEST_LENGTH, 1)))
        diep("calloc");

    MD5_Init(&md5);
    MD5_Update(&md5, buffer, length);
    MD5_Final(hash, &md5);

    return hash;
}

static char *hashhex(unsigned char *hash, int dlength) {
    char *buffer = calloc((dlength * 2) + 1, sizeof(char));
    char *writer = buffer;

    for(int i = 0, j = 0; i < dlength; i++, j += 2) {
        *writer++ = __hex[(hash[i] & 0xF0) >> 4];
        *writer++ = __hex[hash[i] & 0x0F];
    }

    return buffer;
}

char *inode_acl_key(acl_t *acl) {
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
    unsigned char *hash = md5(key, strlen(key));
    char *hashkey = hashhex(hash, MD5_DIGEST_LENGTH);
    free(key);
    free(hash);

    return hashkey;
}


static char *uidstr(struct passwd *passwd, uid_t uid) {
    char *target;

    // if username cannot be found
    // let use user id as username
    if(!passwd) {
        warnp("getpwuid");
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

acl_t inode_acl(const struct stat *sb) {
    acl_t acl;

    acl.mode = sb->st_mode;
    acl.uname = uidstr(getpwuid(sb->st_uid), sb->st_uid);
    acl.gname = gidstr(getgrgid(sb->st_gid), sb->st_gid);
    acl.key = inode_acl_key(&acl);

    return acl;
}

void inode_acl_free(acl_t *acl) {
    free(acl->uname);
    free(acl->gname);
    free(acl->key);
}

static char *path_key(const char *path) {
    uint8_t hash[KEYLENGTH];

    if(blake2b(hash, path, "", KEYLENGTH, strlen(path), 0) < 0)
        dies("blake2 error");

    return hashhex(hash, KEYLENGTH);
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

    directory->hashkey = path_key(directory->fullpath);

    return directory;
}

void directory_free(directory_t *directory) {
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

static directory_t *directory_lookup(directory_t *root, const char *fullpath) {
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

    for(directory_t *source = root->dir_list; source; source = source->next) {
        directory_dumps(source);

        if(source->acl.key == NULL)
            dies("directory aclkey not set");
    }

    for(inode_t *inode = root->inode_list; inode; inode = inode->next) {
        inode_dumps(inode, root);

        if(inode->acl.key == NULL)
            dies("inode aclkey not set");
    }
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

//
// capnp dumper
//
static capn_text chars_to_text(const char *chars) {
    return (capn_text) {
        .len = (int) strlen(chars),
        .str = chars,
        .seg = NULL,
    };
}

void inode_acl_persist(database_t *database, acl_t *acl) {
    if(database_exists(database, acl->key))
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

    printf("[+] writing acl into db: %s\n", acl->key);
    database_set(database, acl->key, buffer, sz);
}

void directory_tree_capn(directory_t *root, database_t *database, directory_t *parent) {
    struct capn c;
    capn_init_malloc(&c);
    capn_ptr cr = capn_root(&c);
    struct capn_segment *cs = cr.seg;

    printf("[+] populating directory: %s\n", root->fullpath);

    // creating this directory entry
    struct Dir dir = {
        .name = chars_to_text(root->name),
        .location = chars_to_text(root->fullpath),
        .contents = new_Inode_list(cs, root->inode_length),
        .parent = chars_to_text(parent->hashkey),
        .size = 4096,
        .aclkey = chars_to_text(root->acl.key),
        .modificationTime = root->modification,
        .creationTime = root->creation,
    };

    inode_acl_persist(database, &root->acl);

    // populating contents
    int index = 0;
    for(inode_t *inode = root->inode_list; inode; inode = inode->next) {
        struct Inode target;

        target.name = chars_to_text(inode->name);
        target.size = inode->size;
        target.attributes_which = inode->type;
        target.aclkey = chars_to_text(inode->acl.key);
        target.modificationTime = inode->modification;
        target.creationTime = inode->creation;

        if(inode->type == INODE_DIRECTORY) {
            struct SubDir sd = {
                .key = chars_to_text(inode->subdirkey),
            };

            target.attributes.dir = new_SubDir(cs);
            write_SubDir(&sd, target.attributes.dir);
        }

        if(inode->type == INODE_LINK) {
            struct Link l = {
                .target = chars_to_text(inode->link),
            };

            target.attributes.link = new_Link(cs);
            write_Link(&l, target.attributes.link);
        }

        if(inode->type == INODE_SPECIAL) {
            struct Special sp = {
                .type = inode->stype,
            };

            // see: https://github.com/opensourcerouting/c-capnproto/blob/master/lib/capnp_c.h#L196
            capn_list8 data = capn_new_list8(cs, 1);
            capn_setv8(data, 0, (uint8_t *) inode->sdata, strlen(inode->sdata));
            sp.data.p = data.p;

            target.attributes.special = new_Special(cs);
            write_Special(&sp, target.attributes.special);
        }

        if(inode->type == INODE_FILE) {
            struct File f = {
                .blockSize = 128, // ignored
                // FIXME: .blocks
            };

            target.attributes.file = new_File(cs);
            write_File(&f, target.attributes.file);
        }

        set_Inode(&target, dir.contents, index);
        inode_acl_persist(database, &inode->acl);

        index += 1;
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
    printf("[+] writing into db: %s\n", root->hashkey);
    database_set(database, root->hashkey, buffer, sz);

    // walking over the sub-directories
    for(directory_t *subdir = root->dir_list; subdir; subdir = subdir->next)
        directory_tree_capn(subdir, database, root);
}

//
// walker
//
static inode_t *flist_process_file(const char *iname, const struct stat *sb, const char *realpath, directory_t *parent) {
    inode_t *inode;

    char vpath[PATH_MAX];
    sprintf(vpath, "%s/%s", parent->fullpath, iname);

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
        inode->subdirkey = path_key(vpath);
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

static int flist_directory_metadata(directory_t *root, const struct stat *sb) {
    printf("[+] fixing root directory: %s\n", root->fullpath);

    root->creation = sb->st_ctime;
    root->modification = sb->st_mtime;
    root->acl = inode_acl(sb);

    return 0;
}

static int flist_create_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    ssize_t length = ftwbuf->base - settings.rootlen - 1;
    char *relpath;

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

    // flag FTW_DP is set when all subdirectories was
    // parsed, now we know the remaining contents of this directory
    // are only files, let reset currentdir, this will be looked up again
    // later, we can't ensure the currentdir is still the relevant directory
    // since it can be one level up
    //
    // we sets everything for underlaying directories but nothing for the directory
    // itself (like it's acl, etc.) let set this now here
    if(typeflag == FTW_DP) {
        const char *virtual = fpath + settings.rootlen + 1;
        printf("[+] all subdirectories done for: %s\n", virtual);

        directory_t *myself = directory_lookup(rootdir, virtual);
        flist_directory_metadata(myself, sb);

        // clear global currentdir
        // will be reset later with right options
        currentdir = NULL;
    }

    // if we reach level 0, we are processing the root directory
    // this directory should not be proceed since we have a virtual root
    // if we are here, we are all done with the walking process
    if(ftwbuf->level == 0) {
        printf("======== ROOT PATH, WE ARE DONE ========\n");
        return flist_directory_metadata(rootdir, sb);
    }

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
    if(inode->type == INODE_DIRECTORY) {
        directory_t *check = directory_lookup(rootdir, inode->fullpath);

        // if the directory is never reached anyway
        // metadata won't be set anywhere else
        flist_directory_metadata(check, sb);
    }

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
    directory_tree_capn(rootdir, database, rootdir);

    printf("[+] recursivly freeing directory tree\n");
    directory_tree_free(rootdir);

    return 0;
}
