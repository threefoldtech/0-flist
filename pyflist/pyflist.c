#include <Python.h>
#include <stdio.h>
#include <stdlib.h>
#include "libflist.h"

#define debug(...) { if(pyflist_debug_flag) { printf(__VA_ARGS__); } }

static int pyflist_debug_flag = 1;

#if 0
remote_t *remotes = NULL;

static PyObject *g8storclient_connect(PyObject *self, PyObject *args) {
    (void) self;
    remote_t *remote;
    const char *host;
    int port;

    if (!PyArg_ParseTuple(args, "si", &host, &port))
        return NULL;

    debug("[+] python binding: connecting %s (port: %d)\n", host, port);
    if(!(remote = remote_connect(host, port)))
        return NULL;

    // capsule will encapsulate our remote_t which contains
    // the redis client object
    return PyCapsule_New(remote, "RemoteClient", NULL);
}
*/

static PyObject *g8storclient_encrypt(PyObject *self, PyObject *args) {
    (void) self;
    buffer_t *buffer;
    char *file;
    size_t finalsize = 0;

    if(!PyArg_ParseTuple(args, "s", &file))
        return NULL;

    // initialize buffer
    if(!(buffer = bufferize(file)))
        return Py_None;

    // chunks
    PyObject *hashes = PyList_New(buffer->chunks);

    // debug("[+] encrypting %d chunks\n", buffer->chunks);
    for(int i = 0; i < buffer->chunks; i++) {
        const unsigned char *data = buffer_next(buffer);

        // encrypting chunk
        chunk_t *chunk = encrypt_chunk(data, buffer->chunksize);

        // inserting hash to the list
        PyObject *pychunk = PyDict_New();
        PyDict_SetItemString(pychunk, "hash", Py_BuildValue("s", chunk->id));
        PyDict_SetItemString(pychunk, "key", Py_BuildValue("s", chunk->cipher));
        PyDict_SetItemString(pychunk, "data", Py_BuildValue("y#", chunk->data, chunk->length));
        PyList_SetItem(hashes, i, pychunk);

        finalsize += chunk->length;
        chunk_free(chunk);
    }

    // debug("[+] finalsize: %lu bytes\n", finalsize);

    // cleaning
    buffer_free(buffer);

    return hashes;
}

static PyObject *g8storclient_decrypt(PyObject *self, PyObject *args) {
    (void) self;
    buffer_t *buffer;
    PyObject *hashes;
    char *file;

    if(!PyArg_ParseTuple(args, "Os", &hashes, &file))
        return NULL;

    // initialize buffer
    if(!(buffer = buffer_writer(file)))
        return Py_None;

    // parsing dictionnary
    int chunks = (int) PyList_Size(hashes);

    // chunks
    // debug("[+] decrypting %d chunks\n", chunks);
    for(int i = 0; i < chunks; i++) {
        char *id, *cipher;
        unsigned char **data, *datadup;
        unsigned int length;

        PyObject *item = PyList_GetItem(hashes, i);

        PyArg_Parse(PyDict_GetItemString(item, "hash"), "s", &id);
        PyArg_Parse(PyDict_GetItemString(item, "key"), "s", &cipher);
        PyArg_Parse(PyDict_GetItemString(item, "data"), "y#", &data, &length);

        if(!(datadup = (unsigned char *) malloc(sizeof(char) * length))) {
            perror("malloc");
            return Py_None;
        }

        memcpy(datadup, data, length);

        chunk_t *chunk = chunk_new(strdup(id), strdup(cipher), datadup, length);
        chunk_t *output = NULL;

        // downloading chunk
        if(!(output = decrypt_chunk(chunk)))
            fprintf(stderr, "[-] decrypt failed\n");

        buffer->chunks += 1;
        buffer->finalsize += output->length;
        // debug("[+] chunk restored: %lu bytes\n", output->length);

        chunk_free(chunk);
    }

    size_t finalsize = buffer->finalsize;
    // debug("[+] finalsize: %lu bytes\n", finalsize);

    // cleaning
    buffer_free(buffer);

    return PyLong_FromLong(finalsize);
}
#endif

