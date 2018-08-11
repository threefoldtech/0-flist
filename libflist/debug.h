#ifndef LIBFLIST_DEBUG_H
    #define LIBFLIST_DEBUG_H

    #define warnp  libflist_warnp
    #define diep   libflist_diep
    #define dies   libflist_dies
    #define warns  libflist_warns

    void libflist_warnp(const char *str);
    void libflist_diep(const char *str);
    void libflist_dies(const char *str);
    void libflist_warns(const char *str);
#endif
