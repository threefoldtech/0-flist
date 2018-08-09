# 0-flist
Shared library and standalone flist implementation.

For more information about `flist`, please check [flist documentation](doc/flist.md).

# Components
This projects contains multiple components:
- libflist: the official flist management library
- zflist: a command-line program to manipulate flists (uses the libflist)
- pyflist: python flist binding native extension

## libflist
You can build the library in debug mode by taping `make` on the `libflist` directory. This build will
contains lot of verbosity and debug symbols.

In order to produce a production library, type `make release` which will strip debug message.

Any build will produce a static version of the library `libflist.a` and a dynamic version `libstatic.so`

Please note this library is in a early stage and was a fully static binary in a first step. Strip is not
complete right now.

# zflist
Command line utility which use `libflist` to list, create and query a flist file.

You can easily ship a (mostly) static version of `zflist` by compiling it using `make embedded`. This build
will static link `libflist` and it's dependencies, but not produce a fully static binary. It will still
be linked to `glibc` in order to use any resolution and users functions correctly.

# pyflist
Python binding of the library. Work in progress.

# Dependencies
In order to compile correctly `0-flist`, you'll need:
- `sqlite3` (library)
- `hiredis`
- `libtar`
- `libsnappy`
- `c-capnp`
- `libb2` (blake2)
- `zlib`
- `jansson`
- `python3` (for binding)

## Ubuntu
- Packages dependencies
```
build-essential libsnappy-dev libz-dev libtar-dev libb2-dev libjansson-dev libhiredis-dev libsqlite3-dev 
```
You will need to compile `c-capnp` yourself.

# Usage
```
Usage: ./zflist [options]
       ./zflist --archive <filename> --list [--output tree]
       ./zflist --archive <filename> --create <root-path>

Command line options:
  -a --archive <flist>     archive (flist) filename
                           (this option is always required)

  -c --create <root>       create an archive from <root> directory

  -b --backend <host:port> upload/download files from archive, on this backend

  -l --list       list archive content
  --action        action to do while listing archive:
                    ls      show kind of 'ls -al' contents (default)
                    tree    show contents in a tree view
                    dump    debug dump of contents
                    json    file list summary in json format (same as --json)

                    blocks  dump files backend blocks (hash, key)
                    check   proceed to backend integrity check
                    cat     request file download (with --file option)

  -j --json       provide (exclusively) json output status
  -f --file       specific inside file to target
  -r --ramdisk    extract archive to tmpfs
  -h --help       shows this help message
```
