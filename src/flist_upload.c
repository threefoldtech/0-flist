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
    redisContext *redis;  // redis context
    int pwrite;           // pending write
    size_t buflen;        // buffer length

} upload_t;

upload_t upcontext = {
    .redis = NULL,
    .pwrite = 0,
    .buflen = 0,
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

void upload_flush(upload_t *context) {
    redisReply *reply;

    printf("[+] upload: flushing: %d items (%.2f KB)\n", context->pwrite, context->buflen / 1024.0);

    for(int i = 0; i < context->pwrite; i++) {
        redisGetReply(context->redis, &reply);

        if(reply->len == 0 || reply->type == REDIS_REPLY_ERROR) {
            fprintf(stderr, "[-] redis error\n");
        }

        freeReplyObject(reply);
    }

    context->pwrite = 0;
    context->buflen = 0;
}

static int chunk_upload(upload_t *context, chunk_t *chunk) {
    // insert new key
    redisAppendCommand(context->redis, "SET %s %b", chunk->id, chunk->data, chunk->length);
    context->pwrite += 1;
    context->buflen += chunk->length;

    // flush 32 MB
    if(context->buflen > 32 * 1024 * 1024) {
        upload_flush(context);

    return 0;
}

static chunks_t *upload_file(upload_t *context, char *filename) {
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
        chunk_upload(context, chunk);

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

    chunks_t *chunks = upload_file(&upcontext, physical);
    free(physical);

    return chunks;
}

void upload_inode_flush() {
    upload_flush(&upcontext);
}
