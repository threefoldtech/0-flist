#include <Python.h>
#include <stdio.h>
#include <stdlib.h>
#include "lib0stor.h"

/*
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

static PyObject *db_open(PyObject *self, PyObject *args) {
    //
}

static PyObject *db_close(PyObject *self, PyObject *args) {
    //
}

static PyObject *backend_open(PyObject *self, PyObject *args) {
    //
}

static PyObject *backend_close(PyObject *self, PyObject *args) {
    //
}

static PyObject *open(PyObject *self, PyObject *args) {
    //
}

static PyObject *create(PyObject *self, PyObject *args) {
    //
}

static PyObject *getfile(PyObject *self, PyObject *args) {
    //
}

static PyMethodDef pyflister_cm[] = {
    // {"connect",  g8storclient_connect,  METH_VARARGS, "Initialize client"},
    {"db_open", db_open, METH_VARARGS, "Open a database handler"},
    {"db_close", db_close, METH_VARARGS, "Close and free a database handler"},

    {"backend_open", backend_open, METH_VARARGS, "Create a backend object from a database"},
    {"backend_close", backend_close, METH_VARARGS, "Close and free a backend objecy"},

    {"open", open, METH_VARARGS, "Open an existing flist"},
    {"create", create, METH_VARARGS, "Create a new empty flist"},
    {"getfile", getfile, METH_VARARGS, "Read a file from the backend"}

    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef pyflister = {
    PyModuleDef_HEAD_INIT,
    "flist",        // name of module
    NULL,           // module documentation, may be NULL
    -1,             // -1 if the module keeps state in global variables.
    pyflister_cm
};

PyMODINIT_FUNC pyflister(void) {
    return PyModule_Create(&pyflister);
}
