#define _DEFAULT_SOURCE
#define _GNU_SOURCE
#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include "libflist.h"
#include "verbose.h"

static const char *inode_type_str[] = {
    "INODE_DIRECTORY",
    "INODE_FILE",
    "INODE_LINK",
    "INODE_SPECIAL",
};

void libflist_inode_dumps(inode_t *inode, dirnode_t *rootdir) {
    debug("[+] inode: rootdir: 0x%p\n", rootdir);
    debug("[+] inode: %s: %s/%s\n", inode_type_str[inode->type], rootdir->fullpath, inode->name);

    debug("[+] inode:   size: %lu bytes (%.2f MB)\n", inode->size, inode->size / (1024 * 1024.0));
    debug("[+] inode:   ctime: %lu, mtime: %lu\n", inode->creation, inode->modification);

    if(inode->racl) {
        debug("[+] inode:   user: %s, group: %s\n", inode->racl->uname, inode->racl->gname);
        debug("[+] inode:   mode: %o\n", inode->racl->mode);
    }

    if(inode->acl.key) {
        debug("[+] inode:   user: %s, group: %s\n", inode->acl.uname, inode->acl.gname);
        debug("[+] inode:   mode: %o\n", inode->acl.mode);
    }

    if(inode->type == INODE_LINK)
        debug("[+] inode:   symlink: %s\n", inode->link);

    if(inode->type == INODE_SPECIAL)
        debug("[+] inode:   special: %s\n", inode->sdata);
}

void libflist_dirnode_dumps(dirnode_t *root) {
    debug("[+] directory: <%s> [fullpath: /%s]\n", root->name, root->fullpath);
    debug("[+]   subdirectories: %lu\n", root->dir_length);
    debug("[+]   inodes: %lu\n", root->inode_length);

    for(dirnode_t *source = root->dir_list; source; source = source->next) {
        libflist_dirnode_dumps(source);

        if(source->acl.key == NULL && source->racl == NULL)
            warns("directory aclkey not set");
    }

    for(inode_t *inode = root->inode_list; inode; inode = inode->next) {
        libflist_inode_dumps(inode, root);

        if(inode->acl.key == NULL && inode->racl == NULL)
            warns("inode aclkey not set");
    }
}
