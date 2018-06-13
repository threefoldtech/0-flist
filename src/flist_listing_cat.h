#ifndef FLISTER_LIST_CAT
    #define FLISTER_LIST_CAT

    void *flist_cat_init();
    int flist_cat(walker_t *walker, directory_t *root);
    void flist_cat_done(walker_t *walker);
#endif