static PyObject *db_open(PyObject *self, PyObject *args) {
    return NULL;
}

static PyObject *db_close(PyObject *self, PyObject *args) {
    return NULL;
}

static PyObject *backend_open(PyObject *self, PyObject *args) {
    return NULL;
}

static PyObject *backend_close(PyObject *self, PyObject *args) {
    return NULL;
}

typedef struct pyflist_obj_t {
    flist_db_t *database;
    flist_backend_t *backend;
    const char *filename;
    char *workspace;

} pyflist_obj_t;

//
// internal utilities
//
PyObject *acl_to_dict(flist_acl_t *acl) {
    PyObject *item = PyDict_New();

    PyDict_SetItemString(item, "user", Py_BuildValue("s", acl->uname));
    PyDict_SetItemString(item, "group", Py_BuildValue("s", acl->gname));
    PyDict_SetItemString(item, "mode", Py_BuildValue("k", acl->mode));

    return item;
}

PyObject *inode_to_dict(inode_t *inode) {
    PyObject *item = PyDict_New();
    PyDict_SetItemString(item, "name", Py_BuildValue("s", inode->name));
    PyDict_SetItemString(item, "fullpath", Py_BuildValue("s", inode->fullpath));
    PyDict_SetItemString(item, "size", Py_BuildValue("i", inode->size));
    PyDict_SetItemString(item, "acl", acl_to_dict(inode->racl));
    PyDict_SetItemString(item, "modificatonTime", Py_BuildValue("k", inode->modification));
    PyDict_SetItemString(item, "creationTime", Py_BuildValue("k", inode->creation));

    if(inode->type == INODE_DIRECTORY) {
        PyDict_SetItemString(item, "type", Py_BuildValue("s", "directory"));
        PyDict_SetItemString(item, "subdir", Py_BuildValue("s", inode->subdirkey));
        return item;
    }

    if(inode->type == INODE_LINK) {
        PyDict_SetItemString(item, "type", Py_BuildValue("s", "symlink"));
        PyDict_SetItemString(item, "link", Py_BuildValue("s", inode->link));
        return item;
    }

    if(inode->type == INODE_SPECIAL) {
        PyDict_SetItemString(item, "type", Py_BuildValue("s", "symlink"));
        return item;
    }

    if(inode->type == INODE_FILE) {
        PyDict_SetItemString(item, "type", Py_BuildValue("s", "regular"));

        PyObject *chunks = PyList_New(inode->chunks->size);

        for(size_t i = 0; i < inode->chunks->size; i++) {
            PyObject *pych = PyDict_New();
            inode_chunk_t *chk = &inode->chunks->list[i];

            PyDict_SetItemString(pych, "id", Py_BuildValue("y#", chk->entryid, chk->entrylen));
            PyDict_SetItemString(pych, "decipher", Py_BuildValue("y#", chk->decipher, chk->decipherlen));

            PyList_SetItem(chunks, i, pych);
        }

        PyDict_SetItemString(item, "chunks", chunks);
        return item;
    }

    return NULL;
}

PyObject *dirnode_to_dict(dirnode_t *dirnode) {
    PyObject *dir = PyDict_New();
    PyDict_SetItemString(dir, "name", Py_BuildValue("s", dirnode->name));

    PyObject *entries = PyDict_New();

    for(inode_t *inode = dirnode->inode_list; inode; inode = inode->next)
        PyDict_SetItemString(entries, inode->name, inode_to_dict(inode));

    PyDict_SetItemString(dir, "entries", entries);
    return dir;
}

