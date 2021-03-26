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
#include "flist.capnp.h"
#include "database.h"
#include "database_redis.h"
#include "database_sqlite.h"
#include "zero_chunk.h"

flist_backend_t *libflist_backend_init(flist_db_t *database, char *rootpath) {
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

int libflist_backend_exists(flist_backend_t *context, flist_chunk_t *chunk) {
    flist_db_t *db = context->database;
    return db->exists(db, chunk->id.data, chunk->id.length);
}

// upload a chunk on the backend
int libflist_backend_upload_chunk(flist_backend_t *context, flist_chunk_t *chunk) {
    flist_db_t *db = context->database;

    if(db->set(db, chunk->id.data, chunk->id.length, chunk->encrypted.data, chunk->encrypted.length))
        return 1;

    return 0;
}

flist_chunks_t *libflist_backend_upload_file(flist_backend_t *context, char *filename) {
    buffer_t *buffer;
    flist_chunks_t *chunks;

    // initialize buffer
    if(!(buffer = bufferize(filename)))
        return NULL;

    if(!(chunks = (flist_chunks_t *) calloc(sizeof(flist_chunks_t), 1)))
        return libflist_errp("upload: calloc");

    // setting number of expected chunks
    chunks->length = buffer->chunks;

    if(!(chunks->chunks = (flist_chunk_t **) malloc(sizeof(flist_chunk_t *) * chunks->length)))
        diep("upload: chunks: calloc");

    // processing each chunks
    debug("[+] processing %d chunks\n", buffer->chunks);

    for(int i = 0; i < buffer->chunks; i++) {
        const unsigned char *data = buffer_next(buffer);

        // encrypting chunk
        flist_chunk_t *chunk;

        if(!(chunk = libflist_chunk_encrypt(data, buffer->chunksize)))
            return NULL;

        chunks->chunks[i] = chunk;
        chunks->upsize += chunk->encrypted.length;

        // check if chunk is already on the backend
        if(libflist_backend_exists(context, chunk)) {
            debug("[+] processing: chunk already on the backend, skipping\n");
            continue;
        }

        // hiredis upload
        if(libflist_backend_upload_chunk(context, chunk)) {
            debug("[-] chunk_upload: %s\n", libflist_strerror());

            libflist_chunk_free(chunk);
            chunks = NULL;
            goto cleanup;
        }
    }

    debug("[+] finalsize: %lu bytes\n", chunks->upsize);

cleanup:
    // cleaning
    buffer_free(buffer);

    return chunks;
}

// check if the chunk is already on the backend
// if it's not on the backend, uploading it
int libflist_backend_chunk_commit(flist_backend_t *context, flist_chunk_t *chunk) {
    // check if chunk is already on the backend
    if(libflist_backend_exists(context, chunk)) {
        debug("[+] libflist: backend: chunk already on the backend, skipping\n");
        return 0;
    }

    debug("[+] libflist: backend: uploading chunk (%lu bytes)\n", chunk->encrypted.length);

    // backend upload
    if(libflist_backend_upload_chunk(context, chunk)) {
        debug("[-] libflist: backend: chunk: upload: %s\n", libflist_strerror());
        return -1;
    }

    return 1;
}

void libflist_backend_chunks_free(flist_chunks_t *chunks) {
    for(size_t i = 0; i < chunks->length; i++)
        libflist_chunk_free(chunks->chunks[i]);

    free(chunks->chunks);
    free(chunks);
}

flist_chunks_t *libflist_backend_upload_inode(flist_backend_t *backend, char *path, char *filename) {
    char *physical = NULL;

    if(asprintf(&physical, "%s/%s/%s", backend->rootpath, path, filename) < 0)
        diep("asprintf");

    flist_chunks_t *chunks = libflist_backend_upload_file(backend, physical);
    free(physical);

    return chunks;
}

flist_chunk_t *libflist_backend_download_chunk(flist_backend_t *backend, flist_chunk_t *chunk) {
    flist_db_t *db = backend->database;
    value_t *value;

    if(libflist_debug_flag) {
        char *key = libflist_hashhex(chunk->id.data, chunk->id.length);
        debug("[+] backend: downloading chunk: %s\n", key);
        free(key);
    }

    if(!(value = db->get(db, chunk->id.data, chunk->id.length))) {
        libflist_set_error("key not found on the backend");
        return NULL;
    }

    chunk->encrypted.data = (uint8_t *) value->data;
    chunk->encrypted.length = value->length;

    if(!libflist_chunk_decrypt(chunk))
        return NULL;

    // clear the downloaded data not needed anymore
    chunk->encrypted.data = NULL;
    chunk->encrypted.length = 0;
    db->clean(value);

    return chunk;
}

void upload_inode_flush() {
    // upload_flush(&bcontext);
}

void libflist_backend_free(flist_backend_t *backend) {
    backend->database->close(backend->database);
    free(backend);
}
