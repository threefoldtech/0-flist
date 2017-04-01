#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <rocksdb/c.h>
#include "database.h"
#include "flister.h"

#define KEYLENGTH 32

static void warndb(char *str) {
    fprintf(stderr, "[-] database: %s\n", str);
}

static void diedb(char *str) {
    warndb(str);
    exit(EXIT_FAILURE);
}

database_t *database_open(char *root) {
    database_t *database;
    char *err = NULL;

    if(!(database = malloc(sizeof(database_t))))
        diep("malloc");

    // initializing database struct
    database->root = root;
    database->options = rocksdb_options_create();

    // initializing rocksdb options
    long cpus = sysconf(_SC_NPROCESSORS_ONLN);
    rocksdb_options_increase_parallelism(database->options, (int)(cpus));
    rocksdb_options_optimize_level_style_compaction(database->options, 0);

    // opening database and options handler
    database->db = rocksdb_open(database->options, root, &err);
    database->readoptions = rocksdb_readoptions_create();

    if(err)
        diedb(err);

    return database;
}

void database_close(database_t *database) {
    // closing handlers, database and freeing struct
    rocksdb_options_destroy(database->options);
    rocksdb_readoptions_destroy(database->readoptions);
    rocksdb_close(database->db);
    free(database);
}

value_t *database_get(database_t *database, const char *key) {
    value_t *value;
    char *err = NULL;

    if(!(value = calloc(1, sizeof(value_t))))
        diep("malloc");

    value->data = rocksdb_get(database->db, database->readoptions, (char *) key, strlen(key), &value->length, &err);

    if(err)
        warndb(err);

    return value;
}

void database_value_free(value_t *value) {
    free(value->data);
    free(value);
}
