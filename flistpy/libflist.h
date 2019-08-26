#ifndef LIBFLIST_H
    #define LIBFLIST_H

    //
    // libflist
    //
    //   flist main library to create, list, update an flist file
    //   this file will be more documented with time, but it already changed
    //   a lot across time and is way more much readable now than before
    //

    #include <stdint.h>
    #include <time.h>
    #include <jansson.h>

    typedef struct acl_t {
        char *uname;     // username (user id if not found)
        char *gname;     // group name (group id if not found)
        uint16_t mode;   // integer file mode
        char *key;       // hash of the payload (dedupe in db)

    } acl_t;

    // mapping capnp type
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
        acl_t *acl;       // access control

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

        acl_t *acl;            // access control
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

        value_t* (*mdget)(struct flist_db_t *db, char *key);
        int (*mdset)(struct flist_db_t *db, char *key, char *data);
        int (*mddel)(struct flist_db_t *db, char *key);

        void (*clean)(value_t *value);

    } flist_db_t;

    typedef enum flist_db_type_t {
        SQLITE3,
        REDIS,

    } flist_db_type_t;

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


    typedef struct flist_ctx_t {
        flist_db_t *db;
        flist_backend_t *backend;
        flist_stats_t stats;

    } flist_ctx_t;

    #define FLIST_ENTRY_KEY_LENGTH  16
    #define FLIST_ACL_KEY_LENGTH    8

    //
    // ------------------------
    //  public function declaration
    // ------------------------
    //

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
    //   everything related to tar format (packing and unpacking)
    //   which is the default base format of flist
    //
    char *libflist_archive_extract(char *filename, char *target);
    char *libflist_archive_create(char *filename, char *source);

    //
    // backend.c
    //
    //   everything related to set/get things onto backend database
    //   which are file payload and chunks
    //
    flist_backend_t *libflist_backend_init(flist_db_t *database, char *rootpath);
    void libflist_backend_free(flist_backend_t *backend);

    flist_chunks_t *libflist_backend_upload_file(flist_backend_t *context, char *filename);
    flist_chunks_t *libflist_backend_upload_inode(flist_backend_t *backend, char *path, char *filename);
    int libflist_backend_upload_chunk(flist_backend_t *context, flist_chunk_t *chunk);
    int libflist_backend_chunk_commit(flist_backend_t *context, flist_chunk_t *chunk);

    flist_chunk_t *libflist_backend_download_chunk(flist_backend_t *backend, flist_chunk_t *chunk);

    void libflist_backend_chunks_free(flist_chunks_t *chunks);

    //
    // database_redis.c
    //
    //   everything related to redis based database, which include zero-db
    //   this can be used as backend or as flist temporary database
    //
    flist_db_t *libflist_db_redis_init_tcp(char *host, int port, char *hset, char *password, char *token);
    flist_db_t *libflist_db_redis_init_unix(char *socket, char *namespace, char *password, char *token);

    //
    // database_sqlite.c
    //
    //   same as redis, but for sqlite, which is the default backend used for
    //   flist metadata and entries
    //
    flist_db_t *libflist_db_sqlite_init(char *rootpath);

    //
    // zero_chunk.c
    //
    //   code used by chunk system, which split, compress and encrypt
    //   a file (or cat, uncompress and uncrypt), into/from a backend
    //
    inode_chunks_t *libflist_chunks_compute(char *localfile);
    inode_chunks_t *libflist_chunks_proceed(char *localfile, flist_ctx_t *ctx);

    uint8_t *libflist_chunk_hash(const void *buffer, size_t length);

    flist_chunk_t *libflist_chunk_new(uint8_t *hash, uint8_t *key, void *data, size_t length);
    flist_chunk_t *libflist_chunk_encrypt(const uint8_t *chunk, size_t chunksize);
    flist_chunk_t *libflist_chunk_decrypt(flist_chunk_t *chunk);

    void libflist_chunk_free(flist_chunk_t *chunk);

    //
    // flist_tools.c
    //
    flist_ctx_t *libflist_context_create(flist_db_t *db, flist_backend_t *backend);
    void libflist_context_free(flist_ctx_t *ctx);

    char *libflist_path_key(char *path);

    //
    // flist_acl.c
    //
    acl_t *libflist_acl_new(char *uname, char *gname, int mode);
    char *libflist_acl_key(acl_t *acl);
    acl_t *libflist_acl_duplicate(acl_t *source);
    acl_t *libflist_acl_commit(acl_t *acl);
    void libflist_acl_free(acl_t *acl);

    //
    // flist_dirnode.c
    //
    dirnode_t *libflist_dirnode_create(char *fullpath, char *name);
    dirnode_t *libflist_dirnode_search(dirnode_t *root, char *dirname);
    dirnode_t *libflist_dirnode_get(flist_db_t *database, char *path);
    dirnode_t *libflist_dirnode_get_recursive(flist_db_t *database, char *path);
    dirnode_t *libflist_dirnode_get_parent(flist_db_t *database, dirnode_t *root);
    dirnode_t *libflist_dirnode_lookup_dirnode(dirnode_t *root, const char *dirname);
    dirnode_t *libflist_dirnode_appends_inode(dirnode_t *root, inode_t *inode);

    void libflist_dirnode_free(dirnode_t *dirnode);
    void libflist_dirnode_free_recursive(dirnode_t *dirnode);

    //
    // flist_inode.c
    //
    inode_t *libflist_inode_mkdir(char *name, dirnode_t *parent);
    inode_t *libflist_inode_rename(inode_t *inode, char *name);
    inode_t *libflist_inode_from_localfile(char *localpath, dirnode_t *parent, flist_ctx_t *ctx);
    inode_t *libflist_inode_from_localdir(char *localdir, dirnode_t *parent, flist_ctx_t *ctx);
    inode_t *libflist_inode_search(dirnode_t *root, char *inodename);
    inode_t *libflist_inode_from_name(dirnode_t *root, char *filename);

    inode_t *libflist_directory_create(dirnode_t *parent, char *name);
    dirnode_t *libflist_directory_rm_inode(dirnode_t *root, inode_t *target);
    int libflist_directory_rm_recursively(flist_db_t *database, dirnode_t *dirnode);

    void libflist_inode_free(inode_t *inode);

    //
    // flist_merger.c
    //
    dirnode_t *libflist_merge(flist_ctx_t *source, flist_ctx_t *target);

    //
    // flist_dumps.c
    //
    //   debug functions used to dumps inode, directories, ...
    //
    void libflist_inode_dumps(inode_t *inode, dirnode_t *rootdir);
    void libflist_dirnode_dumps(dirnode_t *root);
    void libflist_stats_dump(flist_stats_t *stats);

    //
    // metadata.c
    //
    //   everything related to metadata, which include backend information, ports,
    //   volumes, readme, etc.
    //
    int libflist_metadata_set(flist_db_t *database, char *metadata, char *payload);
    int libflist_metadata_remove(flist_db_t *database, char *metadata);
    char *libflist_metadata_get(flist_db_t *database, char *metadata);
    flist_db_t *libflist_metadata_backend_database(flist_db_t *database);
    flist_db_t *libflist_metadata_backend_database_json(char *input);

    //
    // flist_serial.c
    //
    //   everything used for serialization and deserialization from/to capnp object
    //   since capnp can be really a mess, you won't have to deal with this in other place
    //   than this file, where you can set/read object and use internal struct to deal
    //   with information (and not with capnp object which are difficult to use)
    void libflist_serial_dirnode_commit(dirnode_t *root, flist_ctx_t *ctx, dirnode_t *parent);
    acl_t *libflist_serial_acl_get(flist_db_t *database, const char *aclkey);
    //
    // statistics.c
    //
    size_t libflist_stats_regular_add(flist_ctx_t *ctx, size_t amount);
    size_t libflist_stats_directory_add(flist_ctx_t *ctx, size_t amount);
    size_t libflist_stats_symlink_add(flist_ctx_t *ctx, size_t amount);
    size_t libflist_stats_special_add(flist_ctx_t *ctx, size_t amount);
    size_t libflist_stats_size_add(flist_ctx_t *ctx, size_t amount);
    flist_stats_t *libflist_stats_get(flist_ctx_t *ctx);

#endif
