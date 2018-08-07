#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sqlite3.h>
#include "database.h"
#include "database_sqlite.h"
#include "flister.h"

static int database_sqlite_build(database_sqlite_t *db) {
    char *query = "CREATE TABLE entries (key VARCHAR(64) PRIMARY KEY, value BLOB);";
    value_t value;

    if(sqlite3_prepare_v2(db->db, query, -1, (sqlite3_stmt **) &value.handler, NULL) != SQLITE_OK) {
        warndb("build: sqlite3_prepare_v2", sqlite3_errmsg(db->db));
        return 1;
    }

    if(sqlite3_step(value.handler) != SQLITE_DONE) {
        warndb("build: sqlite3_step", sqlite3_errmsg(db->db));
        return 1;
    }

    sqlite3_finalize(value.handler);

    sqlite3_exec(db->db, "BEGIN TRANSACTION;", NULL, NULL, NULL);

    return 0;
}

static database_sqlite_t *database_sqlite_root_init(database_t *database) {
    database_sqlite_t *db = (database_sqlite_t *) database->handler;

    if(sqlite3_open(db->filename, &db->db)) {
        warndb("sqlite3_open", sqlite3_errmsg(db->db));
        return NULL;
    }

    // pre-compute query
    char *select_query = "SELECT value FROM entries WHERE key = ?1";
    char *insert_query = "INSERT INTO entries (key, value) VALUES (?1, ?2)";

    if(sqlite3_prepare_v2(db->db, select_query, -1, &db->select, 0) != SQLITE_OK) {
        warndb("prepare: sqlite3_prepare_v2", sqlite3_errmsg(db->db));
        return NULL;
    }

    if(sqlite3_prepare_v2(db->db, insert_query, -1, &db->insert, 0) != SQLITE_OK) {
        warndb("prepare: sqlite3_prepare_v2", sqlite3_errmsg(db->db));
        return NULL;
    }

    return db;
}

static database_t *database_sqlite_open(database_t *database) {
    database->handler = database_sqlite_root_init(database);
    return database;
}

static database_t *database_sqlite_create(database_t *database) {
    database->handler = database_sqlite_root_init(database);

    if(database_sqlite_build(database->handler))
        return NULL;

    return database;
}

static void database_sqlite_close(database_t *database) {
    database_sqlite_t *db = (database_sqlite_t *) database->handler;

    if(db->updated) {
        sqlite3_exec(db->db, "END TRANSACTION;", NULL, NULL, NULL);
        sqlite3_exec(db->db, "VACUUM;", NULL, NULL, NULL);
    }

    sqlite3_close(db->db);
    free(db);
}

static value_t *database_sqlite_get(database_t *database, char *key) {
    database_sqlite_t *db = (database_sqlite_t *) database->handler;
    value_t *value;

    if(!(value = calloc(1, sizeof(value_t)))) {
        diep("malloc");
        return NULL;
    }

    sqlite3_reset(db->select);
    sqlite3_bind_text(db->select, 1, key, -1, SQLITE_STATIC);

    int data = sqlite3_step(db->select);

    if(data == SQLITE_DONE)
        return value;

    if(data == SQLITE_ROW) {
        value->data = (void *) sqlite3_column_blob(db->select, 0);
        value->length = sqlite3_column_bytes(db->select, 0);
        return value;
    }

    warndb("get: sqlite3_step", sqlite3_errmsg(db->db));
    return value;
}

static void database_sqlite_clean(value_t *value) {
    // database_sqlite_t *db = (database_sqlite_t *) database->handler;

    // sqlite3_finalize(value->stmt);
    free(value);
}

static int database_sqlite_set(database_t *database, char *key, uint8_t *payload, size_t length) {
    database_sqlite_t *db = (database_sqlite_t *) database->handler;

    sqlite3_reset(db->insert);
    sqlite3_bind_text(db->insert, 1, key, -1, SQLITE_STATIC);
    sqlite3_bind_blob(db->insert, 2, payload, length, SQLITE_STATIC);

    if(sqlite3_step(db->insert) != SQLITE_DONE)
        warndb("set: sqlite3_step", sqlite3_errmsg(db->db));

    db->updated = 1;

    return 0;
}

// poor implementation of exists
static int database_sqlite_exists(database_t *database, char *key) {
    int retval = 0;

    value_t *value = database_sqlite_get(database, key);
    if(value->data)
        retval = 1;

    database_sqlite_clean(value);
    return retval;
}

// public sqlite function initializer
database_t *database_sqlite_init(char *rootpath) {
    database_t *db;

    // allocate generic database object
    if(!(db = malloc(sizeof(database_t))))
        return NULL;

    // set our custom sqlite database handler
    if(!(db->handler = malloc(sizeof(database_sqlite_t)))) {
        free(db);
        return NULL;
    }

    database_sqlite_t *handler = (database_sqlite_t *) db->handler;

    // setting the sqlite handler
    handler->root = rootpath;
    handler->updated = 0;

    if(asprintf(&handler->filename, "%s/flistdb.sqlite3", rootpath) < 0) {
        diep("asprintf");
        free(db->handler);
        free(db);
        return NULL;
    }

    // setting global db
    db->type = "SQLITE3";

    // fillin handlers
    db->open = database_sqlite_open;
    db->create = database_sqlite_create;
    db->close = database_sqlite_close;
    db->get = database_sqlite_get;
    db->set = database_sqlite_set;
    db->exists = database_sqlite_exists;
    db->clean = database_sqlite_clean;

    return db;
}
