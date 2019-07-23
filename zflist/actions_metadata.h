#ifndef ZFLISTEDIT_ACTIONS_METADATA_H
    #define ZFLISTEDIT_ACTIONS_METADATA_H

    int zf_metadata_set_backend(zf_callback_t *cb);
    int zf_metadata_set_entry(zf_callback_t *cb);
    int zf_metadata_set_environ(zf_callback_t *cb);
    int zf_metadata_set_port(zf_callback_t *cb);
    int zf_metadata_set_volume(zf_callback_t *cb);
    int zf_metadata_set_readme(zf_callback_t *cb);

    int zf_metadata_get(zf_callback_t *cb);
#endif
