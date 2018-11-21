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
#include "flist_listing.h"
#include "flist_upload.h"

zflist_settings_t settings;

static struct option long_options[] = {
    {"list",     no_argument,       0, 'l'},
    {"create",   required_argument, 0, 'c'},
    {"action",   required_argument, 0, 'o'},
    {"archive",  required_argument, 0, 'a'},
    {"backend",  required_argument, 0, 'b'},
    {"password", required_argument, 0, 'x'},
    {"token",    required_argument, 0, 't'},
    {"upload",   optional_argument, 0, 'u'},
    {"merge",    required_argument, 0, 'm'},
    {"ramdisk",  no_argument,       0, 'r'},
    {"json",     no_argument,       0, 'j'},
    {"file",     required_argument, 0, 'f'},
    {"output",   required_argument, 0, 'O'},
    {"root",     required_argument, 0, 'p'},
    {"help",     no_argument,       0, 'h'},
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
    fprintf(stderr, "  --backend <host:port> upload/download files from archive, on this backend\n");
    fprintf(stderr, "  --password <pwd>      backend namespace password (protected mode)\n");
    fprintf(stderr, "  --token <jwt>         gateway token (gateway upload)\n");
    fprintf(stderr, "  --upload [website]    upload the flist (using --token) on the hub\n\n");

    fprintf(stderr, "  --list       list archive content\n");
    fprintf(stderr, "  --merge      do a merge and add argument to merge list\n");
    fprintf(stderr, "               the --archive will be the result of the merge\n");
    fprintf(stderr, "               merge are done in the order of arguments\n");
    fprintf(stderr, "  --action     action to do while listing archive:\n");
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

void flister_create_json(flist_stats_t *stats) {
    json_t *root = json_object();
    json_object_set_new(root, "regular", json_integer(stats->regular));
    json_object_set_new(root, "symlink", json_integer(stats->symlink));
    json_object_set_new(root, "directory", json_integer(stats->directory));
    json_object_set_new(root, "special", json_integer(stats->special));
    json_object_set_new(root, "failure", json_integer(stats->failure));

    char *output = json_dumps(root, 0);
    json_decref(root);

    puts(output);
    free(output);
}

void flister_create_response(flist_stats_t *stats) {
    printf("[+]\n");
    printf("[+] flist creation summary:\n");
    printf("[+]   flist: regular  : %lu\n", stats->regular);
    printf("[+]   flist: symlink  : %lu\n", stats->symlink);
    printf("[+]   flist: directory: %lu\n", stats->directory);
    printf("[+]   flist: special  : %lu\n", stats->special);
    printf("[+]   flist: failure  : %lu\n", stats->failure);
    printf("[+]   flist: full size: %lu bytes\n", stats->size);
    printf("[+]\n");
}


static int flister_create(char *workspace) {
    // no backend by default
    flist_backend_t *backend = NULL;

    flist_db_t *database = libflist_db_sqlite_init(workspace);
    database->create(database);

    if(settings.backendhost) {
        flist_db_t *backdb;

        debug("[+] initializing backend\n");

        // shortcut
        zflist_settings_t *s = &settings;

        if(!(backdb = libflist_db_redis_init_tcp(s->backendhost, s->backendport, "default", s->bpass, s->token))) {
            fprintf(stderr, "[-] backend: %s\n", libflist_strerror());
            return 1;
        }

        // initizlizing backend as requested
        if(!(backend = libflist_backend_init(backdb, settings.create))) {
            return 1;
        }
    }

    // building database
    flist_stats_t *stats;

    if(!(stats = libflist_create(database, settings.create, backend))) {
        fprintf(stderr, "[-] flist_create: %s\n", libflist_strerror());
        return 1;
    }

    if(settings.json)
        flister_create_json(stats);

    else flister_create_response(stats);

    free(stats);

    // closing database before archiving
    debug("[+] closing database\n");
    database->close(database);

    // removing possible already existing db
    unlink(settings.archive);
    libflist_archive_create(settings.archive, workspace);

    if(settings.upload && settings.token) {
        debug("[+] uploading flist to the hub: %s\n", settings.upload);

        if(flist_upload(settings.archive, settings.upload, settings.token))
            fprintf(stderr, "[-] could not upload the flist\n");
    }

    return 0;
}

static int flister_list(char *workspace) {
    int value = 0;

    // if json flag is set, ensure we output
    // json format listing
    // if(settings.json)
    //    settings.list = LIST_JSON;

    if(!libflist_archive_extract(settings.archive, workspace)) {
        warnp("libflist_archive_extract");
        return 1;
    }

    debug("[+] loading database\n");
    flist_db_t *database = libflist_db_sqlite_init(workspace);
    database->open(database);

    debug("[+] walking over database\n");
    value = flist_listing(database, &settings);

    debug("[+] closing database\n");
    database->close(database);

    return value;
}

static int flister_merge(char *workspace) {
    flist_merge_t *merge = &settings.merge;
    int value = 0;
    char *intermediate;

    #ifndef FLIST_DEBUG
    fprintf(stderr, "[-] merging not yet available in release\n");
    return 1;
    #endif

    flist_db_t *finaldb = libflist_db_sqlite_init(workspace);
    finaldb->create(finaldb);

    dirnode_t *fulltree = NULL;

    for(size_t i = 0; i < merge->length; i++) {
        debug("[+] merging source: %s\n", merge->sources[i]);

        // create a temporary directory for this source
        if(!(intermediate = libflist_workspace_create()))
            diep("intermediate workspace_create");

        // extract this source flist
        if(!libflist_archive_extract(merge->sources[i], intermediate)) {
            warnp("intermediate libflist_archive_extract");
            return 1;
        }

        // load source database
        flist_db_t *database = libflist_db_sqlite_init(intermediate);
        database->open(database);

        // merging this source
        dirnode_t *tree = libflist_directory_get_recursive(database, "/");
        if(!libflist_merge(&fulltree, tree)) {
            fprintf(stderr, "[-] libflist_merge: %s\n", libflist_strerror());
            return 1;
        }

        // closing source database
        database->close(database);

        // cleaning source temporary directory
        if(!libflist_workspace_destroy(intermediate))
            diep("workspace_destroy");

        free(intermediate);
    }

    // writing merged database
    libflist_dirnode_commit(fulltree, finaldb, fulltree, NULL);

    // closing database before archiving
    debug("[+] closing database\n");
    finaldb->close(finaldb);

    // removing possible already existing db
    unlink(settings.archive);
    libflist_archive_create(settings.archive, workspace);

    return value;
}

static int flister() {
    char *workspace;
    int value = 0;

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
        value = flister_create(workspace);
    }

