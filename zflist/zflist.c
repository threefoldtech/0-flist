#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include "libflist.h"
#include "zflist.h"
#include "actions.h"
#include "tools.h"

zfe_settings_t settings;


//
// commands list
//
zf_cmds_t zf_commands[] = {
    {.name = "open",     .db = 0, .callback = zf_open,     .help = "open an flist to enable editing"},
    {.name = "init",     .db = 0, .callback = zf_init,     .help = "initialize an empty flist to enable editing"},
    {.name = "ls",       .db = 1, .callback = zf_ls,       .help = "list the content of a directory"},
    {.name = "find",     .db = 1, .callback = zf_find,     .help = "list full contents of files and directories"},
    {.name = "stat",     .db = 1, .callback = zf_stat,     .help = "dump inode full metadata"},
    {.name = "cat",      .db = 1, .callback = zf_cat,      .help = "print file contents (backend metadata required)"},
    {.name = "get",      .db = 1, .callback = zf_get,      .help = "download remote file (backend metadata required)"},
    {.name = "put",      .db = 1, .callback = zf_put,      .help = "insert local file into the flist"},
    {.name = "putdir",   .db = 1, .callback = zf_putdir,   .help = "insert local directory into the flist (recursively)"},
    {.name = "chmod",    .db = 1, .callback = zf_chmod,    .help = "change mode of a file (like chmod command)"},
    {.name = "rm",       .db = 1, .callback = zf_rm,       .help = "remove a file (not a directory)"},
    {.name = "rmdir",    .db = 1, .callback = zf_rmdir,    .help = "remove a directory (recursively)"},
    {.name = "mkdir",    .db = 1, .callback = zf_mkdir,    .help = "create an empty directory (non-recursive)"},
    {.name = "metadata", .db = 1, .callback = zf_metadata, .help = "get or set metadata"},
    {.name = "merge",    .db = 1, .callback = zf_merge,    .help = "merge another flist into the current one"},
    {.name = "check",    .db = 1, .callback = zf_check,    .help = "check archive integrity (chunks valid in the backed)"},
    {.name = "chunks",   .db = 1, .callback = zf_chunks,   .help = "dumps chunks for all files"},
    {.name = "debug",    .db = 1, .callback = zf_debug,    .help = "provide and apply some debug features"},
    {.name = "prefetch", .db = 0, .callback = zf_prefetch, .help = "read directory contents to fill flist cache"},
    {.name = "hub",      .db = 0, .callback = zf_hub,      .help = "0-hub command line tools"},
    {.name = "commit",   .db = 0, .callback = zf_commit,   .help = "commit changes to a new flist"},
    {.name = "close",    .db = 0, .callback = zf_close,    .help = "close mountpoint and discard files"},
};

int usage(char *basename) {
    fprintf(stderr, "Usage: %s [temporary-point] open <filename>\n", basename);
    fprintf(stderr, "       %s [temporary-point] <action> <arguments>\n", basename);
    fprintf(stderr, "       %s [temporary-point] commit [optional-new-flist]\n", basename);
    fprintf(stderr, "\n");
    fprintf(stderr, "  zflist: version %s\n", ZFLIST_VERSION);
    fprintf(stderr, "  libflist: version %s\n", libflist_version());
    fprintf(stderr, "\n");
    fprintf(stderr, "  The temporary-point should be a directory where temporary files\n");
    fprintf(stderr, "  will be used for keeping tracks of your changes.\n");
    fprintf(stderr, "  You can also use ZFLIST_MNT environment variable to set your\n");
    fprintf(stderr, "  temporary-point directory and not specify it on the command line.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  By default, this tool will print error and information in text format,\n");
    fprintf(stderr, "  you can get a json output by setting ZFLIST_JSON=1 environment variable.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  By default, you won't have any progression information, but you can\n");
    fprintf(stderr, "  get some progression reporting using ZFLIST_PROGRESS=1 environment variable.\n");
    fprintf(stderr, "  This works using JSON output aswell.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  First, you need to -open- an flist, then you can do some -edit-\n");
    fprintf(stderr, "  and finally you can -commit- (close) your changes to a new flist.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  If you want to upload chunks when inserting files, please set\n");
    fprintf(stderr, "  environment variable ZFLIST_BACKEND to a json backend formatted string,\n");
    fprintf(stderr, "  check backend documentation for more information\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  To use the hub subsystem, you need to specify at least a jwt token\n");
    fprintf(stderr, "  via the environment variable ZFLIST_HUB_TOKEN, this jwt needs to be\n");
    fprintf(stderr, "  valid for the hub. In addition, you can specify ZFLIST_HUB_USER if\n");
    fprintf(stderr, "  your token contains multiple member-of users/organizations.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Available actions:\n");

    for(unsigned int i = 0; i < sizeof(zf_commands) / sizeof(zf_cmds_t); i++)
        fprintf(stderr, "  %-15s %s\n", zf_commands[i].name, zf_commands[i].help);

    exit(EXIT_FAILURE);
}

int zf_callback(zf_cmds_t *cmd, int argc, char *argv[], zfe_settings_t *settings) {
    zf_callback_t cb = {
        .argc = argc,
        .argv = argv,
        .settings = settings,
        .ctx = NULL,
        .jout = NULL,
        .userptr = NULL,
        .progress = 0,
    };

    // check whenever json output is requested
    char *json = getenv("ZFLIST_JSON");

    if(json && strcmp(json, "1") == 0)
        zf_internal_json_init(&cb);

    // check whenever progression is requested
    char *progress = getenv("ZFLIST_PROGRESS");

    if(progress && strcmp(progress, "1") == 0)
        cb.progress = 1;

    // open database (if used)
    if(cmd->db)
        cb.ctx = zf_internal_init(settings->mnt);

    // call the callback
    debug("[+] system: callback found for command: %s\n", cmd->name);
    int value = cmd->callback(&cb);

    // dump json response if set
    if(cb.jout)
        zf_internal_json_finalize(&cb);

    // commit database (if used)
    if(cmd->db)
        zf_internal_cleanup(cb.ctx);

    return value;
}

int main(int argc, char *argv[]) {
    char *action = NULL;
    int value = -1;
    int nargc;
    char **nargv;

    debug("[+] initializing zflist\n");

    // reset settings
    memset(&settings, 0, sizeof(zfe_settings_t));

    #ifndef FLIST_DEBUG
    // disable library debug in release mode
    libflist_debug_enable(0);
    #endif

    // checking if environment variable is set
    if((settings.mnt = getenv("ZFLIST_MNT"))) {
        debug("[+] system: entrypoint set from environment variable\n");
        action = argv[1];
        nargc = argc - 1;
        nargv = argv + 1;

    } else {
        settings.mnt = argv[1];
        action = argv[2];
        nargc = argc - 2;
        nargv = argv + 2;
    }

    if(!(settings.baseurl = getenv("ZFLIST_HUB")))
        settings.baseurl = ZFLIST_HUB_BASEURL;

    if(nargc < 1)
        usage(argv[0]);

    debug("[+] system: mountpoint: %s\n", settings.mnt);

    // iterating over available commands and trigger callback
    // for matching command
    for(unsigned int i = 0; i < sizeof(zf_commands) / sizeof(zf_cmds_t); i++)
        if(strcasecmp(zf_commands[i].name, action) == 0)
            value = zf_callback(&zf_commands[i], nargc, nargv, &settings);

    if(value == -1) {
        fprintf(stderr, "Unknown action '%s'\n", action);
        exit(EXIT_FAILURE);
    }

    return value;
}
