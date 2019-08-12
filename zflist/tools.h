#ifndef ZFLISTEDIT_TOOLS_H
    #define ZFLISTEDIT_TOOLS_H

    // automatic cleanup
    void __cleanup_free(void *p);

    flist_ctx_t *zf_internal_init(char *mountpoint);
    void zf_internal_cleanup(flist_ctx_t *ctx);

    void zf_internal_json_init(zf_callback_t *cb);
    void zf_internal_json_finalize(zf_callback_t *cb);

    flist_ctx_t *zf_backend_detect(flist_ctx_t *ctx);

    int zf_open_file(zf_callback_t *cb, char *filename, char *endpoint);
    int zf_remove_database(zf_callback_t *cb, char *mountpoint);

    char zf_ls_inode_type(inode_t *inode);
    void zf_ls_inode_perm(inode_t *inode);
    int zf_stat_inode(zf_callback_t *cb, inode_t *target);

    int zf_find_recursive(zf_callback_t *cb, dirnode_t *dirnode);
    int zf_find_finalize(zf_callback_t *cb);

    void zf_error(zf_callback_t *cb, char *function, char *message, ...);

    void zf_warnp(zf_callback_t *cb, const char *str);
    void zf_diep(zf_callback_t *cb, const char *str);
    void zf_dies(zf_callback_t *cb, const char *str);

#endif
