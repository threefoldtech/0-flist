#ifndef LIBFLIST_FLIST_DIRNODE_H
    #define LIBFLIST_FLIST_DIRNODE_H

    #include <sys/types.h>
    #include <sys/stat.h>

    dirnode_t *flist_dirnode_create(char *fullpath, char *name);
    dirnode_t *flist_dirnode_create_from_stat(dirnode_t *parent, const char *name, const struct stat *sb);
    dirnode_t *flist_dirnode_lazy_appends_inode(dirnode_t *root, inode_t *inode);
    dirnode_t *flist_dirnode_appends_inode(dirnode_t *root, inode_t *inode);
    dirnode_t *flist_dirnode_lazy_appends_dirnode(dirnode_t *root, dirnode_t *dir);
    dirnode_t *flist_dirnode_appends_dirnode(dirnode_t *root, dirnode_t *dir);
    dirnode_t *flist_dirnode_duplicate(dirnode_t *source);
    dirnode_t *flist_dirnode_from_inode(inode_t *inode);
    dirnode_t *flist_dirnode_get(flist_db_t *database, char *path);
    dirnode_t *flist_dirnode_get_recursive(flist_db_t *database, char *path);
    dirnode_t *flist_dirnode_get_parent(flist_db_t *database, dirnode_t *root);
    dirnode_t *flist_dirnode_lookup_dirnode(dirnode_t *root, const char *dirname);

    void flist_dirnode_free(dirnode_t *dirnode);
    void flist_dirnode_free_recursive(dirnode_t *dirnode);

    int flist_dirnode_is_root(dirnode_t *dirnode);
    char *flist_dirnode_virtual_path(dirnode_t *parent, char *target);
#endif
