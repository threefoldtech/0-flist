#include <Python.h>
#include <stdio.h>
#include <stdlib.h>
#include "libflist.h"

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

    printf("[+] initializing workspace\n");
    if(!(root->workspace = libflist_workspace_create())) {
        fprintf(stderr, "workspace_create");
        // TODO: free
        return NULL;
    }

    printf("[+] workspace: %s\n", root->workspace);

    if(!libflist_archive_extract((char *) root->filename, root->workspace)) {
        fprintf(stderr, "libflist_archive_extract");
        return NULL; // FIXME
    }

    printf("[+] loading database\n");
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

    printf("[+] closing database\n");
    root->database->close(root->database);

    printf("[+] cleaning workspace\n");
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

    if(!PyArg_ParseTuple(args, "ssO", &rootpath, &targetfile, &pycaps))
        return NULL;

    if(!(backdb = (flist_db_t *) PyCapsule_GetPointer(pycaps, "FlistBackend")))
        return NULL;

    printf("[+] initializing workspace\n");
    if(!(root.workspace = libflist_workspace_create())) {
        fprintf(stderr, "workspace_create");
        // TODO: free
        return NULL;
    }

    printf("[+] workspace: %s\n", root.workspace);

    root.database = libflist_db_sqlite_init(root.workspace);
    root.database->create(root.database);

    if(!(backend = backend_init(backdb, rootpath))) {
        return NULL;
    }

    // building database
    flist_stats_t *stats = flist_create(root.database, rootpath, backend);
    if(!stats)
        return NULL;

    // building dict response
    PyObject *response = PyDict_New();
    PyDict_SetItemString(response, "regular", Py_BuildValue("i", stats->regular));
    PyDict_SetItemString(response, "symlink", Py_BuildValue("i", stats->symlink));
    PyDict_SetItemString(response, "directory", Py_BuildValue("i", stats->directory));
    PyDict_SetItemString(response, "special", Py_BuildValue("i", stats->special));

    // closing database before archiving
    printf("[+] closing database\n");
    root.database->close(root.database);

        // removing possible already existing db
    unlink(targetfile);
    libflist_archive_create(targetfile, root.workspace);


    printf("[+] cleaning workspace\n");
    if(!libflist_workspace_destroy(root.workspace)) {
        fprintf(stderr, "workspace_destroy\n");
        return NULL;
    }

    return response;
}

static PyObject *pyflist_getfile(PyObject *self, PyObject *args) {
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

static PyObject *pyflist_zdb_open(PyObject *self, PyObject *args) {
    const char *host;
    const char *namespace = "default";
    const char *password = NULL;
    const int port;

    if(!PyArg_ParseTuple(args, "si|ss", &host, &port, &namespace, &password))
        return NULL;

    flist_db_t *backdb;

    if(!(backdb = libflist_db_redis_init_tcp(host, port, namespace, password))) {
        fprintf(stderr, "[-] cannot initialize backend\n");
        return 1;
    }

    return PyCapsule_New(backdb, "FlistBackend", NULL);
}

static PyMethodDef pyflister_cm[] = {
    {"backend_zdb_open", pyflist_zdb_open, METH_VARARGS, "Open a connection to zdb"},

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
