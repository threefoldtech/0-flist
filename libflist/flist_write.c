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
#include "flist_acl.h"
#include "flist_dirnode.h"
#include "flist_serial.h"

// #define FLIST_WRITE_FULLDUMP


#if 0
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
#endif

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
