/*
 * See LICENSE for licensing information
 */

#include "python-plugin.h"
#include "python-global-lock.h"


/*
 * This is actually a pretty dirty hack: The argv0 required here
 * cannot be the argv[0] of the plugin as it is in the wrong path.
 * Then Python will not search in the correct path for the modules.
 * Thus, we make this a static string based on the prefix set during
 * compilation of the plugin. Shouldn't cause too much harm but would
 * like to see a better solution.
 */
#if PY_MAJOR_VERSION >= 3
#define Lstr2(s) L ## s
#define Lstr(s) Lstr2(s)
#define ARGV0 Lstr(INSTALL_PREFIX) L"/bin/shadow-python3"
#else
#define ARGV0 INSTALL_PREFIX "/bin/shadow-python"
#endif
 
/* The used sub-interpreter */
PyThreadState *shd_py_interpreter = NULL;

int main(int argc, char *argv[]) {
    int ret = 0;

    /* Must be the first thing we do to get everything else started
     * We can do this here even though Py_Main will call those again.
     * However, we must run them before creating our sub-interpreter.
     *
     * See comments on ARGV0 as to why we set it like this.
     */
    Py_SetProgramName(ARGV0);
    Py_Initialize();

    /* Create sub-interpreter */
#ifdef DEBUG
    fprintf(stderr, "starting new interpreter\n");
#endif
    shd_py_interpreter = Py_NewInterpreter();
    if(!shd_py_interpreter) {
        PyErr_Print();
        return 1;
    }
    PyThreadState *old = PyThreadState_Swap(shd_py_interpreter);

    ret = python_main(argc, argv);
#ifdef DEBUG
    fprintf(stderr, "shutting down interpreter\n");
#endif
    Py_EndInterpreter(shd_py_interpreter);
    shd_py_interpreter = NULL;
    PyThreadState_Swap(old);
    // this should be enabled when we figure out what's crashing us here
    //Py_Finalize();
    return ret;
#undef PYERR
}
