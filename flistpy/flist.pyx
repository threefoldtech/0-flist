cimport cflist

cdef class Flist:
    def __init__(self, path, mntpath):
        self.path = path
        self.mntpath = mntpath
        self.bmntpath = mntpath.encode()

    cdef init(self, str flist):
        cdef char* flistpath
        bflist = flist.encode()
        flistpath = bflist
        cdef char* mntpath 
        mntpath = self.bmntpath 
        cdef cflist.flist_db_t *database = cflist.libflist_db_sqlite_init(mntpath)
        database.open(database)

        cdef cflist.flist_ctx_t* ctx = cflist.libflist_context_create(database, NULL)

        cdef cflist.dirnode_t *root = cflist.libflist_dirnode_create("", "")
        cflist.libflist_dirnode_free(root)
        database.close(database)

    # def open(flist):
    #     pass
    # def ls(path):
    #     pass
    # def find(path):
    #     pass
    # def stat(path):
    #     pass
    # def cat(file_):
    #     pass
    # def put(file_, flist):
    #     pass
    # def putdir(dir_, flist):
    #     pass

    # def chmod(file_, perms):
    #     pass

    # def rm(file_):
    #     pass

    # def rmdir(path):
    #     pass

    # def mkdir(path):
    #     pass

    # def metadata(flist): # get or set?
    #     pass

    # def merge(flist1, flist2):
    #     pass

    # def commit():
    #     pass

    # def close():
    #     pass
