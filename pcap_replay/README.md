# "pcap-replay": A Shadow plug-in 
=================================

The purpose of this plug-in is to replay network traffic saved in pcap files. The execution of the plug-in is quiet simple. There is one server and one client. The server is started first and waits for the client to connect. When started, the client connects to the server and both of them start replaying the network traffic saved in the pcap file(s) based on the arguments supplied. The client can either be a normal client or a tor-client. If run as a tor-client, the plugin needs to connect to the Tor proxy before transmitting to the server. So make sure you are running the tor & torctl plug-in along with your tor-client when running your experiment. Moreover, check that the tor plugin opens its SocksPort when starting. 

The plug-in is bundled with an example that can serve as a starting point.

Installation
------------
- Follow the steps outlined in the `README.md` file in the project root.
- Running `make` also installs a standalone executable meant to be run outside shadow. This may be useful for testing and debugging purposes.


Usage: General
--------------
The plugin is primarily driven by the arguments supplied to it.
```bash
./shadow-plugin-pcap_replay-exe <node-type> <server-host> <server-port> <pcap_client_ip> <pcap_client_port> <pcap_server_ip> <pcap_server_port> <timeout> <pcap_trace1> <pcap_trace2>.. 
```

- **node-type**: Takes a value `client | client-tor | server`.
- **server-host, server-port**: The hostname and port the server binds to and the client connects to.
- **pcap_client_ip, pcap_client_port**: The client IP and port in the pcap file that _our_ client must replay. 
- **pcap_server_ip, pcap_server_port**: The server IP and port in the pcap file that _our_ server must replay. In conjunction with the previous two options, these 4 attributes denote a single 5-tuple connection between a client application and a server (the fifth value is the protocol, TCP).
- **timeout**: n seconds after which the plugin will exit gracefully.
- **[pcap_traces]**: One or more pcap traffic captures to replay.

The tool essentially extracts the payload from a packet that matches the 'filters' we provide above, repackages it into a fresh TCP packet and sends it to the stack while respecting the relative packet timings from the pcap.

Note that the TCP control messages (Handshake, ACK, Options, etc...) will not be replayed since the payload of such packets is empty.


Sample Usage : Standalone Executable
------------------------------------
The bundled example with this plugin contains a `sample.pcap` file which can be used for testing. Place the built binary into the directory containing the pcap file and and run the following commands in separate terminal windows:

```bash
./shadow-plugin-pcap_replay-exe server localhost 1337 192.168.3.131 53892 72.14.213.147 443 500 sample.pcap 
```
```bash
./shadow-plugin-pcap_replay-exe client localhost 1337 192.168.3.131 53892 72.14.213.147 443 500 sample.pcap 
```


Sample Usage : Shadow
---------------------
The `shadow.example.config.xml` file in the example directory contains a simple client-server configuration. Run the simulation as:

```bash
shadow shadow.example.config.xml > shadow.log
```

Shadow creates one client and one server. The server starts at time t=1 and binds to the port 80. The client starts at t=2 and connects to the hostname `server` on port 80. Both will replay the traffic captured in the `sample.pcap`. 

```xml
    <host id="server">
        <process plugin="pcap_replay" starttime="1" arguments="server server 80 192.168.3.131 53892 72.14.213.147 443 1000 sample.pcap" />
    </host>

    <host id="client">
        <process plugin="pcap_replay" starttime="2" arguments="client server 80 192.168.3.131 53892 72.14.213.147 443 1000 sample.pcap" />
    </host>

```


Sample Usage : Shadow with Tor
------------------------------
The example directory further contains the following files and directories which allow us to setup the tor infrastructure on shadow:
- `shadowtor.example.config.xml`
- `conf/*`
- `shadow.data.template/hosts/*`

```bash
shadow shadowtor.example.config.xml > shadow.log
```

In addition to the `pcap_replay` process, we also boot the `tor` and `torctl` plugins on our client host. The server host remains unchanged. The additional delays are added to let the tor infrastructure finish its setup early in the simulation.   
More importantly, the only change we make on the `pcap_replay` plugin is the addition of the SocksPort argument at the second position on the client host (e.g., 9000).

```xml
    <host id="server">
        <process plugin="pcap_replay" starttime="1" arguments="server server 80 192.168.3.131 52152 72.14.213.147 443 2000 sample.pcap" />
    </host>

    <host id="clienttor">
        <process plugin="tor" preload="tor-preload" starttime="900" arguments="--Address ${NODEID} <clipped>" />
        <process plugin="torctl" starttime="901" arguments="localhost 9051 <clipped>" />
        <process plugin="pcap_replay" starttime="1200" arguments="client-tor 9000 server 80 192.168.3.131 52152 72.14.213.147 443 800 sample.pcap" />
    </host>
```


Licensing deviations
--------------------
No deviations from LICENSE.


Last known working version
--------------------------
- Shadow v1.14.0 (built 2021-03-18) running GLib v2.56.4 and IGraph v0.7.1

