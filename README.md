# :bangbang: ARCHIVAL NOTICE - 2021-10-02 :bangbang:

In Shadow v2.x.x, we have transitioned to a new architecture that can run binary executables directly rather than building them as plugins that are loaded into Shadow. This means you can use various tools (e.g., cURL) directly and no longer need to use the custom, Shadow-specific build processes from this repository.

We've archived this repository for posterity and for those wanting to use Shadow v1.15.0 or earlier, but **no further development updates will be posted here**.

**Use at your own risk**; if it breaks, you get to keep both pieces.

-----

# shadow-plugins-extra

This repository holds additional plug-ins for Shadow, including a basic "hello world" example useful for developing new plug-ins.

# quick setup

```bash
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=`readlink -f ~`/.shadow
make
make install
```

# cmake options

The `cmake` command above takes multiple options, specified as

```bash
cmake .. -DOPT=VAL
```

+ SHADOW_ROOT = "path/to/shadow/install/root" (default is "~/.shadow")  
+ CMAKE_BUILD_TYPE = "Debug" or "Release" (default is "Debug")  
+ CMAKE_INSTALL_PREFIX = "path/to/install/root" (default is ${SHADOW_ROOT})  

For example:

```bash
cmake .. -DSHADOW_ROOT=/home/rob/.shadow -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/home/rob/.shadow
```

# troubleshooting

First try rebuilding to ensure that the cmake cache is up to date

```bash
rm -rf build
mkdir build
cd build
```

using `VERBOSE=1` for more verbose output

```bash
VERBOSE=1 cmake ..
make
```

# contributing

Please submit pull requests to contribute new plug-ins to this repository.
When contributing a plug-in to this repository, please add a README.md to
the top level of your plug-in directory that includes:

 + copyright holders
 + licensing deviations from the LICENSE file, or other restrictions
 + the most recent version of Shadow with which this plug-in is known to work
 + how to use the plug-in

You may wish to use `README.template.md` as a starting point.

