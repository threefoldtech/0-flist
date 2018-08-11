#ifndef LIBFLIST_WRITE_H
    #define LIBFLIST_WRITE_H

    #include "database.h"

    int flist_create(database_t *database, const char *root, backend_t *backend, settings_t *settings);
#endif
