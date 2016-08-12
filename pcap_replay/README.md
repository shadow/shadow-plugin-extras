# "pcap-replay": A Shadow plug-in 
=================================

The purpose of this plug-in is to replay network traffic saved in pcap files. The execution of the plug-in is quiet simple. There is one server and one client. The server is started first and waits for the client to connect. When started, the client connects to the server and both of them starts replaying the network traffic saved in the pcap file(s) given in arguments. The client can etheir be a normal client or a tor-client. If run as a tor-client, the plugin needs to connect to the Tor proxy before sending to the remote server. So make sure you are running the tor & torctl plug-in along with your tor-client when running your experiment. Moreover, check that the tor plugin opens its SocksPort when starting. 

Install
-------

- Place the pcap-replay folder in ~/shadow-plugin-tor/src/
- Add the following line to ~/shadow-plugin-tor/src/CMakeLists.txt :
```
 add_subdirectory(pcap_replay)
```
- Then build and install the changes :

```bash
./setup buid
./setup install
```

Usage : Shadow
--------------
A small example is given in the archive shadowtor-pcap-replay-example.

See the 'shadow.example.config.xml`, which may be run using Shadow

```bash
shadow example.xml
```

Let's have a closer look at the arguments given to both the client and the server :

```
<node id="server" geocodehint="US">
    <application plugin="pcap_replay" time="60" arguments="server server 80 192.168.1.2 52000 192.168.1.3 22 950 trace1.pcap trace2.pcap"/>
</node>

<node id="client" geocodehint="US">
    <application plugin="pcap_replay" time="120" arguments="client server 80 192.168.1.2 52000 192.168.1.3 22 950 trace1.pcap trace2.pcap"/>
</node>
```

In this example, Shadow creates one client and one server. The server starts at 60 seconds and binds to the port 80. The client starts at 120 seconds connects to the hostname server on port 80. The client and the server will replay the traffic stored in the trace1.pcap and trace2.pcap files. The client will replay the TCP traffic with IP.source=192.168.1.2 & Port.source=52000 & IP.dest=192.168.1.3 & Port.dest=80 while respecting the sending timings of the original packets. On the other hand, the server will replay the TCP traffic flowing from the server to the client. Note that the TCP control messages (Handshake, ACK, Options, etc...) will not be replayed since the payload of these packets are empty. 

Usage : Shadow-tor
------------------

Please see the `shadowtor.example.config` which may be run using Shadow-tor


```bash
shadow-tor -i example.tor.xml
```

Let's have a closer look at the arguments given to both the client-tor and the server :

```
<node id="server" geocodehint="US" loglevel="debug">
    <application plugin="pcap_replay" time="200" arguments="server server 80 192.168.1.2 52000 192.168.1.3 22 950 /home/tor/pcap_traces/ssh_capture1.pcap /home/tor/pcap_traces/ssh_1.pcap /home/tor/pcap_traces/ssh_2.pcap"/>
</node>

<node id="client-tor" quantity="1" loglevel="debug" logpcap="true" pcapdir="test">
    <application plugin="tor" time="100" arguments="--Address ${NODEID} --Nickname ${NODEID} --DataDirectory shadow.data/hosts/${NODEID} --GeoIPFile ~/.shadow/share/geoip --defaults-torrc conf/tor.common.torrc -f conf/tor.client.torrc --BandwidthRate 1024000 --BandwidthBurst 1024000" />
    <application plugin="torctl" starttime="101" arguments="localhost 9051 STREAM,CIRC,CIRC_MINOR,ORCONN,BW,STREAM_BW,CIRC_BW,CONN_BW,BUILDTIMEOUT_SET,CLIENTS_SEEN,GUARD,CELL_STATS,TB_EMPTY,HS_DESC,HS_DESC_CONTENT"/>
    <application plugin="pcap_replay" time="250" arguments="client-tor 9000 server 80 192.168.1.2 52000 192.168.1.3 22 950 /home/*/pcap_traces/ssh_1.pcap /home/*/pcap_traces/ssh_2.pcap"/>
</node>
```

This example is slightly different from the first one. In this example, the client-tor connects to the Tor proxy that starts with the tor plug-in. The client plug-in connects to the SocksPort 9000 and negociate a remote connection to the server on port 80. When the negociation is over, the client and the server starts sending packets. 


Licensing deviations
--------------------

No deviations from LICENSE.


Last known working version
--------------------------

+ Shadow 'v1.11.1-25-g9609e85 2016-08-04 running GLib v2.46.2 and IGraph v0.7.1'
+ Shadow-tor 'commit b918ca70d8a4733d379a0c9b0571c304f786bb91 on Monday June 6 2016' 



