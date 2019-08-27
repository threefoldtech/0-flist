cimport cflist
import os

class MountPointHasDatabaseAlready(Exception):
    pass

cdef class Flist:
    def __init__(self, path, mntpath):
        self.path = path
        self.bpath = self.path.encode()
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
        print("inited..")

    @property
    def sqlite_db_path(self):
        return "{}/flistdb.sqlite3".format(self.mntpath)

    def open(self):
        os.makedirs(self.mntpath, exist_ok=True)
        if os.path.exists(self.sqlite_db_path):
            raise MountPointHasDatabaseAlready("mount point already contains db")

        done = cflist.libflist_archive_extract(self.bpath, self.bmntpath)
        if not done:
            raise RuntimeError("extract/archive")
        
        print("done open.")

    def commit(self):
        os.unlink(self.path)
        done = cflist.libflist_archive_create(self.bpath, self.bmntpath)
        if not done:
            raise RuntimeError("couldn't create flist")
        print("flist created")

    def close(self):
        if os.path.isfile(self.sqlite_db_path):
            os.unlink(self.sqlite_db_path)
        else:
            print("no database at {}".format(self.sqlite_db_path))

    cdef ls(self, path="/"):
        bthepath = path.encode()
        cdef char* dirpath = bthepath
        mntpath = self.bmntpath 

        print("listing ", path)

        cdef cflist.flist_db_t *database = cflist.libflist_db_sqlite_init(mntpath)
        database.open(database)

        cdef cflist.flist_ctx_t* ctx = cflist.libflist_context_create(database, NULL)

        cdef cflist.dirnode_t *dirnode

        dirnode = cflist.libflist_dirnode_get(database, dirpath)
        if not dirnode:
            raise RuntimeError("ls", "no such directory (file parent directory)");

        # if(cb->jout)
        #     return zf_ls_json(cb, dirnode);
        cdef cflist.inode_t *inode
        inode = dirnode.inode_list
        while inode != NULL:
            print("{} {} {} {} {}".format(inode.acl.uname, inode.acl.gname, inode.size, inode.name))
            inode = inode.next
            # inode = nextaddr
        cflist.libflist_dirnode_free(dirnode)

    def put(self, filepath_, intopath_):
        #/opt/0-flist/zflist/zflist put /tmp/config-err-uhZViD /
        bfilepath = filepath_.encode()
        cdef char* filepath = bfilepath
        bintopath = intopath_.encode()
        cdef char* intopath = bintopath


        envbackend = os.getenv("UPLOADBACKEND")
        envbackend_encoded = envbackend.encode()
        cdef char* benvbackend = envbackend_encoded


        cdef cflist.flist_ctx_t* ctx

        cdef cflist.flist_db_t* backdb
        backdb = cflist.libflist_metadata_backend_database_json(benvbackend)
        
        
        ctx = cflist.libflist_context_create(backdb, NULL)

        ctx.db = backdb
        
        blocalfile = os.path.basename(filepath_).encode()
        cdef char* localfile = blocalfile
        cdef char* filename = blocalfile

        bdirpath = os.path.dirname(intopath_).encode()
        btargetname = os.path.basename(intopath_).encode()

        cdef char* dirpath = bdirpath
        cdef char* targetname = btargetname



        # // avoid root directory and directory name
        # if(strcmp(targetname, "/") == 0 || strcmp(targetname, dirpath) == 0)
        #     targetname = filename;

        cdef cflist.dirnode_t *dirnode
        cdef cflist.inode_t *inode

        dirnode = cflist.libflist_dirnode_get(backdb, intopath)
        if not dirnode:
            print("put looking for directory")
            dirnode = cflist.libflist_dirnode_get(backdb, dirpath)
            if not dirnode:
                raise RuntimeError("put no such parent dir.")

        if targetname == dirnode.fullpath:
            targetname = filename


        inode = cflist.libflist_inode_from_name(dirnode, targetname)
        if not inode:
            print("[+] action: put: requested filename  already exists, overwriting: ", targetname)
            if not cflist.libflist_directory_rm_inode(dirnode, inode):
                raise RuntimeError( "put : could not overwrite existing inode");

            cflist.libflist_inode_free(inode)

        inode = cflist.libflist_inode_from_localfile(localfile, dirnode, ctx)
        if not inode:
            raise RuntimeError("put could not load local file")

        cflist.libflist_inode_rename(inode, targetname)

        cflist.libflist_dirnode_appends_inode(dirnode, inode)

        cdef cflist.dirnode_t *parent
        parent = cflist.libflist_dirnode_get_parent(backdb, dirnode)
        cflist.libflist_serial_dirnode_commit(dirnode, ctx, parent)
        cflist.libflist_dirnode_free(parent)
        cflist.libflist_dirnode_free(dirnode)


    def putdir(self, dirpath_, intopath_):
        #/opt/0-flist/zflist/zflist put /tmp/config-err-uhZViD /
        bdirpath = dirpath_.encode()
        cdef char* localdir = bdirpath
        bintopath = intopath_.encode()
        cdef char* destdir = bintopath


        envbackend = os.getenv("UPLOADBACKEND")
        envbackend_encoded = envbackend.encode()
        cdef char* benvbackend = envbackend_encoded


        cdef cflist.flist_ctx_t* ctx

        cdef cflist.flist_db_t* backdb
        backdb = cflist.libflist_metadata_backend_database_json(benvbackend)
        
        
        ctx = cflist.libflist_context_create(backdb, NULL)

        ctx.db = backdb
        

        cdef cflist.dirnode_t *dirnode
        cdef cflist.inode_t *inode

        dirnode = cflist.libflist_dirnode_get(ctx.db, destdir)
        if not dirnode:
            raise RuntimeError("putdir no such parent directory")


        inode = cflist.libflist_inode_from_localdir(localdir, dirnode, ctx)
        if not inode:
            raise RuntimeError("putdir could not load local directory")

        cflist.libflist_stats_dump(&ctx.stats)
        cflist.libflist_dirnode_free(dirnode)

    



    # def cat(file_):
    #     pass


    # def stat(path):
    #     pass
    # def find(path):
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


