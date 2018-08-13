#ifndef LIBFLIST_H
    #define LIBFLIST_H

    #include <stdint.h>
    #include <time.h>

    #ifdef FLIST_DEBUG
        #define debug(...) { printf(__VA_ARGS__); }
    #else
        #define debug(...) ((void)0)
    #endif

    // FIXME -- should not be defined here
    typedef struct settings_t {
        int ramdisk;       // do we extract to ramdisk
        char *archive;     // required input/output archive

        char *create;      // create root path
        size_t rootlen;    // create root string length

        char *backendhost;   // backend host
        int backendport;     // backend port

        int json;            // json output flag
        char *targetfile;    // specific target file (see cat action)
        char *outputfile;    // specific output file (see cat action)
        // list_mode_t list;    // list view mode
        // merge_list_t merge;  // list of merging flists

    } settings_t;

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
        int (*exists)(struct flist_db_t *db, uint8_t *key, size_t keylen);

        value_t* (*sget)(struct flist_db_t *db, char *key);
        int (*sset)(struct flist_db_t *db, char *key, uint8_t *data, size_t datalen);
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


    // initialize a backend
    flist_backend_t *backend_init(flist_db_t *database, char *rootpath);



    char *libflist_archive_extract(char *filename, char *target);
    int libflist_archive_create(char *filename, char *source);


    int flist_merger(flist_db_t *database, void *merge);


    // initializers
    flist_db_t *libflist_db_redis_init_tcp(char *host, int port, char *hset, char *password);
    flist_db_t *libflist_db_redis_init_unix(char *socket, char *namespace, char *password);

    // initializers
    flist_db_t *libflist_db_sqlite_init(char *rootpath);

    // hashing
    uint8_t *zchunk_hash(const void *buffer, size_t length);



    char *libflist_workspace_create();
    char *libflist_workspace_destroy(char *mountpoint);
    char *libflist_ramdisk_create();
    char *libflist_ramdisk_destroy(char *mountpoint);

    // convert a binary hash into hexadecimal hash
    char *libflist_hashhex(unsigned char *hash, int length);

    dirnode_t *libflist_directory_get(flist_db_t *database, char *path);

    directory_t *flist_directory_get(flist_db_t *database, char *key, char *fullpath);
    void flist_directory_close(flist_db_t *database, directory_t *dir);

    char *libflist_path_key(char *path);

    int flist_walk(flist_db_t *database);

    dirnode_t *dirnode_appends_inode(dirnode_t *root, inode_t *inode);

    void *libflist_bufdup(void *source, size_t length);
#endif
