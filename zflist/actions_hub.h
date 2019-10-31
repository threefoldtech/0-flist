#ifndef ZFLIST_ACTIONS_HUB_H
    #define ZFLIST_ACTIONS_HUB_H

    int zf_hub_upload(zf_callback_t *cb);
    int zf_hub_promote(zf_callback_t *cb);
    int zf_hub_symlink(zf_callback_t *cb);
    int zf_hub_delete(zf_callback_t *cb);
    int zf_hub_readlink(zf_callback_t *cb);
    int zf_hub_login(zf_callback_t *cb);
    int zf_hub_refresh(zf_callback_t *cb);
#endif
