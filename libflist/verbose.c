#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include "verbose.h"

char libflist_internal_error[1024] = "Success";

// global static flag to enable or disable
// debug message on the whole library
//
// you should not change this variable directoy
// but use the 'libflist_debug_enable' function
// which could potentially do more than just changing
// the variable
int libflist_debug_flag = 1;

void libflist_debug_enable(int enable) {
    libflist_debug_flag = enable;
}

// error handling
//
// here are defined how error handling works
// basicly we keep a static string buffer in memory
// which will contains the last string error
//
// this error can be retrived via 'libflist_strerror'
const char *libflist_strerror() {
    return (const char *) libflist_internal_error;
}

void *libflist_set_error(const char *format, ...) {
    va_list args;

    va_start(args, format);
    vsnprintf(libflist_internal_error, sizeof(libflist_internal_error), format, args);
    va_end(args);

    return NULL;
}

void *libflist_errp(const char *str) {
    libflist_set_error("%s: %s", str, strerror(errno));
    return NULL;
}

void *libflist_diep(const char *str) {
    return libflist_errp(str);
}

void *libflist_dies(const char *str) {
    if(!libflist_debug_flag)
        return NULL;

    fprintf(stderr, "[-] %s\n", str);
    return NULL;
}

void libflist_warnp(const char *str) {
    debug("%s: %s", str, strerror(errno));
}

void libflist_warns(const char *str) {
    if(!libflist_debug_flag)
        return;

    printf("[-] %s\n", str);
}


// hex dumps
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

// duplicate a buffer
void *libflist_bufdup(void *source, size_t length) {
    void *buffer;

    if(!(buffer = malloc(length)))
        return NULL;

    memcpy(buffer, source, length);

    return buffer;
}


