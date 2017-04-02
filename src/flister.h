#ifndef FLISTER_H
    #define FLISTER_H

    void diep(char *str);
    void dies(char *str);

    typedef struct settings_t {
        int verbose;
        char *archive;

        int list;
        int tree;

    } settings_t;

    extern settings_t settings;

    #define verbose(...) { if(settings.verbose) { printf(__VA_ARGS__); } }
#endif
