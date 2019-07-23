#ifndef ZFLISTEDIT_ACTIONS_H
    #define ZFLISTEDIT_ACTIONS_H


    int zf_open(zf_callback_t *cb);
    int zf_init(zf_callback_t *cb);
    int zf_commit(zf_callback_t *cb);

    int zf_chmod(zf_callback_t *cb);
    int zf_rm(zf_callback_t *cb);
    int zf_rmdir(zf_callback_t *cb);
    int zf_mkdir(zf_callback_t *cb);
    int zf_ls(zf_callback_t *cb);
    int zf_stat(zf_callback_t *cb);
    int zf_cat(zf_callback_t *cb);
    int zf_put(zf_callback_t *cb);
    int zf_putdir(zf_callback_t *cb);
    int zf_metadata(zf_callback_t *cb);
#endif