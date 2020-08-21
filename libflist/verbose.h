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

    // wrap printf style error definiton
    void *libflist_set_error(const char *format, ...);

    // progression
    void libflist_progress(flist_ctx_t *ctx, char *message, size_t current, size_t total);

    #define debug(...) { if(libflist_debug_flag) { printf(__VA_ARGS__); } }
#endif
