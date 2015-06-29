#include <Python.h>
#include <structmember.h>
#include <shd-library.h>

typedef struct {
    PyObject_HEAD
    /* Type-specific fields go here. */
    ShadowLogFunc log;
} Logger;

static void
Logger_dealloc(Logger* self)
{
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static int
Logger_init(Logger *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"log", NULL};
    PyObject *log_tmp = NULL;

    if (! PyArg_ParseTupleAndKeywords(args, kwds, "O", kwlist, &log_tmp))
        return -1;

    self->log = (ShadowLogFunc)PyCapsule_GetPointer(log_tmp, NULL);
    if(!self->log)
        return -1;
    return 0;
}


static PyMemberDef Logger_members[] = {
    {"log", T_OBJECT_EX, offsetof(Logger, log), 0, "The log function"},
    {NULL}  /* Sentinel */
};

static PyObject *
Logger_write(Logger *self, PyObject *args)
{
    char *msg = NULL;
    int level = -1, shadow_level;

    if(!PyArg_ParseTuple(args, "is", &level, &msg))
        return NULL;
    switch(level) {
        case 0:
            shadow_level = SHADOW_LOG_LEVEL_ERROR;
            break;
        case 1:
            shadow_level = SHADOW_LOG_LEVEL_CRITICAL;
            break;
        case 2:
            shadow_level = SHADOW_LOG_LEVEL_WARNING;
            break;
        case 3:
            shadow_level = SHADOW_LOG_LEVEL_MESSAGE;
            break;
        case 4:
            shadow_level = SHADOW_LOG_LEVEL_DEBUG;
            break;
        default:
            PyErr_SetString(PyExc_ValueError, "Invalid log level");
            return NULL;
    }
    self->log(shadow_level, __FUNCTION__, "%s", msg);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyMethodDef Logger_methods[] = {
    {"write", (PyCFunction)Logger_write, METH_VARARGS,
     "Log a message"
    },
    {NULL}  /* Sentinel */
};

static PyTypeObject LoggerType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "shadow_python.Logger",             /*tp_name*/
    sizeof(Logger), /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)Logger_dealloc,/*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,        /*tp_flags*/
    "Logger objects",           /* tp_doc */
    0,                     /* tp_traverse */
    0,                     /* tp_clear */
    0,                     /* tp_richcompare */
    0,                     /* tp_weaklistoffset */
    0,                     /* tp_iter */
    0,                     /* tp_iternext */
    Logger_methods,             /* tp_methods */
    Logger_members,             /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)Logger_init,      /* tp_init */
    0,                         /* tp_alloc */
    PyType_GenericNew,                 /* tp_new */
};

static PyMethodDef module_methods[] = {
    {NULL}  /* Sentinel */
};


#if PY_MAJOR_VERSION >= 3

static struct PyModuleDef moduledef = {
        PyModuleDef_HEAD_INIT,
        "shadow_python",
        NULL,
        0,
        module_methods,
        NULL,
        NULL,
        NULL,
        NULL
};
#endif

PyObject* init_logger()
{
    if (PyType_Ready(&LoggerType) < 0)
        return NULL;

#if PY_MAJOR_VERSION >= 3
    PyObject *module = PyModule_Create(&moduledef);
#else
    PyObject *module = Py_InitModule3("shadow_python", module_methods, "foo");
#endif
    if(!module)
        return NULL;

    Py_INCREF(&LoggerType);
    if(PyModule_AddObject(module, "Logger", (PyObject *)&LoggerType) != 0)
        return NULL;

    return module;
}
