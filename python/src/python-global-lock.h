#ifndef SHD_PYTHON_GLOBAL_LOCK_
#define SHD_PYTHON_GLOBAL_LOCK_
#include <glib.h>

void shadow_python_lock();
void shadow_python_unlock();

#endif /* SHD_PYTHON_GLOBAL_LOCK_ */