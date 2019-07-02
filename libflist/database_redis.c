#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <hiredis/hiredis.h>
#include "libflist.h"
#include "verbose.h"
#include "database.h"
#include "database_redis.h"

static void database_redis_close(flist_db_t *database) {
    database_redis_t *db = (database_redis_t *) database->handler;
    redisFree(db->redis);

    free(database->handler);
    free(database);
}

static flist_db_t *database_redis_dummy(flist_db_t *database) {
    (void) database;
    return 0;
}

//
// GET
//
static redisReply *database_redis_get_real(flist_db_t *database, uint8_t *key, size_t keylen) {
    database_redis_t *db = (database_redis_t *) database->handler;
    return redisCommand(db->redis, "HGET %s %b", db->namespace, key, keylen);
}

static redisReply *database_redis_get_zdb(flist_db_t *database, uint8_t *key, size_t keylen) {
    database_redis_t *db = (database_redis_t *) database->handler;
    return redisCommand(db->redis, "GET %b", key, keylen);
}

static value_t *database_redis_get(flist_db_t *database, uint8_t *key, size_t keylen) {
    database_redis_t *db = (database_redis_t *) database->handler;
    redisReply *reply;
    value_t *value = NULL;

    if(!(value = calloc(1, sizeof(value_t)))) {
        diep("malloc");
        return NULL;
    }

    if(!(reply = db->internal_get(database, key, keylen))) {
       free(value);
       return NULL;
    }

    if(reply->type != REDIS_REPLY_STRING) {
        freeReplyObject(reply);
        return NULL;
    }

    value->data = reply->str;
    value->length = reply->len;
    value->handler = reply;

    return value;
}

static value_t *database_redis_sget(flist_db_t *database, char *key) {
    return database_redis_get(database, (uint8_t *) key, strlen(key));
}

//
// SET
//
static redisReply *database_redis_set_real(flist_db_t *database, uint8_t *key, size_t keylen, uint8_t *payload, size_t length) {
    database_redis_t *db = (database_redis_t *) database->handler;
    redisReply *reply;

    // we are on a real redis backend
    if(!(reply = redisCommand(db->redis, "HSET %s %b %b", db->namespace, key, keylen, payload, length)))
        return NULL;

    if(reply->type == REDIS_REPLY_ERROR) {
        libflist_set_error("redis: set: %s", reply->str);
        freeReplyObject(reply);
        return NULL;
    }

    return reply;
}

static redisReply *database_redis_set_zdb(flist_db_t *database, uint8_t *key, size_t keylen, uint8_t *payload, size_t length) {
    database_redis_t *db = (database_redis_t *) database->handler;
    redisReply *reply;

    // we are on zero-db
    if(!(reply = redisCommand(db->redis, "SET %b %b", key, keylen, payload, length)))
        return NULL;

    if(reply->len == 0)
        return reply;

    if(memcmp(reply->str, key, keylen)) {
        libflist_set_error("set: invalid response: %s", reply->str);
        freeReplyObject(reply);
        return NULL;
    }

    return reply;
}

static int database_redis_set(flist_db_t *database, uint8_t *key, size_t keylen, uint8_t *payload, size_t length) {
    database_redis_t *db = (database_redis_t *) database->handler;
    redisReply *reply;

    if(!(reply = db->internal_set(database, key, keylen, payload, length)))
        return 1;

    freeReplyObject(reply);

    return 0;
}

static int database_redis_sset(flist_db_t *database, char *key, uint8_t *payload, size_t length) {
    return database_redis_set(database, (uint8_t *) key, strlen(key), payload, length);
}

static void database_redis_clean(value_t *value) {
    freeReplyObject(value->handler);
    free(value);
}


// poor implementation of exists
static int database_redis_exists(flist_db_t *database, uint8_t *key, size_t keylen) {
    int retval = 0;

    value_t *value = database_redis_get(database, key, keylen);
    if(!value)
        return 0;

    if(value->data)
        retval = 1;

    database_redis_clean(value);
    return retval;
}

static int database_redis_sexists(flist_db_t *database, char *key) {
    return database_redis_exists(database, (uint8_t *) key, strlen(key));
}