    //
    // listing
    //
    if(settings.list) {
        debug("[+] extracting archive\n");
        if((value = flister_list(workspace)))
            goto clean;
    }

    //
    // merging
    //
    if(settings.merge.length) {
        debug("[+] merging flists\n");
        if((value = flister_merge(workspace)))
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

    return value;
}

int main(int argc, char *argv[]) {
    int option_index = 0;
    int i;

    #ifndef FLIST_DEBUG
    // disable library debug in release mode
    libflist_debug_enable(0);
    #endif

    // reset settings
    memset(&settings, 0, sizeof(zflist_settings_t));
    libflist_merge_list_init(&settings.merge);

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
                libflist_merge_list_append(&settings.merge, optarg);
                break;
            }

            case 'r': {
                settings.ramdisk = 1;
                break;
            }

            case 'c': {
                // root path
                settings.create = optarg;

                break;
            }

            case 'x': {
                settings.bpass = optarg;
                break;
            }

            case 't': {
                settings.token = optarg;
                break;
            }

            case 'u': {
                if(!optarg)
                    optarg = "https://hub.grid.tf/api/flist/me/upload-flist";

                settings.upload = optarg;
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

    if(settings.list && settings.create) {
        fprintf(stderr, "[-] cannot create and list on the same time\n");
        exit(EXIT_FAILURE);
    }

    if(settings.list && settings.merge.sources) {
        fprintf(stderr, "[-] cannot list and merge on the same time\n");
        exit(EXIT_FAILURE);
    }

    if(settings.create && settings.merge.sources) {
        fprintf(stderr, "[-] cannot create and merge on the same time\n");
        exit(EXIT_FAILURE);
    }

    // archive is required
    if(!settings.archive)
        usage(argv[0]);

    // process
    int value = flister();

    // cleaning
    free(settings.archive);
    free(settings.backendhost);
    libflist_merge_list_free(&settings.merge);

    return value;
}
