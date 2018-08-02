#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include "database.h"

#ifdef DATABASE_BACKEND_ROCKSDB
#include <rocksdb/c.h>
#include "flister.h"

#define KEYLENGTH 32

static void warndb(char *str) {
    fprintf(stderr, "[-] database: %s\n", str);
}

static void diedb(char *str) {
    warndb(str);
    exit(EXIT_FAILURE);
}

static database_t *database_init(char *root, int create) {
    database_t *database;
    char *err = NULL;

    if(!(database = malloc(sizeof(database_t))))
        diep("malloc");

    // initializing database struct
    database->root = root;
    database->options = rocksdb_options_create();

    // initializing rocksdb options
    long cpus = sysconf(_SC_NPROCESSORS_ONLN);
    rocksdb_options_increase_parallelism(database->options, (int) cpus);
    rocksdb_options_optimize_level_style_compaction(database->options, 0);

    if(create)
        rocksdb_options_set_create_if_missing(database->options, 1);

    // 512 MB
    rocksdb_options_set_write_buffer_size(database->options, 512 * 1024 * 1024);

    // opening database and options handler
    database->db = rocksdb_open(database->options, root, &err);
    database->readoptions = rocksdb_readoptions_create();
    database->writeoptions = rocksdb_writeoptions_create();

    if(err)
        diedb(err);

    return database;
}

database_t *database_open(char *root) {
    return database_init(root, 0);
}

database_t *database_create(char *root) {
    return database_init(root, 1);
}

void database_close(database_t *database) {
    // closing handlers, database and freeing struct
    rocksdb_options_destroy(database->options);
    rocksdb_readoptions_destroy(database->readoptions);
    rocksdb_writeoptions_destroy(database->writeoptions);
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

value_t *database_value_free(value_t *value) {
    free(value->data);
    free(value);

    return NULL;
}

int database_set(database_t *database, const char *key, const unsigned char *payload, size_t length) {
    char *err = NULL;

    rocksdb_put(database->db, database->writeoptions, (char *) key, strlen(key), (char *) payload, length, &err);

    if(err) {
        warndb(err);
        return 1;
    }

    return 0;
}

// poor implementation of exists
int database_exists(database_t *database, const char *key) {
    int retval = 0;

    value_t *value = database_get(database, (char *) key);
    if(value->data)
        retval = 1;

    database_value_free(value);
    return retval;
}

#endif
