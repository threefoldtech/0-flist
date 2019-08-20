# Libflist API

The `libflist` is a public api written in C which provide mostly all the logic
needed to use, create, modify, publish an flist file.

This library is still in developmment but can already be used to build project.

The `zflist` software can be used as an example how to use the library.

# Name convention

Public functions on the library are all prefixed by `libflist_`, if you try
to use a function without this prefix, it's not intended to be public.

Function prefixed by `flist_` are used internally and could change over the time.

The current api will be kept as it as much as possible and fix will be hidden under
the hood if needed.

# Datatypes

There are some important datatype you'll need to use in order to use the library:
- `flist_ctx_t`
- `flist_db_t`
- `dirnode_t`
- `inode_t`
- `acl_t`

## flist_ctx_t

This is the first datatype you'll need to use. This represent an flist context.
One context represent one opened flist, ready for manipulation. This context contains
a handler on a database, an optional backend and some statistics.

You need to create a context using: `libflist_context_create(db, backend)` and cleanup
your context using `libflist_context_free(context)`.

You need to open the database and the backend before and pass them to the creator.

## flist_db_t

This datatype represent and allow manipulation over a database, whatever is it.
Right now, sqlite3 and redis are supported as database and share the same public api, which
allows you to use any of them for any operations (flist, backend, ...)

## dirnode_t

This datatype represent a directory entry. This directory entry is probably in a chain
of more directories and can be followed using a chained list. A directory contains a list of
inodes (`inode_t`) in it's simpler version and also contains a list of subdirectories in it's recursive
version.

When you want to persist/load anything into/from the database, you need to use a `dirnode_t`, even if
you want to change a single file, you need to use the full directory, it's the way it's stored on the database.

## inode_t

This datatype represent one inode. An inode is one entry on a directory. This can be anything (regular file,
symlink, directory, character device, block device, unix socket, fifo, ...).

Please note: directories needs to have an inode linked to be discovered on the directory contents.

## acl_t

This datatype represent permissions for an inode, permissions are dedupe on the database.
This contains username, groupname and permissions bits.

# Handling

To get the full list of functions available, please see: [libflist.h](../libflist/libflist.h)

## Create a new empty flist environment

```c
// 1
flist_db_t *database = libflist_db_sqlite_init("/tmp/demo");

flist_ctx_t *ctx = libflist_context_create(database, NULL);
ctx->db->open(ctx->db);

// 2
dirnode_t *root = libflist_dirnode_create("", "");

// 3
libflist_serial_dirnode_commit(root, ctx, root);
libflist_dirnode_free(root);

// 4
ctx->db->close(ctx->db);
libflist_context_free(ctx);
```

Some points here like context creation/destruction are always the same and won't be shown again later.

1. Open an empty database, all you need to specify is a **directory** path, inside that directory will be created anything needed for flist handling
2. Create a new empty directory, root directory is an empty string, all path start without leading slash
3. Write this empty directory in the database
4. Close the database (this is important to ensure everything is written on database)

> Note: this only create database and data inside. A real flist file is a compressed version of all of this, see below.

## Open an existing flist

Now that you know how to create an empty flist flist, let's see how to open an existing released one.
A flist file is a .tar.gz

```c
if(!libflist_archive_extract("/tmp/existing.flist", "/tmp/demo"))
    printf("something went wrong\n");
```

That's all you need, after that you can open the `/tmp/demo` like the previous point.

## Saving your environment to a final flist

When you're done with the database and want to release an flist final file, you have
to archive and compress the environment directory. The library provide everything needed for that.

```c
if(!libflist_archive_create("/tmp/newfile.flist", "/tmp/demo"))
    printf("something went wrong\n");
```

## Listing a directory contents

In order to find things on the flist contents, you have multiple way to query the database.
> Note: don't forget that root directory is an empty string.

```c
dirnode_t *dirnode;

// create context here

if(!(dirnode = libflist_dirnode_get(cb->ctx->db, "/"))) {
    zf_error(cb, "find", "no such root directory");
    return 1;
}

for(inode_t *inode = dirnode->inode_list; inode; inode = inode->next)
    printf("/%s\n", inode->fullpath);

libflist_dirnode_free(dirnode);

// clean context here
```

This example shows how to iterate over the contents of a directory and print contents.
You can use any properties in the `inode_t` structure to query things.

## Adding local file to the flist

In order to add files to the flist, you can insert local file into a directory.
When adding a file, you only add metadata to the flist, this only store metadata.
The file payload aren't saved on the flist, but some chunks id are saved. The payload have to
be uploaded into a « backend »

```c
dirnode_t *dirnode;
inode_t *inode;

if(!(dirnode = libflist_dirnode_get(cb->ctx->db, "/root")))
    printf("directory doesn't exists\n");

if(!(inode = libflist_inode_from_localfile("/tmp/mylocalfile", dirnode, cb->ctx)))
    printf("could not add the file\n");

libflist_dirnode_appends_inode(dirnode, inode);

dirnode_t *parent = libflist_dirnode_get_parent(cb->ctx->db, dirnode);
libflist_serial_dirnode_commit(dirnode, cb->ctx, parent);

libflist_dirnode_free(parent);
libflist_dirnode_free(dirnode);
```

When committing you need to specify the parent to update linked stuff.

If you don't specify any backend when creating your context, the file payload chunks
will be computed and stored but nothing will be uploaded. If you want to upload chunks in the
same time, you have to provide a backend to the context creation.

# Backend

A backend is just a database used to store chunks (payload) of files.
You can create a backend the same way you connect a database.

```c
flist_db_t *backdb = libflist_db_redis_init_tcp("hostname", 9900, "default", NULL, NULL);
flist_backend_t *backend = libflist_backend_init(backdb, "/");
```


