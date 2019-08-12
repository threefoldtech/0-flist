#ifndef LIBFLIST_ZERO_CHUNK_H
    #define LIBFLIST_ZERO_CHUNK_H

    #include <stdint.h>

    #define ZEROCHUNK_HASH_LENGTH   16

    typedef struct buffer_t {
        FILE *fp;
        uint8_t *data;
        size_t length;
        size_t current;
        size_t chunksize;
        size_t finalsize;
        int chunks;

    } buffer_t;

    // file buffer
    buffer_t *bufferize(char *filename);
    buffer_t *buffer_writer(char *filename);
    const uint8_t *buffer_next(buffer_t *buffer);
    void buffer_free(buffer_t *buffer);

    // chunk
    inode_chunks_t *flist_chunks_duplicate(inode_chunks_t *source);
#endif
