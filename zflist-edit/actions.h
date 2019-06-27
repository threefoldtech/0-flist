#ifndef ZFLISTEDIT_ACTIONS_H
    #define ZFLISTEDIT_ACTIONS_H


    int zf_open(int argc, char *argv[], zfe_settings_t *settings);
    int zf_commit(int argc, char *argv[], zfe_settings_t *settings);

    int zf_chmod(int argc, char *argv[], zfe_settings_t *settings);
    int zf_rm(int argc, char *argv[], zfe_settings_t *settings);
    int zf_ls(int argc, char *argv[], zfe_settings_t *settings);
#endif
