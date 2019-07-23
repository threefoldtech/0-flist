#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <snappy-c.h>
#include <zlib.h>
#include <math.h>
#include <time.h>
#include <blake2.h>
#include "libflist.h"
#include "verbose.h"
#include "xxtea.h"
#include "zero_chunk.h"

#define CHUNK_SIZE    1024 * 512    // 512 KB

//
// buffer manager
//
static size_t file_length(FILE *fp) {
    size_t length;

    fseek(fp, 0, SEEK_END);
    length = ftell(fp);

    fseek(fp, 0, SEEK_SET);

    return length;
}

static ssize_t file_load(char *filename, buffer_t *buffer) {
    if(!(buffer->fp = fopen(filename, "r"))) {
        perror(filename);
        return -1;
    }

    buffer->length = file_length(buffer->fp);
    debug("[+] libflist: chunks: local filesize: %lu bytes\n", buffer->length);

    if(buffer->length == 0)
        return 0;

    if(!(buffer->data = malloc(sizeof(char) * buffer->chunksize))) {
        perror("[-] malloc");
        return 0;
    }

    return buffer->length;
}

buffer_t *bufferize(char *filename) {
    buffer_t *buffer;

    if(!(buffer = calloc(1, sizeof(buffer_t)))) {
        perror("[-] malloc");
        return NULL;
    }

    buffer->chunksize = CHUNK_SIZE;
    if(file_load(filename, buffer) < 0)
        return NULL;

    // file empty, nothing to do
    if(buffer->length == 0) {
        debug("[-] libflist: chunks: file is empty, nothing to do\n");
        free(buffer);
        return NULL;
    }

    buffer->chunks = ceil(buffer->length / (float) buffer->chunksize);

    // if the file is smaller than a chunks, hardcoding 1 chunk.
    if(buffer->length < buffer->chunksize)
        buffer->chunks = 1;

    return buffer;
}

buffer_t *buffer_writer(char *filename) {
    buffer_t *buffer;

    if(!(buffer = calloc(1, sizeof(buffer_t)))) {
        perror("[-] malloc");
        return NULL;
    }

    if(!(buffer->fp = fopen(filename, "w"))) {
        perror(filename);
        free(buffer);
        return NULL;
    }

    return buffer;
}

const unsigned char *buffer_next(buffer_t *buffer) {
    // resize chunksize if it's smaller than the remaining
    // amount of data
    if(buffer->current + buffer->chunksize > buffer->length)
        buffer->chunksize = buffer->length - buffer->current;

    // loading this chunk in memory
    if(fread(buffer->data, buffer->chunksize, 1, buffer->fp) != 1) {
        perror("[-] fread");
        return NULL;
    }

    buffer->current += buffer->chunksize;

    return (const uint8_t *) buffer->data;
}

void buffer_free(buffer_t *buffer) {
    fclose(buffer->fp);
    free(buffer->data);
    free(buffer);
}

//
// hashing
//
uint8_t *libflist_chunk_hash(const void *buffer, size_t length) {
    uint8_t *hash;

    if(!(hash = malloc(ZEROCHUNK_HASH_LENGTH)))
        return libflist_errp("chunk_hash: malloc");

    if(blake2b(hash, buffer, "", ZEROCHUNK_HASH_LENGTH, length, 0)) {
        libflist_set_error("blake2 failed");
        return NULL;
    }

    return hash;
}


//
// chunks manager
//
flist_buffer_t libflist_buffer_new(uint8_t *data, size_t length) {
    flist_buffer_t buffer = {
        .data = data,
        .length = length,
    };

    return buffer;
}

void libflist_buffer_free(flist_buffer_t *buffer) {
    free(buffer->data);
}

flist_chunk_t *libflist_chunk_new(uint8_t *id, uint8_t *cipher, void *data, size_t datalen) {
    flist_chunk_t *chunk;

    if(!(chunk = calloc(sizeof(flist_chunk_t), 1)))
        return libflist_errp("chunk new: malloc");

    chunk->id = libflist_buffer_new(id, ZEROCHUNK_HASH_LENGTH);
    chunk->cipher = libflist_buffer_new(cipher, ZEROCHUNK_HASH_LENGTH);
    chunk->plain = libflist_buffer_new(data, datalen);

    return chunk;
}

void libflist_chunk_free(flist_chunk_t *chunk) {
    libflist_buffer_free(&chunk->id);
    libflist_buffer_free(&chunk->cipher);
    libflist_buffer_free(&chunk->plain);
    libflist_buffer_free(&chunk->encrypted);
    free(chunk);
}

//
// encryption and decryption
//
// encrypt a buffer
// returns a chunk with key, cipher, data and it's length
flist_chunk_t *libflist_chunk_encrypt(const uint8_t *chunk, size_t chunksize) {
    // hashing this chunk
    unsigned char *hashkey = libflist_chunk_hash(chunk, chunksize);

    if(libflist_debug_flag) {
        char *inhash = libflist_hashhex(hashkey, ZEROCHUNK_HASH_LENGTH);
        debug("[+] libflist: chunk: encrypt: original hash: %s\n", inhash);
        free(inhash);
    }

    //
    // compress
    //
    size_t output_length = snappy_max_compressed_length(chunksize);
    char *compressed = (char *) malloc(output_length);

    if(snappy_compress((char *) chunk, chunksize, compressed, &output_length) != SNAPPY_OK) {
        libflist_set_error("snappy compression error");
        return NULL;
    }

    // printf("Compressed size: %lu\n", output_length);

    //
    // encrypt
    //
    size_t encrypt_length;
    unsigned char *encrypt_data = xxtea_encrypt_bkey(compressed, output_length, hashkey, ZEROCHUNK_HASH_LENGTH, &encrypt_length);

    unsigned char *hashcrypt = libflist_chunk_hash(encrypt_data, encrypt_length);

    if(libflist_debug_flag) {
        char *inhash = libflist_hashhex(hashcrypt, ZEROCHUNK_HASH_LENGTH);
        debug("[+] libflist: chunk: encrypt: final hash: %s\n", inhash);
        free(inhash);
    }

    // cleaning
    free(compressed);

    flist_chunk_t *response = libflist_chunk_new(hashcrypt, hashkey, NULL, 0);
    response->encrypted.data = encrypt_data;
    response->encrypted.length = encrypt_length;

    return response;
}

