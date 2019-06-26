#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include "zflist-edit.h"
#include "actions.h"

zfe_settings_t settings;

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


//
// commands list
//
zf_cmds_t zf_commands[] = {
    {.name = "open",   .callback = zf_open,   .help = "open an flist to enable editing"},
    {.name = "chmod",  .callback = zf_chmod,  .help = "change mode of a file (like chmod command)"},
    {.name = "rm",     .callback = zf_rm,     .help = "remove a file (not a directory)"},
    {.name = "commit", .callback = zf_commit, .help = "close an flist and commit changes"},
};

int usage(char *basename) {
    fprintf(stderr, "Usage: %s [temporary-point] open <filename>\n", basename);
    fprintf(stderr, "       %s [temporary-point] <action> <arguments>\n", basename);
    fprintf(stderr, "       %s [temporary-point] commit [optional-new-flist]\n", basename);
    fprintf(stderr, "\n");
    fprintf(stderr, "  The temporary-point should be a directory where temporary files\n");
    fprintf(stderr, "  will be used for keeping tracks of your changes.\n");
    fprintf(stderr, "  You can also use FLISTDIR environment variable to set your\n");
    fprintf(stderr, "  temporary-point directory and not specify it on the command line.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  First, you need to -open- an flist, then you can do some -edit-\n");
    fprintf(stderr, "  and finally you can -commit- (close) your changes to a new flist.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Available actions:\n");

    for(unsigned int i = 0; i < sizeof(zf_commands) / sizeof(zf_cmds_t); i++)
        fprintf(stderr, "  %-15s %s\n", zf_commands[i].name, zf_commands[i].help);

    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    char *action = NULL;
    int value = -1;
    int nargc;
    char **nargv;

    debug("[+] initializing zflist-edit\n");

    // reset settings
    memset(&settings, 0, sizeof(zfe_settings_t));

    #ifndef FLIST_DEBUG
    // disable library debug in release mode
    libflist_debug_enable(0);
    #endif

    // checking if environment variable is set
    if((settings.mnt = getenv("FLISTDIR"))) {
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

    if(nargc < 1)
        usage(argv[0]);

    debug("[+] system: mountpoint: %s\n", settings.mnt);

    // iterating over available commands and trigger callback
    // for matching command
    for(unsigned int i = 0; i < sizeof(zf_commands) / sizeof(zf_cmds_t); i++)
        if(strcasecmp(zf_commands[i].name, action) == 0)
            value = zf_commands[i].callback(nargc, nargv, &settings);

    if(value == -1) {
        fprintf(stderr, "Unknown action '%s'\n", action);
        exit(EXIT_FAILURE);
    }

    return value;
}
