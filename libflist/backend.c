#define _DEFAULT_SOURCE
#define _GNU_SOURCE
#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include "libflist.h"
#include "verbose.h"
#include "backend.h"
#include "flist.capnp.h"
#include "database.h"
#include "database_redis.h"
#include "database_sqlite.h"
#include "zero_chunk.h"

flist_backend_t *backend_init(flist_db_t *database, char *rootpath) {
    flist_backend_t *backend;

    if(!(backend = malloc(sizeof(flist_backend_t))))
        return NULL;

    backend->database = database;
    backend->rootpath = rootpath;

    return backend;
}

#if 0
// FIXME: don't use global variable
typedef struct flist_backend_t {
    redisContext *redis;  // redis context
    int pwrite;           // pending write
    size_t buflen;        // buffer length

} flist_backend_t;

flist_backend_t bcontext = {
    .redis = NULL,
    .pwrite = 0,
    .buflen = 0,
};
#endif

#if 0
static redisContext *backend_connect(flist_backend_t *context, char *hostname, int port) {
    debug("[+] backend: connecting (%s, %d)\n", hostname, port);

    if(!(context->redis = redisConnect(hostname, port)))
        diep("redisConnect");

    if(context->redis->err)
        dies(context->redis->errstr);

    return context->redis;
}

void upload_free() {
    if(bcontext.redis)
        redisFree(bcontext.redis);
}
#endif

void upload_flush(flist_backend_t *context) {
    (void) context;
#if 0
    redisReply *reply;

    debug("[+] upload: flushing: %d items (%.2f KB)\n", context->pwrite, context->buflen / 1024.0);

    for(int i = 0; i < context->pwrite; i++) {
        redisGetReply(context->redis, (void **) &reply);

        if(reply->type == REDIS_REPLY_ERROR) {
            fprintf(stderr, "[-] redis error: %s\n", reply->str);
        }

        if(reply->len == 0) {
            // if 0-db behind
            // it's a duplicated key
        }

        freeReplyObject(reply);
    }

    context->pwrite = 0;
    context->buflen = 0;
#endif
}

static int chunk_upload(flist_backend_t *context, chunk_t *chunk) {
#if 0
    // insert new key
    redisAppendCommand(context->redis, "SET %b %b", chunk->id, LIB0STOR_HASH_LENGTH, chunk->data, chunk->length);
    context->pwrite += 1;
    context->buflen += chunk->length;

    // flush 32 MB
    if(context->buflen > 32 * 1024 * 1024)
        upload_flush(context);
#endif

#if 0
    redisReply *reply;
    reply = redisCommand(context->redis, "SET %b %b", chunk->id, LIB0STOR_HASH_LENGTH, chunk->data, chunk->length);

    if(!reply)
        diep("redis command");

    if(reply->len == 0 || reply->type == REDIS_REPLY_ERROR) {
        fprintf(stderr, "[-] redis error\n");
    }

    freeReplyObject(reply);
#endif
    flist_db_t *db = context->database;
    if(db->set(db, chunk->id, ZEROCHUNK_HASH_LENGTH, chunk->data, chunk->length))
        return 1;

    return 0;
}

static chunks_t *upload_file(flist_backend_t *context, char *filename) {
    buffer_t *buffer;
    chunks_t *chunks;

    // initialize buffer
    if(!(buffer = bufferize(filename)))
        return NULL;

    if(!(chunks = (chunks_t *) calloc(sizeof(chunks_t), 1)))
        diep("upload: calloc");

    // setting number of expected chunks
    chunks->length = buffer->chunks;

    if(!(chunks->chunks = (backend_chunk_t *) malloc(sizeof(backend_chunk_t) * chunks->length)))
        diep("upload: chunks: calloc");

    // processing each chunks
    debug("[+] processing %d chunks\n", buffer->chunks);
    for(int i = 0; i < buffer->chunks; i++) {
        const unsigned char *data = buffer_next(buffer);

        // encrypting chunk
        chunk_t *chunk = encrypt_chunk(data, buffer->chunksize);

        chunks->chunks[i].id = libflist_bufdup(chunk->id, ZEROCHUNK_HASH_LENGTH);
        chunks->chunks[i].cipher = libflist_bufdup(chunk->cipher, ZEROCHUNK_HASH_LENGTH);
        chunks->upsize += chunk->length;

        // hiredis upload
        if(chunk_upload(context, chunk)) {
            debug("[-] chunk_upload: %s\n", libflist_strerror());

            chunk_free(chunk);
            chunks = NULL;
            goto cleanup;
        }

        // cleaning
        chunk_free(chunk);
    }

    debug("[+] finalsize: %lu bytes\n", chunks->upsize);

cleanup:
    // cleaning
    buffer_free(buffer);

    return chunks;
}

void chunks_free(chunks_t *chunks) {
    for(size_t i = 0; i < chunks->length; i++) {
        free(chunks->chunks[i].id);
        free(chunks->chunks[i].cipher);
    }

    free(chunks->chunks);
    free(chunks);
}

chunks_t *upload_inode(flist_backend_t *backend, char *path, char *filename) {
    char *physical = NULL;

    /*
    if(bcontext.redis == NULL)
        backend_connect(&bcontext, settings.backendhost, settings.backendport);
    */

    if(asprintf(&physical, "%s/%s/%s", backend->rootpath, path, filename) < 0)
        diep("asprintf");

    chunks_t *chunks = upload_file(backend, physical);
    free(physical);

    return chunks;
}

flist_backend_data_t *download_block(flist_backend_t *backend, uint8_t *id, size_t idlen, uint8_t *cipher, size_t cipherlen) {
    // redisReply *reply;
    (void) cipherlen;
    flist_backend_data_t *data;    // internal data
    chunk_t input, *output;  // input chunk, output decrypted chunk

    /*
    if(bcontext.redis == NULL)
        backend_connect(&bcontext, settings.backendhost, settings.backendport);

    reply = redisCommand(bcontext.redis, "GET %b", id, idlen);
    if(!reply->str)
        return NULL;
    */

    flist_db_t *db = backend->database;
    value_t *value = db->get(db, id, idlen);

    if(value == NULL)
        return NULL;

    // fill-in input struct
    input.id = id;
    input.cipher = cipher;
    input.data = (unsigned char *) value->data;
    input.length = value->length;

    if(!(output = decrypt_chunk(&input)))
        return NULL;

    // internal chunk, which make lib0stor opaque
    if(!(data = malloc(sizeof(flist_backend_data_t))))
        diep("backend malloc");

    data->opaque = output;
    data->payload = output->data;
    data->length = output->length;

    db->clean(value);

    return data;
}

void download_free(flist_backend_data_t *data) {
    chunk_free(data->opaque);
    free(data);
}

void upload_inode_flush() {
    // upload_flush(&bcontext);
}

void backend_free(flist_backend_t *backend) {
    backend->database->close(backend->database);
    free(backend);
}
