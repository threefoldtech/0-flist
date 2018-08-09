#ifndef DATABASE_H
    #define DATABASE_H

    typedef struct value_t {
        void *handler;

        char *data;
        size_t length;

    } value_t;

    typedef struct database_t {
        void *handler;
        char *type;

        struct database_t* (*open)(struct database_t *db);
        struct database_t* (*create)(struct database_t *db);
        void (*close)(struct database_t *db);

        value_t* (*get)(struct database_t *db, char *key, size_t keylen);
        int (*set)(struct database_t *db, char *key, size_t keylen, uint8_t *data, size_t datalen);
        int (*exists)(struct database_t *db, char *key, size_t keylen);

        void (*clean)(value_t *value);

    } database_t;

    typedef enum database_type_t {
        SQLITE3,
        REDIS,

    } database_type_t;

    void warndb(char *source, const char *str);
#endif
