#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

// global static flag to enable or disable
// debug message on the whole library
int libflist_debug_flag = 1;

void libflist_debug_enable(int enable) {
    libflist_debug_flag = enable;
}

void libflist_warnp(const char *str) {
    if(!libflist_debug_flag)
        return;

    fprintf(stderr, "[-] %s: %s\n", str, strerror(errno));
}

void libflist_diep(const char *str) {
    if(!libflist_debug_flag)
        return;

    libflist_warnp(str);
    // exit(EXIT_FAILURE);
}

void libflist_dies(const char *str) {
    if(!libflist_debug_flag)
        return;

    fprintf(stderr, "[-] %s\n", str);
    // exit(EXIT_FAILURE);
}

void libflist_warns(const char *str) {
    if(!libflist_debug_flag)
        return;

    printf("[-] %s\n", str);
}

static char __hex[] = "0123456789abcdef";

char *libflist_hashhex(unsigned char *hash, int length) {
    char *buffer = calloc((length * 2) + 1, sizeof(char));
    char *writer = buffer;

    for(int i = 0, j = 0; i < length; i++, j += 2) {
        *writer++ = __hex[(hash[i] & 0xF0) >> 4];
        *writer++ = __hex[hash[i] & 0x0F];
    }

    return buffer;
}

void *libflist_bufdup(void *source, size_t length) {
    void *buffer;

    if(!(buffer = malloc(length)))
        return NULL;

    memcpy(buffer, source, length);

    return buffer;
}


