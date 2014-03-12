shadow-plugins-extra
====================

This repository holds additional plug-ins for Shadow, including a basic "hello world" example useful for developing new plug-ins.

quick setup
===========

```bash
mkdir build
cd build
CC=`which clang` CXX=`which clang++` cmake .. -DCMAKE_INSTALL_PREFIX=`readlink -f ~`/.shadow
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

For example:

```bash
CC=`which clang` CXX=`which clang++` cmake .. -DSHADOW_ROOT=/home/rob/.shadow -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/home/rob/.shadow
```

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

contributing
============

Please feel free to submit pull requests to contribute new plug-ins to
this repository. If contributing a plug-in to this repository, please
add a README.md to the top level of your plug-in directory that includes:

 + copyright holders
 + licensing deviations from the LICENSE file, or other restrictions
 + the most recent version of Shadow with which this plug-in is known to work
 + how to use the plug-in

You may wish to use `README.template.md` as a starting point.
