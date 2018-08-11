#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

void libflist_warnp(const char *str) {
    fprintf(stderr, "[-] %s: %s\n", str, strerror(errno));
}

void libflist_diep(const char *str) {
    libflist_warnp(str);
    exit(EXIT_FAILURE);
}

void libflist_dies(const char *str) {
    fprintf(stderr, "[-] %s\n", str);
    exit(EXIT_FAILURE);
}

void libflist_warns(const char *str) {
    printf("[-] %s\n", str);
}

