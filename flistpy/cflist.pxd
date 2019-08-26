cdef extern from "<stdint.h>" nogil:

    # 7.18.1 Integer types
    # 7.18.1.1 Exact-width integer types
    ctypedef   signed char  int8_t
    ctypedef   signed short int16_t
    ctypedef   signed int   int32_t
    ctypedef   signed long  int64_t
    ctypedef unsigned char  uint8_t
    ctypedef unsigned short uint16_t
    ctypedef unsigned int   uint32_t
    ctypedef unsigned long long uint64_t
    # 7.18.1.2 Minimum-width integer types
    ctypedef   signed char  int_least8_t
    ctypedef   signed short int_least16_t
    ctypedef   signed int   int_least32_t
    ctypedef   signed long  int_least64_t
    ctypedef unsigned char  uint_least8_t
    ctypedef unsigned short uint_least16_t
    ctypedef unsigned int   uint_least32_t
    ctypedef unsigned long long uint_least64_t
    # 7.18.1.3 Fastest minimum-width integer types
    ctypedef   signed char  int_fast8_t
    ctypedef   signed short int_fast16_t
    ctypedef   signed int   int_fast32_t
    ctypedef   signed long  int_fast64_t
    ctypedef unsigned char  uint_fast8_t
    ctypedef unsigned short uint_fast16_t
    ctypedef unsigned int   uint_fast32_t
    ctypedef unsigned long long uint_fast64_t
    # 7.18.1.4 Integer types capable of holding object pointers
    ctypedef ssize_t intptr_t
    ctypedef  size_t uintptr_t
    # 7.18.1.5 Greatest-width integer types
    ctypedef signed   long long intmax_t
    ctypedef unsigned long long uintmax_t

    # 7.18.2 Limits of specified-width integer types
    # 7.18.2.1 Limits of exact-width integer types
    int8_t   INT8_MIN
    int16_t  INT16_MIN
    int32_t  INT32_MIN
    int64_t  INT64_MIN
    int8_t   INT8_MAX
    int16_t  INT16_MAX
    int32_t  INT32_MAX
    int64_t  INT64_MAX
    uint8_t  UINT8_MAX
    uint16_t UINT16_MAX
    uint32_t UINT32_MAX
    uint64_t UINT64_MAX
    #7.18.2.2 Limits of minimum-width integer types
    int_least8_t     INT_LEAST8_MIN
    int_least16_t   INT_LEAST16_MIN
    int_least32_t   INT_LEAST32_MIN
    int_least64_t   INT_LEAST64_MIN
    int_least8_t     INT_LEAST8_MAX
    int_least16_t   INT_LEAST16_MAX
    int_least32_t   INT_LEAST32_MAX
    int_least64_t   INT_LEAST64_MAX
    uint_least8_t   UINT_LEAST8_MAX
    uint_least16_t UINT_LEAST16_MAX
    uint_least32_t UINT_LEAST32_MAX
    uint_least64_t UINT_LEAST64_MAX
    #7.18.2.3 Limits of fastest minimum-width integer types
    int_fast8_t     INT_FAST8_MIN
    int_fast16_t   INT_FAST16_MIN
    int_fast32_t   INT_FAST32_MIN
    int_fast64_t   INT_FAST64_MIN
    int_fast8_t     INT_FAST8_MAX
    int_fast16_t   INT_FAST16_MAX
    int_fast32_t   INT_FAST32_MAX
    int_fast64_t   INT_FAST64_MAX
    uint_fast8_t   UINT_FAST8_MAX
    uint_fast16_t UINT_FAST16_MAX
    uint_fast32_t UINT_FAST32_MAX
    uint_fast64_t UINT_FAST64_MAX
    #7.18.2.4 Limits of integer types capable of holding object pointers
    enum:  INTPTR_MIN
    enum:  INTPTR_MAX
    enum: UINTPTR_MAX
    # 7.18.2.5 Limits of greatest-width integer types
    enum:  INTMAX_MAX
    enum:  INTMAX_MIN
    enum: UINTMAX_MAX

    # 7.18.3 Limits of other integer types
    # ptrdiff_t
    enum: PTRDIFF_MIN
    enum: PTRDIFF_MAX
    # sig_atomic_t
    enum: SIG_ATOMIC_MIN
    enum: SIG_ATOMIC_MAX
    # size_t
    size_t SIZE_MAX
    # wchar_t
    enum: WCHAR_MIN
    enum: WCHAR_MAX
    # wint_t
    enum: WINT_MIN
    enum: WINT_MAX

