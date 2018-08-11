#ifndef LIBFLIST_ZERO_CHUNK_H
    #define LIBFLIST_ZERO_CHUNK_H

    #include <stdint.h>

    #define LIB0STOR_HASH_LENGTH   16

    typedef struct remote_t {
        void *redis;

    } remote_t;

    typedef struct buffer_t {
        FILE *fp;
        uint8_t *data;
        size_t length;
        size_t current;
        size_t chunksize;
        size_t finalsize;
        int chunks;

    } buffer_t;

    typedef struct chunk_t {
        uint8_t *id;
        uint8_t *cipher;
        uint8_t *data;
        size_t length;

    } chunk_t;

    // hashing
    uint8_t *zchunk_hash(const void *buffer, size_t length);

    // file buffer
    buffer_t *bufferize(char *filename);
    buffer_t *buffer_writer(char *filename);
    const uint8_t *buffer_next(buffer_t *buffer);
    void buffer_free(buffer_t *buffer);

    // chunk
    chunk_t *chunk_new(uint8_t *hash, uint8_t *key, void *data, size_t length);
    void chunk_free(chunk_t *chunk);
    char *hashhex(void *hash, size_t length);

    // encryption
    chunk_t *encrypt_chunk(const uint8_t *chunk, size_t chunksize);
    chunk_t *decrypt_chunk(chunk_t *chunk);
#endif
