#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <blake2.h>
#include <pwd.h>
#include <grp.h>
#include "libflist.h"
#include "verbose.h"

//
// access-control list management
//
static char *uidstr(struct passwd *passwd, uid_t uid) {
    char *target;

    // if username cannot be found
    // let use user id as username
    if(!passwd) {
        libflist_warnp("getpwuid");

        if(asprintf(&target, "%d", uid) < 0)
            diep("asprintf");

        return target;
    }

    return strdup(passwd->pw_name);
}

static char *gidstr(struct group *group, gid_t gid) {
    char *target;

    // if group name cannot be found
    // let use group id as groupname
    if(!group) {
        warnp("getgrpid");
        if(asprintf(&target, "%d", gid) < 0)
            diep("asprintf");

        return target;
    }

    return strdup(group->gr_name);
}

char *flist_acl_key(acl_t *acl) {
    char *key;
    char strmode[32];

    // re-implementing the python original version
    // the mode is created using 'oct(mode)[4:]'
    //
    // when doing oct(int), the returns is '0oXXXX'
    // so we fake the same behavior
    sprintf(strmode, "0o%o", acl->mode);

    char *un = acl->uname;
    char *gn = acl->gname;
    int64_t uid = acl->uid;
    int64_t gid = acl->gid;
    char *mode = strmode + 4;

    // intermediate string key
    if(asprintf(&key, "user:%s\ngroup:%s\nmode:%s\nuid:%ld\ngid:%ld", un, gn, mode, uid, gid) < 0)
        diep("asprintf");

    // hashing payload
    unsigned char hash[FLIST_ACL_KEY_LENGTH];

    if(blake2b(hash, key, "", FLIST_ACL_KEY_LENGTH, strlen(key), 0) < 0)
        dies("blake2 acl error");

    char *hashkey = libflist_hashhex(hash, FLIST_ACL_KEY_LENGTH);
    free(key);

    return hashkey;
}

char *libflist_acl_key(acl_t *acl) {
    return flist_acl_key(acl);
}

acl_t *flist_acl_commit(acl_t *acl) {
    if(acl->key)
        free(acl->key);

    acl->key = flist_acl_key(acl);
    return acl;
}

acl_t *libflist_acl_commit(acl_t *acl) {
    return flist_acl_commit(acl);
}

acl_t *flist_acl_new(char *uname, char *gname, int mode, int64_t uid, int64_t gid) {
    acl_t *acl;

    if(!(acl = malloc(sizeof(acl_t)))) {
        warnp("acl: new: malloc");
        return NULL;
    }

    acl->mode = mode;
    acl->uname = strdup(uname);
    acl->gname = strdup(gname);
    acl->uid = uid;
    acl->gid = gid;

    // generate key from content
    acl->key = flist_acl_key(acl);

    return acl;
}

acl_t *libflist_acl_new(char *uname, char *gname, int mode, int64_t uid, int64_t gid) {
    return flist_acl_new(uname, gname, mode, uid, gid);
}

acl_t *flist_acl_duplicate(acl_t *source) {
    acl_t *s = source;
    return flist_acl_new(s->uname, s->gname, s->mode, s->uid, s->gid);
}

acl_t *libflist_acl_duplicate(acl_t *source) {
    return flist_acl_duplicate(source);
}

acl_t *flist_acl_from_stat(const struct stat *sb) {
    char *uname = uidstr(getpwuid(sb->st_uid), sb->st_uid);
    char *gname = gidstr(getgrgid(sb->st_gid), sb->st_gid);

    // keep only the permissions mode
    mode_t mode = sb->st_mode & ~S_IFMT;

    acl_t *acl = flist_acl_new(uname, gname, mode, sb->st_uid, sb->st_gid);

    free(uname);
    free(gname);

    return acl;
}

acl_t *libflist_acl_from_stat(const struct stat *sb) {
    return flist_acl_from_stat(sb);
}

void flist_acl_free(acl_t *acl) {
    free(acl->uname);
    free(acl->gname);
    free(acl->key);
    free(acl);
}

void libflist_acl_free(acl_t *acl) {
    flist_acl_free(acl);
}

