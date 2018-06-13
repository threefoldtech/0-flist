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
#include "flist_backend.h"

// FIXME: don't use global variable
typedef struct backend_t {
    redisContext *redis;  // redis context
    int pwrite;           // pending write
    size_t buflen;        // buffer length

} backend_t;

backend_t bcontext = {
    .redis = NULL,
    .pwrite = 0,
    .buflen = 0,
};

static redisContext *backend_connect(backend_t *context, char *hostname, int port) {
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

void upload_flush(backend_t *context) {
    redisReply *reply;

    debug("[+] upload: flushing: %d items (%.2f KB)\n", context->pwrite, context->buflen / 1024.0);

    for(int i = 0; i < context->pwrite; i++) {
        redisGetReply(context->redis, (void **) &reply);

        if(reply->len == 0 || reply->type == REDIS_REPLY_ERROR) {
            fprintf(stderr, "[-] redis error\n");
        }

        freeReplyObject(reply);
    }

    context->pwrite = 0;
    context->buflen = 0;
}

static int chunk_upload(backend_t *context, chunk_t *chunk) {
    // insert new key
    redisAppendCommand(context->redis, "SET %s %b", chunk->id, chunk->data, chunk->length);
    context->pwrite += 1;
    context->buflen += chunk->length;

    // flush 32 MB
    if(context->buflen > 32 * 1024 * 1024)
        upload_flush(context);

    return 0;
}

static chunks_t *upload_file(backend_t *context, char *filename) {
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
    debug("[+] processing %d chunks\n", buffer->chunks);
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

    debug("[+] finalsize: %lu bytes\n", chunks->upsize);

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

    if(bcontext.redis == NULL)
        backend_connect(&bcontext, settings.backendhost, settings.backendport);

    if(asprintf(&physical, "%s/%s/%s", root, path, filename) < 0)
        diep("asprintf");

    chunks_t *chunks = upload_file(&bcontext, physical);
    free(physical);

    return chunks;
}

backend_data_t *download_block(char *id, char *cipher) {
    redisReply *reply;
    backend_data_t *data;    // internal data
    chunk_t input, *output;  // input chunk, output decrypted chunk

    if(bcontext.redis == NULL)
        backend_connect(&bcontext, settings.backendhost, settings.backendport);

    reply = redisCommand(bcontext.redis, "GET %s", id);
    if(!reply->str)
        return NULL;

    // fill-in input struct
    input.id = id;
    input.cipher = cipher;
    input.data = (unsigned char *) reply->str;
    input.length = reply->len;

    if(!(output = decrypt_chunk(&input)))
        return NULL;

    // internal chunk, which make lib0stor opaque
    if(!(data = malloc(sizeof(backend_data_t))))
        diep("backend malloc");

    data->opaque = output;
    data->payload = output->data;
    data->length = output->length;

    freeReplyObject(reply);

    return data;
}

void download_free(backend_data_t *data) {
    chunk_free(data->opaque);
    free(data);
}

void upload_inode_flush() {
    upload_flush(&bcontext);
}
