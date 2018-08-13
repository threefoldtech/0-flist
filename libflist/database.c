#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <unistd.h>
#include "libflist.h"
#include "debug.h"
#include "database.h"
#include "database_redis.h"
#include "database_sqlite.h"

void warndb(char *source, const char *str) {
    debug("[-] database: %s: %s\n", source, str);
}

