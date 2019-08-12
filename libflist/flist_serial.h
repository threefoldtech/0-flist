#ifndef LIBFLIST_FLIST_SERIAL_H
    #define LIBFLIST_FLIST_SERIAL_H

    void flist_dirnode_commit(dirnode_t *root, flist_ctx_t *ctx, dirnode_t *parent);
    char *flist_inode_fullpath(struct Dir *dir, struct Inode *inode);
    dirnode_t *flist_dir_to_dirnode(flist_db_t *database, struct Dir *dir);
    dirnode_t *xxx_flist_dirnode_get(flist_db_t *database, char *key, char *fullpath);
#endif
