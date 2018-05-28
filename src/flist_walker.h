#ifndef FLISTER_WALKER
    #define FLISTER_WALKER

    typedef struct walker_t {
        database_t *database;
        void *userptr;
        void (*callback)(struct walker_t *, directory_t *);
        void (*postproc)(struct walker_t *);

    } walker_t;

    int flist_walk_directory(walker_t *walker, const char *key);
#endif
