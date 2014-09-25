The `browser` and `webserver` plugins in this directory are meant to
be used together. Mixing with other plugins, e.g., the `browser` and
`filetransfer` plugins in the top level directory, is not guaranteed
to work.

# dependencies

The following are required in addition to those Shadow requires:

    * `boost`
    * `spdylay` (https://github.com/tatsuhiro-t/spdylay)

# quick setup

```bash
mkdir build
cd build
CC=`which clang` CXX=`which clang++` cmake .. -DCMAKE_INSTALL_PREFIX=`readlink -f ~`/.shadow
```

If you installed `spdylay` in a custom location, specify `-DCMAKE_EXTRA_INCLUDES=/path/to/include -DCMAKE_EXTRA_LIBRARIES=/path/to/lib` when running `cmake`.

Next, to make the C++ compiler happy (if you know a more elegant way, please do tell -- I can't get `extern "C"` stuff to work):

```bash
sed -i -e "s/PluginNewInstanceFunc new/PluginNewInstanceFunc/g" $HOME/.shadow/include/shd-library.h
```

Then, build and install the plugins, typically to `$HOME/.shadow/plugins`:

```bash
make -jN
make install
```

Replace `N` with the number of cores you want to use for a parallel build.
