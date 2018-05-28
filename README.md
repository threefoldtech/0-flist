# flister
Command line tool to list or create `flist` archives.

# Compilation
There are two target:
 - Developpment: by running `make` you will produce a dynamic debuggable executable (use shared library)
 - Production: by running `make release` you will produce a full stripped static binary, avoiding lot of dependencies

# Dependencies
In order to compile correctly `flister`, you'll need:
- `libtar`
- `libsnappy`
- `rocksdb`
- `c-capnp`
- `libb2` (blake2)
- `zlib`
- `liblz4`
- `libbz2`
- `jemalloc`
- `gflags`
- `jansson`
- `libcrypto` (openssl)

## Ubuntu
- Packages dependencies
```
libsnappy-dev libbz2-dev liblz4-dev libz-dev libtar-dev libb2-dev libjemalloc-dev libgflags-dev libjemalloc-dev
```
You will need to compile `c-capnp` and `rocksdb` yourself.

## Gentoo
- Packages available on portage (using `static-libs` USE flags to produce production executable):
```
libtar dev-cpp/gflags app-arch/bzip2 jemalloc snappy zlib lz4 jansson
```
You will need to compile `rocksdb` yourself.

# Usage
```
Usage: ./flister [options] --verbose
       ./flister --archive <filename> --list [--output tree]
       ./flister --archive <filename> --create <root-path>

Command line options:
  -a --archive <flist>     archive (flist) filename
                           (this option is always required)

  -c --create <root>       create an archive from <root> directory

  -u --upload <host:port>  upload files from creating archive, to the backend

  -l --list       list archive content
  -o --output     list output format, possible values:
                    ls      show kind of 'ls -al' contents (default)
                    tree    show contents in a tree view
                    dump    debug dump of contents
                    json    file list summary in json format

                    blocks  dump files backend blocks (hash, key)

  -r --ramdisk    extract archive to tmpfs
  -v --verbose    enable verbose messages
  -h --help       shows this help message
```
