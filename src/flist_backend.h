#ifndef FLIST_BACKEND_H
    #define FLIST_BACKEND_H

    #include "database.h"

    typedef struct inode_chunk_t {
        char *id;
        char *cipher;

    } inode_chunk_t;

    typedef struct chunks_t {
        size_t upsize;  // uploaded size
        size_t length;  // amount of chunks
        inode_chunk_t *chunks;

    } chunks_t;

    typedef struct backend_data_t {
        void *opaque;
        unsigned char *payload;
        size_t length;

    } backend_data_t;

    chunks_t *upload_inode(char *root, char *path, char *filename);
    void upload_inode_flush();

    backend_data_t *download_block(uint8_t *id, uint8_t *cipher);
    void download_free(backend_data_t *data);

    void chunks_free(chunks_t *chunks);

    // FIXME
    void upload_free();
#endif
