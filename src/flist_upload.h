#ifndef FLIST_UPLOAD_H
    #define FLIST_UPLOAD_H

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

    chunks_t *upload_inode(char *root, char *path, char *filename);
    void upload_inode_flush();

    void chunks_free(chunks_t *chunks);

    // FIXME
    void upload_free();
#endif
