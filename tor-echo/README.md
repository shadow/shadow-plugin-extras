"tor-echo": a Shadow plug-in
=========================

This Shadow plugin is meant to demonstrate a simple blocking TCP
socket client/server where a client connects to a Tor hidden service 
on Shadow-Tor.

Specifically, it demonstrates:
	+ Establishing the SOCK5 proxy handshake
	+ Connecting to the provided onion address
	+ Sending data to the hidden service
	+ Reading data sent from the hidden service

copyright holders
-----------------

No copyright is claimed by the University of Maryland, College Park.

licensing deviations
--------------------

No deviations from LICENSE.

last known working version
--------------------------

This plug-in was last tested and known to work with

`Shadow v1.13.0-37-g1361b461 2019-02-22`
 
usage
-----

Please see the `example.xml`, which may be run in Shadow. 

[shadow-plugin-tor](https://github.com/shadow/shadow-plugin-tor) should also be installed.

```bash
shadow example.xml
```

After running the above, check the following directories for process output:

  + `shadow.data/hosts/helloclient/stdout-hello-1000.log`
  + `shadow.data/hosts/helloserver/stdout-hello-1000.log`

A binary version of the code is available for usage outside of Shadow.
Run the program `tor-echo-client` with no arguments to start the server:

```bash
./tor-echo-server 8080
./tor-echo-client <onion> <msg>
```
