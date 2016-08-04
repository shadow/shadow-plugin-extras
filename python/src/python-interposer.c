#include <Python.h>
#include <dlfcn.h>

typedef void (*orig_Py_Finalize_type)();
typedef PyThreadState* (*orig_Py_NewInterpreter_type)();
typedef void (*orig_Py_EndInterpreter_type)(PyThreadState *tstate);

int interpreter_count = 0;


PyThreadState *Py_NewInterpreter() {
#ifdef DEBUG
    fprintf(stderr, "Py_NewInterpreter called with %d sub-interpreters already present\n", interpreter_count);
#endif
    interpreter_count++;
    orig_Py_NewInterpreter_type orig_Py_NewInterpreter = (orig_Py_NewInterpreter_type)dlsym(RTLD_NEXT,"Py_NewInterpreter");
    if(orig_Py_NewInterpreter == NULL) {
        fprintf(stderr, "Error loading from SO %s\n", dlerror());
        exit(1);
    }
    return orig_Py_NewInterpreter();
}


void Py_EndInterpreter(PyThreadState *tstate) {
    interpreter_count--;
    orig_Py_EndInterpreter_type orig_Py_EndInterpreter = (orig_Py_EndInterpreter_type)dlsym(RTLD_NEXT,"Py_EndInterpreter");
    if(orig_Py_EndInterpreter == NULL) {
        fprintf(stderr, "Error loading from SO: %s\n", dlerror());
        exit(1);
    }
#ifdef DEBUG
    fprintf(stderr, "Py_EndInterpreter called with %d sub-interpreters remaining\n", interpreter_count);
#endif
    return orig_Py_EndInterpreter(tstate);
}



void Py_Finalize() {
    orig_Py_Finalize_type orig_Py_Finalize = (orig_Py_Finalize_type)dlsym(RTLD_NEXT,"Py_Finalize");
    if(orig_Py_Finalize == NULL) {
        fprintf(stderr, "Error loading from SO: %s\n", dlerror());
        exit(1);
    }
#ifdef DEBUG
    fprintf(stderr, "Py_Finalize called with %d sub-interpreters\n", interpreter_count);
#endif
    if(interpreter_count == 0)
        orig_Py_Finalize();
}