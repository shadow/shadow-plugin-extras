/*
 * See LICENSE for licensing information
 */

#ifndef PCAP_REPLAY_H_
#define PCAP_REPLAY_H_

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/epoll.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <glib.h>
#include <pcap.h> 
#include <time.h>
#include <netinet/tcp.h>

#define MTU 2000 // Size of the buffer for recv() function (in bytes)

typedef void (*PcapReplayLogFunc)(GLogLevelFlags level, const char* functionName, const char* format, ...);

/* Custom packets are used to store the packets we read in the pacp file */
typedef struct Custom_Packet {
	struct timeval timestamp;
	char* payload;
	gint payload_size;	
} Custom_Packet_t;

/* all state for the pcap replayer is stored here */
typedef struct _Pcap_Replay {
	guint magic;
	/* the function we use to log messages
	 * needs level, functionname, and format */
	PcapReplayLogFunc slogf; 

	/* the epoll descriptor to which we will add our sockets.
	 * we use this descriptor with epoll to watch events on our sockets. */
	gint ed;

	/* Timeout of the pcap replayer. Instance stops when timeout is reached */
	guint64 timeout;

	gboolean isClient; /* client or server */
	gboolean isTorClient; /* normal client or tor-client */
	gboolean isAllowedToSend;
	gboolean isFirstPacketReceived;
	gboolean isRestarting; /* Is the server/client is in restarting state ? */
	gboolean isDone; /* our client/server has finished or timeout occured, we can exit */
	gint nmb_conn; /* Number of connections already made */

	/* The following queues are used to keep tack of the pcap files
	 * the plugin has to send */
	GQueue* pcapStructQueue;
	GQueue* pcapFilePathQueue;

	/* Pcap structure that is used to progress in the pcap file */
 	pcap_t *pcap; // Current pcap structure in use 	
	gint nmb_pcap_file; // nmb of pcap files received in argument

	/* nextPacket is a pointer to the next packet to send 
	 * See get_next_packet() */
	Custom_Packet_t * nextPacket;

	/* Infos used by the client to connect to the Tor proxy */
	in_addr_t proxyIP; /* stored in network order */
	in_port_t proxyPort; /*  Tor SocksPort (default 9000) */

	/* Infos used by the client to connect to the remote server */
	GString* serverHostName;
	gint  serverPortInt;
	in_addr_t serverIP; /* stored in network order */
	in_port_t serverPort; /* stored in network order */

	/* IP & ports used by the client/server in the pcap file */
	/* These are used to pick the right packets in the pcap file */
	struct in_addr client_IP_in_pcap;
	gushort client_port_in_pcap;
	struct in_addr server_IP_in_pcap;
	gushort server_port_in_pcap;

	struct {	 
		int sd; /* Socket descriptor to connect to distant server */
	} client;

	/* Infos used by the pcap server */
	struct {
		int sd; /* Socket descriptor to listen to connecting client */
	} server;
} Pcap_Replay;

Pcap_Replay* pcap_replay_new(gint argc, gchar* argv[], PcapReplayLogFunc slogf); 

gboolean pcap_StartClient(Pcap_Replay* pcapReplay);
gboolean pcap_StartServer(Pcap_Replay* pcapReplay);
gboolean pcap_StartClientTor(Pcap_Replay* pcapReplay);

gboolean restart_server(Pcap_Replay* pcapReplay);
gboolean restart_client(Pcap_Replay* pcapReplay);

/* Tor client specific */
gboolean initiate_conn_to_proxy(Pcap_Replay* pcapReplay);
gssize send_to_proxy(Pcap_Replay* pcapReplay, gpointer buffer, gsize length);
gssize recv_from_proxy(Pcap_Replay* pcapReplay, gpointer buffer, gsize length);

void pcap_replay_ready(Pcap_Replay* pcapReplay);
void _pcap_activateClient(Pcap_Replay* pcapReplay, gint sd, uint32_t events);
void _pcap_activateServer(Pcap_Replay* pcapReplay, gint sd, uint32_t events);

gint pcap_replay_getEpollDescriptor(Pcap_Replay* pcapReplay);
void _pcap_server_epoll(Pcap_Replay* pcapReplay, gint operation, guint32 events);
void _pcap_client_epoll(Pcap_Replay* pcapReplay, gint operation, guint32 events);

gboolean get_next_packet(Pcap_Replay* pcapReplay);
ssize_t send_packet(Custom_Packet_t* cp, gint sd);
gboolean change_pcap_file_to_send(Pcap_Replay* pcapReplay);
void compute_wait_time(struct timeval tv1, struct timeval tv2, struct timespec res);
int timeval_subtract (struct timespec *result, struct timeval *y, struct timeval *x);

gboolean pcap_replay_isDone(Pcap_Replay* h); 
void pcap_replay_free(Pcap_Replay* h); 
void deinstanciate(Pcap_Replay* pcapReplay, gint sd);


/* The following structures are dedicated to parse the pcap file easily */
/* Ethernet header */
#define SIZE_ETHERNET 14
#define ETHER_ADDR_LEN  6
struct sniff_ethernet {
	u_char ether_dhost[ETHER_ADDR_LEN]; /* Destination host address */
	u_char ether_shost[ETHER_ADDR_LEN]; /* Source host address */
	u_short ether_type; /* IP? ARP? RARP? etc */
};

/* IP header */
struct sniff_ip {
	u_char ip_vhl;	  /* version << 4 | header length >> 2 */
	u_char ip_tos;	  /* type of service */
	u_short ip_len;	 /* total length */
	u_short ip_id;	  /* identification */
	u_short ip_off;	 /* fragment offset field */
#define IP_RF 0x8000		/* reserved fragment flag */
#define IP_DF 0x4000		/* dont fragment flag */
#define IP_MF 0x2000		/* more fragments flag */
#define IP_OFFMASK 0x1fff   /* mask for fragmenting bits */
	u_char ip_ttl;	  /* time to live */
	u_char ip_p;		/* protocol */
	u_short ip_sum;	 /* checksum */
	struct in_addr ip_src;
	struct in_addr ip_dst; /* source and dest address */
};
#define IP_HL(ip)	   (((ip)->ip_vhl) & 0x0f)
#define IP_V(ip)		(((ip)->ip_vhl) >> 4)

/* TCP header */
struct sniff_tcp {
	u_short th_sport;   /* source port */
	u_short th_dport;   /* destination port */
	u_int32_t th_seq;	   /* sequence number */
	u_int32_t th_ack;	   /* acknowledgement number */

	u_char th_offx2;	/* data offset, rsvd */
#define TH_OFF(th)  (((th)->th_offx2 & 0xf0) >> 4)
	u_char th_flags;
#define TH_FIN 0x01
#define TH_SYN 0x02
#define TH_RST 0x04
#define TH_PUSH 0x08
#define TH_ACK 0x10
#define TH_URG 0x20
#define TH_ECE 0x40
#define TH_CWR 0x80
#define TH_FLAGS (TH_FIN|TH_SYN|TH_RST|TH_ACK|TH_URG|TH_ECE|TH_CWR)
	u_short th_win;	 /* window */
	u_short th_sum;	 /* checksum */
	u_short th_urp;	 /* urgent pointer */
};

#endif /* PCAP_REPLAY_H_*/
