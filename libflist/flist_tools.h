#ifndef LIBFLIST_FLIST_TOOLS_H
    #define LIBFLIST_FLIST_TOOLS_H

    char *flist_path_key(char *path);
    char *flist_clean_path(char *path);
    void *flist_strdup_safe(char *source);
    void *flist_memdup(void *input, size_t length);
#endif
