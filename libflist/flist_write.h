#ifndef LIBFLIST_FLIST_WRITE_H
    #define LIBFLIST_FLIST_WRITE_H

    /*
    typedef struct excluder_t {
        regex_t regex;
        char *str;

    } excluder_t;

    typedef struct excluders_t {
        size_t length;
        excluder_t *list;

    } excluders_t;
    */

    inode_t *flist_inode_mkdir(char *name, dirnode_t *parent);
    void inode_free(inode_t *inode);
#endif
