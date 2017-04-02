#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include "flister.h"
#include "archive.h"
#include "ramdisk.h"
#include "database.h"
#include "flist.h"

settings_t settings;

static struct option long_options[] = {
	{"list",    no_argument,       0, 'l'},
	{"tree",    no_argument,       0, 't'},
	{"archive", required_argument, 0, 'a'},
	{"verbose", no_argument,       0, 'v'},
	{"help",    no_argument,       0, 'h'},
	{0, 0, 0, 0}
};

void warnp(char *str) {
	fprintf(stderr, "[-] %s: %s\n", str, strerror(errno));
}

void diep(char *str) {
	warnp(str);
	exit(EXIT_FAILURE);
}

void dies(char *str) {
	fprintf(stderr, "[-] %s\n", str);
	exit(EXIT_FAILURE);
}

int usage(char *basename) {
	fprintf(stderr, "Usage: %s --archive <filename> [--list] [--tree] [--verbose]\n", basename);
	fprintf(stderr, "\n");
	fprintf(stderr, "Command line options:\n");
	fprintf(stderr, "  -a --archive    archive filename (required)\n");
	fprintf(stderr, "  -l --list       list archive content\n");
	fprintf(stderr, "  -t --tree       list archive content in a tree view\n");
	fprintf(stderr, "  -v --verbose    enable verbose messages\n");
	fprintf(stderr, "  -h --help       shows this help message\n");
	exit(EXIT_FAILURE);
}

static int flister() {
	char *tmpfs;

	verbose("[+] initializing ramdisk\n");
	if(!(tmpfs = ramdisk_create()))
		diep("ramdisk_create");

	verbose("[+] ramdisk: %s\n", tmpfs);

	verbose("[+] extracting archive\n");
	if(!archive_extract(settings.archive, tmpfs)) {
		warnp("archive_extract");
		goto clean;
	}

	verbose("[+] loading rocksdb database\n");
	database_t *database = database_open(tmpfs);

	verbose("[+] walking over database\n");
	flist_walk(database);

	verbose("[+] closing database\n");
	database_close(database);

clean:
	verbose("[+] cleaning ramdisk\n");
	if(!ramdisk_destroy(tmpfs))
		diep("ramdisk_destroy");

	free(tmpfs);

	return 0;
}

int main(int argc, char *argv[]) {
	int option_index = 0;
	int i;

	// initializing settings
	settings.list = 1;
	settings.tree = 0;
	settings.verbose = 0;
	settings.archive = NULL;

	while(1) {
		i = getopt_long(argc, argv, "ltfa:vh", long_options, &option_index);

		if(i == -1)
			break;

		switch(i) {
			case 'l':
				settings.list = 1;
				break;

			case 't':
				settings.tree = 1;
				break;

			case 'a':
				settings.archive = strdup(optarg);
				break;

			case 'v':
				settings.verbose = 1;
				break;

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

	return value;
}
