#include "python-global-lock.h"

G_LOCK_DEFINE_STATIC(shadow_python_global_lock);

void shadow_python_lock() {
    G_LOCK(shadow_python_global_lock);
}

void shadow_python_unlock() {
    G_UNLOCK(shadow_python_global_lock);
}