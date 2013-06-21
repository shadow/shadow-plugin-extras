shadow-plugins-extra
====================

This repository holds additional plug-ins for Shadow, including a basic "hello world" example useful for developing new plug-ins.

quick setup
===========

```bash
mkdir build
cd build
CC=`which clang` CXX=`which clang++` cmake ..
make -jN
make install
```

Replace `N` with the number of cores you want to use for a parallel build.

cmake options
=============

The `cmake` command above takes multiple options, specified as

```bash
CC=`which clang` CXX=`which clang++` cmake .. -DOPT=VAL
```

SHADOW_ROOT = "path/to/shadow/install/root" (default is "~/.shadow")
CMAKE_BUILD_TYPE = "Debug" or "Release" (default is "Debug")
CMAKE_INSTALL_PREFIX = "path/to/install/root" (default is ${SHADOW_ROOT})

troubleshooting
===============

First try rebuilding to ensure that the cmake cache is up to date

```bash
rm -rf build
mkdir build
cd build
```

using `VERBOSE=1` for more verbose output

```bash
VERBOSE=1 CC=`which clang` CXX=`which clang++` cmake ..
make
```

