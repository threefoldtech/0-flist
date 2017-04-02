#ifndef DATABASE_H
    #define DATABASE_H

    #include <rocksdb/c.h>

    typedef struct database_t {
        char *root;
        rocksdb_t *db;
        rocksdb_options_t *options;
        rocksdb_readoptions_t *readoptions;
        rocksdb_writeoptions_t *writeoptions;

    } database_t;

    typedef struct value_t {
        char *data;
        size_t length;

    } value_t;

    database_t *database_open(char *root);
    database_t *database_create(char *root);
    void database_close(database_t *database);

    value_t *database_get(database_t *database, const char *key);
    void database_value_free(value_t *value);
#endif
