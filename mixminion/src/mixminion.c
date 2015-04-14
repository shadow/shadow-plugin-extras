/*
 * See LICENSE for licensing information
 */

#include "mixminion.h"


Mixminion *mixminion_new(int argc, char *argv[], ShadowLogFunc slogf) {
    PyObject *get_handle = NULL, *args = NULL;
    Mixminion* m = NULL;
    int error = 0;

#define PYERR()                 \
    do {                               \
        PyErr_Print();                 \
        error = 1; \
        goto err;                      \
    } while(0)                         \

    Py_Initialize();
    PySys_SetArgvEx(argc, argv, 0);

    m = calloc(1, sizeof(Mixminion));
    assert(m);
    memset(m, 0, sizeof(Mixminion));
    m->slogf = slogf;

    m->module = PyImport_ImportModule("mixminion-runner");
    if(m->module == NULL) 
        PYERR();

    get_handle = PyObject_GetAttrString(m->module, "get_handle");
    if(get_handle == NULL)
        PYERR();

    args = PyTuple_New(0);
    if(args == NULL)
        PYERR();

    m->handle = PyObject_Call(get_handle, args, NULL);
    if(m->handle == NULL)
        PYERR();

    m->process = PyObject_GetAttrString(m->handle, "process");
    if(m->process == NULL)
        PYERR();

    m->finish = PyObject_GetAttrString(m->handle, "finish");
    if(m->finish == NULL)
        PYERR();
err:
    Py_XDECREF(get_handle);
    Py_XDECREF(args);
    if(error) {
        mixminion_free(m);
        m = NULL;
    }
    return m;
#undef PYERR
}

int mixminion_ready(Mixminion *m) {
    PyObject *args = PyTuple_New(0), *retval = NULL;
    if(args == NULL) {
        PyErr_Print();
        return 1;
    }
    retval = PyObject_Call(m->process, args, NULL);
    if(retval == NULL) {
        PyErr_Print();
        Py_XDECREF(args);
        mixminion_free(m);
        return 1;
    }
    int retint = PyObject_Not(retval);
    Py_XDECREF(args);
    Py_XDECREF(retval);
    return retint;
}

void mixminion_free(Mixminion *m) {
    if(m != NULL) {
        // stop server if its running
        if(m->finish) {
            PyObject *args = PyTuple_New(0);
            if(args != NULL) {
                if(PyObject_Call(m->finish, args, NULL) == NULL)
                    PyErr_Print();
                Py_XDECREF(args);
                Py_XDECREF(m->finish);   
            } else {
                PyErr_Print();
            }
        }
        if(m->process)
            Py_XDECREF(m->process);
        if(m->handle)
            Py_XDECREF(m->handle);
        if(m->module)
            Py_XDECREF(m->module);
        free(m);
    }
    Py_Finalize();
}