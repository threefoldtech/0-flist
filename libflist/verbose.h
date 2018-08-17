#ifndef LIBFLIST_DEBUG_H
    #define LIBFLIST_DEBUG_H

    extern char libflist_internal_error[1024];

    #define diep   libflist_diep
    #define dies   libflist_dies
    #define warns  libflist_warns
    #define warnp  libflist_warnp

    void *libflist_errp(const char *str);
    void *libflist_diep(const char *str);
    void *libflist_dies(const char *str);
    void libflist_warns(const char *str);
    void libflist_warnp(const char *str);

    #define debug(...) { if(libflist_debug_flag) { printf(__VA_ARGS__); } }

    #define libflist_set_error(...) \
        snprintf(libflist_internal_error, sizeof(libflist_internal_error), __VA_ARGS__);
#endif
