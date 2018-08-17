#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ftw.h>
#include <unistd.h>
#include <sys/mount.h>
#include "libflist.h"
#include "verbose.h"

static int workspace_clean_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
	(void) sb;
	(void) typeflag;
	(void) ftwbuf;

    if(remove(fpath))
		libflist_warnp(fpath);

    return 0;
}

static int workspace_clean(char *workspace) {
    return nftw(workspace, workspace_clean_cb, 64, FTW_DEPTH | FTW_PHYS);
}

char *libflist_workspace_create() {
	char *mountpoint;
	char path[32] = "/tmp/flist_XXXXXX";

	if(!(mountpoint = mkdtemp(path)))
		return libflist_errp(path);

	return strdup(mountpoint);
}

char *libflist_workspace_destroy(char *mountpoint) {
	workspace_clean(mountpoint);
	return (char *) 1;
}

char *libflist_ramdisk_create(char *mountpoint) {
	if(mount("tmpfs", mountpoint, "tmpfs", 0, "size=32M"))
		return libflist_errp("mount");

	return mountpoint;
}

char *libflist_ramdisk_destroy(char *mountpoint) {
	if(umount(mountpoint))
		return libflist_errp("umount");

	return mountpoint;
}


