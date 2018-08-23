#define _DEFAULT_SOURCE
#define _GNU_SOURCE
#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <time.h>
#include <ftw.h>
#include <unistd.h>
#include <blake2.h>
#include <linux/limits.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <sys/sysmacros.h>
#include <regex.h>
#include "libflist.h"
#include "verbose.h"
#include "database.h"
#include "flist.capnp.h"

#define FLIST_WRITE_FULLDUMP

// hardcoded version of the 0-hub
const char *excluderstr[] = {
    "\\.pyc$",
    ".*__pycache__",
};

regex_t excluders[sizeof(excluderstr) / sizeof(char *)] = {0};

void excluders_init() {
    int errcode;
    char errstr[1024];

    for(size_t i = 0; i < sizeof(excluderstr) / sizeof(char *); i++) {
        if((errcode = regcomp(&excluders[i], excluderstr[i], REG_NOSUB | REG_EXTENDED))) {
            regerror(errcode, &excluders[i], errstr, sizeof(errstr));
            dies(errstr);
        }
    }
}

void excluders_free() {
    for(size_t i = 0; i < sizeof(excluderstr) / sizeof(char *); i++) {
        regfree(&excluders[i]);
    }
}

int excluders_matches(const char *input) {
    // allows empty strings
    if(strlen(input) == 0)
        return 0;

    for(size_t i = 0; i < sizeof(excluderstr) / sizeof(char *); i++) {
        if(regexec(&excluders[i], input, 0, NULL, 0) != REG_NOMATCH)
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

typedef struct flist_write_global_t {
    char *root;
    flist_db_t *database;
    flist_backend_t *backend;
    dirnode_t *rootdir;
    dirnode_t *currentdir;
    flist_stats_t stats;

} flist_write_global_t;

// WARNING:
//   we use nftw to walk over the filesystem
//   this method doesn't allow to pass custom variable
//   to the callback, we __need__ to use a global variable
//   to reach data from the callback
//
//   this global variable (only accessible from this file) is
//   there for that purpose **only**
//
//   THIS MAKE ANY CALL TO FLIST __CREATION__
//   NOT THREADS SAFE
//
//   maybe for a next release, some lock will be added
//   to ensure threads are not breaking but for now
//   just assume all of this are NOT THREADS SAFE.
static flist_write_global_t globaldata = {
    .root = NULL,
    .database = NULL,
    .backend = NULL,
    .rootdir = NULL,
    .currentdir = NULL,
};

//
//

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
    unsigned char hash[ACLLENGTH];

    if(blake2b(hash, key, "", ACLLENGTH, strlen(key), 0) < 0)
        dies("blake2 acl error");

    char *hashkey = libflist_hashhex(hash, ACLLENGTH);
    free(key);

    return hashkey;
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

    return directory;
}

void dirnode_free(dirnode_t *directory) {
    inode_acl_free(&directory->acl);

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
    free(inode->subdirkey);
    free(inode);
}

void inode_dumps(inode_t *inode, dirnode_t *rootdir) {
    debug("[+] inode: rootdir: 0x%p\n", rootdir);
    debug("[+] inode: %s: %s/%s\n", inode_type_str[inode->type], rootdir->fullpath, inode->name);

    debug("[+] inode:   size: %lu bytes (%.2f MB)\n", inode->size, inode->size / (1024 * 1024.0));
    debug("[+] inode:   ctime: %lu, mtime: %lu\n", inode->creation, inode->modification);
    debug("[+] inode:   user: %s, group: %s\n", inode->acl.uname, inode->acl.gname);
    debug("[+] inode:   mode: %o\n", inode->acl.mode);

    if(inode->type == INODE_LINK)
        debug("[+] inode:   symlink: %s\n", inode->link);

    if(inode->type == INODE_SPECIAL)
        debug("[+] inode:   special: %s\n", inode->sdata);
}

dirnode_t *dirnode_appends_inode(dirnode_t *root, inode_t *inode) {
    inode->next = NULL;

    if(!root->inode_list)
        root->inode_list = inode;

    if(root->inode_last)
        root->inode_last->next = inode;

    root->inode_last = inode;
    root->inode_length += 1;

    return root;
}

dirnode_t *dirnode_appends_dirnode(dirnode_t *root, dirnode_t *dir) {
    dir->next = NULL;

    if(!root->dir_list)
        root->dir_list = dir;

    if(root->dir_last)
        root->dir_last->next = dir;

    root->dir_last = dir;
    root->dir_length += 1;

    return root;
}

static dirnode_t *dirnode_search(dirnode_t *root, char *dirname) {
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

static dirnode_t *dirnode_lookup_step(dirnode_t *root, char *token, char *incrpath) {
    dirnode_t *subdir = NULL;

    // incrpath can be safely strcat, the memory allocated
    // is the same length of the fullpath
    strcat(incrpath, token);
    strcat(incrpath, "/");

    // looking for that directory
    // if it doesn't exists, create a new one
    if(!(subdir = dirnode_search(root, token))) {
        if(!(subdir = dirnode_create(incrpath, token)))
            return NULL;

        dirnode_appends_dirnode(root, subdir);
    }

    // looking for new token
    if(!(token = strtok(NULL, "/")))
        return subdir;

    return dirnode_lookup_step(subdir, token, incrpath);
}

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

#ifdef FLIST_WRITE_FULLDUMP
static void dirnode_dumps(dirnode_t *root) {
    debug("[+] directory: <%s> [fullpath: /%s]\n", root->name, root->fullpath);
    debug("[+]   subdirectories: %lu\n", root->dir_length);
    debug("[+]   inodes: %lu\n", root->inode_length);

    for(dirnode_t *source = root->dir_list; source; source = source->next) {
        dirnode_dumps(source);

        if(source->acl.key == NULL)
            warns("directory aclkey not set");
    }

    for(inode_t *inode = root->inode_list; inode; inode = inode->next) {
        inode_dumps(inode, root);

        if(inode->acl.key == NULL)
            warns("inode aclkey not set");
    }
}
#endif

#if 0
#endif

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

void inode_acl_persist(flist_db_t *database, acl_t *acl) {
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


// flist_json_t jsonresponse = {0};

void dirnode_tree_capn(dirnode_t *root, flist_db_t *database, dirnode_t *parent, flist_backend_t *backend) {
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
        .aclkey = chars_to_text(root->acl.key),
        .modificationTime = root->modification,
        .creationTime = root->creation,
    };

    inode_acl_persist(database, &root->acl);

    // populating contents
    int index = 0;
    for(inode_t *inode = root->inode_list; inode; inode = inode->next, index += 1) {
        struct Inode target;

        debug("[+]   populate inode: <%s>\n", inode->name);

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

            globaldata.stats.directory += 1;
        }

        if(inode->type == INODE_LINK) {
            struct Link l = {
                .target = chars_to_text(inode->link),
            };

            target.attributes.link = new_Link(cs);
            write_Link(&l, target.attributes.link);

            globaldata.stats.symlink += 1;
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

            globaldata.stats.special += 1;
        }

        if(inode->type == INODE_FILE) {
            struct File f = {
                .blockSize = 128, // ignored, hardcoded value
            };

            // upload non-empty files
            if(inode->size && backend) {
                flist_chunks_t *chunks;

                if(!(chunks = libflist_backend_upload_inode(backend, root->fullpath, inode->name))) {
                    globaldata.stats.failure += 1;
                    continue;
                }

                f.blocks = new_FileBlock_list(cs, chunks->length);

                for(size_t i = 0; i < chunks->length; i++) {
                    struct FileBlock block;
                    flist_chunk_t *chk = chunks->chunks[i];

                    block.hash.p = capn_databinary(cs, (char *) chk->id.data, chk->id.length);
                    block.key.p = capn_databinary(cs, (char *) chk->cipher.data, chk->cipher.length);

                    set_FileBlock(&block, f.blocks, i);
                }

                // chunks_free(chunks);
                // FIXME: free
            }

            target.attributes.file = new_File(cs);
            write_File(&f, target.attributes.file);

            globaldata.stats.regular += 1;
        }

        set_Inode(&target, dir.contents, index);
        inode_acl_persist(database, &inode->acl);
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
    if(database->sset(database, root->hashkey, buffer, sz))
        dies("database error");

    free(buffer);

    // walking over the sub-directories
    for(dirnode_t *subdir = root->dir_list; subdir; subdir = subdir->next)
        dirnode_tree_capn(subdir, database, root, backend);
}

//
// walker
//
static inode_t *flist_process_file(const char *iname, const struct stat *sb, const char *realpath, dirnode_t *parent) {
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
        inode->subdirkey = libflist_path_key(vpath);
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

static int flist_dirnode_metadata(dirnode_t *root, const struct stat *sb) {
    debug("[+] fixing root directory: %s\n", root->fullpath);

    root->creation = sb->st_ctime;
    root->modification = sb->st_mtime;

    // skipping already set acl
    if(!root->acl.key)
        root->acl = inode_acl(sb);

    return 0;
}

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

static int flist_create_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    char *relpath = NULL;
    char *parent = NULL;

    // checking if entry is rejected by exclude filter
    // if exclude matches, we don't parse this entry at all
    if(excluders_matches(fpath)) {
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
        inode_t *inode = flist_process_file(itemname, sb, fpath, parentnode);
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

    inode_t *inode = flist_process_file(itemname, sb, fpath, globaldata.currentdir);
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

flist_stats_t *flist_create(flist_db_t *database, const char *root, flist_backend_t *backend) {
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
    excluders_init();

    if(!(globaldata.rootdir = dirnode_create("", "")))
        return NULL;

    // FIXME: thread safe ? add one global lock ?

    globaldata.root = flist_rootpath_clean(root);
    globaldata.database = database;
    globaldata.backend = backend;

    memset(&globaldata.stats, 0x00, sizeof(flist_stats_t));

    debug("[+] building database\n");
    if(nftw(root, flist_create_cb, 512, FTW_DEPTH | FTW_PHYS))
        diep("nftw");

    // FIXME: release the global lock ?

#ifdef FLIST_WRITE_FULLDUMP
    debug("===================================\n");
    dirnode_dumps(globaldata.rootdir);
#endif

    debug("[+] =========================================\n");
    debug("[+] building capnp from memory tree\n");
    dirnode_tree_capn(globaldata.rootdir, database, globaldata.rootdir, backend);

    debug("[+] recursivly freeing directory tree\n");
    dirnode_tree_free(globaldata.rootdir);
    // upload_inode_flush(); // FIXME

    if(backend)
        libflist_backend_free(backend);

    free(globaldata.root);
    excluders_free();

    // convert local stats
    stats = libflist_bufdup(&globaldata.stats, sizeof(flist_stats_t));

    return stats;
}
