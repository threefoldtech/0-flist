#ifndef ZFLISTEDIT_TOOLS_H
    #define ZFLISTEDIT_TOOLS_H

    // automatic cleanup
    void __cleanup_free(void *p);

    flist_db_t *zf_init(char *mountpoint);

    char zf_ls_inode_type(inode_t *inode);
    void zf_ls_inode_perm(inode_t *inode);
#endif