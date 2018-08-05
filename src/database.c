#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <unistd.h>
#include "database.h"
#include "database_redis.h"
#include "database_sqlite.h"

void warndb(char *source, const char *str) {
    fprintf(stderr, "[-] database: %s: %s\n", source, str);
}

void diedb(char *source, const char *str) {
    warndb(source, str);
    exit(EXIT_FAILURE);
}

database_t *database_init(database_type_t type) {
    if(type == SQLITE3)
        return database_sqlite_init();

    return NULL;
}
