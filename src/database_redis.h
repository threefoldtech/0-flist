#ifndef DATABASE_REDIS_H
    #define DATABASE_REDIS_H

    #if 0
    #include <sqlite3.h>

    typedef struct database_t {
        char *root;
        char *filename;
        sqlite3 *db;
        int updated;
        sqlite3_stmt *select;
        sqlite3_stmt *insert;

    } database_t;

    typedef struct value_t {
        char *data;
        size_t length;
        sqlite3_stmt *stmt;

    } value_t;

    database_t *database_open(char *root);
    database_t *database_create(char *root);
    void database_close(database_t *database);

    value_t *database_get(database_t *database, const char *key);
    int database_set(database_t *database, const char *key, const unsigned char *payload, size_t length);
    int database_exists(database_t *database, const char *key);

    value_t *database_value_free(value_t *value);

    #endif
#endif
