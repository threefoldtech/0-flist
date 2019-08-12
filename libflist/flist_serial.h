#ifndef LIBFLIST_FLIST_SERIAL_H
    #define LIBFLIST_FLIST_SERIAL_H

    // serializers
    void flist_serial_commit_acl(flist_db_t *database, acl_t *acl);
    void flist_serial_commit_dirnode(dirnode_t *root, flist_ctx_t *ctx, dirnode_t *parent);

    // deserializers
    dirnode_t *flist_serial_get_dirnode(flist_db_t *database, char *key, char *fullpath);
    acl_t *flist_serial_get_acl(flist_db_t *database, const char *aclkey);
#endif
