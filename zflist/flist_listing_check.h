#ifndef LIBFLIST_LIST_CHECK
    #define LIBFLIST_LIST_CHECK

    void *flist_check_init(zflist_settings_t *settings);
    int flist_check(walker_t *walker, directory_t *root);
    void flist_check_done(walker_t *walker);
#endif
