#ifndef LIBFLIST_FLIST_INODE_H
    #define LIBFLIST_FLIST_INODE_H

    inode_t *flist_inode_create(const char *name, size_t size, const char *fullpath);
    inode_t *flist_inode_duplicate(inode_t *source);
    void flist_inode_chunks_free(inode_t *inode);
    void flist_inode_free(inode_t *inode);

    inode_t *flist_inode_search(dirnode_t *root, char *inodename);

    dirnode_t *flist_directory_rm_inode(dirnode_t *root, inode_t *target);
    int flist_directory_rm_recursively(flist_db_t *database, dirnode_t *dirnode);

    inode_t *flist_inode_mkdir(char *name, dirnode_t *parent);
    inode_t *flist_inode_rename(inode_t *inode, char *name);

    inode_t *flist_inode_from_localfile(char *localpath, dirnode_t *parent, flist_ctx_t *ctx);
    inode_t *flist_inode_from_localdir(char *localdir, dirnode_t *parent, flist_ctx_t *ctx);
    inode_t *flist_inode_from_dirnode(dirnode_t *dirnode);
#endif
