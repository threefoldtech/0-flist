#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sqlite3.h>
#include "libflist.h"
#include "verbose.h"
#include "database.h"
#include "database_sqlite.h"

static int database_sqlite_build(database_sqlite_t *db) {
    char *query = "CREATE TABLE entries (key VARCHAR(64) PRIMARY KEY, value BLOB);";
    value_t value;

    if(sqlite3_prepare_v2(db->db, query, -1, (sqlite3_stmt **) &value.handler, NULL) != SQLITE_OK) {
        libflist_set_error("create: sqlite3_prepare_v2: %s", sqlite3_errmsg(db->db));
        return 1;
    }

    if(sqlite3_step(value.handler) != SQLITE_DONE) {
        libflist_set_error("create: sqlite3_step: %s", sqlite3_errmsg(db->db));
        return 1;
    }

    sqlite3_finalize(value.handler);

    // if the database is opened in creation mode
    // it's probably to do motification
    //
    // we prepare one transaction to do everything in batch
    // this could not be always what the user wants, this will
    // maybe move to some external call later
    sqlite3_exec(db->db, "BEGIN TRANSACTION;", NULL, NULL, NULL);

    return 0;
}

static int database_sqlite_optimize(flist_db_t *database) {
    database_sqlite_t *db = (database_sqlite_t *) database->handler;

    // pre-compute query
    // preparing statements we will reuse all the way
    // since we only do like key-value store with the database
    // we only do the same GET and the same SET/INSERT all the time
    // reusing the same prepared statement improve hugely performances
    // and memory usage
    char *select_query = "SELECT value FROM entries WHERE key = ?1";
    char *insert_query = "INSERT INTO entries (key, value) VALUES (?1, ?2)";
    char *delete_query = "DELETE FROM entries WHERE key = ?1";

    if(sqlite3_prepare_v2(db->db, select_query, -1, &db->select, 0) != SQLITE_OK) {
        libflist_set_error("sqlite3_prepare_v2: %s", sqlite3_errmsg(db->db));
        return 1;
    }

    if(sqlite3_prepare_v2(db->db, insert_query, -1, &db->insert, 0) != SQLITE_OK) {
        libflist_set_error("sqlite3_prepare_v2: %s", sqlite3_errmsg(db->db));
        return 1;
    }

    if(sqlite3_prepare_v2(db->db, delete_query, -1, &db->delete, 0) != SQLITE_OK) {
        libflist_set_error("sqlite3_prepare_v2: %s", sqlite3_errmsg(db->db));
        return 1;
    }


    return 0;
}

static database_sqlite_t *database_sqlite_root_init(flist_db_t *database) {
    database_sqlite_t *db = (database_sqlite_t *) database->handler;

    if(sqlite3_open(db->filename, &db->db)) {
        libflist_set_error("sqlite3_open: %s", sqlite3_errmsg(db->db));
        return NULL;
    }

    return db;
}

static flist_db_t *database_sqlite_open(flist_db_t *database) {
    database->handler = database_sqlite_root_init(database);
    database_sqlite_t *db = (database_sqlite_t *) database->handler;

    if(db->select == NULL)
        database_sqlite_optimize(database);

    return database;
}

static flist_db_t *database_sqlite_create(flist_db_t *database) {
    database->handler = database_sqlite_root_init(database);
    database_sqlite_t *db = (database_sqlite_t *) database->handler;

    if(database_sqlite_build(db))
        return NULL;

    if(db->select == NULL)
        database_sqlite_optimize(database);

    return database;
}

static void database_sqlite_close(flist_db_t *database) {
    database_sqlite_t *db = (database_sqlite_t *) database->handler;

    if(db->updated) {
        sqlite3_exec(db->db, "END TRANSACTION;", NULL, NULL, NULL);
        sqlite3_exec(db->db, "VACUUM;", NULL, NULL, NULL);
    }

    // closing prepared statements
    sqlite3_finalize(db->select);
    sqlite3_finalize(db->insert);

    // closing database
    sqlite3_close(db->db);

    // freeing handler
    free(db->filename);
    free(db);

    // freeing global database object
    free(database);
}

