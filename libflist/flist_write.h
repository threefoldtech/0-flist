#ifndef LIBFLIST_FLIST_WRITE_H
    #define LIBFLIST_FLIST_WRITE_H

    typedef struct excluder_t {
        regex_t regex;
        char *str;

    } excluder_t;

    typedef struct excluders_t {
        size_t length;
        excluder_t *list;

    } excluders_t;

#endif
