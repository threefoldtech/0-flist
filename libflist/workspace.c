#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ftw.h>
#include <unistd.h>
#include <sys/mount.h>
#include "debug.h"
#include "workspace.h"
#include "flister.h"

static int workspace_clean_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
	(void) sb;
	(void) typeflag;
	(void) ftwbuf;

    if(remove(fpath))
		warnp(fpath);

    return 0;
}

static int workspace_clean(char *workspace) {
    return nftw(workspace, workspace_clean_cb, 64, FTW_DEPTH | FTW_PHYS);
}

char *workspace_create() {
	char *mountpoint;
	char path[32] = "/tmp/flist_XXXXXX";

	if(!(mountpoint = mkdtemp(path)))
		return NULL;

	return strdup(mountpoint);
}

char *workspace_destroy(char *mountpoint) {
	workspace_clean(mountpoint);
	return (char *) 1;
}

char *ramdisk_create(char *mountpoint) {
	if(mount("tmpfs", mountpoint, "tmpfs", 0, "size=32M"))
		return NULL;

	return mountpoint;
}

char *ramdisk_destroy(char *mountpoint) {
	if(umount(mountpoint))
		return NULL;

	return mountpoint;
}


