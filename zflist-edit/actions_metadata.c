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

//
// FIXME: json stuff should be on the library
//

//
// helpers
//
static int zf_metadata_reset(zf_callback_t *cb, char *key) {
    debug("[+] action: metadata: removing metadata: %s\n", key);

    if(!libflist_metadata_remove(cb->database, key)) {
        fprintf(stderr, "[-] action: metadata: %s: %s\n", key, libflist_strerror());
        return 1;
    }

    return 0;
}

static int zf_metadata_apply(zf_callback_t *cb, char *metadata, json_t *object) {
    discard char *value = json_dumps(object, 0);

    debug("[+] action: metadata: setting up: %s\n", metadata);
    debug("[+] action: metadata: %s\n", value);

    if(!libflist_metadata_set(cb->database, metadata, value)) {
        fprintf(stderr, "[-] action: metadata: %s\n", libflist_strerror());
        json_decref(object);
        return 1;
    }

    json_decref(object);
    return 0;
}

// fetch an entry from database, if not found, returns empty set
// last argument is the callback used to initialize the json if entry
// is not found (can be object, array, etc.)
static json_t *zf_metadata_fetch(zf_callback_t *cb, char *metadata, json_t *(*initial)()) {
    char *value;
    json_t *input;
    json_error_t error;

    if(!(value = libflist_metadata_get(cb->database, metadata))) {
        debug("[-] action: metadata: get: initial metadata not found, initializing\n");
        return initial();
    }

    debug("[+] action: metadata: initial value found, parsing\n");

    if(!(input = json_loads(value, 0, &error))) {
        fprintf(stderr, "[-] action: metadata: json: %s\n", error.text);
        return initial();
    }

    debug("[+] action: metadata: initial json loaded\n");

    return input;
}

//
// getter
//
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

//
// setters
//

//
// backend
//
static struct option backend_long_options[] = {
    {"host",      required_argument, 0, 'H'},
    {"port",      required_argument, 0, 'p'},
    {"socket",    required_argument, 0, 's'},
    {"namespace", required_argument, 0, 'n'},
    {"password",  required_argument, 0, 'x'},
    {"reset",     no_argument,       0, 'r'},
    {"help",      no_argument,       0, 'h'},
    {0, 0, 0, 0}
};

int zf_metadata_set_backend(zf_callback_t *cb) {
    json_t *root = json_object();
    int option_index = 0;

    char *host = "hub.grid.tf";
    int port = 9900;

    while(1) {
        int i = getopt_long_only(cb->argc, cb->argv, "", backend_long_options, &option_index);

        if(i == -1)
            break;

        switch(i) {
            case 'r':
                return zf_metadata_reset(cb, "backend");

            case 'H':
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

            case 'h':
                printf("[+] action: metadata: arguments:\n");
                printf("[+]   -H --host       <host>        tcp remote host\n");
                printf("[+]   -p --port       <tcp-port>    tcp remote port\n");
                printf("[+]   -s --socket     <hostname>    unix socket path\n");
                printf("[+]   -n --namespace  <namespace>   zdb namespace name (optional)\n");
                printf("[+]   -x --password   <password>    zdb namespace password (optional)\n");
                printf("[+]   -r --reset                    remove backend metadata\n");
                printf("[+]   -h --help                     show this message\n");
                printf("[+]\n");
                printf("[+] tcp connection to <%s>, port %d will be set\n", host, port);
                printf("[+] if you set the unix socket, the port needs to be set to 0\n");
                return 1;

            case '?':
            default:
               return 1;
        }
    }

    json_object_set(root, "host", json_string(host));
    json_object_set(root, "port", json_integer(port));

    return zf_metadata_apply(cb, "backend", root);
}

//
// entry point
//
static struct option entry_long_options[] = {
    {"reset",  no_argument, 0, 'r'},
    {"help",   no_argument, 0, 'h'},
    {0, 0, 0, 0}
};

int zf_metadata_set_entry(zf_callback_t *cb) {
    json_t *root = json_array();
    int option_index = 0;

    while(1) {
        int i = getopt_long_only(cb->argc, cb->argv, "", entry_long_options, &option_index);

        if(i == -1)
            break;

        switch(i) {
            case 'r':
                return zf_metadata_reset(cb, "entrypoint");

            case 'h':
                printf("[+] action: metadata: arguments:\n");
                printf("[+]   -r --reset       remove metadata (all environment variables)\n");
                printf("[+]   -h --help        show this message\n");
                printf("[+]\n");
                printf("[+] note: you should use -- as first argument to ensure\n");
                printf("[+] everything you'll pass after will be used as-it for\n");
                printf("[+] your arguments, eg:\n");
                printf("[+]   metadata entrypoint -- /bin/sh /etc/init.d/myscript --force\n");
                return 1;

            case '?':
            default:
               return 1;
        }
    }

    for(; optind < cb->argc; optind++)
        json_array_append_new(root, json_string(cb->argv[optind]));

    return zf_metadata_apply(cb, "entrypoint", root);
}

