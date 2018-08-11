#ifndef LIBFLIST_WORKSPACE_H
    #define LIBFLIST_WORKSPACE_H

    char *workspace_create();
    char *workspace_destroy(char *mountpoint);

    char *ramdisk_create();
    char *ramdisk_destroy(char *mountpoint);
#endif
