#ifndef LIBFLIST_DATABASE_REDIS_H
    #define LIBFLIST_DATABASE_REDIS_H

    #include <hiredis/hiredis.h>

    typedef struct database_redis_t {
        redisContext *redis;
        char *namespace;

        redisReply* (*internal_get)(database_t *database, uint8_t *key, size_t keylen);
        redisReply* (*internal_set)(database_t *database, uint8_t *key, size_t keylen, uint8_t *payload, size_t length);

    } database_redis_t;

    // initializers
    database_t *database_redis_init_tcp(char *host, int port, char *hset);
    database_t *database_redis_init_unix(char *socket, char *namespace);
#endif
