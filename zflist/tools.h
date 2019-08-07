#ifndef ZFLISTEDIT_TOOLS_H
    #define ZFLISTEDIT_TOOLS_H

    // automatic cleanup
    void __cleanup_free(void *p);

    flist_ctx_t *zf_internal_init(char *mountpoint);
    flist_ctx_t *zf_backend_detect(flist_ctx_t *ctx);

    char zf_ls_inode_type(inode_t *inode);
    void zf_ls_inode_perm(inode_t *inode);
    int zf_stat_inode(inode_t *target);

    void zf_error(char *function, char *message, ...);
#endif
