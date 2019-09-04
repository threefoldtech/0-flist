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

    cdef open(self):
        os.makedirs(self.mntpath, exist_ok=True)
        if os.path.exists(self.sqlite_db_path):
            raise MountPointHasDatabaseAlready("mount point already contains db")

        done = cflist.libflist_archive_extract(self.bpath, self.bmntpath)
        if not done:
            raise RuntimeError("extract/archive")
        
        print("done open.")

    cdef commit(self):
        os.unlink(self.path)
        done = cflist.libflist_archive_create(self.bpath, self.bmntpath)
        if not done:
            raise RuntimeError("couldn't create flist")
        print("flist created")

    cdef close(self):
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

    
    def cat(file_):
        envbackend = os.getenv("UPLOADBACKEND")
        envbackend_encoded = envbackend.encode()
        cdef char* benvbackend = envbackend_encoded


        cdef cflist.flist_ctx_t* ctx

        cdef cflist.flist_db_t* backdb
        backdb = cflist.libflist_metadata_backend_database_json(benvbackend)
        
        ctx = cflist.libflist_context_create(backdb, NULL)

        ctx.db = backdb
        

        backdb = cflist.libflist_metadata_backend_database(ctx.db)
        if not backdb:
            raise RuntimeError("cat", "backend: ")

        cdef cflist.flist_backend_t *backend
        backend = cflist.libflist_backend_init(backdb, "/")


        bfilename = os.path.basename(file_).encode()
        bdirpath = os.path.dirname(file_).encode()

        cdef char* filename = bfilename
        cdef char* dirpath = bdirpath

        print("[+] action: cat: looking for: {} in {}\n".format(bfilename, bdirpath))

        cdef cflist.dirnode_t *dirnode
        cdef cflist.inode_t *inode


        dirnode = cflist.libflist_dirnode_get(ctx.db, dirpath)
        if not dirnode:
            print("cat no such parent directory")
            raise RuntimeError("err no such parent")


        inode = cflist.libflist_inode_from_name(dirnode, filename)
        
        if not inode:
            print("cat no such file")
            raise RuntimeError("no such file..")

        cdef size_t i = 0
        cdef cflist.inode_chunk_t *ichunk = &inode.chunks.list[i];
        cdef cflist.flist_chunk_t *chunk = cflist.libflist_chunk_new(ichunk.entryid, ichunk.decipher, NULL, 0)

        while i < inode.chunks.size:

            downloaded = cflist.libflist_backend_download_chunk(backend, chunk)
            if not downloaded:
                print("couldn't download {}".format(cflist.libflist_strerror()))
                raise RuntimeError("couldn't download {}".format(cflist.libflist_strerror()))
                # zf_error(cb, "cat", "could not download file: %s", libflist_strerror());
            

            print("{} {}".format(chunk.plain.length, chunk.plain.data))
            cflist.libflist_chunk_free(chunk)
            i += 1


        cflist.libflist_dirnode_free(dirnode)
        cflist.libflist_backend_free(backend)



    cdef stat(self, path):
        bdirpath = path.encode()
        parent_ = os.path.dirname(path)
        filename_ = os.path.basename(path)

        bfilename = filename_.encode()
        bparentdir = parent_.encode()

        cdef char* parentdir = bparentdir
        cdef char* filename = bfilename


        cdef cflist.dirnode_t *dirnode
        cdef cflist.inode_t *inode



        envbackend = os.getenv("UPLOADBACKEND")
        envbackend_encoded = envbackend.encode()
        cdef char* benvbackend = envbackend_encoded


        cdef cflist.flist_ctx_t* ctx

        cdef cflist.flist_db_t* backdb
        backdb = cflist.libflist_metadata_backend_database_json(benvbackend)

        ctx = cflist.libflist_context_create(backdb, NULL)

        ctx.db = backdb

        dirnode = cflist.libflist_dirnode_get(ctx.db, parentdir)

        if not dirnode:
            print("stat error no parent")
            raise RuntimeError("stat error no parent")

        inode = cflist.libflist_inode_from_name(dirnode, filename)
        if not inode:
            print("stat no such file or directory")
            raise RuntimeError("stat no such file or directory")


        cdef int value
        ## TODO fixme.
        print("stats here..")
        # value = self._stat_inode(inode);
        
        cflist.libflist_dirnode_free(dirnode);

        # return value

    cdef _stat_inode(self, inode, json=False):
        if json:
            return self._stat_inode_json(inode)
        else:
            return self._stat_inode_text(inode)

    cdef _stat_inode_text(self, inode):
        pass

    cdef _stat_inode_json(self, inode):
        pass

    # def find(path):
    #     pass
    # def chmod(file_, perms):
    #     pass

    cdef rm(self, path):
        bfilename = os.path.basename(path).encode()
        bdirpath = os.path.dirname(path).encode()
        print("[+] action: rm: removing {} from {}", bfilename, bdirpath)

        cdef char* filename = bfilename
        cdef char* dirpath = bdirpath

        cdef cflist.dirnode_t* dirnode
        cdef cflist.inode_t* inode

        envbackend = os.getenv("UPLOADBACKEND")
        envbackend_encoded = envbackend.encode()
        cdef char* benvbackend = envbackend_encoded
        cdef cflist.flist_ctx_t* ctx
        cdef cflist.flist_db_t* backdb
        backdb = cflist.libflist_metadata_backend_database_json(benvbackend)

        ctx = cflist.libflist_context_create(backdb, NULL)

        ctx.db = backdb

        dirnode = cflist.libflist_dirnode_get(ctx.db, dirpath)
        
        if not dirnode:
            print("rm no such directory (file parent directory)")
            raise RuntimeError("rm no such directory (file parent directory)")

        inode = cflist.libflist_inode_from_name(dirnode, filename)

        if not inode:        
            print("rm no such file")
            raise RuntimeError("rm no such file")
            

        print("[+] action: rm: file found (size: %lu bytes)\n"%inode.size)
        print("[+] action: rm: files in the directory: %lu\n"%dirnode.inode_length)

        removed = cflist.libflist_directory_rm_inode(dirnode, inode)
        if not removed:
            print("rm something went wrong when removing the file")
            raise RuntimeError("rm something went wrong when removing the file")
            

        cflist.libflist_inode_free(inode)

        print("[+] action: rm: file removed\n")
        print("[+] action: rm: files in the directory: %lu\n"%dirnode.inode_length)

        cdef cflist.dirnode_t *parent
        parent = cflist.libflist_dirnode_get_parent(ctx.db, dirnode)
        cflist.libflist_serial_dirnode_commit(dirnode, ctx, parent)

        cflist.libflist_dirnode_free(parent)
        cflist.libflist_dirnode_free(dirnode)


    def rmdir(path):
        if path == "/":
            print("rm cannot remove root directory")
            raise RuntimeError("rm cannot remove root directory")

        print("[+] action: rmdir: removing recursively {}\n".format(path))

        cdef char* dirpath = path

        envbackend = os.getenv("UPLOADBACKEND")
        envbackend_encoded = envbackend.encode()
        cdef char* benvbackend = envbackend_encoded
        cdef cflist.flist_ctx_t* ctx
        cdef cflist.flist_db_t* backdb
        backdb = cflist.libflist_metadata_backend_database_json(benvbackend)

        ctx = cflist.libflist_context_create(backdb, NULL)
        ctx.db = backdb
        cdef cflist.dirnode_t* dirnode

        dirnode = cflist.libflist_dirnode_get_recursive(ctx.db, dirpath)
        if not dirnode:
            print("rmdir no such directory")

        # fetching parent of this directory
        cdef cflist.dirnode_t *parent = cflist.libflist_dirnode_get_parent(ctx.db, dirnode);
        print("[+] action: rmdir: directory found, parent: {}\n".format(parent.fullpath))

        # removing all subdirectories
        removed = cflist.libflist_directory_rm_recursively(ctx.db, dirnode)
        # FIXME:
        if not removed:
            print("rmdir could not remove directories")
            raise RuntimeError("rmdir could not remove directories")

        # all subdirectories removed
        # looking for directory inode inside the parent now
        print("[+] action: rmdir: all subdirectories removed, removing directory from parent\n")
        cdef cflist.inode_t* inode
        inode = cflist.libflist_inode_search(parent, os.path.basename(dirnode.fullpath.decode()))

        # FIXME:
        # removing inode from the parent directory
        cdef cflist.dirnode_t* removed_dirnode
        removed_dirnode = cflist.libflist_directory_rm_inode(parent, inode)
        if removed_dirnode:
            print("rmdir something went wrong when removing the file")

        cflist.libflist_inode_free(inode);

        # commit changes in the parent (and parent of the parent)
        cdef cflist.dirnode_t *pparent
        pparent = cflist.libflist_dirnode_get_parent(ctx.db, parent)
        cflist.libflist_serial_dirnode_commit(parent, ctx, pparent)

        cflist.libflist_dirnode_free(parent)
        cflist.libflist_dirnode_free(pparent)
        cflist.libflist_dirnode_free_recursive(dirnode)


    cdef mkdir(path):
        bdirpath = path.encode()
        parent_ = os.path.dirname(path)
        dirname_ = os.path.basename(path)
        bparent = parent_.encode()
        bdirname = dirname_.encode()


        envbackend = os.getenv("UPLOADBACKEND")
        envbackend_encoded = envbackend.encode()
        cdef char* benvbackend = envbackend_encoded
        cdef cflist.flist_ctx_t* ctx
        cdef cflist.flist_db_t* backdb
        backdb = cflist.libflist_metadata_backend_database_json(benvbackend)

        ctx = cflist.libflist_context_create(backdb, NULL)

        ctx.db = backdb


        cdef cflist.dirnode_t *dirnode
        cdef cflist.inode_t *newdir

        cdef char* parent = bparent
        cdef char* dirname = bdirname
        cdef char* dirpath = bdirpath


        dirnode = cflist.libflist_dirnode_get(ctx.db, dirpath)
        if not dirnode:
            print("mkdir cannot create directory: file exists")
            raise RuntimeError("mkdir cannot create directory: file exists")


        dirnode = cflist.libflist_dirnode_get(ctx.db, parent)
        if not dirnode:
            print("mkdir parent directory doesn't exists")
            raise RuntimeError("mkdir parent directory doesn't exists")


        dirnode = cflist.libflist_dirnode_get(ctx.db, parent)
        if not dirnode:
            print("mkdir parent directory doesn't exists")
            raise RuntimeError("mkdir parent directory doesn't exists")

        print("[+] action: mkdir: creating {} inside {}>\n".format(bdirname, bparent))


        newdir = cflist.libflist_directory_create(dirnode, dirname)
        if not newdir:
            print("mkdir could not create new directory")

        # commit changes in the parent
        cdef cflist.dirnode_t *dparent
        dparent = cflist.libflist_dirnode_get_parent(ctx.db, dirnode)
        cflist.libflist_serial_dirnode_commit(dirnode, ctx, dparent)

        cflist.libflist_dirnode_free(dirnode);
        cflist.libflist_dirnode_free(dparent);

    # def metadata(flist): # get or set?
    #     pass

    # def merge(flist1, flist2):
    #     pass


