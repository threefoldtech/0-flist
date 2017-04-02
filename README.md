# flister
Command line tool to manipulate `flist` archives

# Compilation
You can choose two compilation method:
 - Developpment: by running `make debug` you will produce a shared library debuggable executable
 - Production: by running `make` you will produce a full stripped static binary, avoiding lot of dependencies

# Dependencies
In order to compile correctly `flister`, you'll need:
- libtar
- libsnappy
- rocksdb
- c-capnp
- libb2 (blake2)
- zlib
- liblz4
- libbz2
- jemalloc
- gflags

## Ubuntu
- Packages dependencies
```
libsnappy-dev libbz2-dev liblz4-dev libz-dev libtar-dev libb2-dev libjemalloc-dev libgflags-dev
```
You will need to compile c-capnp and rocksdb yourself.

## Gentoo
- Packages available on portage (using `static-libs` USE flags to produce production executable):
```
libtar dev-cpp/gflags app-arch/bzip2 jemalloc snappy zlib lz4`
```
You will need to compile rocksdb yourself.
