#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include "flister.h"
#include "database.h"
#include "backend.h"
#include "flist.capnp.h"
#include "flist_read.h"
#include "flist_write.h"

// database is target output archive
int flist_merger(database_t *database, merge_list_t *merge) {
    (void) database;

    for(size_t i = 0; i < merge->length; i++) {
        printf("[+] merging source: %s\n", merge->sources[i]);
    }

    return 0;
}
