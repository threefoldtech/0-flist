#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libtar.h>
#include <zlib.h>
#include <libgen.h>
#include "archive.h"
#include "flister.h"

#define CHUNK 0x4000
#define ENABLE_ZLIB_GZIP 32

//
// uncompress source to destination
//
static int ungzip(FILE *source, FILE *destination) {
	z_stream strm;
	unsigned char in[CHUNK];
	unsigned char out[CHUNK];

	memset(&strm, 0, sizeof(z_stream));
	strm.next_in = in;

	// initializing inflate
	if(inflateInit2(&strm, 15 | ENABLE_ZLIB_GZIP) != Z_OK)
		return 1;

	while (1) {
		int bytes_read;

		if((bytes_read = fread(in, sizeof(char), sizeof(in), source)))
			if(ferror(source))
				return 1;

		strm.avail_in = bytes_read;

		do {
			unsigned have;
			strm.avail_out = CHUNK;
			strm.next_out = out;

			if((inflate(&strm, Z_NO_FLUSH)) < 0)
				return 1;

			have = CHUNK - strm.avail_out;
			fwrite(out, sizeof(unsigned char), have, destination);
		}

		while(strm.avail_out == 0);

		if(feof(source)) {
			inflateEnd(&strm);
			break;
		}
	}

	return 0;
}

//
// uncompress gzip archive to the temporary directory
//
static char *archive_prepare(char *filename, char *target) {
	char *destination;
	FILE *src, *dst;

	if(!(src = fopen(filename, "r")))
		diep(filename);

	if(asprintf(&destination, "%s/%s", target, basename(filename)) < 0)
		diep("asprintf");

	if(!(dst = fopen(destination, "w")))
		diep(destination);

	// uncompress source to destination
	printf("[+] uncompressing to: %s\n", destination);
	if(ungzip(src, dst)) {
		free(destination);
		destination = NULL;
		errno = ENOEXEC; // FIXME
	}

	fclose(src);
	fclose(dst);

	return destination;
}

char *archive_extract(char *filename, char *target) {
	TAR *th = NULL;
	struct stat st;
	char *destination;

	// checking if file is reachable
    if(stat(filename, &st))
		return NULL;

	// uncompression tar file to our ramdisk
	printf("[+] uncompressing archive: %s\n", filename);
	if(!(destination = archive_prepare(filename, target)))
		return NULL;

	// loading tar and extracting contents
	printf("[+] loading archive: %s\n", destination);
	if(tar_open(&th, destination, NULL, O_RDONLY, 0644, TAR_GNU))
		return NULL;

	// TODO: ensure security on files

	printf("[+] extracting archive\n");
	if(tar_extract_all(th, target))
		filename = NULL;

	tar_close(th);
	free(destination);

	return filename;
}