//
// public functions
//
static PyObject *pyflist_open(PyObject *self, PyObject *args) {
    (void) self;
    pyflist_obj_t *root;
    const char *filename;

    if(!PyArg_ParseTuple(args, "s", &filename))
        return NULL;

    if(!(root = (pyflist_obj_t *) PyMem_Malloc(sizeof(pyflist_obj_t))))
        return NULL;

    root->filename = strdup(filename);

    debug("[+] initializing workspace\n");
    if(!(root->workspace = libflist_workspace_create())) {
        fprintf(stderr, "workspace_create");
        // TODO: free
        return NULL;
    }

    debug("[+] workspace: %s\n", root->workspace);

    if(!libflist_archive_extract((char *) root->filename, root->workspace)) {
        fprintf(stderr, "libflist_archive_extract");
        return NULL; // FIXME
    }

    debug("[+] loading database\n");
    root->database = libflist_db_sqlite_init(root->workspace);
    root->database->open(root->database);

    // flist_listing(root->database, &settings);

    // capsule will encapsulate our object which contains
    // everything needed
    return PyCapsule_New(root, "FlistObject", NULL);
}

static PyObject *pyflist_close(PyObject *self, PyObject *args) {
    PyObject *pycaps = NULL;

    if(!PyArg_ParseTuple(args, "O", &pycaps))
        return NULL;

    pyflist_obj_t *root;
    if(!(root = (pyflist_obj_t *) PyCapsule_GetPointer(pycaps, "FlistObject")))
        return NULL;

    debug("[+] closing database\n");
    root->database->close(root->database);

    debug("[+] cleaning workspace\n");
    if(!libflist_workspace_destroy(root->workspace)) {
        fprintf(stderr, "workspace_destroy\n");
        return NULL;
    }

    free(root->workspace);
    PyMem_Free(root);

    return Py_BuildValue("");
}

static PyObject *pyflist_create(PyObject *self, PyObject *args) {
    flist_backend_t *backend = NULL;
    flist_db_t *backdb;
    const char *rootpath = NULL;
    const char *targetfile = NULL;
    pyflist_obj_t root;
    PyObject *pycaps = NULL;
    PyObject *response = NULL;

    if(!PyArg_ParseTuple(args, "ssO", &rootpath, &targetfile, &pycaps))
        return NULL;

    if(!(backdb = (flist_db_t *) PyCapsule_GetPointer(pycaps, "FlistBackend")))
        return NULL;

    debug("[+] initializing workspace\n");
    if(!(root.workspace = libflist_workspace_create())) {
        PyErr_SetString(PyExc_RuntimeError, libflist_strerror());
        return NULL;
    }

    debug("[+] workspace: %s\n", root.workspace);

    root.database = libflist_db_sqlite_init(root.workspace);
    root.database->create(root.database);

    if(!(backend = backend_init(backdb, rootpath))) {
        PyErr_SetString(PyExc_RuntimeError, libflist_strerror());
        goto reply;
    }

    // building database
    flist_stats_t *stats;

    if(!(stats = flist_create(root.database, rootpath, backend))) {
        PyErr_SetString(PyExc_RuntimeError, libflist_strerror());
        goto reply;
    }

    // everything was fine
    // building clean dict response
    response = PyDict_New();
    PyDict_SetItemString(response, "regular", Py_BuildValue("i", stats->regular));
    PyDict_SetItemString(response, "symlink", Py_BuildValue("i", stats->symlink));
    PyDict_SetItemString(response, "directory", Py_BuildValue("i", stats->directory));
    PyDict_SetItemString(response, "special", Py_BuildValue("i", stats->special));


reply:
    // closing database before archiving
    debug("[+] closing database\n");
    root.database->close(root.database);

    if(response) {
        // removing possible already existing db
        unlink(targetfile);

        if(!(libflist_archive_create(targetfile, root.workspace)))
            PyErr_SetString(PyExc_RuntimeError, libflist_strerror());
    }

    debug("[+] cleaning workspace\n");
    if(!libflist_workspace_destroy(root.workspace)) {
        return NULL;
    }

    return response;
}

