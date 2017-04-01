#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "archive.h"
#include "ramdisk.h"
#include "database.h"
#include "flist.h"

void warnp(char *str) {
	fprintf(stderr, "[-] %s: %s\n", str, strerror(errno));
}

void diep(char *str) {
	warnp(str);
	exit(EXIT_FAILURE);
}

int usage(char *basename) {
	fprintf(stderr, "[-] Usage: %s archive.flist\n", basename);
	return EXIT_FAILURE;
}

int main(int argc, char *argv[]) {
	char *tmpfs, *archive;

	if(argc < 2)
		return usage(argv[0]);

	archive = argv[1];


	printf("[+] initializing ramdisk\n");
	if(!(tmpfs = ramdisk_create()))
		diep("ramdisk_create");

	printf("[+] ramdisk: %s\n", tmpfs);

	printf("[+] extracting archive\n");
	if(!archive_extract(archive, tmpfs)) {
		warnp("archive_extract");
		goto clean;
	}

	printf("[+] loading rocksdb database\n");
	database_t *database = database_open(tmpfs);

	printf("[+] walking over database\n");
	flist_walk(database);

	printf("[+] closing database\n");
	database_close(database);

clean:
	printf("[+] cleaning ramdisk\n");
	if(!ramdisk_destroy(tmpfs))
		diep("ramdisk_destroy");

	free(tmpfs);

	return 0;
}
