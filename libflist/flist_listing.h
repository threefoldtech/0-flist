#ifndef LIBFLIST_LISTING
    #define LIBFLIST_LISTING

    typedef struct flist_json_t {
        json_t *root;
        json_t *content;

        size_t regular;
        size_t symlink;
        size_t directory;
        size_t special;

    } flist_json_t;

    int flist_listing(database_t *database, settings_t *settings);
#endif
