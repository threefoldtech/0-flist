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
to link `libflist` code. The code produced contains debug and production version (customized by a flag
on runtime to enable or not debug output).

Development note:
- `flist_xxx` functions are internal functions
- `libflist_xxx` functions are public functions

More information about how to use the library will be available soon.

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
In order to compile correctly `libflist`, you'll need theses libraries:
- `sqlite3` (database, libflist)
- `hiredis` (redis, libflist)
- `libtar` (archive, libflist)
- `libsnappy` (compression, libflist)
- `c-capnp` (serialization, libflist)
- `libb2` (hashing [blake2], libflist)
- `zlib` (compression, libflist)

To compile `zflist`, you'll also need:
- `jansson` (serialization, zflist)

To compile the python binding, you'll also need:
- `python3` (obviously, extension, pyflist)

## Ubuntu
- Packages dependencies
```
build-essential libsnappy-dev libz-dev libtar-dev libb2-dev libjansson-dev libhiredis-dev libsqlite3-dev 
```
You will need to compile `c-capnp` yourself, see autobuild directory.

# Notes
## Merge priority
When merging multiple flist together, the order of the merge is important.
All files are proceed one by one from first to last flist. When file collision occures
(same filename is present in multiple flist), the first file found is the one used on the final archive.

Example:
```
zflist init
zflist merge first.flist
zflist merge second.flist
zflist commit merged.flist
```

If `/bin/ls` is on both flist, the file from `first.flist` will be found in `merged.flist`.

# Prefetch

The best part of flist system and concept is the fact that files are downloaded on-the-fly.
This is really useful for lot of use cases but sometime it's important performance wise to
get files locally to not spend time downloading on runtime.

With the prefetch action, you can force the system to read all files in a directory and thus,
downloading all files not availables on the `0-fs` cache.

This action is only interresting when using on top of [`0-fs`](https://github.com/threefoldtech/0-fs).

# Usage

```
Usage: ./zflist [temporary-point] open <filename>
       ./zflist [temporary-point] <action> <arguments>
       ./zflist [temporary-point] commit [optional-new-flist]

  The temporary-point should be a directory where temporary files
  will be used for keeping tracks of your changes.
  You can also use ZFLIST_MNT environment variable to set your
  temporary-point directory and not specify it on the command line.

  By default, this tool will print error and information in text format,
  you can get a json output by setting ZFLIST_JSON=1 environment variable.

  First, you need to -open- an flist, then you can do some -edit-
  and finally you can -commit- (close) your changes to a new flist.

  If you want to upload chunks when inserting files, please set
  environment variable ZFLIST_BACKEND to a json backend formatted string,
  check backend documentation for more information

  To use the hub subsystem, you need to specify at least a jwt token
  via the environment variable ZFLIST_HUB_TOKEN, this jwt needs to be
  valid for the hub. In addition, you can specify ZFLIST_HUB_USER if
  your token contains multiple member-of users/organizations.

Available actions:
  open            open an flist to enable editing
  init            initialize an empty flist to enable editing
  ls              list the content of a directory
  find            list full contents of files and directories
  stat            dump inode full metadata
  cat             print file contents (backend metadata required)
  put             insert local file into the flist
  putdir          insert local directory into the flist (recursively)
  chmod           change mode of a file (like chmod command)
  rm              remove a file (not a directory)
  rmdir           remove a directory (recursively)
  mkdir           create an empty directory (non-recursive)
  metadata        get or set metadata
  merge           merge another flist into the current one
  hub             0-hub command line tools
  commit          commit changes to a new flist
  close           close mountpoint and discard files
```

# Example

How to create an flist, upload playloads and upload flist into playground hub. By default, `zflist`
will use production hub (`hub.grid.tf`) but you can override that by setting `ZFLIST_HUB` environment
variable, like below.

```
export ZFLIST_MNT=/tmp/zflistmnt
export ZFLIST_HUB="https://playground.hub.grid.tf"
export ZFLIST_BACKEND='{"host": "playground.hub.grid.tf", "port": 9910}'
export ZFLIST_HUB_TOKEN=kiUTd9jRjgt7QB6lRh2bcpNiC2UqvTLI

zflist init
zflist putdir /tmp/20191021_150741 /
zflist commit /tmp/committed-flist.flist
zflist hub upload /tmp/committed-flist.flist


[+] response [200]: {"payload": {"timing": {}, "name": "committed-flist.flist", "files": {"regular": 890, "directory": 2, "symlink": 0, "failure": 0, "special": 0}}, "status": "success"}
```

# Repository Owner
- [Maxime Daniel](https://github.com/maxux), Telegram: [@maxux](http://t.me/maxux)

