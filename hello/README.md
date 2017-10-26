"hello": a Shadow plug-in
=========================

This plug-in provides a quick example of how to interface with Shadow.
The most important features of the code that enable this are:
 + no process forking
 + no busy loops

copyright holders
-----------------

No copyright is claimed by the United States Government.

licensing deviations
--------------------

No deviations from LICENSE.

last known working version
--------------------------

This plug-in was last tested and known to work with

`Shadow v1.12.0-15-g9502fca 2017-10-23 (built 2017-10-26) running GLib v2.46.2 and IGraph v0.7.1`

usage
-----

Please see the `example.xml`, which may be run in Shadow

```bash
shadow example.xml
```

After running the above, check the following directories for process output:

  + `shadow.data/hosts/helloclient/stdout-hello-1000.log`
  + `shadow.data/hosts/helloserver/stdout-hello-1000.log`

A binary version of the code is available for usage outside of Shadow.
Run the program `hello` with no arguments to start the server:

```bash
hello
```

Run the program `hello` with the IP address or hostname of the listening
server to run client mode:

```bash
hello localhost
```
