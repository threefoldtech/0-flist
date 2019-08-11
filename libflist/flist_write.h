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

    inode_t *inode_create(const char *name, size_t size, const char *fullpath);
    void inode_free(inode_t *inode);

    inode_t *flist_inode_mkdir(char *name, dirnode_t *parent);
#endif
