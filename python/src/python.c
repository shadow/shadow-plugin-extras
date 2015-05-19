/*
 * See LICENSE for licensing information
 */

#include "python-plugin.h"
#include "pygetopt.h"
#include "osdefs.h"
#include "code.h" /* For CO_FUTURE_DIVISION */
#include "import.h"

#ifdef __VMS
#include <unixlib.h>
#endif

#if defined(MS_WINDOWS) || defined(__CYGWIN__)
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#endif

#define COPYRIGHT \
    "Type \"help\", \"copyright\", \"credits\" or \"license\" " \
    "for more information."

#if (defined(PYOS_OS2) && !defined(PYCC_GCC)) || defined(MS_WINDOWS)
#define PYTHONHOMEHELP "<prefix>\\lib"
#else
#if defined(PYOS_OS2) && defined(PYCC_GCC)
#define PYTHONHOMEHELP "<prefix>/Lib"
#else
#define PYTHONHOMEHELP "<prefix>/pythonX.X"
#endif
#endif


static PyObject *prepare_interpreter(int argc, char *argv[], python_data *m) {
    if(argc < 2) {
        PyErr_SetString(PyExc_RuntimeError, "Need a file");
        return NULL;   
    }

    /* Create sub-interpreter */
    m->log(SHADOW_LOG_LEVEL_MESSAGE, __FUNCTION__, "starting new interpreter");
    m->interpreter = Py_NewInterpreter();
    if(!m->interpreter)
        return NULL;

    PySys_SetArgv(argc-1, argv+1);
    PyObject *argv0 = PyString_FromString(argv[1]);
    if(!argv0)
        return NULL;

    PyObject *split_result = PyObject_CallMethod(argv0, "rsplit", "(si)", "/", 1);
    Py_DECREF(argv0);
    Py_ssize_t split_size = PyList_Size(split_result);
    if(!split_result || split_size > 2 || split_size < 1) {
        Py_XDECREF(split_result);
        return NULL;
    }
    PyObject *path, *mod_filename;
    if(split_size == 1) {
        path = PyString_FromString(".");
        mod_filename = PyList_GetItem(split_result, 0);
    } else {
        /* split_size == 2 */
        path = PyList_GetItem(split_result, 0);
        if(path)
            Py_INCREF(path);
        mod_filename = PyList_GetItem(split_result, 1);
    }
    if(mod_filename)
        Py_INCREF(mod_filename);
    Py_DECREF(split_result);
    if(!mod_filename || !path) {
        Py_XDECREF(mod_filename);
        Py_XDECREF(path);
        return NULL;
    }

    PyObject *sys_path = PySys_GetObject("path");
    if(!sys_path) {
        Py_DECREF(mod_filename);
        Py_DECREF(path);
        return NULL;
    }

    if(PyList_SetItem(sys_path, 0, path) != 0) {
        Py_DECREF(mod_filename);
        Py_DECREF(path);
        return NULL;
    }

    split_result = PyObject_CallMethod(mod_filename, "rsplit", "(si)", ".", 1);
    Py_DECREF(mod_filename);
    if(!split_result)
        return NULL;
    PyObject *module_name = PyList_GetItem(split_result, 0);
    Py_INCREF(module_name);
    Py_DECREF(split_result);
    return module_name;
}


python_data *python_new(int argc, char *argv[], ShadowLogFunc log) {
    PyObject *get_handle = NULL, *args = NULL, *module_name;
    python_data* m = NULL;
    PyThreadState *saved_tstate;
    int error = 0;

#define PYERR()                 \
    do {                               \
        PyErr_Print();                 \
        error = 1; \
        goto err;                      \
    } while(0)

    log(SHADOW_LOG_LEVEL_MESSAGE, __FUNCTION__, "python_new called");
    /* Must be the first thing we do to get everything else started */
    Py_SetProgramName(argv[0]);
    Py_Initialize();

    /* We start a new sub interpreter, so we back the old one up to 
     * restore it later
     */
    saved_tstate = PyThreadState_Swap(NULL);

    m = calloc(1, sizeof(python_data));
    assert(m);
    m->log = log;

    if(!(module_name = prepare_interpreter(argc, argv, m)))
         PYERR();

    m->module = PyImport_Import(module_name);
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
    Py_XDECREF(module_name);
    PyThreadState_Swap(saved_tstate);
    if(error) {
        python_free(m);
        m = NULL;
    }
    return m;
#undef PYERR
}

int python_ready(python_data *m) {
    m->log(SHADOW_LOG_LEVEL_DEBUG, __FUNCTION__, "python_ready called");
    /* we need to switch to our interpreter */
    PyThreadState *saved_tstate = PyThreadState_Get();
    PyThreadState_Swap(m->interpreter);

    PyObject *args = PyTuple_New(0), *retval = NULL;
    if(args == NULL) {
        PyErr_Print();
        return 1;
    }
    retval = PyObject_Call(m->process, args, NULL);
    if(retval == NULL) {
        PyErr_Print();
        Py_XDECREF(args);
        python_free(m);
        return 1;
    }
    int retint = PyObject_Not(retval);
    Py_XDECREF(args);
    Py_XDECREF(retval);
    PyThreadState_Swap(saved_tstate);
    return retint;
}

void python_free(python_data *m) {
    if(m != NULL) {
        /* we need to switch to our interpreter */
        PyThreadState *saved_tstate = PyThreadState_Get();
        if(m->interpreter)
            PyThreadState_Swap(m->interpreter);

        /* call finish function if it was started */
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

        if(m->interpreter)
            Py_EndInterpreter(m->interpreter);
        PyThreadState_Swap(saved_tstate);
        free(m);
    }
    Py_Finalize();
}