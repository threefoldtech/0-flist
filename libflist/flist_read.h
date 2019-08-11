#ifndef LIBFLIST_FLIST_READ_H
    #define LIBFLIST_FLIST_READ_H

    dirnode_t *flist_dirnode_from_inode(inode_t *inode);
    inode_t *flist_inode_from_dirnode(dirnode_t *dirnode);

    void inode_chunks_free(inode_t *inode);
#endif
