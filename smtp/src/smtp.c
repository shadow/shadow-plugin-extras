/*
 * See LICENSE for licensing information
 */

#include "smtp.h"


Smtp *smtp_new(int argc, char *argv[], ShadowLogFunc slogf) {
    PyObject *get_handle = NULL, *args = NULL;
    Smtp* s = NULL;
    int error = 0;

#define PYERR()                 \
    do {                               \
        PyErr_Print();                 \
        error = 1; \
        goto err;                      \
    } while(0)                         \


    wchar_t **argv_w = malloc(sizeof(wchar_t *) * argc);
    if(!argv_w) {
        error = 1;
        goto err;
    }
    int i;
    for(i = 0; i < argc; i++) {
        int req_size = mbstowcs(NULL, argv[i], 0) + 1;
        if(req_size <= 1) {
            error = 1;
            goto err;
        }
        argv_w[i] = calloc(req_size, sizeof(wchar_t));
        if(mbstowcs(argv_w[i], argv[i], req_size) <= 0) {
            error = 1;
            goto err;
        }
    }
    Py_Initialize();
    PySys_SetArgvEx(argc, argv_w, 0);

    for(i = 0; i < argc; i++)
        free(argv_w[i]);
    free(argv_w);

    s = calloc(1, sizeof(Smtp));
    assert(s);
    memset(s, 0, sizeof(Smtp));
    s->slogf = slogf;

    s->module = PyImport_ImportModule("smtp-runner");
    if(s->module == NULL) 
        PYERR();

    get_handle = PyObject_GetAttrString(s->module, "get_handle");
    if(get_handle == NULL)
        PYERR();

    args = PyTuple_New(0);
    if(args == NULL)
        PYERR();

    s->handle = PyObject_Call(get_handle, args, NULL);
    if(s->handle == NULL)
        PYERR();

    s->process = PyObject_GetAttrString(s->handle, "process");
    if(s->process == NULL)
        PYERR();

    s->finish = PyObject_GetAttrString(s->handle, "finish");
    if(s->finish == NULL)
        PYERR();
err:
    Py_XDECREF(get_handle);
    Py_XDECREF(args);
    if(error) {
        smtp_free(s);
        s = NULL;
    }
    return s;
#undef PYERR
}

int smtp_ready(Smtp *s) {
    PyObject *args = PyTuple_New(0), *retval = NULL;
    if(args == NULL) {
        PyErr_Print();
        return 1;
    }
    retval = PyObject_Call(s->process, args, NULL);
    if(retval == NULL) {
        PyErr_Print();
        Py_XDECREF(args);
        smtp_free(s);
        return 1;
    }
    int retint = PyObject_Not(retval);
    Py_XDECREF(args);
    Py_XDECREF(retval);
    return retint;
}

void smtp_free(Smtp *s) {
    if(s != NULL) {
        // stop server if its running
        if(s->finish) {
            PyObject *args = PyTuple_New(0);
            if(args != NULL) {
                if(PyObject_Call(s->finish, args, NULL) == NULL)
                    PyErr_Print();
                Py_XDECREF(args);
                Py_XDECREF(s->finish);   
            } else {
                PyErr_Print();
            }
        }
        if(s->process)
            Py_XDECREF(s->process);
        if(s->handle)
            Py_XDECREF(s->handle);
        if(s->module)
            Py_XDECREF(s->module);
        free(s);
    }
    Py_Finalize();
}