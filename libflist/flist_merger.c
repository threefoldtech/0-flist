#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <libgen.h>
#include "libflist.h"
#include "verbose.h"
#include "database.h"
#include "flist_read.h"
#include "flist_dirnode.h"
#include "flist_serial.h"

// #define FULLMERGE_DEBUG

#ifdef FULLMERGE_DEBUG
static void list_dirnode_inodes(dirnode_t *root) {
    debug("[+] libflist: merge: dirnode inodes count: %lu\n", root->inode_length);

    for(inode_t *inode = root->inode_list; inode; inode = inode->next)
        debug("[+] libflist: merge: dirnode inode: %s\n", inode->name);
}
#endif

// database is target output archive
// fulltree is a pointer of pointer, in case when the fulltree is NULL,
// we replace it with the new source, as first source
dirnode_t *libflist_merge(dirnode_t **fulltree, dirnode_t *source) {
    // if fulltree is NULL, this is the first
    // merging, we just returns the original dirnode tree
    // as the source one
    if(*fulltree == NULL) {
        debug("[+] merger: first flist, just installing this one as tree\n");
        *fulltree = source;
        return source;
    }

    // shortcut pointer
    dirnode_t *finaltree = *fulltree;
    debug("[+] merge: entering <%s> - <%s>\n", finaltree->name, source->name);

    // iterating over all the inodes found on this directory
    // if the inode doesn't exists yet on the finaltree, adding it
    // if it already exists, checking deeper what to do
    for(inode_t *inode = source->inode_list; inode; inode = inode->next) {
        debug("[+] merge: looking up for inode: <%s>\n", inode->name);
        inode_t *found = NULL;

        // looking up for this inode into the finaltree
        if(!(found = libflist_inode_search(finaltree, inode->name))) {
            debug("[+] merge: inode <%s> not found on fulltree, just adding it\n", inode->name);

            // duplucating the inode (to clean linked list pointer)
            inode_t *newcopy = inode_lazy_duplicate(inode);
            dirnode_lazy_appends_inode(finaltree, newcopy);

            // if it's a file, just let go to the next file
            if(inode->type != INODE_DIRECTORY)
                continue;

            debug("[+] merge: inode <%s> is a directory, looking up dirnode entry\n", inode->name);
            // this inode is a directory
            // let's find it's directory entry (dirnode) and adding it too
            dirnode_t *lookup;

            if(!(lookup = libflist_dirnode_search(source, inode->name)))
                return libflist_dies("merge: directory entry not found but inode exists, malformed\n");

            debug("[+] merge: dirnode found, appending it\n");
            dirnode_t *newdir = dirnode_lazy_duplicate(lookup);
            dirnode_appends_dirnode(finaltree, newdir);

            // jumping to the next entry
            continue;
        }

        // we found the inode on the finaltree
        // let's checking if it's a directory, if it's a directory, entering
        // this directory and looking up what do to inside
        debug("[+] merge: inode <%s> found on fulltree, checking deeper\n", inode->name);
        if(inode->type == INODE_DIRECTORY) {
            debug("[+] merge: inode <%s> is a directory, entering...\n", inode->name);

            // this inode is a directory
            // let's find it's directory entry (dirnode) and adding it too
            dirnode_t *dir_fulltree, *dir_source;

            if(!(dir_source = libflist_dirnode_search(source, inode->name)))
                return libflist_dies("merge: directory source entry not found but inode exists, malformed\n");

            if(!(dir_fulltree = libflist_dirnode_search(finaltree, inode->name)))
                return libflist_dies("merge: directory fulltree entry not found but inode exists, malformed\n");

            libflist_merge(&dir_fulltree, dir_source);

            // jumping to the next entry
            continue;
        }

        // it's a file, found on the finaltree, let's check if the file needs to be updated
        // FIXME. Collision, we should check timestamp to see which file we need to keep.
    }

    debug("[+] merge: directory fully scanned\n");
    return finaltree;
}