// uncrypt a chunk
// it takes a chunk as parameter
// returns a chunk (without key and cipher) with payload data and length
flist_chunk_t *libflist_chunk_decrypt(flist_chunk_t *chunk) {
    char *uncipherdata = NULL;
    size_t uncipherlength;

    //
    // uncrypt payload
    //
    char *key = libflist_hashhex(chunk->cipher.data, chunk->cipher.length);
    debug("[+] libflist: chunk: uncrypt %lu buffer, with key: %s\n", chunk->encrypted.length, key);
    free(key);

    if(!(uncipherdata = xxtea_decrypt_bkey(chunk->encrypted.data, chunk->encrypted.length, chunk->cipher.data, chunk->cipher.length, &uncipherlength))) {
        libflist_set_error("cannot decrypt data, invalid key or payload");
        return NULL;
    }

    //
    // decompress
    //
    size_t uncompressed_length = 0;
    snappy_status status;

    debug("[+] libflist: chunk: uncompressing %lu bytes\n", uncipherlength);

    if((status = snappy_uncompressed_length(uncipherdata, uncipherlength, &uncompressed_length)) != SNAPPY_OK) {
        libflist_set_error("snappy uncompression length error: %d", status);
        return NULL;
    }

    char *uncompress = (char *) malloc(uncompressed_length);
    if((status = snappy_uncompress(uncipherdata, uncipherlength, uncompress, &uncompressed_length)) != SNAPPY_OK) {
        libflist_set_error("snappy uncompression error: %d", status);
        return NULL;
    }

    chunk->plain.data = (uint8_t *) uncompress;
    chunk->plain.length = uncompressed_length;

    //
    // testing integrity
    //
    unsigned char *integrity = libflist_chunk_hash((unsigned char *) uncompress, uncompressed_length);
    // printf("[+] integrity: %s\n", integrity);


    if(memcmp(integrity, chunk->cipher.data, chunk->cipher.length)) {
        char *inhash = libflist_hashhex(integrity, ZEROCHUNK_HASH_LENGTH);
        char *outhash = libflist_hashhex(chunk->cipher.data, chunk->cipher.length);

        debug("[-] libflist: integrity check failed: hash mismatch\n");
        debug("[-] libflist: %s <> %s\n", inhash, outhash);
        libflist_set_error("chunk integrity mismatch");

        free(inhash);
        free(outhash);

        return NULL;
    }

    free(integrity);
    free(uncipherdata);

    return chunk;
}

static uint8_t *buffer_duplicate(flist_buffer_t *input) {
    uint8_t *copy;

    if(!(copy = malloc(input->length)))
        diep("libflist: buffer: duplicate: malloc");

    memcpy(copy, input->data, input->length);
    return copy;
}

// compute file chunks, if context backend is specified (not NULL), committing
// the chunk into the backend
inode_chunks_t *libflist_chunks_proceed(char *localfile, flist_ctx_t *ctx) {
    buffer_t *buffer;
    inode_chunks_t *chunks;
    size_t totalsize = 0;

    // initialize buffer
    if(!(buffer = bufferize(localfile)))
        return NULL;

    if(!(chunks = (inode_chunks_t *) calloc(sizeof(inode_chunks_t), 1)))
        return libflist_errp("chunks: compute: calloc");

    // setting number of expected chunks
    chunks->size = buffer->chunks;
    chunks->blocksize = 512; // ignored

    if(!(chunks->list = (inode_chunk_t *) malloc(sizeof(inode_chunk_t) * chunks->size)))
        diep("libflist: chunks: malloc");

    // processing each chunks
    debug("[+] libflist: chunks: processing %d chunks\n", buffer->chunks);

    for(int i = 0; i < buffer->chunks; i++) {
        const unsigned char *data = buffer_next(buffer);

        // encrypting chunk
        flist_chunk_t *chunk;
        inode_chunk_t *ichunk = &chunks->list[i];

        if(!(chunk = libflist_chunk_encrypt(data, buffer->chunksize)))
            return NULL;

        ichunk->entryid = buffer_duplicate(&chunk->id);
        ichunk->entrylen = chunk->id.length;
        ichunk->decipher = buffer_duplicate(&chunk->cipher);
        ichunk->decipherlen = chunk->cipher.length;

        // if context is provided
        // uploading this chunk
        if(ctx && ctx->backend) {
            if(libflist_backend_chunk_commit(ctx->backend, chunk) < 0) {
                fprintf(stderr, "[-] libflist: chunk: could not upload this chunk\n");
            }
        }

        totalsize += chunk->encrypted.length;

        libflist_chunk_free(chunk);
    }

    debug("[+] libflist: chunks: %lu bytes\n", totalsize);

    // cleaning
    buffer_free(buffer);

    return chunks;
}

// compute file chunks, without uploading anything
inode_chunks_t *libflist_chunks_compute(char *localfile) {
    return libflist_chunks_proceed(localfile, NULL);
}
