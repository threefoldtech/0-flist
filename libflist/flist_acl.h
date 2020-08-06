#ifndef LIBFLIST_FLIST_ACL_H
    #define LIBFLIST_FLIST_ACL_H

    char *flist_acl_key(acl_t *acl);
    acl_t *flist_acl_commit(acl_t *acl);
    acl_t *flist_acl_new(char *uname, char *gname, int mode, int64_t uid, int64_t gid);
    acl_t *flist_acl_duplicate(acl_t *source);
    acl_t *flist_acl_from_stat(const struct stat *sb);
    void flist_acl_free(acl_t *acl);
#endif
