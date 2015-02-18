/*
 * See LICENSE for licensing information
 */

#include "mixminion.h"


Mixminion *mixminion_new(int argc, char *argv[], ShadowLogFunc slogf) {
    PyObject *module = NULL, *get_server_handle = NULL, *args = NULL, *cmd = NULL, *arg = NULL, *call_args = NULL;
    Mixminion* m = NULL;
    int error = 0;

#define ERR(...)              \
    do {                               \
        fprintf(stderr, __VA_ARGS__);  \
        error = 1; \
        goto err;                      \
    } while (0)
#define PYERR()                 \
    do {                               \
        PyErr_Print();                 \
        error = 1; \
        goto err;                      \
    } while(0)                         \

    Py_Initialize();

    // if(argc < 2)
    //     ERR("Need type argument: client or server\n");
    // if(strncmp(argv[1], "client", 6) == 0)
    //     py_argv[0] = "/home/javex/.shadow/bin/mixminion";
    // else if(strncmp(argv[1], "server", 6) == 0)
    //     py_argv[0] = "/home/javex/.shadow/bin/mixminiond";
    // else
    //     ERR("Unknown type %s\n", argv[1]);

    module = PyImport_ImportModule("mixminion-runner");
    if(module == NULL) 
        PYERR();

    m = calloc(1, sizeof(Mixminion));
    assert(m);
    memset(m, 0, sizeof(Mixminion));
    m->slogf = slogf;
    m->module = module;
    Py_INCREF(module);

    get_server_handle = PyObject_GetAttrString(module, "get_server_handle");
    if(get_server_handle == NULL)
        PYERR();

    // init server
    call_args = PyTuple_New((Py_ssize_t) 2);
    if(call_args == NULL)
        PYERR();

    cmd = PyString_FromString("start");
    if(cmd == NULL)
        PYERR();
    if(PyTuple_SetItem(call_args, 0, cmd) != 0) {
        Py_XDECREF(cmd);
        PYERR();
    }

    args = PyList_New((Py_ssize_t)0);
    if(args == NULL)
        PYERR();
    for(int i = 0; i < argc - 3; i++) {
        arg = PyString_FromString(argv[i + 3]);
        if(arg == NULL)
            PYERR();
        if(PyList_Append(args, arg) == -1) {
            Py_XDECREF(arg);
            PYERR();
        }
        Py_XDECREF(arg);
    }
    if(PyTuple_SetItem(call_args, 1, args) != 0) {
        Py_XDECREF(args);
        PYERR();
    }
    PyObject_Print(call_args, stderr, 0);

    m->server = PyObject_Call(get_server_handle, call_args, NULL);
    if(m->server == NULL)
        PYERR();

    m->run_server_step = PyObject_GetAttrString(module, "run_server_step");
    if(m->run_server_step == NULL)
        PYERR();

    m->run_server_step_args = PyTuple_New((Py_ssize_t) 1);
    if(m->run_server_step_args == NULL)
        PYERR();
    if(PyTuple_SetItem(m->run_server_step_args, 0, m->server) != 0)
        PYERR();

    m->server_stop = PyObject_GetAttrString(module, "server_stop");
    if(m->server_stop == NULL)
        PYERR();

err:
    Py_XDECREF(module);
    Py_XDECREF(get_server_handle);
    Py_XDECREF(call_args);
    if(error) {
        mixminion_free(m);
        m = NULL;
    }
    return m;
#undef ERR
#undef PYERR
}

int mixminion_ready(Mixminion *m) {
    if(PyObject_Call(m->run_server_step, m->run_server_step_args, NULL) == NULL) {
        mixminion_free(m);
        return 1;
    }
    return 0;
}

void mixminion_free(Mixminion *m) {
    if(m != NULL) {
        // stop server if its running
        if(m->server_stop)
            if(PyObject_Call(m->server_stop, m->run_server_step_args, NULL) == NULL)
                PyErr_Print();
        Py_XDECREF(m->server_stop);
        Py_XDECREF(m->module);
        Py_XDECREF(m->run_server_step);
        Py_XDECREF(m->run_server_step_args);
        Py_XDECREF(m->server);
    }
    Py_Finalize();
}