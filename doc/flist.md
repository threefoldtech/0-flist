# flist (Files List)
The `flist` file format is a general puspose format to store `metadata` about a (posix) `filesystem`.
It's main goal is keeping a small file with enough information to make a complete filesystem available
without the data payload itself, in an efficient way.

The file format was never versioned, this document will describe the last version used by `flister` tool.
This tool is C based implementation of the flist management (which was written in python at the beginning).

In summary, a `flist` file is a `tar` archive (usually compressed), containing a database. This database
can be be anything supporting key/value store. This database will contains one entry per directory. Each
entries are a capnp struct describing the contents of each files on that directory. Moreover, the database will
contains one key per permissions (ACL) found on files (deduped).

At the end of this document you'll find some explanation about choice of technology used and the history.

# Specification

1. [Package](#package)
2. [Database](#database)
3. [Filesystem](#filesystem)
4. [File chunks](#file-chunks)

## Package
A `flist` file is a `tar` archive which contains one single `sqlite3` database called `flistdb.sqlite3`.
This tar archive can, of course, be compressed (gzip, bz2, xz, ...).
You should at least support `gzip` compression.

## Database
The `sqlite3` database uses a reallu simple and small schema:
```sql
CREATE TABLE entries (key VARCHAR(64) PRIMARY KEY, value BLOB);
CREATE INDEX entries_index ON entries (key);
```

This reflect a simple `key/value` concept.

## Filesystem
Each directories of the filesystem is bundle in a capnp object. The main capnp schema can be found
[here](https://github.com/threefoldtech/jumpscale_lib/blob/development/JumpscaleLib/data/flist/model.capnp).

Each object are stored on the database and the key is a `blake2` hash (16 bytes) of the directory name.

One directory is stored on the `Dir` capnp object, and contains:
- `name`: the name of the directory
- `location`: full path of this directory
- `contents`: a list of Inode object
- `parent`: the hash of the parent directory
- `size`: not used (usualy, hardcoded to 4096)
- `aclkey`: the hash of the acl attached
- `modificationTime`: unix timestamp of last modification
- `creationTime`: unix timestamp of creation time

For each file in this directory, if will be converted in `Inode` capnp object, and added on the `contents` list:
- `name`: the filename
- `size`: file size in bytes
- `aclkey`: the key hash of permission attached
- `modificationTime`: unix timestamp of modification time
- `creationTime`: unix timestamp of creation time
- `attributes`: special field which describe the file

The `attributes` field is a `capnp union`, and can be 4 special types:
- `dir` when target is another directory
- `file` when the target is a regular file
- `link` when target is a symbolic link
- `special` when target is a special file

Theses attributes are all specified by custom struct:
- `dir` is specified by a `SubDir` object, which contains:
  - `key`: a single field with the hash of that directory in the flist
- `link` is specified by a `Link` object, which contains:
  - `target`: a text field with the endpoint of the symlink (can be relative)
- `file` is specified by a `File` object, which contains:
  - `blocksize`: hardcoded value of 512 KB
  - `blocks`: list of blocks, each one represented by object `FileBlock`:
    - `hash`: file hash stored on the backend
    - `key`: encryption key used to encrypt the file
- `special` is specified by a `Special` object which contains:
  - `type`: enum, which can be: `socket`, `block`, `chardev`, `fifopipe`, `unknown`
  - `data`: optional field, used for example on block device to store `major,minor` id

**Note:** a flist will always contains at least one directory, the root directory, which is a blake2 hash of
empty string. All directory names never have any slash (`/`) and everything uses relative path.

A minimal flist is basicly 2 keys: one for the root directory, one for the acl of that directory.

## File chunks
Since the payload of the files are not stored, we only keep metadata, we need a way to be able to
get the contents of the file, and if possible, in an efficient way. The method we use is using the
[lib0stor](https://github.com/maxux/lib0stor) which split file in chunks and compress/encrypt theses chunks.
At the end, you can save anything on the blocks. Our backend uses lib0stor.

## Dependencies
Here is a brief list of what an flist depends on, to be read/written:
- `tar` (archive package)
- `gzip` (archive compression)
- `sqlite3` (database)
- `blake2` (directory hash)
- `capnp` (directory contents)
- `lib0stor` (file chunks)

# History

## Package
The database is a `tar` archive, because in the first version, a `rocksdb` database was used,
and contains multiple files. The `tar` archive was the simplest idea to package everything easily.

Now, using sqlite3, we have a single file but to keep a flexible futur, let's keep it in a `tar` archive
if we need to bundle more file later (or change the database).

Since rocksdb was good on a performance point of view, embedding rocksdb is huge and use lot of memory.
We moved to sqlite3, performance was mostly the same and it's lot more easy to read a sqlite3 database
on a lot of different plateform than rocksdb. Moreover the list of dependencies is dramaticaly reduced.
