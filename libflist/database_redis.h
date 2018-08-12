#ifndef LIBFLIST_DATABASE_REDIS_H
    #define LIBFLIST_DATABASE_REDIS_H

    #include <hiredis/hiredis.h>

    typedef struct database_redis_t {
        redisContext *redis;
        char *namespace;

        redisReply* (*internal_get)(flist_db_t *database, uint8_t *key, size_t keylen);
        redisReply* (*internal_set)(flist_db_t *database, uint8_t *key, size_t keylen, uint8_t *payload, size_t length);

    } database_redis_t;

#endif
