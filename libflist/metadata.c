#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "libflist.h"
#include "verbose.h"

int libflist_metadata_set(flist_db_t *database, char *metadata, char *payload) {
    if(database->mdset(database, metadata, payload)) {
        debug("[-] libflist: metadata: set: %s\n", libflist_strerror());
        return 0;
    }

    debug("[+] libflist: metadata: <%s> set into database\n", metadata);
    return 1;
}

char *libflist_metadata_get(flist_db_t *database, char *metadata) {
    value_t *rawdata = database->mdget(database, metadata);

    if(!rawdata->data) {
        debug("[-] libflist: metadata: get: metadata not found\n");
        free(rawdata);
        return NULL;
    }

    debug("[+] libflist: metadata: value for <%s>\n", metadata);
    debug("[+] %s\n", rawdata->data);

    char *value = rawdata->data;
    free(rawdata);

    return value;
}

int libflist_metadata_remove(flist_db_t *database, char *metadata) {
    if(database->mddel(database, metadata)) {
        debug("[-] libflist: metadata: del: %s\n", libflist_strerror());
        return 0;
    }

    debug("[+] libflist: metadata: <%s> deleted\n", metadata);
    return 1;
}

flist_db_t *libflist_metadata_backend_database_json(char *input) {
    flist_db_t *backdb;
    json_error_t error;
    json_t *backend = json_loads(input, 0, &error);

    char *host = (char *) json_string_value(json_object_get(backend, "host"));
    char *namespace = (char *) json_string_value(json_object_get(backend, "namespace"));
    char *password = (char *) json_string_value(json_object_get(backend, "password"));
    int port = json_integer_value(json_object_get(backend, "port"));

    if(!(backdb = libflist_db_redis_init_tcp(host, port, namespace, password, NULL))) {
        json_decref(backend);
        return NULL;
    }

    json_decref(backend);

    return backdb;
}

flist_db_t *libflist_metadata_backend_database(flist_db_t *database) {
    // fetching backend from metadata
    char *value;

    if(!(value = libflist_metadata_get(database, "backend"))) {
        debug("[-] libflist: backend database: metadata not found\n");
        return NULL;
    }

    debug("[+] libflist: metadata: raw backend: %s\n", value);

    return libflist_metadata_backend_database_json(value);
}
