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
#include "flist_read.h"
#include "flist_write.h"

settings_t settings;

static struct option long_options[] = {
    {"list",    no_argument,       0, 'l'},
    {"create",  required_argument, 0, 'c'},
    {"output",  required_argument, 0, 'o'},
    {"archive", required_argument, 0, 'a'},
    {"upload",  required_argument, 0, 'u'},
    {"verbose", no_argument,       0, 'v'},
    {"ramdisk", no_argument,       0, 'r'},
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
    fprintf(stderr, "Usage: %s [options] --verbose\n", basename);
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
    fprintf(stderr, "                    json    file list summary in json format\n\n");
    fprintf(stderr, "                    blocks  dump files backend blocks (hash, key)\n\n");

    fprintf(stderr, "  -r --ramdisk    extract archive to tmpfs\n");
    fprintf(stderr, "  -v --verbose    enable verbose messages\n");
    fprintf(stderr, "  -h --help       shows this help message\n");
    exit(EXIT_FAILURE);
}

static int flister_create(char *workspace) {
    database_t *database = database_create(workspace);

    // building database
    flist_create(database, settings.create);

    // removing possible already existing db
    unlink(settings.archive);
    archive_create(settings.archive, workspace);

    verbose("[+] closing database\n");
    database_close(database);

    return 0;
}

static int flister_list(char *workspace) {
    if(!archive_extract(settings.archive, workspace)) {
        warnp("archive_extract");
        return 1;
    }

    verbose("[+] loading rocksdb database\n");
    database_t *database = database_open(workspace);

    verbose("[+] walking over database\n");
    flist_walk(database);

    verbose("[+] closing database\n");
    database_close(database);

    return 0;
}

static int flister() {
    char *workspace;

    verbose("[+] initializing workspace\n");
    if(!(workspace = workspace_create()))
        diep("workspace_create");

    if(settings.ramdisk) {
        verbose("[+] initializing ramdisk\n");

        if(!ramdisk_create(workspace))
            diep("ramdisk_create");
    }

    verbose("[+] workspace: %s\n", workspace);

    //
    // creating
    //
    if(settings.create) {
        verbose("[+] creating rocksdb database\n");
        flister_create(workspace);
    }

    //
    // listing
    //
    if(settings.list) {
        verbose("[+] extracting archive\n");
        if(flister_list(workspace))
            goto clean;
    }

clean:
    if(settings.ramdisk) {
        verbose("[+] cleaning ramdisk\n");

        if(!ramdisk_destroy(workspace))
            diep("ramdisk_destroy");
    }

    verbose("[+] cleaning workspace\n");
    if(!workspace_destroy(workspace))
        diep("workspace_destroy");

    free(workspace);

    return 0;
}

int main(int argc, char *argv[]) {
    int option_index = 0;
    int i;

    // initializing settings
    settings.list = LIST_DISABLED;
    settings.verbose = 0;
    settings.archive = NULL;
    settings.ramdisk = 0;
    settings.create = NULL;
    settings.uploadhost = NULL;
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

                if(!strcmp(optarg, "json"))
                    settings.list = LIST_JSON;

                if(!strcmp(optarg, "blocks"))
                    settings.list = LIST_BLOCKS;

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

            case 'v': {
                settings.verbose = 1;
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

    return value;
}