//
// environment variables
//
static struct option environ_long_options[] = {
    {"name",   required_argument, 0, 'n'},
    {"value",  required_argument, 0, 'v'},
    {"unset",  no_argument,       0, 'u'},
    {"reset",  no_argument,       0, 'r'},
    {"help",   no_argument,       0, 'h'},
    {0, 0, 0, 0}
};

int zf_metadata_set_environ(zf_callback_t *cb) {
    int option_index = 0;
    char *name = NULL;
    char *value = NULL;
    int unset = 0;

    while(1) {
        int i = getopt_long_only(cb->argc, cb->argv, "", environ_long_options, &option_index);

        if(i == -1)
            break;

        switch(i) {
            case 'r':
                return zf_metadata_reset(cb, "environ");

            case 'u':
                unset = 1;
                break;

            case 'n':
                name = optarg;
                break;

            case 'v':
                value = optarg;
                break;

            case 'h':
                printf("[+] action: metadata: arguments:\n");
                printf("[+]   -n --name        environment variable name\n");
                printf("[+]   -v --value       environment variable value\n");
                printf("[+]   -u --unset       remove the specified environment variable\n");
                printf("[+]   -r --reset       remove metadata (all environment variables)\n");
                printf("[+]   -h --help        show this message\n");
                return 1;

            case '?':
            default:
               return 1;
        }
    }

    if(!name) {
        fprintf(stderr, "[-] action: metadata: missing environment variable name\n");
        return 1;
    }

    if(!unset && !value) {
        fprintf(stderr, "[-] action: metadata: missing environment variable value\n");
        return 1;
    }

    json_t *root = zf_metadata_fetch(cb, "environ", json_object);

    if(!unset) {
        // set environment variable
        json_object_set_new(root, name, json_string(value));

    } else {
        // remove requested key
        json_object_del(root, name);
    }

    return zf_metadata_apply(cb, "environ", root);
}

//
// network port mapping
//
static struct option port_long_options[] = {
    {"protocol", required_argument, 0, 'p'},
    {"in",       required_argument, 0, 'i'},
    {"out",      required_argument, 0, 'o'},
    {"unset",    no_argument,       0, 'u'},
    {"reset",    no_argument,       0, 'r'},
    {"help",     no_argument,       0, 'h'},
    {0, 0, 0, 0}
};


int zf_metadata_set_port(zf_callback_t *cb) {
    int option_index = 0;
    char *protocol = "tcp";
    int in = 0;
    int out = 0;
    int unset = 0;

    while(1) {
        int i = getopt_long_only(cb->argc, cb->argv, "", port_long_options, &option_index);

        if(i == -1)
            break;

        switch(i) {
            case 'r':
                return zf_metadata_reset(cb, "port");

            case 'u':
                unset = 1;
                break;

            case 'p':
                protocol = optarg;
                break;

            case 'i':
                in = atoi(optarg);
                break;

            case 'o':
                out = atoi(optarg);
                break;

            case 'h':
                printf("[+] action: metadata: arguments:\n");
                printf("[+]   -p --protocol    protocol name (should be tcp or udp, default: tcp)\n");
                printf("[+]   -i --in          input port number\n");
                printf("[+]   -o --out         destination port (default: same as in)\n");
                printf("[+]   -u --unset       remove the specified input port\n");
                printf("[+]   -r --reset       remove metadata (all exported ports)\n");
                printf("[+]   -h --help        show this message\n");
                return 1;

            case '?':
            default:
               return 1;
        }
    }

    if(in == 0) {
        fprintf(stderr, "[-] action: metadata: missing input port number\n");
        return 1;
    }

    if(out == 0) {
        debug("[+] action: metadata: port: output not set, copying input\n");
        out = in;
    }

    char keyname[512];
    json_t *root = zf_metadata_fetch(cb, "port", json_object);

    snprintf(keyname, sizeof(keyname), "%s/%d", protocol, in);

    if(!unset) {
        json_t *item = json_object();
        json_object_set(root, keyname, json_integer(out));
        json_array_append(root, item);

    } else {
        json_object_del(root, keyname);
    }

    return zf_metadata_apply(cb, "port", root);
}

