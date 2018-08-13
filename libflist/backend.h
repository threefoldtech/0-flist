#ifndef LIBFLIST_BACKEND_H
    #define LIBFLIST_BACKEND_H

    #include "database.h"

    typedef struct backend_chunk_t {
        char *id;
        char *cipher;

    } backend_chunk_t;

    typedef struct chunks_t {
        size_t upsize;  // uploaded size
        size_t length;  // amount of chunks
        backend_chunk_t *chunks;

    } chunks_t;

        // write a file into the backend
    chunks_t *upload_inode(flist_backend_t *backend, char *path, char *filename);

    // download stuff
    flist_backend_data_t *download_block(flist_backend_t *backend, uint8_t *id, size_t idlen, uint8_t *cipher, size_t cipherlen);

    // clean (and close) a backend object
    void backend_free(flist_backend_t *backend);

    // internal cleaners
    void chunks_free(chunks_t *chunks);
    void download_free(flist_backend_data_t *data);

#if 0

    chunks_t *upload_inode(char *root, char *path, char *filename);
    void upload_inode_flush();

    flist_backend_data_t *download_block(uint8_t *id, size_t idlen, uint8_t *cipher, size_t cipherlen);
    void download_free(flist_backend_data_t *data);

    void chunks_free(chunks_t *chunks);

    // FIXME
    void upload_free();
#endif

#endif
