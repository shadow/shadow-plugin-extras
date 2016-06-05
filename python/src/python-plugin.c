#include "python-plugin.h"
#include "python-global-lock.h"

extern PyThreadState *shd_py_interpreter;

void __shadow_plugin_enter__() {
    shadow_python_lock();
    if(shd_py_interpreter)
        PyThreadState_Swap(shd_py_interpreter);
}

void __shadow_plugin_exit__() {
    shadow_python_unlock();
}