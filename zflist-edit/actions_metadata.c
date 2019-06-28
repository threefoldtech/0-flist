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

static struct option long_options[] = {
    {"host",      required_argument, 0, 'h'},
    {"port",      required_argument, 0, 'p'},
    {"socket",    required_argument, 0, 's'},
    {"namespace", required_argument, 0, 'n'},
    {"password",  required_argument, 0, 'x'},
    {"help",      no_argument,       0, 'H'},
    {0, 0, 0, 0}
};

int zf_metadata_set_backend(int argc, char *argv[], zfe_settings_t *settings) {
    flist_db_t *database = zf_init(settings->mnt);
    json_t *root = json_object();
    int option_index = 0;

    char *host = "hub.grid.tf";
    int port = 9900;

    while(1) {
        int i = getopt_long_only(argc, argv, "", long_options, &option_index);

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

            case 'H':
                printf("[+] action: metadata: arguments:\n");
                printf("[+]   --host       <host>        tcp remote host\n");
                printf("[+]   --port       <tcp-port>    tcp remote port\n");
                printf("[+]   --socket     <hostname>    unix socket path\n");
                printf("[+]   --namespace  <namespace>   zdb namespace name (optional)\n");
                printf("[+]   --password   <password>    zdb namespace password (optional)\n");
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

    json_object_set(root, "host", json_string(host));
    json_object_set(root, "port", json_integer(port));

    char *value = json_dumps(root, 0);

    printf("[+] action: metadata: setting up backend\n");
    printf("[+] action: metadata: %s\n", value);

    if(database->mdset(database, "backend", value)) {
        printf(">> %s\n", libflist_strerror());
        exit(EXIT_FAILURE);
    }

    database->close(database);
    free(value);

    return 0;
}

int zf_metadata_get(int argc, char *argv[], zfe_settings_t *settings) {
    flist_db_t *database = zf_init(settings->mnt);

    value_t *rawdata = database->mdget(database, argv[1]);
    if(!rawdata->data) {
        debug("[-] action: metadata: get: metadata not found\n");
        return 1;
    }

    printf("[+] action: metadata: value for <%s>:\n", argv[1]);
    printf("%s\n", rawdata->data);
    return 0;
}

