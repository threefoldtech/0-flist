#ifndef ZFLIST_H
    #define ZFLIST_H

    #define ZFLIST_VERSION  "2.0.1"

    #define ZFLIST_HUB_BASEURL   "https://hub.grid.tf"

    #ifdef FLIST_DEBUG
        #define debug(...) { printf(__VA_ARGS__); }
    #else
        #define debug(...) ((void)0)
    #endif

    #define discard __attribute__((cleanup(__cleanup_free)))

    typedef struct zfe_settings_t {
        char *mnt;           // temporary-point directory

        char *backendhost;   // backend host
        int backendport;     // backend port
        char *bpass;         // backend password
        char *token;         // 0-hub jwt token
        char *user;          // 0-hub active user
        char *baseurl;       // 0-hub base url

    } zfe_settings_t;

    typedef struct zf_callback_t {
        int argc;
        zfe_settings_t *settings;
        flist_ctx_t *ctx;
        char **argv;
        json_t *jout;
        void *userptr;
        int progress;

    } zf_callback_t;

    typedef struct zf_cmds_t {
        char *name;  // command name
        int (*callback)(zf_callback_t *cb);
        char *help;  // help message
        int db;      // does the callback need the db

    } zf_cmds_t;

#endif
