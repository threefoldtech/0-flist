#ifndef LIBFLIST_LIST_TREE
    #define LIBFLIST_LIST_TREE

    void *flist_tree_init();
    int flist_tree(walker_t *walker, directory_t *root);
    void flist_tree_done(walker_t *walker);
#endif