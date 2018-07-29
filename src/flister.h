#ifndef FLISTER_H
    #define FLISTER_H

    void diep(const char *str);
    void dies(const char *str);
    void warnp(const char *str);

    void *bufdup(void *source, size_t length);

    #ifdef FLIST_DEBUG
        #define debug(...) { printf(__VA_ARGS__); }
    #else
        #define debug(...) ((void)0)
    #endif

    typedef enum list_mode_t {
        LIST_DISABLED,
        LIST_LS,
        LIST_TREE,
        LIST_DUMP,
        LIST_JSON,
        LIST_BLOCKS,
        LIST_CHECK,
        LIST_CAT,

    } list_mode_t;

    typedef struct merge_list_t {
        size_t length;
        char **sources;

    } merge_list_t;

    typedef struct settings_t {
        int ramdisk;       // do we extract to ramdisk
        char *archive;     // required input/output archive

        char *create;      // create root path
        size_t rootlen;    // create root string length

        char *backendhost;   // backend host
        int backendport;     // backend port

        int json;            // json output flag
        char *targetfile;    // specific target file (see cat action)
        list_mode_t list;    // list view mode
        merge_list_t merge;  // list of merging flists

    } settings_t;

    extern settings_t settings;
#endif
