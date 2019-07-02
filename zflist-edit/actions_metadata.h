#ifndef ZFLISTEDIT_ACTIONS_METADATA_H
    #define ZFLISTEDIT_ACTIONS_METADATA_H

    int zf_metadata_set_backend(int argc, char *argv[], zfe_settings_t *settings);
    int zf_metadata_set_entry(int argc, char *argv[], zfe_settings_t *settings);

    int zf_metadata_get(int argc, char *argv[], zfe_settings_t *settings);
#endif
