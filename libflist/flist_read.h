#ifndef LIBFLIST_FLIST_READ_H
    #define LIBFLIST_FLIST_READ_H

    inode_t *flist_itementry_to_inode(flist_db_t *database, struct Dir *dir, int fileindex);
    inode_t *flist_inode_from_dirnode(dirnode_t *dirnode);
    char *flist_clean_path(char *path);

    void inode_chunks_free(inode_t *inode);
#endif
