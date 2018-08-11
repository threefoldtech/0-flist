#ifndef LIBFLIST_DATABASE_SQLITE_H
    #define LIBFLIST_DATABASE_SQLITE_H

    #include <sqlite3.h>

    typedef struct database_sqlite_t {
        char *root;
        char *filename;
        sqlite3 *db;

        int updated;
        sqlite3_stmt *select;
        sqlite3_stmt *insert;

    } database_sqlite_t;

    // initializers
    database_t *database_sqlite_init(char *rootpath);
#endif
