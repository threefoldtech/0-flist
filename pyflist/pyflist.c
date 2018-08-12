#include <Python.h>
#include <stdio.h>
#include <stdlib.h>
#include "flister.h"
#include "archive.h"
#include "workspace.h"
#include "backend.h"
#include "database.h"
#include "database_sqlite.h"
#include "database_redis.h"
#include "flist_merger.h"
#include "flist_write.h"


#if 0
remote_t *remotes = NULL;

static PyObject *g8storclient_connect(PyObject *self, PyObject *args) {
    (void) self;
    remote_t *remote;
    const char *host;
    int port;

    if (!PyArg_ParseTuple(args, "si", &host, &port))
        return NULL;

    printf("[+] python binding: connecting %s (port: %d)\n", host, port);
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

    // printf("[+] encrypting %d chunks\n", buffer->chunks);
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

    // printf("[+] finalsize: %lu bytes\n", finalsize);

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
    // printf("[+] decrypting %d chunks\n", chunks);
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
        // printf("[+] chunk restored: %lu bytes\n", output->length);

        chunk_free(chunk);
    }

    size_t finalsize = buffer->finalsize;
    // printf("[+] finalsize: %lu bytes\n", finalsize);

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

typedef struct flist_pyobj_t {
    database_t *database;
    backend_t *backend;
    const char *filename;
    char *workspace;

} flist_pyobj_t;

static PyObject *pyflist_open(PyObject *self, PyObject *args) {
    (void) self;
    flist_pyobj_t *root;
    const char *filename;

    if(!PyArg_ParseTuple(args, "s", &filename))
        return NULL;

    if(!(root = (flist_pyobj_t *) PyMem_Malloc(sizeof(flist_pyobj_t))))
        return NULL;

    root->filename = strdup(filename);

    debug("[+] initializing workspace\n");
    if(!(root->workspace = workspace_create())) {
        fprintf(stderr, "workspace_create");
        // TODO: free
        return NULL;
    }

    debug("[+] workspace: %s\n", workspace);

    if(!archive_extract(root->filename, root->workspace)) {
        fprintf(stderr, "archive_extract");
        return NULL; // FIXME
    }

    debug("[+] loading database\n");
    root->database = database_sqlite_init(root->workspace);
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

    flist_pyobj_t *root;
    if(!(root = (flist_pyobj_t *) PyCapsule_GetPointer(pycaps, "FlistObject")))
        return NULL;

    debug("[+] closing database\n");
    root->database->close(root->database);

    debug("[+] cleaning workspace\n");
    if(!workspace_destroy(root->workspace)) {
        fprintf(stderr, "workspace_destroy\n");
        return NULL;
    }

    free(root->workspace);
    PyMem_Free(root);

    return Py_BuildValue("");
}

static PyObject *pyflist_create(PyObject *self, PyObject *args) {
    return NULL;
}

static PyObject *pyflist_getfile(PyObject *self, PyObject *args) {
    return NULL;
}

static PyObject *pyflist_getdirectory(PyObject *self, PyObject *args) {
    return NULL;
}


static PyMethodDef pyflister_cm[] = {
#if 0
    {"db_open", db_open, METH_VARARGS, "Open a database handler"},
    {"db_close", db_close, METH_VARARGS, "Close and free a database handler"},

    {"backend_open", backend_open, METH_VARARGS, "Create a backend object from a database"},
    {"backend_close", backend_close, METH_VARARGS, "Close and free a backend objecy"},
#endif

    {"open", pyflist_open, METH_VARARGS, "Open an existing flist"},
    {"close", pyflist_close, METH_VARARGS, "Close an opened flist"},
    {"create", pyflist_create, METH_VARARGS, "Create a new empty flist"},
    {"getdirectory", pyflist_getdirectory, METH_VARARGS, "List the contents of a directory"},
    {"getfile", pyflist_getfile, METH_VARARGS, "Read a file from the backend"},

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