static value_t *database_redis_mdget(flist_db_t *database, char *key) {
    (void) database;
    (void) key;

    debug("[-] libflist: database_redis_mdget: not implemented\n");
    return NULL;
}

static int database_redis_mdset(flist_db_t *database, char *key, char *payload) {
    (void) database;
    (void) key;
    (void) payload;

    debug("[-] libflist: database_redis_mdset: not implemented\n");
    return 1;
}

static int database_redis_mddel(flist_db_t *database, char *key) {
    (void) database;
    (void) key;

    debug("[-] libflist: database_redis_mddel: not implemented\n");
    return 1;
}

static flist_db_t *database_redis_init_global(flist_db_t *db) {
    // setting global db
    db->type = "REDIS";

    // fillin handlers
    db->open = database_redis_dummy;
    db->create = database_redis_dummy;
    db->close = database_redis_close;
    db->get = database_redis_get;
    db->set = database_redis_set;
    db->exists = database_redis_exists;
    db->clean = database_redis_clean;
    db->sget = database_redis_sget;
    db->sset = database_redis_sset;
    db->sexists = database_redis_sexists;
    db->mdget = database_redis_mdget;
    db->mdset = database_redis_mdset;
    db->mddel = database_redis_mddel;

    return db;
}

// public sqlite function initializer
static flist_db_t *database_redis_init() {
    flist_db_t *db;

    // allocate generic database object
    if(!(db = malloc(sizeof(flist_db_t))))
        return NULL;

    // set our custom redis database handler
    if(!(db->handler = malloc(sizeof(database_redis_t)))) {
        free(db);
        return NULL;
    }

    return database_redis_init_global(db);
}

static int database_redis_set_namespace(database_redis_t *db, char *namespace, char *password, char *token) {
    redisReply *reply;

    if(!(reply = redisCommand(db->redis, "INFO")))
        return 1;

    if(reply->len == 0)
        return 1;

    if(strstr("0-db (zdb)", reply->str) == 0) {
        // this is a zero-db server
        freeReplyObject(reply);

        // linking to zdb settings
        db->namespace = NULL;
        db->internal_get = database_redis_get_zdb;
        db->internal_set = database_redis_set_zdb;

        debug("[+] database: zero-db detected, selecting namespace\n");
        if(token) {
            debug("[+] database: authenticating token\n");

            if(!(reply = redisCommand(db->redis, "AUTH %s", token)))
                return 1;
        }

        if(password) {
            debug("[+] database: authenticating using password\n");

            if(!(reply = redisCommand(db->redis, "SELECT %s %s", namespace, password)))
                return 1;

        } else {
            if(!(reply = redisCommand(db->redis, "SELECT %s", namespace)))
                return 1;
        }

        if(strcmp(reply->str, "OK")) {
            libflist_set_error("%s: %s", namespace, reply->str);
            freeReplyObject(reply);
            return 1;
        }

    } else {
        // this is a redis-compatible server
        debug("[+] database: redis compatible detected\n");

        // linking to redis settings
        db->namespace = namespace;
        db->internal_get = database_redis_get_real;
        db->internal_set = database_redis_set_real;
    }

    freeReplyObject(reply);
    return 0;

}

flist_db_t *libflist_db_redis_init_tcp(char *host, int port, char *namespace, char *password, char *token) {
    flist_db_t *db = database_redis_init();
    database_redis_t *handler = db->handler;

    if(!(handler->redis = redisConnect(host, port)))
        return libflist_set_error("redis: connect: cannot allocate memory");

    if(handler->redis->err) {
        libflist_set_error("redis: connect: tcp: %s", handler->redis->errstr);
        redisFree(handler->redis);
        return NULL;
    }

    database_redis_set_namespace(handler, namespace, password, token);

    return db;
}

flist_db_t *libflist_db_redis_init_unix(char *socket, char *namespace, char *password, char *token) {
    flist_db_t *db = database_redis_init();
    database_redis_t *handler = db->handler;

    if(!(handler->redis = redisConnectUnix(socket)))
        return libflist_set_error("redis: connect: unix: cannot allocate memory");

    if(handler->redis->err) {
        libflist_set_error("redis: connect: unix: %s", handler->redis->errstr);
        redisFree(handler->redis);
        return NULL;
    }

    database_redis_set_namespace(handler, namespace, password, token);

    return db;
}
