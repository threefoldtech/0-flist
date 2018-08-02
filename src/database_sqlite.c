#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <sqlite3.h>
#include "database.h"
#include "database_sqlite.h"
#include "flister.h"

#ifdef DATABASE_BACKEND_SQLITE

#define KEYLENGTH 32

static void warndb(char *source, const char *str) {
    fprintf(stderr, "[-] database: %s: %s\n", source, str);
}

static void diedb(char *source, const char *str) {
    warndb(source, str);
    exit(EXIT_FAILURE);
}

int database_build(database_t *database) {
    char *query = "CREATE TABLE entries (key VARCHAR(64) PRIMARY KEY, value BLOB);";
    value_t value;

    if(sqlite3_prepare_v2(database->db, query, -1, &value.stmt, NULL) != SQLITE_OK)
        diedb("build: sqlite3_prepare_v2", sqlite3_errmsg(database->db));

    if(sqlite3_step(value.stmt) != SQLITE_DONE)
        diedb("build: sqlite3_step", sqlite3_errmsg(database->db));

    sqlite3_finalize(value.stmt);

    sqlite3_exec(database->db, "BEGIN TRANSACTION;", NULL, NULL, NULL);

    return 0;
}

static database_t *database_init(char *root, int create) {
    database_t *database;

    if(!(database = malloc(sizeof(database_t))))
        diep("malloc");

    database->updated = 0;
    database->root = root;

    if(asprintf(&database->filename, "%s/flistdb.sqlite3", root) < 0)
        diep("asprintf");

    if(sqlite3_open(database->filename, &database->db))
        diedb("sqlite3_open", sqlite3_errmsg(database->db));

    if(create)
        database_build(database);

    // pre-compute query
    char *select_query = "SELECT value FROM entries WHERE key = ?1";
    char *insert_query = "INSERT INTO entries (key, value) VALUES (?1, ?2)";

    if(sqlite3_prepare_v2(database->db, select_query, -1, &database->select, 0) != SQLITE_OK)
        diedb("prepare: sqlite3_prepare_v2", sqlite3_errmsg(database->db));

    if(sqlite3_prepare_v2(database->db, insert_query, -1, &database->insert, 0) != SQLITE_OK)
        diedb("prepare: sqlite3_prepare_v2", sqlite3_errmsg(database->db));

    return database;
}

database_t *database_open(char *root) {
    return database_init(root, 0);
}

database_t *database_create(char *root) {
    return database_init(root, 1);
}

void database_close(database_t *database) {
    if(database->updated) {
        sqlite3_exec(database->db, "END TRANSACTION;", NULL, NULL, NULL);
        sqlite3_exec(database->db, "CREATE INDEX entries_index ON entries (key);", NULL, NULL, NULL);
        sqlite3_exec(database->db, "VACUUM;", NULL, NULL, NULL);
    }

    sqlite3_close(database->db);
    free(database);
}

value_t *database_get(database_t *database, const char *key) {
    value_t *value;

    if(!(value = calloc(1, sizeof(value_t))))
        diep("malloc");

    /*
    if(sqlite3_prepare_v2(database->db, query, -1, &value->stmt, 0) != SQLITE_OK)
        diedb("get: sqlite3_prepare_v2", sqlite3_errmsg(database->db));
    */

    sqlite3_reset(database->select);
    sqlite3_bind_text(database->select, 1, key, -1, SQLITE_STATIC);

    int data = sqlite3_step(database->select);

    if(data == SQLITE_DONE)
        return value;

    if(data == SQLITE_ROW) {
        value->data = (void *) sqlite3_column_blob(database->select, 0);
        value->length = sqlite3_column_bytes(database->select, 0);
        return value;
    }


    diedb("get: sqlite3_step", sqlite3_errmsg(database->db));
    return value;
}

value_t *database_value_free(value_t *value) {
    // sqlite3_finalize(value->stmt);
    free(value);

    return NULL;
}

int database_set(database_t *database, const char *key, const unsigned char *payload, size_t length) {
    value_t *value;

    if(!(value = calloc(1, sizeof(value_t))))
        diep("malloc");

    /*
    if(sqlite3_prepare_v2(database->db, query, -1, &value->stmt, 0) != SQLITE_OK)
        diedb("set: sqlite3_prepare_v2", sqlite3_errmsg(database->db));
    */

    sqlite3_reset(database->insert);
    sqlite3_bind_text(database->insert, 1, key, -1, SQLITE_STATIC);
    sqlite3_bind_blob(database->insert, 2, payload, length, SQLITE_STATIC);

    if(sqlite3_step(database->insert) != SQLITE_DONE)
        diedb("set: sqlite3_step", sqlite3_errmsg(database->db));

    database->updated = 1;

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
