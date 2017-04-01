#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mount.h>
#include "ramdisk.h"
#include "flister.h"

char *ramdisk_create() {
	char *mountpoint;
	char path[32] = "/tmp/flist_XXXXXX";

	if(!(mountpoint = mkdtemp(path)))
		return NULL;

	if(mount("tmpfs", mountpoint, "tmpfs", 0, "size=32M")) {
		rmdir(mountpoint);
		return NULL;
	}

	return strdup(mountpoint);
}

char *ramdisk_destroy(char *mountpoint) {
	if(umount(mountpoint))
		return NULL;

	if(rmdir(mountpoint))
		return NULL;

	return mountpoint;
}