//
// volumes mountpoint
//
static struct option volume_long_options[] = {
    {"host",   required_argument, 0, 'H'},
    {"target", required_argument, 0, 't'},
    {"unset",  no_argument,       0, 'u'},
    {"reset",  no_argument,       0, 'r'},
    {"help",   no_argument,       0, 'h'},
    {0, 0, 0, 0}
};

int zf_metadata_set_volume(zf_callback_t *cb) {
    int option_index = 0;
    char *host = NULL;
    char *target = NULL;
    int unset = 0;

    while(1) {
        int i = getopt_long_only(cb->argc, cb->argv, "", volume_long_options, &option_index);

        if(i == -1)
            break;

        switch(i) {
            case 'r':
                return zf_metadata_reset(cb, "volume");

            case 'u':
                unset = 1;
                break;

            case 'H':
                host = optarg;
                break;

            case 't':
                target = optarg;
                break;

            case 'h':
                printf("[+] action: metadata: arguments:\n");
                printf("[+]   -h --host        source file/directory on the host side\n");
                printf("[+]   -t --target      target file/directory on the container\n");
                printf("[+]   -u --unset       remove the specified source point\n");
                printf("[+]   -r --reset       remove metadata (all volumes)\n");
                printf("[+]   -h --help        show this message\n");
                return 1;

            case '?':
            default:
               return 1;
        }
    }

    if(!host) {
        fprintf(stderr, "[-] action metadata: missing host path\n");
        return 1;
    }

    if(!target) {
        debug("[-] action: metadata: volume: target not set, copying host\n");
        target = host;
    }

    json_t *root = zf_metadata_fetch(cb, "volume", json_object);

    if(!unset) {
        json_t *item = json_object();
        json_object_set(item, "target", json_string(target));
        json_object_set_new(root, host, item);

    } else {
        json_object_del(root, host);
    }

    return zf_metadata_apply(cb, "volume", root);
}

//
// readme
//
static struct option readme_long_options[] = {
    {"license", required_argument, 0, 'l'},
    {"edit",    no_argument,       0, 'e'},
    {"reset",   no_argument,       0, 'r'},
    {"help",    no_argument,       0, 'h'},
    {0, 0, 0, 0}
};

static char *zf_metadata_edit_readme(const char *original) {
    // generate a temporary file
    char template[] = "/tmp/zflistXXXXXX";
    char fname[1024];
    char *readmeval;

    // open the temporary file
    strcpy(fname, template);
    int fd = mkstemp(fname);

    debug("[+] action: metadata: temporary file: %s\n", fname);

    if(original) {
        // copying original contents into
        // the new temporary file
        size_t sl = strlen(original);

        if(write(fd, original, sl) != (ssize_t) sl)
            diep(fname);
    }

    // starting interactive editor
    char tmpcmd[2048];
    sprintf(tmpcmd, "$EDITOR %s", fname);

    if(system(tmpcmd) < 0)
        diep(tmpcmd);

    // fetching length of the new file
    off_t length = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    // allocating variable for it
    if(!(readmeval = malloc(length)))
        diep("malloc");

    // reading new file
    if(read(fd, readmeval, length) != length)
        diep(fname);

    // closing and cleaning
    close(fd);
    unlink(fname);

    return readmeval;
}

int zf_metadata_set_readme(zf_callback_t *cb) {
    int option_index = 0;
    char *license = NULL;
    char *newvalue = NULL;
    const char *existing = NULL;
    int edit = 0;

    while(1) {
        int i = getopt_long_only(cb->argc, cb->argv, "", readme_long_options, &option_index);

        if(i == -1)
            break;

        switch(i) {
            case 'r':
                return zf_metadata_reset(cb, "readme");

            case 'e':
                edit = 1;
                break;

            case 'l':
                license = optarg;
                break;

            case 'h':
                printf("[+] action: metadata: arguments:\n");
                printf("[+]   -l --license     set a specific license name\n");
                printf("[+]   -r --reset       remove metadata (all volumes)\n");
                printf("[+]   -h --help        show this message\n");
                return 1;

            case '?':
            default:
               return 1;
        }
    }

    json_t *root = zf_metadata_fetch(cb, "readme", json_object);
    json_t *readme;

    if((readme = json_object_get(root, "readme")))
        existing = json_string_value(readme);

    if(existing)
        debug("[+] action: metadata: existing readme found (%lu bytes)\n", strlen(existing));

    if(edit)
        newvalue = zf_metadata_edit_readme(existing);

    if(license)
        json_object_set(root, "license", json_string(license));

    json_object_set(root, "readme", json_string(newvalue));
    free(newvalue);

    return zf_metadata_apply(cb, "readme", root);
}



