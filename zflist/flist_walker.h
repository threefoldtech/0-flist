#ifndef LIBFLIST_WALKER
    #define LIBFLIST_WALKER

    typedef struct walker_t {
        flist_db_t *database;
        void *userptr;
        int (*callback)(struct walker_t *, directory_t *);
        void (*postproc)(struct walker_t *);

    } walker_t;

    int flist_walk_directory(walker_t *walker, const char *key, const char *fullpath);
#endif