static PyObject *pyflist_getfile(PyObject *self, PyObject *args) {
    PyErr_SetString(PyExc_NotImplementedError, "Not Implemented (yet)");
    return NULL;
}

static PyObject *pyflist_getdirectory(PyObject *self, PyObject *args) {
    PyObject *pycaps = NULL;
    const char *directory;

    if(!PyArg_ParseTuple(args, "Os", &pycaps, &directory))
        return NULL;

    pyflist_obj_t *root;
    if(!(root = (pyflist_obj_t *) PyCapsule_GetPointer(pycaps, "FlistObject")))
        return NULL;

    dirnode_t *dirnode = libflist_directory_get(root->database, (char *) directory);
    if(dirnode == NULL)
        return NULL;

    PyObject *dict = dirnode_to_dict(dirnode);

    // FIXME: free everything

    return dict;
}

static PyObject *pyflist_dumps(PyObject *self, PyObject *args) {
    PyObject *pycaps = NULL;
    const char *directory;

    if(!PyArg_ParseTuple(args, "Os", &pycaps, &directory))
        return NULL;

    pyflist_obj_t *root;
    if(!(root = (pyflist_obj_t *) PyCapsule_GetPointer(pycaps, "FlistObject")))
        return NULL;

    dirnode_t *dirnode = libflist_directory_get(root->database, (char *) directory);
    if(dirnode == NULL)
        return NULL;

    PyObject *dict = dirnode_to_dict(dirnode);

    // FIXME: free everything

    return dict;
}


static PyObject *pyflist_zdb_open(PyObject *self, PyObject *args) {
    const char *host;
    const char *namespace = "default";
    const char *password = NULL;
    const int port;

    if(!PyArg_ParseTuple(args, "si|ss", &host, &port, &namespace, &password))
        return NULL;

    flist_db_t *backdb;

    if(!(backdb = libflist_db_redis_init_tcp(host, port, namespace, password))) {
        PyErr_SetString(PyExc_RuntimeError, "Could not initialize tcp connection");
        return NULL;
    }

    return PyCapsule_New(backdb, "FlistBackend", NULL);
}

// enable or disable internal debug and libflist debug
// take a boolean as argument (True or False) in order to enable
// or disable debug message
//
// debug message are always printed on stdout and are handled on lowlevel
// and not python level
static PyObject *pyflist_debug(PyObject *self, PyObject *args) {
    int enabled;

    if(!PyArg_ParseTuple(args, "p", &enabled))
        return NULL;

    libflist_debug_enable(enabled);
    pyflist_debug_flag = enabled;

    return PyBool_FromLong(1);
}

static PyMethodDef pyflister_cm[] = {
    {"backend_zdb_open", pyflist_zdb_open, METH_VARARGS, "Open a connection to zdb"},

    {"open", pyflist_open, METH_VARARGS, "Open an existing flist"},
    {"dumps", pyflist_dumps, METH_VARARGS, "Dump opened flist into a dict"},
    {"close", pyflist_close, METH_VARARGS, "Close an opened flist"},
    {"create", pyflist_create, METH_VARARGS, "Create a new empty flist"},
    {"getdirectory", pyflist_getdirectory, METH_VARARGS, "List the contents of a directory"},
    {"getfile", pyflist_getfile, METH_VARARGS, "Read a file from the backend"},

    {"debug", pyflist_debug, METH_VARARGS, "Enable or disable debug"},

    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef pyflister = {
    PyModuleDef_HEAD_INIT,
    "flist",        // name of module
    NULL,           // module documentation, may be NULL
    -1,             // -1 if the module keeps state in global variables.
    pyflister_cm
};

PyMODINIT_FUNC PyInit_pyflist(void) {
    return PyModule_Create(&pyflister);
}
