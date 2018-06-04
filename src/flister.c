#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <unistd.h>
#include "flister.h"
#include "archive.h"
#include "workspace.h"
#include "database.h"
#include "flist_listing.h"
#include "flist_merger.h"
#include "flist_write.h"

settings_t settings;

static struct option long_options[] = {
    {"list",    no_argument,       0, 'l'},
    {"create",  required_argument, 0, 'c'},
    {"output",  required_argument, 0, 'o'},
    {"archive", required_argument, 0, 'a'},
    {"upload",  required_argument, 0, 'u'},
    {"merge",   required_argument, 0, 'm'},
    {"ramdisk", no_argument,       0, 'r'},
    {"json",    no_argument,       0, 'j'},
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
    fprintf(stderr, "\n");
    fprintf(stderr, "Command line options:\n");
    fprintf(stderr, "  -a --archive <flist>     archive (flist) filename\n");
    fprintf(stderr, "                           (this option is always required)\n\n");

    fprintf(stderr, "  -c --create <root>       create an archive from <root> directory\n\n");
    fprintf(stderr, "  -u --upload <host:port>  upload files from creating archive, to the backend\n\n");

    fprintf(stderr, "  -l --list       list archive content\n");
    fprintf(stderr, "  -o --output     list output format, possible values:\n");
    fprintf(stderr, "                    ls      show kind of 'ls -al' contents (default)\n");
    fprintf(stderr, "                    tree    show contents in a tree view\n");
    fprintf(stderr, "                    dump    debug dump of contents\n");
    fprintf(stderr, "                    json    file list summary in json format (same as --json)\n\n");
    fprintf(stderr, "                    blocks  dump files backend blocks (hash, key)\n\n");

    fprintf(stderr, "  -j --json       provide (exclusively) json output status\n");
    fprintf(stderr, "  -r --ramdisk    extract archive to tmpfs\n");
    fprintf(stderr, "  -h --help       shows this help message\n");
    exit(EXIT_FAILURE);
}

static int flister_create(char *workspace) {
    database_t *database = database_create(workspace);

    // building database
    flist_create(database, settings.create);

    // closing database before archiving
    debug("[+] closing database\n");
    database_close(database);

    // removing possible already existing db
    unlink(settings.archive);
    archive_create(settings.archive, workspace);

    return 0;
}

static int flister_list(char *workspace) {
    // if json flag is set, ensure we output
    // json format listing
    if(settings.json)
        settings.list = LIST_JSON;

    if(!archive_extract(settings.archive, workspace)) {
        warnp("archive_extract");
        return 1;
    }

    debug("[+] loading rocksdb database\n");
    database_t *database = database_open(workspace);

    debug("[+] walking over database\n");
    flist_listing(database);

    debug("[+] closing database\n");
    database_close(database);

    return 0;
}

static int flister_merge(char *workspace) {
    database_t *database = database_create(workspace);

    // building database
    flist_merger(database, &settings.merge);

    // closing database before archiving
    debug("[+] closing database\n");
    database_close(database);

    // removing possible already existing db
    unlink(settings.archive);
    archive_create(settings.archive, workspace);


    return 0;
}

static int flister() {
    char *workspace;

    debug("[+] initializing workspace\n");
    if(!(workspace = workspace_create()))
        diep("workspace_create");

    if(settings.ramdisk) {
        debug("[+] initializing ramdisk\n");

        if(!ramdisk_create(workspace))
            diep("ramdisk_create");
    }

    debug("[+] workspace: %s\n", workspace);

    //
    // creating
    //
    if(settings.create) {
        debug("[+] creating rocksdb database\n");
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

        if(!ramdisk_destroy(workspace))
            diep("ramdisk_destroy");
    }

    debug("[+] cleaning workspace\n");
    if(!workspace_destroy(workspace))
        diep("workspace_destroy");

    free(workspace);

    return 0;
}

int main(int argc, char *argv[]) {
    int option_index = 0;
    int i;

    // reset settings
    memset(&settings, 0, sizeof(settings_t));

    // initializing default settings
    settings.list = LIST_DISABLED;
    settings.uploadport = 16379;

    while(1) {
        i = getopt_long(argc, argv, "lcta:vrp:h", long_options, &option_index);

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

            case 'u': {
                char *token;
                size_t length;

                settings.uploadhost = strdup(optarg);

                // does the host contains the port
                if((token = strchr(settings.uploadhost, ':'))) {
                    length = token - settings.uploadhost;

                    settings.uploadport = atoi(token + 1);
                    settings.uploadhost[length] = '\0';
                }

                // maybe only the port was set with colon
                // let's deny this
                if(strlen(settings.uploadhost) == 0)
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

                if(settings.create[settings.rootlen - 1] == '/') {
                    settings.create[settings.rootlen - 1] = '\0';
                    settings.rootlen -= 1;
                }

                break;
            }

            case '?':
            case 'h':
                usage(argv[0]);
            break;

            default:
                abort();
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
    free(settings.uploadhost);
    free(settings.merge.sources);

    return value;
}
