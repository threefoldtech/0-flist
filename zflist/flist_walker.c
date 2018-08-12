#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include "libflist.h"
#include "database.h"
#include "flist.capnp.h"
#include "flist_walker.h"
#include "zflist.h"

int flist_walk_directory(walker_t *walker, const char *_key, const char *_fullpath) {
    char *key = (char *) _key;
    char *fullpath = (char *) _fullpath;
    directory_t *dir;

    if(!(dir = flist_directory_get(walker->database, key, fullpath)))
        return 1;

    // walking over this directory
    if(walker->callback(walker, dir))
        return 1;

    // cleaning this directory
    flist_directory_close(walker->database, dir);

    return 0;
}


