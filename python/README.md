The Python Shadow Plug-in
=========================

Run any Python script with Shadow using the Python plug-in. The limiations
of Shadow itself apply in it as well:

* Everything must be non-blocking I/O and non-blocking system calls
* Using epoll for all otherwise-blocking I/O calls
* No multiprocessing / forking but threading is allowed

Usage
-----

Please see the `example.xml`, which may be run in Shadow. For all executions
in the context of the shadow plug-in (either the library to run it with
Shadow or the standalone interpreter mentioned below), it requires its own
preloaded library. You can either run
´export LD_PRELOAD="$HOME/.shadow/lib/libshadow-python-interpose.so"` or
execute Shadow with the `--preload` argument (the latter one only works
for Shadow itself, not for the standlone binaries).

```bash
shadow example.xml
```

You can also run all parts of the simulation outside of Shadow using the
`shadow-python` binary. Included with the plug-in is a reimplementation
of the Shadow `echo` plug-in in Python. This script can be run either
from Python directly using your standard interpreter or using the
plug-in `shadow-python` mentioned above (which mostly behaves like a regular
Python interpreter). Run the server like this:

```bash
shadow-python tests/echo.py server 0.0.0.0
```

The execute the client:

```bash
shadow-python tests/echo.py client localhost
```

Substitute `shadow-python` with just `python` to run it with a regular
interpreter. There is nothing special about the script other than them being
written specifically to work with Shadow (meeting the requirements listed above
such as using `epoll`).

Python 3
--------

The plug-in currently does not support Python 3 due to conflicts of running
both in parallel. Interpreters and plug-ins for both exist and can be compiled.
It might be possible to just load the Python 3 plug-in and it could just work
if Python 2 is not enabled at all. However this has not been tested.

The plugin is called `libshadow-plugin-python3.so` and the interpreter is
`shadow-python3`.

Last Known Working Version
--------------------------

This plug-in was last tested and known to work with 
Shadow v1.11.1-23-g490e4f1
