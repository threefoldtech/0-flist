#ifndef FLIST_READ_H
    #define FLIST_READ_H

    #include "database.h"

    typedef struct directory_t {
        value_t *value;
        struct capn ctx;
        Dir_ptr dirp;
        struct Dir dir;

    } directory_t;

    directory_t *flist_directory_get(database_t *database, const char *key);
    void flist_directory_close(directory_t *dir);

    char *flist_fullpath(directory_t *root, struct Inode *inode);
    char *flist_read_permstr(unsigned int mode, char *modestr, size_t slen);
    const char *flist_pathkey(char *path);

    int flist_walk(database_t *database);
#endif
