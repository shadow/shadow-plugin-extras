/*
 * See LICENSE for licensing information
 */

#include "mixminion.h"


Mixminion *mixminion_new(int argc, char *argv[], ShadowLogFunc slogf) {
    char *py_argv[argc - 1]; // arguments for python module
    char *type; // client or server?
    FILE *mixminion = NULL;
    PyObject *main_module, *main_dict, *run_handle = NULL;
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

    if(argc < 2)
        ERR("Need type argument: client or server\n");
    if(strncmp(argv[1], "client", 6) == 0)
        py_argv[0] = "/home/javex/.shadow/bin/mixminion";
    else if(strncmp(argv[1], "server", 6) == 0)
        py_argv[0] = "/home/javex/.shadow/bin/mixminiond";
    else
        ERR("Unknown type %s\n", argv[1]);

    for(int i = 2; i < argc; i++)
        py_argv[i - 1] = argv[i];
    PySys_SetArgv(argc - 1, py_argv);

    main_module = PyImport_AddModule("__main__");
    if(main_module == NULL) 
        PYERR();

    main_dict = PyModule_GetDict(main_module);
    if(main_dict == NULL)
        PYERR();

    mixminion = fopen(py_argv[0], "r");
    if(mixminion == NULL)
        ERR("Could not open file '%s': %s\n", py_argv[0], strerror(errno));

    run_handle = PyRun_File(mixminion, "mixminion", Py_file_input, main_dict, main_dict);
    if(run_handle == NULL)
        PYERR();

    m = calloc(1, sizeof(Mixminion));
    assert(m);
    m->slogf = slogf;
    m->server = run_handle;
    Py_INCREF(run_handle);

err:
    if(mixminion != NULL)
        fclose(mixminion);
    if(error)
        mixminion_free(m);
    Py_XDECREF(run_handle);
    return m;
#undef ERR
#undef PYERR
}

void mixminion_ready(Mixminion *m) {

}

void mixminion_free(Mixminion *m) {
    if(m != NULL)
        Py_XDECREF(m->server);
    Py_Finalize();
}