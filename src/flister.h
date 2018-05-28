#ifndef FLISTER_H
    #define FLISTER_H

    void diep(const char *str);
    void dies(const char *str);
    void warnp(const char *str);

    typedef enum list_mode_t {
        LIST_DISABLED,
        LIST_LS,
        LIST_TREE,
        LIST_DUMP,
        LIST_JSON,
        LIST_BLOCKS,

    } list_mode_t;

    typedef struct settings_t {
        int verbose;       // enable verbose output
        int ramdisk;       // do we extract to ramdisk
        char *archive;     // required input/output archive

        char *create;      // create root path
        size_t rootlen;    // create root string length

        list_mode_t list;  // list view mode

    } settings_t;

    extern settings_t settings;

    #define verbose(...) { if(settings.verbose) { printf(__VA_ARGS__); } }
#endif
