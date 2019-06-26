# 0-flist
Shared library and standalone flist implementation.

For more information about `flist`, please check [flist documentation](doc/flist.md).

# Components
This projects contains multiple components:
- `libflist`: the official flist management library
- `zflist`: a command-line program to manipulate flists (uses the libflist)
- `pyflist`: python flist binding native extension

## libflist
Library don't have special compilation mode. Default build produce a shared library and a static archive
to static link `libflist` code. The code produced contains debug and production version (customized by a flag
on runtime to enable or not debug output).

Please note this library is in a early stage and was a fully static binary in a first step. Strip is not
complete right now.

Development note:
- `flist_xxx` functions are internal functions
- `libflist_xxx` functions are public functions

# zflist
Command line utility which use `libflist` to list, create and query a flist file.

You can easily ship a (mostly) static version of `zflist` by compiling it using `make embedded`. This build
will static link `libflist` and it's dependencies, but not produce a fully static binary. It will still
be linked to `glibc` in order to use any resolution and users functions correctly.

The `embedded` target produce a debug version, there is also a `production` target which disable any
debug output by default.

# pyflist
Python binding of the library. Work in progress.

# Dependencies
In order to compile correctly `0-flist`, you'll need theses libraries:
- `sqlite3` (database, libflist)
- `hiredis` (redis, libflist)
- `libtar` (archive, libflist)
- `libsnappy` (compression, libflist)
- `c-capnp` (serialization, libflist)
- `libb2` (hashing [blake2], libflist)
- `zlib` (compression, libflist)
- `jansson` (serialization, zflist)
- `python3` (extension, pyflist)

## Ubuntu
- Packages dependencies
```
build-essential libsnappy-dev libz-dev libtar-dev libb2-dev libjansson-dev libhiredis-dev libsqlite3-dev 
```
You will need to compile `c-capnp` yourself, see autobuild directory.

# Notes
## Merge priority
When merging multiple flist together, the order of `--merge` argument is important.
All files are proceed one by one from left to right. When file collision occures (same filename is present is multiple flist),
the first file found is the one used on the final archive.

Example: `zflist --archive target.flist --merge first.flist --merge second.flist`

If `/bin/ls` is on both flist, the file from `first.flist` will be found `target.flist`

# Usage
```
Usage: ./zflist [options]
       ./zflist --archive <filename> --list [--output tree]
       ./zflist --archive <filename> --create <root-path>
       ./zflist --archive <filename> [options] --backend <host:port>

Command line options:
  --archive <flist>     archive (flist) filename
                        (this option is always required)

  --create <root>       create an archive from <root> directory

  --backend <host:port> upload/download files from archive, on this backend
  --password <pwd>      backend namespace password (protected mode)
  --token <jwt>         gateway token (gateway upload)
  --upload [website]    upload the flist (using --token) on the hub

  --list       list archive content
  --merge      do a merge and add argument to merge list
               the --archive will be the result of the merge
               merge are done in the order of arguments
  --action     action to do while listing archive:
                    ls      show kind of 'ls -al' contents (default)
                    tree    show contents in a tree view
                    dump    debug dump of contents
                    json    file list summary in json format (same as --json)

                    blocks  dump files backend blocks (hash, key)
                    check   proceed to backend integrity check
                    cat     request file download (with --file option)

  --json       provide (exclusively) json output status
  --file       specific inside file to target
  --ramdisk    extract archive to tmpfs
  --help       shows this help message
```

# Repository Owner
- [Maxime Daniel](https://github.com/maxux), Telegram: [@maxux](http://t.me/maxux)