static value_t *database_sqlite_get(flist_db_t *database, uint8_t *key, size_t keylen) {
    database_sqlite_t *db = (database_sqlite_t *) database->handler;
    value_t *value;

    if(!(value = calloc(1, sizeof(value_t)))) {
        diep("malloc");
        return NULL;
    }

    sqlite3_reset(db->select);
    sqlite3_bind_text(db->select, 1, (char *) key, keylen, SQLITE_STATIC);

    int data = sqlite3_step(db->select);

    if(data == SQLITE_DONE)
        return value;

    if(data == SQLITE_ROW) {
        value->data = (void *) sqlite3_column_blob(db->select, 0);
        value->length = sqlite3_column_bytes(db->select, 0);
        return value;
    }

    libflist_set_error("get: sqlite3_step: %s", sqlite3_errmsg(db->db));
    return value;
}

static value_t *database_sqlite_sget(flist_db_t *database, char *key) {
    return database_sqlite_get(database, (uint8_t *) key, strlen(key));
}

static void database_sqlite_clean(value_t *value) {
    // database_sqlite_t *db = (database_sqlite_t *) database->handler;

    // sqlite3_finalize(value->stmt);
    free(value);
}

static int database_sqlite_set(flist_db_t *database, uint8_t *key, size_t keylen, uint8_t *payload, size_t length) {
    database_sqlite_t *db = (database_sqlite_t *) database->handler;

    sqlite3_reset(db->insert);
    sqlite3_bind_text(db->insert, 1, (char *) key, keylen, SQLITE_STATIC);
    sqlite3_bind_blob(db->insert, 2, payload, length, SQLITE_STATIC);

    if(sqlite3_step(db->insert) != SQLITE_DONE) {
        libflist_set_error("set: sqlite3_step: %s", sqlite3_errmsg(db->db));
        return 1;
    }

    db->updated = 1;

    return 0;
}

static int database_sqlite_sset(flist_db_t *database, char *key, uint8_t *payload, size_t length) {
    return database_sqlite_set(database, (uint8_t *) key, strlen(key), payload, length);
}

static int database_sqlite_del(flist_db_t *database, uint8_t *key, size_t keylen) {
    database_sqlite_t *db = (database_sqlite_t *) database->handler;

    sqlite3_reset(db->delete);
    sqlite3_bind_text(db->delete, 1, (char *) key, keylen, SQLITE_STATIC);

    if(sqlite3_step(db->delete) != SQLITE_DONE) {
        libflist_set_error("del: sqlite3_step: %s", sqlite3_errmsg(db->db));
        return 1;
    }

    db->updated = 1;

    return 0;
}

static int database_sqlite_sdel(flist_db_t *database, char *key) {
    return database_sqlite_del(database, (uint8_t *) key, strlen(key));
}


// poor implementation of exists
static int database_sqlite_exists(flist_db_t *database, uint8_t *key, size_t keylen) {
    int retval = 0;

    value_t *value = database_sqlite_get(database, key, keylen);
    if(value->data)
        retval = 1;

    database_sqlite_clean(value);
    return retval;
}

static int database_sqlite_sexists(flist_db_t *database, char *key) {
    return database_sqlite_exists(database, (uint8_t *) key, strlen(key));
}

// public sqlite function initializer
flist_db_t *libflist_db_sqlite_init(char *rootpath) {
    flist_db_t *db;

    // allocate generic database object
    if(!(db = malloc(sizeof(flist_db_t))))
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

    // database not optimized yet
    handler->insert = NULL;
    handler->select = NULL;

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
    db->del = database_sqlite_del;
    db->exists = database_sqlite_exists;
    db->clean = database_sqlite_clean;
    db->sset = database_sqlite_sset;
    db->sget = database_sqlite_sget;
    db->sdel = database_sqlite_sdel;
    db->sexists = database_sqlite_sexists;

    return db;
}
