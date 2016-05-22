#include <Python.h>
#include <structmember.h>
#include <glib.h>

void _py_log(GLogLevelFlags level, const gchar* functionName, const gchar* format, ...) {
    va_list variableArguments;
    va_start(variableArguments, format);
    g_logv(G_LOG_DOMAIN, level, format, variableArguments);
    va_end(variableArguments);
}

typedef enum {
    PY_LOG_LEVEL_CRITICAL = 50,
    PY_LOG_LEVEL_ERROR = 40,
    PY_LOG_LEVEL_WARNING = 30,
    PY_LOG_LEVEL_INFO = 20,
    PY_LOG_LEVEL_DEBUG = 10,
    PY_LOG_LEVEL_NOTSET = 0
} PyLogLevel;

static PyObject *
shadow_python_write(PyObject *self, PyObject *args)
{
    char *msg = NULL;
    int level = -1, shadow_level;

    if(!PyArg_ParseTuple(args, "is", &level, &msg))
        return NULL;
    switch(level) {
        case PY_LOG_LEVEL_CRITICAL:
            shadow_level = G_LOG_LEVEL_ERROR;
            break;
        case PY_LOG_LEVEL_ERROR:
            shadow_level = G_LOG_LEVEL_CRITICAL;
            break;
        case PY_LOG_LEVEL_WARNING:
            shadow_level = G_LOG_LEVEL_WARNING;
            break;
        case PY_LOG_LEVEL_INFO:
            shadow_level = G_LOG_LEVEL_INFO;
            break;
        case PY_LOG_LEVEL_DEBUG:
        case PY_LOG_LEVEL_NOTSET:
            shadow_level = G_LOG_LEVEL_DEBUG;
            break;
        default:
            PyErr_SetString(PyExc_ValueError, "Invalid log level");
            return NULL;
    }
    _py_log(shadow_level, __FUNCTION__, "%s", msg);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyMethodDef module_methods[] = {
    {"write", shadow_python_write, METH_VARARGS, "Log a line via glib"},
    {NULL}  /* Sentinel */
};


#if PY_MAJOR_VERSION >= 3

static struct PyModuleDef moduledef = {
        PyModuleDef_HEAD_INIT,
        "shadow_python",
        NULL,
        -1,
        module_methods,
        NULL,
        NULL,
        NULL,
        NULL
};
#endif

PyObject* init_logger()
{
#if PY_MAJOR_VERSION >= 3
    PyObject *module = PyModule_Create(&moduledef);
#else
    PyObject *module = Py_InitModule("shadow_python", module_methods);
#endif
    if(!module)
        return NULL;
    return module;
}
