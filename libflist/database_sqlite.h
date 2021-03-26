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
        sqlite3_stmt *delete;

        sqlite3_stmt *mdset;
        sqlite3_stmt *mdget;
        sqlite3_stmt *mddel;

    } database_sqlite_t;

#endif
