#define _DEFAULT_SOURCE
#define _GNU_SOURCE
#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <lib0stor.h>
#include <hiredis/hiredis.h>
#include "flister.h"
#include "flist_write.h"
#include "flist.capnp.h"
#include "flist_upload.h"

// FIXME: don't use global variable
typedef struct upload_t {
    redisContext *redis;

} upload_t;

upload_t upcontext = {
    .redis = NULL
};

static redisContext *upload_connect(upload_t *context, char *hostname, int port) {
    if(!(context->redis = redisConnect(hostname, port)))
        diep("redisConnect");

    if(context->redis->err)
        dies(context->redis->errstr);

    return context->redis;
}

void upload_free() {
    if(upcontext.redis)
        redisFree(upcontext.redis);
}

static int chunk_upload(redisContext *redis, chunk_t *chunk) {
    redisReply *reply;
    int retval = 0;

    // does key already exists
    reply = redisCommand(redis, "EXISTS %s", chunk->id);
    if(reply->type == REDIS_REPLY_INTEGER) {
        if(reply->integer == 1) {
            freeReplyObject(reply);
            return 1;
        }
    }

    freeReplyObject(reply);

    // insert new key
    reply = redisCommand(redis, "SET %s %b", chunk->id, chunk->data, chunk->length);
    if(reply->len == 0 || reply->type == REDIS_REPLY_ERROR) {
        fprintf(stderr, "[-] redis error\n");
        retval = 1;
    }

    freeReplyObject(reply);

    return retval;
}

static chunks_t *upload_file(redisContext *redis, char *filename) {
    buffer_t *buffer;
    chunks_t *chunks;

    // initialize buffer
    if(!(buffer = bufferize(filename)))
        return NULL;

    if(!(chunks = (chunks_t *) calloc(sizeof(chunks_t), 1)))
        diep("upload: calloc");

    // setting number of expected chunks
    chunks->length = buffer->chunks;

    if(!(chunks->chunks = (inode_chunk_t *) malloc(sizeof(inode_chunk_t) * chunks->length)))
        diep("upload: chunks: calloc");

    // processing each chunks
    printf("[+] processing %d chunks\n", buffer->chunks);
    for(int i = 0; i < buffer->chunks; i++) {
        const unsigned char *data = buffer_next(buffer);

        // encrypting chunk
        chunk_t *chunk = encrypt_chunk(data, buffer->chunksize);

        chunks->chunks[i].id = strdup(chunk->id);
        chunks->chunks[i].cipher = strdup(chunk->cipher);
        chunks->upsize += chunk->length;

        // hiredis upload
        chunk_upload(redis, chunk);

        // cleaning
        chunk_free(chunk);
    }

    printf("[+] finalsize: %lu bytes\n", chunks->upsize);

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

chunks_t *upload_inode(char *root, char *path, char *filename) {
    char *physical = NULL;

    if(upcontext.redis == NULL)
        upload_connect(&upcontext, settings.uploadhost, settings.uploadport);

    if(asprintf(&physical, "%s/%s/%s", root, path, filename) < 0)
        diep("asprintf");

    chunks_t *chunks = upload_file(upcontext.redis, physical);
    free(physical);

    return chunks;
}

