#ifndef FLISTER_H
    #define FLISTER_H

    void diep(const char *str);
    void dies(const char *str);
    void warnp(const char *str);

    typedef struct settings_t {
        int verbose;
        int ramdisk;

        char *archive;
        char *root;
        size_t rootlen;

        int create;
        int list;
        int tree;

    } settings_t;

    extern settings_t settings;

    #define verbose(...) { if(settings.verbose) { printf(__VA_ARGS__); } }
#endif