cdef extern from "<time.h>" nogil:
    ctypedef long time_t


cdef extern from "libflist.h":
    ctypedef struct acl_t:
        char *uname;     
        char *gname;     
        uint16_t mode;   
        char *key;       

    
    ctypedef enum inode_type_t:
        INODE_DIRECTORY,
        INODE_FILE,
        INODE_LINK,
        INODE_SPECIAL,


    ctypedef enum inode_special_t:
        SOCKET,
        BLOCK,
        CHARDEV,
        FIFOPIPE,
        UNKNOWN,


    
    ctypedef struct inode_chunk_t:
        uint8_t *entryid;      
        uint8_t entrylen;      
        uint8_t *decipher;     
        uint8_t decipherlen;   


    
    
    ctypedef struct inode_chunks_t:
        size_t size;          
        size_t blocksize;     
        inode_chunk_t *list;  


    ctypedef struct inode_t:
        char *name;       
        char *fullpath;   
        size_t size;      
        acl_t *acl;       

        inode_type_t type;       
        time_t modification;     
        time_t creation;         

        char *subdirkey;         
        inode_special_t stype;   
        char *sdata;             
        char *link;              
        inode_chunks_t *chunks;  

        inode_t *next;


    ctypedef struct dirnode_t:
        inode_t *inode_list;
        inode_t *inode_last;
        size_t inode_length;

        dirnode_t *dir_list;
        dirnode_t *dir_last;
        size_t dir_length;

        char *fullpath;        
        char *name;            
        char *hashkey;         

        acl_t *acl;            
        time_t creation;       
        time_t modification;   

        dirnode_t *next;




    ctypedef struct flist_db_value_t:
        void *handler;

        char *data;
        size_t length;

    ctypedef flist_db_value_t value_t 


    ctypedef struct flist_db_t:
        void *handler;
        char *type;

        flist_db_t* (*open)(flist_db_t *db);
        flist_db_t* (*create)(flist_db_t *db);
        void (*close)(flist_db_t *db);

        value_t* (*get)(flist_db_t *db, uint8_t *key, size_t keylen);
        int (*set)(flist_db_t *db, uint8_t *key, size_t keylen, uint8_t *data, size_t datalen);
        #int (*del)(flist_db_t *db, uint8_t *key, size_t keylen);
        int (*exists)(flist_db_t *db, uint8_t *key, size_t keylen);

        value_t* (*sget)(flist_db_t *db, char *key);
        int (*sset)(flist_db_t *db, char *key, uint8_t *data, size_t datalen);
        int (*sdel)(flist_db_t *db, char *key);
        int (*sexists)(flist_db_t *db, char *key);

        value_t* (*mdget)(flist_db_t *db, char *key);
        int (*mdset)(flist_db_t *db, char *key, char *data);
        int (*mddel)(flist_db_t *db, char *key);

        void (*clean)(value_t *value);


    ctypedef enum flist_db_type_t:
        SQLITE3,
        REDIS,


    ctypedef struct flist_backend_t:
        flist_db_t *database;
        char *rootpath;


    ctypedef struct flist_backend_data_t:
        void *opaque;
        unsigned char *payload;
        size_t length;



    ctypedef struct flist_stats_t:
        size_t regular;     
        size_t directory;   
        size_t symlink;     
        size_t special;     
        size_t failure;     
        size_t size;        



    ctypedef struct flist_buffer_t:
        uint8_t *data;
        size_t length;


    ctypedef struct flist_chunk_t:
        flist_buffer_t id;
        flist_buffer_t cipher;
        flist_buffer_t plain;
        flist_buffer_t encrypted;


    ctypedef struct flist_chunks_t:
        size_t upsize;  
        size_t length;  
        flist_chunk_t **chunks;



    ctypedef struct flist_ctx_t:
        flist_db_t *db;
        flist_backend_t *backend;
        flist_stats_t stats;


    #define FLIST_ENTRY_KEY_LENGTH  16
    #define FLIST_ACL_KEY_LENGTH    8


    extern int libflist_debug_flag;

    const char *libflist_strerror();
    void libflist_debug_enable(int enable);

    char *libflist_hashhex(unsigned char *hash, int length);
    void *libflist_bufdup(void *source, size_t length);

    char *libflist_archive_extract(char *filename, char *target);
    char *libflist_archive_create(char *filename, char *source);

    flist_backend_t *libflist_backend_init(flist_db_t *database, char *rootpath);
    void libflist_backend_free(flist_backend_t *backend);

    flist_chunks_t *libflist_backend_upload_file(flist_backend_t *context, char *filename);
    flist_chunks_t *libflist_backend_upload_inode(flist_backend_t *backend, char *path, char *filename);
    int libflist_backend_upload_chunk(flist_backend_t *context, flist_chunk_t *chunk);
    int libflist_backend_chunk_commit(flist_backend_t *context, flist_chunk_t *chunk);

    flist_chunk_t *libflist_backend_download_chunk(flist_backend_t *backend, flist_chunk_t *chunk);

    void libflist_backend_chunks_free(flist_chunks_t *chunks);

    flist_db_t *libflist_db_redis_init_tcp(char *host, int port, char *hset, char *password, char *token);
    flist_db_t *libflist_db_redis_init_unix(char *socket, char *namespace, char *password, char *token);

    flist_db_t *libflist_db_sqlite_init(char *rootpath);

    inode_chunks_t *libflist_chunks_compute(char *localfile);
    inode_chunks_t *libflist_chunks_proceed(char *localfile, flist_ctx_t *ctx);

    uint8_t *libflist_chunk_hash(const void *buffer, size_t length);

    flist_chunk_t *libflist_chunk_new(uint8_t *hash, uint8_t *key, void *data, size_t length);
    flist_chunk_t *libflist_chunk_encrypt(const uint8_t *chunk, size_t chunksize);
    flist_chunk_t *libflist_chunk_decrypt(flist_chunk_t *chunk);

    void libflist_chunk_free(flist_chunk_t *chunk);

    
    flist_ctx_t *libflist_context_create(flist_db_t *db, flist_backend_t *backend);
    void libflist_context_free(flist_ctx_t *ctx);

    char *libflist_path_key(char *path);
    
    acl_t *libflist_acl_new(char *uname, char *gname, int mode);
    char *libflist_acl_key(acl_t *acl);
    acl_t *libflist_acl_duplicate(acl_t *source);
    acl_t *libflist_acl_commit(acl_t *acl);
    void libflist_acl_free(acl_t *acl);

    dirnode_t *libflist_dirnode_create(char *fullpath, char *name);
    dirnode_t *libflist_dirnode_search(dirnode_t *root, char *dirname);
    dirnode_t *libflist_dirnode_get(flist_db_t *database, char *path);
    dirnode_t *libflist_dirnode_get_recursive(flist_db_t *database, char *path);
    dirnode_t *libflist_dirnode_get_parent(flist_db_t *database, dirnode_t *root);
    dirnode_t *libflist_dirnode_lookup_dirnode(dirnode_t *root, const char *dirname);
    dirnode_t *libflist_dirnode_appends_inode(dirnode_t *root, inode_t *inode);

    void libflist_dirnode_free(dirnode_t *dirnode);
    void libflist_dirnode_free_recursive(dirnode_t *dirnode);

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

    dirnode_t *libflist_merge(flist_ctx_t *source, flist_ctx_t *target);

    
    void libflist_inode_dumps(inode_t *inode, dirnode_t *rootdir);
    void libflist_dirnode_dumps(dirnode_t *root);
    void libflist_stats_dump(flist_stats_t *stats);

    int libflist_metadata_set(flist_db_t *database, char *metadata, char *payload);
    int libflist_metadata_remove(flist_db_t *database, char *metadata);
    char *libflist_metadata_get(flist_db_t *database, char *metadata);
    flist_db_t *libflist_metadata_backend_database(flist_db_t *database);
    flist_db_t *libflist_metadata_backend_database_json(char *input);
    
    void libflist_serial_dirnode_commit(dirnode_t *root, flist_ctx_t *ctx, dirnode_t *parent);
    acl_t *libflist_serial_acl_get(flist_db_t *database, const char *aclkey);

    size_t libflist_stats_regular_add(flist_ctx_t *ctx, size_t amount);
    size_t libflist_stats_directory_add(flist_ctx_t *ctx, size_t amount);
    size_t libflist_stats_symlink_add(flist_ctx_t *ctx, size_t amount);
    size_t libflist_stats_special_add(flist_ctx_t *ctx, size_t amount);
    size_t libflist_stats_size_add(flist_ctx_t *ctx, size_t amount);
    flist_stats_t *libflist_stats_get(flist_ctx_t *ctx);

    