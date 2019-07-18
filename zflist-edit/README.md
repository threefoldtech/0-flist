# zflist (currently zflist-edit)

This is the main command line tool to use when you want to create/update an flist. It's a central point
where you can do everything you want.

# Motivation

# Actions

Creating or modifying an flist can require a lot of flexibility. This tool try to keep the usual
command line tools and do the same action on the flist contents, with some extras. Here are the list
of actions possible:

- open
- init
- ls
- stat
- cat
- put
- chmod
- rm
- rmdir
- mkdir
- metadata
- commit

In order to use this tool, you need to at least `open` (an existing) or `init` (create) an flist. When you're
done with the changes, you `commit` changes to a new flist.

## open

## init

## ls

## stat

## put

## chmod

## rm

## rmdir

## mkdir

## metadata

## commit

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

## Backend

The flist format contains only metadata about a filesystem, not the file contents. On theses metadata,
there is a field which contains list of chunks identifier which can be used to query a backend and
retreive file contents from it. This backend can be public or private, but keep in mind that any backend
options specified inside the flist metadata can be read by anyone who have this flist.

There is an important distinction to keep in mind: the `Backend Metadata` is used to **download** file
contents, this backend won't be used when **uploading** file (if you want to add new file in the flist).

Usually, backend are publicly readable but not writable without authentication. You don't want to expose
your personnal credentials inside the flist metadata, obviously. In order to upload files, you need to
set the `UPLOADBACKEND` environment variable.

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

You can provide an arbitraty text and licence to describe your flist.
