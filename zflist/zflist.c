#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <getopt.h>
#include <unistd.h>
#include <jansson.h>
#include "libflist.h"
#include "zflist.h"

zflist_settings_t settings;

static struct option long_options[] = {
    {"list",    no_argument,       0, 'l'},
    {"create",  required_argument, 0, 'c'},
    {"action",  required_argument, 0, 'o'},
    {"archive", required_argument, 0, 'a'},
    {"backend", required_argument, 0, 'b'},
    {"merge",   required_argument, 0, 'm'},
    {"ramdisk", no_argument,       0, 'r'},
    {"json",    no_argument,       0, 'j'},
    {"file",    required_argument, 0, 'f'},
    {"output",  required_argument, 0, 'O'},
    {"root",    required_argument, 0, 'p'},
    {"help",    no_argument,       0, 'h'},
    {0, 0, 0, 0}
};

void warnp(const char *str) {
    fprintf(stderr, "[-] %s: %s\n", str, strerror(errno));
}

void diep(const char *str) {
    warnp(str);
    exit(EXIT_FAILURE);
}

void dies(const char *str) {
    fprintf(stderr, "[-] %s\n", str);
    exit(EXIT_FAILURE);
}

int usage(char *basename) {
    fprintf(stderr, "Usage: %s [options]\n", basename);
    fprintf(stderr, "       %s --archive <filename> --list [--output tree]\n", basename);
    fprintf(stderr, "       %s --archive <filename> --create <root-path>\n", basename);
    fprintf(stderr, "       %s --archive <filename> [options] --backend <host:port>\n", basename);
    fprintf(stderr, "\n");
    fprintf(stderr, "Command line options:\n");
    fprintf(stderr, "  --archive <flist>     archive (flist) filename\n");
    fprintf(stderr, "                        (this option is always required)\n\n");

    fprintf(stderr, "  --create <root>       create an archive from <root> directory\n\n");
    fprintf(stderr, "  --backend <host:port> upload/download files from archive, on this backend\n\n");

    fprintf(stderr, "  --list       list archive content\n");
    fprintf(stderr, "  --action        action to do while listing archive:\n");
    fprintf(stderr, "                    ls      show kind of 'ls -al' contents (default)\n");
    fprintf(stderr, "                    tree    show contents in a tree view\n");
    fprintf(stderr, "                    dump    debug dump of contents\n");
    fprintf(stderr, "                    json    file list summary in json format (same as --json)\n\n");
    fprintf(stderr, "                    blocks  dump files backend blocks (hash, key)\n");
    fprintf(stderr, "                    check   proceed to backend integrity check\n");
    fprintf(stderr, "                    cat     request file download (with --file option)\n\n");

    fprintf(stderr, "  --json       provide (exclusively) json output status\n");
    fprintf(stderr, "  --file       specific inside file to target\n");
    fprintf(stderr, "  --ramdisk    extract archive to tmpfs\n");
    fprintf(stderr, "  --help       shows this help message\n");
    exit(EXIT_FAILURE);
}

static int flister_create(char *workspace) {
    // no backend by default
    flist_backend_t *backend = NULL;

    flist_db_t *database = libflist_db_sqlite_init(workspace);
    database->create(database);

    if(settings.backendhost) {
        flist_db_t *backdb;

        if(!(backdb = libflist_db_redis_init_tcp(settings.backendhost, settings.backendport, "default"))) {
            fprintf(stderr, "[-] cannot initialize backend\n");
            return 1;
        }

        // initizlizing backend as requested
        if(!(backend = backend_init(backdb, settings.create))) {
            return 1;
        }
    }

    // building database
    flist_create(database, settings.create, backend, &settings);

    // closing database before archiving
    debug("[+] closing database\n");
    database->close(database);

    // removing possible already existing db
    unlink(settings.archive);
    libflist_archive_create(settings.archive, workspace);

    return 0;
}

static int flister_list(char *workspace) {
    // if json flag is set, ensure we output
    // json format listing
    if(settings.json)
        settings.list = LIST_JSON;

    if(!libflist_archive_extract(settings.archive, workspace)) {
        warnp("libflist_archive_extract");
        return 1;
    }


    debug("[+] loading database\n");
    flist_db_t *database = libflist_db_sqlite_init(workspace);
    database->open(database);

    debug("[+] walking over database\n");
    flist_listing(database, &settings);

    debug("[+] closing database\n");
    database->close(database);

    return 0;
}

