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

slist_t libflist_metadata_list(flist_db_t *database) {
    slist_t raw = database->mdlist(database);
    return raw;
}

void libflist_metadata_list_free(slist_t *list) {
    for(size_t a = 0; a < list->length; a++)
        free(list->list[a]);
}

flist_db_t *libflist_metadata_backend_database_json(char *input) {
    flist_db_t *backdb;
    json_error_t error;
    json_t *backend = json_loads(input, 0, &error);

    if(!backend) {
        libflist_set_error("backend json could not be parsed");
        return NULL;
    }

    char *host = (char *) json_string_value(json_object_get(backend, "host"));
    char *namespace = (char *) json_string_value(json_object_get(backend, "namespace"));
    char *password = (char *) json_string_value(json_object_get(backend, "password"));
    char *token = (char *) json_string_value(json_object_get(backend, "token"));
    int port = json_integer_value(json_object_get(backend, "port"));

    debug("[+] libflist: backend: %s, %d (ns: %s)\n", host, port, namespace);
    debug("[+] libflist: backend: password: %s, token: %s\n", password ? "yes" : "no", token ? "yes" : "no");

    if(!(backdb = libflist_db_redis_init_tcp(host, port, namespace, password, token))) {
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
        libflist_set_error("backend metadata not found, could not fetch data");
        return NULL;
    }

    debug("[+] libflist: metadata: raw backend: %s\n", value);

    return libflist_metadata_backend_database_json(value);
}
