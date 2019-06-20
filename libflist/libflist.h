#ifndef LIBFLIST_H
    #define LIBFLIST_H

    //
    // ---------------------------------------------------------------------
    //
    // brace yourself
    //
    // most of this file will hurt your eyes...
    //                    ...for now (I promise, this will change)
    //                    ...look at the end for a better idea of the futur
    //
    // ---------------------------------------------------------------------
    //

    #include <stdint.h>
    #include <time.h>

    typedef struct acl_t {
        char *uname;     // username (user id if not found)
        char *gname;     // group name (group id if not found)
        uint16_t mode;   // integer file mode
        char *key;       // hash of the payload (dedupe in db)

    } acl_t;

    typedef struct flist_acl_t {
        char *uname;     // username (user id if not found)
        char *gname;     // group name (group id if not found)
        uint16_t mode;   // integer file mode

    } flist_acl_t;


    #if 0
    // link internal type to capnp type directly
    // this make things easier later
    typedef enum inode_type_t {
        INODE_DIRECTORY = Inode_attributes_dir,
        INODE_FILE = Inode_attributes_file,
        INODE_LINK = Inode_attributes_link,
        INODE_SPECIAL = Inode_attributes_special,

    } inode_type_t;

    typedef enum inode_special_t {
        SOCKET = Special_Type_socket,
        BLOCK = Special_Type_block,
        CHARDEV = Special_Type_chardev,
        FIFOPIPE = Special_Type_fifopipe,
        UNKNOWN = Special_Type_unknown,

    } inode_special_t;
    #endif


    // link internal type to capnp type directly
    // this make things easier later
    typedef enum inode_type_t {
        INODE_DIRECTORY,
        INODE_FILE,
        INODE_LINK,
        INODE_SPECIAL,

    } inode_type_t;

    typedef enum inode_special_t {
        SOCKET,
        BLOCK,
        CHARDEV,
        FIFOPIPE,
        UNKNOWN,

    } inode_special_t;

    // represent one chunk of a file payload
    typedef struct inode_chunk_t {
        uint8_t *entryid;      // binary identifier (key in the backend)
        uint8_t entrylen;      // length of the identifier
        uint8_t *decipher;     // decipher key to uncrypt the payload
        uint8_t decipherlen;   // length of the decipher key

    } inode_chunk_t;

    // represent list of chunks to get
    // file payload
    typedef struct inode_chunks_t {
        size_t size;          // amount of chunks on the list
        size_t blocksize;     // size of each block (this is ignored)
        inode_chunk_t *list;  // list of chunks

    } inode_chunks_t;

    typedef struct inode_t {
        char *name;       // relative inode name (filename)
        char *fullpath;   // full relative path
        size_t size;      // size in byte
        acl_t acl;        // access control

        flist_acl_t *racl;       // file permissions

        inode_type_t type;       // internal file type
        time_t modification;     // modification time
        time_t creation;         // creation time

        char *subdirkey;         // for directory: directory target key
        inode_special_t stype;   // for special: special type
        char *sdata;             // for special: special metadata
        char *link;              // for symlink: symlink target
        inode_chunks_t *chunks;  // for regular file: list of chunks

        struct inode_t *next;

    } inode_t;

    typedef struct dirnode_t {
        struct inode_t *inode_list;
        struct inode_t *inode_last;
        size_t inode_length;

        struct dirnode_t *dir_list;
        struct dirnode_t *dir_last;
        size_t dir_length;

        char *fullpath;        // virtual full path
        char *name;            // directory name
        char *hashkey;         // internal hash (for db)

        acl_t acl;             // access control
        flist_acl_t *racl;     // read acl
        time_t creation;       // creation time
        time_t modification;   // modification time

        struct dirnode_t *next;

    } dirnode_t;



    typedef struct flist_db_value_t {
        void *handler;

        char *data;
        size_t length;

    } value_t;

    typedef struct flist_db_t {
        void *handler;
        char *type;

        struct flist_db_t* (*open)(struct flist_db_t *db);
        struct flist_db_t* (*create)(struct flist_db_t *db);
        void (*close)(struct flist_db_t *db);

        value_t* (*get)(struct flist_db_t *db, uint8_t *key, size_t keylen);
        int (*set)(struct flist_db_t *db, uint8_t *key, size_t keylen, uint8_t *data, size_t datalen);
        int (*del)(struct flist_db_t *db, uint8_t *key, size_t keylen);
        int (*exists)(struct flist_db_t *db, uint8_t *key, size_t keylen);

        value_t* (*sget)(struct flist_db_t *db, char *key);
        int (*sset)(struct flist_db_t *db, char *key, uint8_t *data, size_t datalen);
        int (*sdel)(struct flist_db_t *db, char *key);
        int (*sexists)(struct flist_db_t *db, char *key);

        void (*clean)(value_t *value);

    } flist_db_t;

    typedef enum flist_db_type_t {
        SQLITE3,
        REDIS,

    } flist_db_type_t;

    // FIXME
    #include "flist.capnp.h"

    typedef struct directory_t {
        value_t *value;
        struct capn ctx;
        Dir_ptr dirp;
        struct Dir dir;

    } directory_t;


    typedef struct flist_backend_t {
        flist_db_t *database;
        char *rootpath;

    } flist_backend_t;

    typedef struct flist_backend_data_t {
        void *opaque;
        unsigned char *payload;
        size_t length;

    } flist_backend_data_t;


    typedef struct flist_stats_t {
        size_t regular;     // number of regular files
        size_t directory;   // number of directories
        size_t symlink;     // number of symbolic links
        size_t special;     // number of special devices/files
        size_t failure;     // number of failure (upload, check, ...)
        size_t size;        // total amount of bytes of files

    } flist_stats_t;


    typedef struct flist_buffer_t {
        uint8_t *data;
        size_t length;

    } flist_buffer_t;

    typedef struct flist_chunk_t {
        flist_buffer_t id;
        flist_buffer_t cipher;
        flist_buffer_t plain;
        flist_buffer_t encrypted;

    } flist_chunk_t;

    typedef struct flist_chunks_t {
        size_t upsize;  // uploaded size
        size_t length;  // amount of chunks
        flist_chunk_t **chunks;

    } flist_chunks_t;


    typedef struct flist_merge_t {
        size_t length;
        char **sources;

    } flist_merge_t;

    //
    // ----------------------
    // welcome in the jungle
    // everything here need to be review
    // and rewritten in a library perspective
    //
    // see below for better


    // initialize a backend



    // hashing




    // convert a binary hash into hexadecimal hash

    dirnode_t *libflist_directory_get(flist_db_t *database, char *path);
    dirnode_t *libflist_directory_get_recursive(flist_db_t *database, char *path);
    void inode_free(inode_t *inode);

    directory_t *flist_directory_get(flist_db_t *database, char *key, char *fullpath);

    void flist_directory_close(flist_db_t *database, directory_t *dir);

    char *libflist_path_key(char *path);

    int flist_walk(flist_db_t *database);

    dirnode_t *dirnode_appends_inode(dirnode_t *root, inode_t *inode);
    dirnode_t *dirnode_lazy_appends_inode(dirnode_t *root, inode_t *inode);

    dirnode_t *dirnode_lazy_appends_dirnode(dirnode_t *root, dirnode_t *dir);
    dirnode_t *dirnode_appends_dirnode(dirnode_t *root, dirnode_t *dir);

    dirnode_t *dirnode_lazy_duplicate(dirnode_t *source);
    inode_t *inode_lazy_duplicate(inode_t *source);

    flist_acl_t *libflist_get_permissions(flist_db_t *database, const char *aclkey);
    flist_acl_t *libflist_racl_to_acl(acl_t *dst, flist_acl_t *src);
    void inode_acl_persist(flist_db_t *database, acl_t *acl);

    //
    // --------------------------------------------------
    // at that point, function are reviewed and handle
    // error in a library point of view and are exported
    // here publicly

    //
    // verbose.c
    //
    extern int libflist_debug_flag;

    const char *libflist_strerror();
    void libflist_debug_enable(int enable);

    char *libflist_hashhex(unsigned char *hash, int length);
    void *libflist_bufdup(void *source, size_t length);

    //
    // archive.c
    //
    char *libflist_archive_extract(char *filename, char *target);
    char *libflist_archive_create(char *filename, char *source);

    //
    // backend.c
    //
    flist_backend_t *libflist_backend_init(flist_db_t *database, char *rootpath);
    void libflist_backend_free(flist_backend_t *backend);

    flist_chunks_t *libflist_backend_upload_file(flist_backend_t *context, char *filename);
    flist_chunks_t *libflist_backend_upload_inode(flist_backend_t *backend, char *path, char *filename);
    int libflist_backend_upload_chunk(flist_backend_t *context, flist_chunk_t *chunk);

    flist_chunk_t *libflist_backend_download_chunk(flist_backend_t *backend, flist_chunk_t *chunk);

    void libflist_backend_chunks_free(flist_chunks_t *chunks);

    //
    // workspace.c
    //
    char *libflist_workspace_create();
    char *libflist_workspace_destroy(char *mountpoint);

    char *libflist_ramdisk_create();
    char *libflist_ramdisk_destroy(char *mountpoint);

    //
    // database_redis.c
    //
    flist_db_t *libflist_db_redis_init_tcp(char *host, int port, char *hset, char *password, char *token);
    flist_db_t *libflist_db_redis_init_unix(char *socket, char *namespace, char *password, char *token);

    //
    // database_sqlite.c
    //
    flist_db_t *libflist_db_sqlite_init(char *rootpath);

    //
    // zero_chunk.c
    //
    uint8_t *libflist_chunk_hash(const void *buffer, size_t length);

    flist_chunk_t *libflist_chunk_new(uint8_t *hash, uint8_t *key, void *data, size_t length);
    flist_chunk_t *libflist_chunk_encrypt(const uint8_t *chunk, size_t chunksize);
    flist_chunk_t *libflist_chunk_decrypt(flist_chunk_t *chunk);

    void libflist_chunk_free(flist_chunk_t *chunk);

    //
    // flist_write.c
    //
    int libflist_create_excluders_append(char *regex);
    void libflist_create_excluders_free();

    dirnode_t *libflist_dirnode_search(dirnode_t *root, char *dirname);
    inode_t *libflist_inode_search(dirnode_t *root, char *inodename);

    void libflist_dirnode_commit(dirnode_t *root, flist_db_t *database, dirnode_t *parent, flist_backend_t *backend);
    char *libflist_inode_acl_key(acl_t *acl);

    flist_stats_t *libflist_create(flist_db_t *database, const char *root, flist_backend_t *backend);

    //
    // flist_merger.c
    //
    int libflist_merge_list_init(flist_merge_t *merge);
    int libflist_merge_list_append(flist_merge_t *merge, char *path);
    int libflist_merge_list_free(flist_merge_t *merge);
    dirnode_t *libflist_merge(dirnode_t **fulltree, dirnode_t *source);

    //
    // flist_dumps.c
    //
    void libflist_inode_dumps(inode_t *inode, dirnode_t *rootdir);
    void libflist_dirnode_dumps(dirnode_t *root);
#endif