static int flister_merge(char *workspace) {
    flist_db_t *database = libflist_db_sqlite_init(workspace);
    database->create(database);

    // building database
    flist_merger(database, &settings.merge);

    // closing database before archiving
    debug("[+] closing database\n");
    database->close(database);

    // removing possible already existing db
    unlink(settings.archive);
    libflist_archive_create(settings.archive, workspace);


    return 0;
}

static int flister() {
    char *workspace;

    debug("[+] initializing workspace\n");
    if(!(workspace = libflist_workspace_create()))
        diep("workspace_create");

    if(settings.ramdisk) {
        debug("[+] initializing ramdisk\n");

        if(!libflist_ramdisk_create(workspace))
            diep("ramdisk_create");
    }

    debug("[+] workspace: %s\n", workspace);

    //
    // creating
    //
    if(settings.create) {
        debug("[+] creating database\n");
        flister_create(workspace);
    }

    //
    // listing
    //
    if(settings.list) {
        debug("[+] extracting archive\n");
        if(flister_list(workspace))
            goto clean;
    }

    //
    // merging
    //
    if(settings.merge.length) {
        debug("[+] merging flists\n");
        if(flister_merge(workspace))
            goto clean;
    }

clean:
    if(settings.ramdisk) {
        debug("[+] cleaning ramdisk\n");

        if(!libflist_ramdisk_destroy(workspace))
            diep("ramdisk_destroy");
    }

    debug("[+] cleaning workspace\n");
    if(!libflist_workspace_destroy(workspace))
        diep("workspace_destroy");

    free(workspace);

    return 0;
}

int main(int argc, char *argv[]) {
    int option_index = 0;
    int i;

    // reset settings
    memset(&settings, 0, sizeof(zflist_settings_t));

    // initializing default settings
    settings.list = LIST_DISABLED;
    settings.backendport = 16379;

    while(1) {
        i = getopt_long(argc, argv, "", long_options, &option_index);

        if(i == -1)
            break;

        switch(i) {
            case 'l': {
                settings.list = LIST_LS;
                break;
            }

            case 'o': {
                if(!strcmp(optarg, "ls"))
                    settings.list = LIST_LS;

                if(!strcmp(optarg, "tree"))
                    settings.list = LIST_TREE;

                if(!strcmp(optarg, "dump"))
                    settings.list = LIST_DUMP;

                if(!strcmp(optarg, "blocks"))
                    settings.list = LIST_BLOCKS;

                if(!strcmp(optarg, "json")) {
                    settings.json = 1;
                    settings.list = LIST_JSON;
                }

                if(!strcmp(optarg, "check"))
                    settings.list = LIST_CHECK;

                if(!strcmp(optarg, "cat"))
                    settings.list = LIST_CAT;

                break;
            }

            case 'j': {
                settings.json = 1;
                break;
            }

            case 'a': {
                settings.archive = strdup(optarg);
                break;
            }

            case 'b': {
                char *token;
                size_t length;

                settings.backendhost = strdup(optarg);

                // does the host contains the port
                if((token = strchr(settings.backendhost, ':'))) {
                    length = token - settings.backendhost;

                    settings.backendport = atoi(token + 1);
                    settings.backendhost[length] = '\0';
                }

                // maybe only the port was set with colon
                // let's deny this
                if(strlen(settings.backendhost) == 0)
                    dies("upload: missing hostname");

                break;
            }

            case 'm': {
                // shortcut
                merge_list_t *m = &settings.merge;

                m->length += 1;
                if(!(m->sources = (char **) realloc(m->sources, m->length * sizeof(char *))))
                    diep("merge realloc");

                m->sources[m->length - 1] = optarg;
                break;
            }

            case 'r': {
                settings.ramdisk = 1;
                break;
            }

            case 'c': {
                // root path
                settings.create = strdup(optarg);
                settings.rootlen = strlen(settings.create);

                // forcing rootpath to not have trailing slash
                while(settings.create[settings.rootlen - 1] == '/') {
                    settings.create[settings.rootlen - 1] = '\0';
                    settings.rootlen -= 1;
                }

                break;
            }

            case 'f': {
                settings.targetfile = optarg;
                break;
            }

            case 'O': {
                settings.outputfile = optarg;
                break;
            }

            case '?':
            case 'h':
            default:
                usage(argv[0]);
        }
    }

    // archive is required
    if(!settings.archive)
        usage(argv[0]);

    // process
    int value = flister();

    // cleaning
    free(settings.archive);
    free(settings.create);
    free(settings.backendhost);
    free(settings.merge.sources);

    return value;
}
