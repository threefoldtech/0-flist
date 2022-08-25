# zflist

This is the main command line tool to use when you want to create/update an flist. It's a central point
where you can do everything you want, using the `libflist`.

# Motivation

# Actions

Creating or modifying an flist can require a lot of flexibility. This tool try to keep the usual
command line tools and do the same action on the flist contents, with some extras. Here are the list
of actions possible:

- open
- init
- close
- ls
- stat
- cat
- put
- putdir
- chmod
- rm
- rmdir
- mkdir
- metadata
- commit

In order to use this tool, you need to at least `open` (an existing) or `init` (create) an flist. When you're
done with the changes, you `commit` changes to a new flist.

## open

Open an existing flist file and extract everything to the temporary directory in order to be
able to modify contents later.

```
$ zflist open /tmp/existing.flist
```

## init

Create an empty environment with a root filesystem and nothing inside.

```
$ zflist init
```

## ls

List the content of one directory (if not specified, default is root directory)

```
$ zflist ls
drwxr-xr-x root     root           1344  bin
drwxr-xr-x root     root              0  boot
drwxr-xr-x root     root           1890  etc
drwxr-xr-x root     root              0  home
drwxr-xr-x root     root            106  lib
drwxr-xr-x root     root             40  lib64
drwxr-xr-x root     root              0  media
[...]

$ zflist ls /var
drwxr-xr-x root     root              0  backups
drwxr-xr-x root     root             36  cache
drwxr-xr-x root     root            190  lib
drwxrwxr-x root     staff             0  local
drwxr-xr-x root     root            142  log
drwxrwxr-x root     mail              0  mail
[...]
```

## stat

Dumps informations about a single inode

```
$ zflist stat /etc/hostname
  File: /etc/hostname
  Size: 13 bytes
Access: (100644/-rw-r--r--)  UID: root, GID: root
Access: 1536587642
Create: 1536588043
Chunks: key: f1a594ca047379ad6942572350cc022a, decipher: b4649e4b54a1426a05d93580cba9bf5f
```

## put

## chmod

Change the mode of a file

```
$ zflist chmod 777 /etc/hostname
```

## rm

Remove an inode (nothing recursive)

```
$ zflist ls /etc/init
-rw-r--r-- root     root            284  hostname.conf
-rw-r--r-- root     root            300  hostname.sh.conf
-rw-r--r-- root     root            561  hwclock-save.conf
-rw-r--r-- root     root            674  hwclock.conf
[...]

$ zflist rm /etc/init/hostname.conf

$ zflist ls /etc/init
-rw-r--r-- root     root            300  hostname.sh.conf
-rw-r--r-- root     root            561  hwclock-save.conf
-rw-r--r-- root     root            674  hwclock.conf
[...]
```

## rmdir

Recursively remove a directory and all subdirectories

```
$ zflist rmdir /etc/init
$ zflist ls /etc/init
zflist: ls: no such directory (file parent directory)
```

## mkdir

Create an ew directory

```
$ zflist ls /mnt
$ zflist mkdir /mnt/hello
$ zflist ls /mnt
drwxr-xr-x root     root           4096  hello
```

## metadata

Set or get metadata informations (see below)

```
$ zflist metadata backend
zflist: metadata: metadata not found

$ zflist metadata backend --host hub.grid.tf --port 9900

$ zflist metadata backend
{"host": "hub.grid.tf", "port": 9900}
```

## commit

Export temporary directory and create a new flist with the new database

```
$ zflist commit /tmp/newfile.flist
```

# Metadata

There are couple of metadata you can set **inside** the flist. Theses metadata can be used to
describe or provide information about your flist dependencies, requirements, etc.

Here are the list of metadata available:
- Backend
- Entrypoint
- Environment variables
- Exposed ports
- Volumes
- Readme

In addition, any generic metadata can now be set as well.

## Backend

The flist format contains only metadata about a filesystem, not the file contents. On theses metadata,
there is a field which contains list of chunks identifier which can be used to query a backend and
retreive file contents from it. This backend can be public or private, but keep in mind that any backend
options specified inside the flist metadata can be read by anyone who have this flist.

There is an important distinction to keep in mind: the `Backend Metadata` is used to **download** file
contents, this backend won't be used when **uploading** file (if you want to add new file in the flist).

Usually, backend are publicly readable but not writable without authentication. You don't want to expose
your personnal credentials inside the flist metadata, obviously. In order to upload files, you need to
set the `ZFLIST_BACKEND` environment variable.

Example to upload:
```
ZFLIST_BACKEND='{"host":"localhost","port":9900}' ./zflist put ...
```

## Entrypoint

You can specify a command line to executed when your flist is started inside an
environment (eg: Zero-OS Container).

## Environment variables

You can set default global environment variables. Theses variables will be exported and available
for your entrypoint or any executed command inside the running environment.

## Exposed ports

If your flist want to provide some network services, you can specify a list of port mapping, theses
ports will be requested by the environment hypervisor when your environment will start. You can moreover
provide a redirection mapping (eg: tcp/80 -> tcp/8080).

Note: your environment can require extra information.

## Volumes

When your environment needs extra persistant mountpoint, this metadata become really useful.
You can specify a list of mountpoint your flist needs to have, in order to works properly
(eg: `/data` to get persistant data). You can optionnaly specify where on the host you expect to find
this mountpoint, but this is not needed.

## Readme

You can provide an arbitraty text and license name to describe your flist.

## Generic Metadata

You can list, set and get any other value for metadata, eg:
 - Listing metadata keys available: `./zflist metadata`
 - Set (or replace) a metadata entry: `./zflist metadata hello world`
 - Get a metadata entry: `./zflist metadata hello`
