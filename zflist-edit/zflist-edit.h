#ifndef ZFLISTEDIT_H
    #define ZFLISTEDIT_H

    #ifdef FLIST_DEBUG
        #define debug(...) { printf(__VA_ARGS__); }
    #else
        #define debug(...) ((void)0)
    #endif

    void warnp(const char *str);
    void diep(const char *str);
    void dies(const char *str);

    typedef struct zfe_settings_t {
        char *mnt;         // temporary-point directory

        char *backendhost;   // backend host
        int backendport;     // backend port
        char *bpass;         // backend password
        char *token;         // 0-hub gateway jwt token

    } zfe_settings_t;

    typedef struct zf_cmds_t {
        char *name;
        int (*callback)(int argc, char *argv[], zfe_settings_t *settings);
        char *help;

    } zf_cmds_t;


#endif
