#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <libgen.h>
#include <getopt.h>
#include <capnp_c.h>
#include "libflist.h"
#include "zflist-edit.h"
#include "tools.h"

static struct option backend_long_options[] = {
    {"host",      required_argument, 0, 'h'},
    {"port",      required_argument, 0, 'p'},
    {"socket",    required_argument, 0, 's'},
    {"namespace", required_argument, 0, 'n'},
    {"password",  required_argument, 0, 'x'},
    {"reset",     no_argument,       0, 'r'},
    {"help",      no_argument,       0, 'H'},
    {0, 0, 0, 0}
};

int zf_metadata_set_backend(zf_callback_t *cb) {
    json_t *root = json_object();
    int option_index = 0;
    int reset = 0;

    char *host = "hub.grid.tf";
    int port = 9900;

    while(1) {
        int i = getopt_long_only(cb->argc, cb->argv, "", backend_long_options, &option_index);

        if(i == -1)
            break;

        switch(i) {
            case 'h':
                host = optarg;
                break;

            case 'p':
                port = atoi(optarg);
                break;

            case 's':
                port = 0;
                json_object_set(root, "socket", json_string(optarg));
                break;

            case 'n':
                json_object_set(root, "namespace", json_string(optarg));
                break;

            case 'x':
                json_object_set(root, "password", json_string(optarg));
                break;

            case 'r':
                reset = 1;
                break;

            case 'H':
                printf("[+] action: metadata: arguments:\n");
                printf("[+]   --host       <host>        tcp remote host\n");
                printf("[+]   --port       <tcp-port>    tcp remote port\n");
                printf("[+]   --socket     <hostname>    unix socket path\n");
                printf("[+]   --namespace  <namespace>   zdb namespace name (optional)\n");
                printf("[+]   --password   <password>    zdb namespace password (optional)\n");
                printf("[+]   --reset                    remove this metadata\n");
                printf("[+]   --help                     show this message\n");
                printf("[+]\n");
                printf("[+] tcp connection to <%s>, port %d will be set\n", host, port);
                printf("[+] if you set the unix socket, the port needs to be set to 0\n");
                return 1;

            case '?':
            default:
               exit(EXIT_FAILURE);
        }
    }

    if(reset) {
        debug("[+] action: metadata: removing backend metadata\n");

        if(!libflist_metadata_remove(cb->database, "backend")) {
            fprintf(stderr, "[-] action: metadata: %s\n", libflist_strerror());
            return 1;
        }

        return 0;
    }

    json_object_set(root, "host", json_string(host));
    json_object_set(root, "port", json_integer(port));

    discard char *value = json_dumps(root, 0);

    debug("[+] action: metadata: setting up backend\n");
    debug("[+] action: metadata: %s\n", value);

    if(!libflist_metadata_set(cb->database, "backend", value)) {
        fprintf(stderr, "[-] action: metadata: %s\n", libflist_strerror());
        return 1;
    }

    return 0;
}

static struct option entry_long_options[] = {
    {"reset",  no_argument, 0, 'r'},
    {"help",   no_argument, 0, 'h'},
    {0, 0, 0, 0}
};

int zf_metadata_set_entry(zf_callback_t *cb) {
    json_t *root = json_array();
    int option_index = 0;
    int reset = 0;

    while(1) {
        int i = getopt_long_only(cb->argc, cb->argv, "", entry_long_options, &option_index);

        if(i == -1)
            break;

        switch(i) {
            case 'r':
                reset = 1;
                break;

            case 'h':
                printf("[+] action: metadata: arguments:\n");
                printf("[+]   --reset        remove entrypoint metadata\n");
                printf("[+]   --help         show this message\n");
                printf("[+]\n");
                printf("[+] note: you should use -- as first argument to ensure\n");
                printf("[+] everything you'll pass after will be used as-it for\n");
                printf("[+] your arguments, eg:\n");
                printf("[+]   metadata entrypoint -- /bin/sh /etc/init.d/myscript --force\n");
                return 1;

            case '?':
            default:
               exit(EXIT_FAILURE);
        }
    }

    if(reset) {
        debug("[+] action: metadata: removing entrypoint metadata\n");

        if(!libflist_metadata_remove(cb->database, "entrypoint")) {
            fprintf(stderr, "[-] action: metadata: %s\n", libflist_strerror());
            return 1;
        }

        return 0;
    }

    for(; optind < cb->argc; optind++)
        json_array_append_new(root, json_string(cb->argv[optind]));

    discard char *value = json_dumps(root, 0);

    debug("[+] action: metadata: setting up entrypoint\n");
    debug("[+] action: metadata: %s\n", value);

    if(!libflist_metadata_set(cb->database, "entrypoint", value)) {
        printf("[-] action: metadata: %s\n", libflist_strerror());
        return 1;
    }

    return 0;
}


int zf_metadata_get(zf_callback_t *cb) {
    char *value;

    if(!(value = libflist_metadata_get(cb->database, cb->argv[1]))) {
        debug("[-] action: metadata: get: metadata not found\n");
        return 1;
    }

    debug("[+] action: metadata: value for <%s>\n", cb->argv[1]);
    printf("%s\n", value);

    return 0;
}

