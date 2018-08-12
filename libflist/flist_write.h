#ifndef LIBFLIST_WRITE_H
    #define LIBFLIST_WRITE_H

    #include "database.h"

    int flist_create(flist_db_t *database, const char *root, flist_backend_t *backend, settings_t *settings);
#endif
