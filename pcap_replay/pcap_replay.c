/*
 * See LICENSE for licensing information
 */

#include "pcap_replay.h"

#define MAGIC 0xFFEEDDCC

const gchar* USAGE = "USAGE: 'client'|'client-tor|'server' [SocksPort] serverHostName serverPort IP_client_in_pcap Port_client IP_server_in_pcap Port_server timeout [file.pcap,...]\n";

/* pcap_activateClient() is called when the epoll descriptor has an event for the client */
void _pcap_activateClient(Pcap_Replay* pcapReplay, gint sd, uint32_t events) {
	assert(pcapReplay->client.sd == sd);
	pcapReplay->slogf(G_LOG_LEVEL_DEBUG, __FUNCTION__, 
				"Activate client : An event is available for the client to process");
 
	/* Function vars*/
	struct timespec timeToWait;
	ssize_t numBytes = 0;
	char receivedPacket[MTU];

	/* Save a pointer to the packet to send */
	Custom_Packet_t *pckt_to_send = pcapReplay->nextPacket;


	/* Get the next packet of the pcap file for the client */
	if(!get_next_packet(pcapReplay)) {
		/* No more packet to send ! 
		 * Then restart client with the next pcap file to send */
		pcapReplay->slogf(G_LOG_LEVEL_INFO, __FUNCTION__, 
					"Send last pcap of the current file : open next pcap file to send !");
		// Send last packet to the remote server
		send_packet(pckt_to_send,sd);
		free(pckt_to_send);

		/* Now, we need to restart the client.
		 * The client will close its socket to the proxy
		 * and the proxy will close the connection to the remote host.
		 * Then the client will wait for an arbitrary time before trying to reconnnect to the proxy & remote server */
		if(restart_client(pcapReplay)==TRUE) {
			pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__, "Successfully restarted the client !");
			return;
		} else{
			deinstanciate(pcapReplay,sd);
			return;
		}
	}

	/* Compute waiting time before sending next packet.
	 * The wait time is equal to the difference of the timestamp of the two next packet to send  */
	timeval_subtract(&timeToWait, &pckt_to_send->timestamp,&pcapReplay->nextPacket->timestamp);
	
	/* LOG event */
	if(events & EPOLLOUT) {
		pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__, "Client EPOLLOUT is set");
	}
	if(events & EPOLLIN) {
		pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__, "Client EPOLLIN is set");
	}
	if((events & EPOLLIN) && (events & EPOLLOUT)) {
		pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__, "Client EPOLLIN & EPOLLOUT are set");
	}

	/* Process events */ 
	if((events & EPOLLIN) && (events & EPOLLOUT)) {
		/* EPOLLIN && EPOLLOUT activated :
		 * The client have packets to send and receive */

		/* send the next pcap packet */
		numBytes = send_packet(pckt_to_send, sd);
		
		/* log result */
		if(numBytes > 0) {
			pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
						"Successfully sent a %d (bytes) packet to the server.", numBytes);
		} else if(numBytes == 0) {
			/* The client doesn't recreate TCP control message */
			pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
						"The last packet to send was an ACK. Skipped sending.", numBytes);
		} else {
			pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
						"Unable to send message");
		}

		/* prepare to accept the incoming message */
		memset(receivedPacket, 0, (size_t)MTU);
		numBytes = recv(sd, receivedPacket, (size_t)MTU, 0);

		/* log result */
		if(numBytes > 0) {
			// Drop message and log event 
			pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
						"Successfully received a packet from the server");
		} else if(numBytes==0) {
			/* The connection have been closed by the distant peer
			 * The client need to close and restart after a given time */
			pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
						"Server closed connection? Restarting..");
			if(restart_client(pcapReplay)) {
				pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__, 
							"Successfully restarted the server !");
				return;
			} else{
				deinstanciate(pcapReplay,sd);
				return;
			}
		} else {
			pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
					"Unable to receive message");
		}
	} else if(events & EPOLLOUT) {
		/* The kernel can accept data from us,
		 * and we care because we registered EPOLLOUT on sd with epoll */

		/* send the next pcap packet */
		numBytes = send_packet(pckt_to_send, sd);
	
		/* log result */
		if(numBytes>0) {
			pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
					"Successfully sent a '%d' (bytes) packet to the server.", numBytes);
		} else if(numBytes == 0) {
			/* The client doesn't recreate TCP control messages */
			pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
					"The last packet to send was an ACK. Skipped sending.", numBytes);
		} else {
			pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
					"Unable to send message");
		}
		/* tell epoll we care about reading and writing now */
		struct epoll_event ev;
		memset(&ev, 0, sizeof(struct epoll_event));
		ev.events = EPOLLIN|EPOLLOUT;
		ev.data.fd = sd;
		epoll_ctl(pcapReplay->ed, EPOLL_CTL_MOD, sd, &ev);

	} else if(events & EPOLLIN) {
		/* There is data available to read from the kernel,
		 * and we care because we registered EPOLLIN on sd with epoll */

		/* prepare to accept the message */
		memset(receivedPacket, 0, (size_t)MTU);
		/* receive the packet */
		numBytes = recv(sd, receivedPacket, (size_t)MTU, 0);

		/* log result */
		if(numBytes > 0) {
			pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
						"Successfully received a packet from server.", numBytes);
		} else if(numBytes==0) {
			/* The connection have been closed by the distant peer.
			 * The client need to close the connection and restart (or quit because of timeout */
			pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
						"Server closed connection? Restarting..");
			if(restart_client(pcapReplay)) {
				pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__, 
							"Successfully restarted the client !");
				return;
			} else{
				deinstanciate(pcapReplay,sd);
				return;
			}
		} else{
			pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
						"Unable to receive message");
		}
	}

	free(pckt_to_send);
	/* Sleep waitingTime before reentering in this function. */
	pcapReplay->slogf(G_LOG_LEVEL_DEBUG, __FUNCTION__, 
				"The client will now sleep for %d.%.9ld second(s)",timeToWait.tv_sec,timeToWait.tv_nsec);	
	nanosleep((const struct timespec*)&timeToWait,NULL); 

	/*  If the timeout is reached, close the plugin ! */
	GDateTime* dt = g_date_time_new_now_local();
	if(g_date_time_to_unix(dt) >= pcapReplay->timeout) {
		/* tell epoll we no longer want to watch this socket */
		pcapReplay->slogf(G_LOG_LEVEL_INFO, __FUNCTION__,  "Timeout reached!");
		deinstanciate(pcapReplay,sd);
	}
}

/* pcap_activateServer() is called when the epoll descriptor has an event for the server */
void _pcap_activateServer(Pcap_Replay* pcapReplay, gint sd, uint32_t events) {
	pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__, "Activate server !");

	/* Function vars */
	struct timespec timeToWait;
	ssize_t numBytes = 0;
	char receivedPacket[MTU];
	struct epoll_event ev;

	/* Save a pointer to the next packet to send */
	Custom_Packet_t* pckt_to_send = pcapReplay->nextPacket;

	/* When the server has changed its pcap file (restart()),
	 * it needs to wait for the client to send the first packet.
	 * This keeps the exchange of packets synchronized. */
	if((events & EPOLLIN) || ((events & EPOLLIN) && (events & EPOLLOUT))) {
		if(pcapReplay->isAllowedToSend==FALSE) {
			memset(receivedPacket, 0, (size_t)MTU);
			while( (numBytes = recv(sd, receivedPacket, (size_t)MTU, 0)) >= 0) {
				pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__, "Receiving pending packet to empty serve buffer !");
				memset(receivedPacket, 0, (size_t)MTU);
			}
			pcapReplay->isAllowedToSend=TRUE;
			return;
		}
		
	} else{
		if(pcapReplay->isFirstPacketReceived==FALSE) {
			timeToWait.tv_sec=0;
			timeToWait.tv_nsec=500000000; // sleep 0.5 sec
			pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__, "Client still not connected after changing pcap. Wait 0.5sec !");
			nanosleep((const struct timespec*)&timeToWait,NULL); 
			return;
		}
	}

	if(!get_next_packet(pcapReplay)) {
		/* No more packet to send ! */
		pcapReplay->slogf(G_LOG_LEVEL_DEBUG, __FUNCTION__, "Send last pcap of the current file : open next pcap file to send !");
		/* Send last packet and restart */
		send_packet(pckt_to_send,sd);
		free(pckt_to_send);
		/* Restart server (after 10secs (new bind)) */
		if(restart_server(pcapReplay)) {
			pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__, "Successfully restarted the server !");
			return;
		} else{
			deinstanciate(pcapReplay,sd);
			return;
		}	
	}

	/* Compute waiting time */
	timeval_subtract (&timeToWait, &pckt_to_send->timestamp,&pcapReplay->nextPacket->timestamp);
	
	/* LOG event */
	if(events & EPOLLOUT) {
		pcapReplay->slogf(G_LOG_LEVEL_DEBUG, __FUNCTION__, "Server EPOLLOUT is set");
	}
	if(events & EPOLLIN) {
		pcapReplay->slogf(G_LOG_LEVEL_DEBUG, __FUNCTION__, "Server EPOLLIN is set");
	}
	if((events & EPOLLIN) && (events & EPOLLOUT)) {
		pcapReplay->slogf(G_LOG_LEVEL_DEBUG, __FUNCTION__, "Server EPOLLIN & EPOLLOUT are set");
	}

	/* Process events */
	if(sd == pcapReplay->server.sd) {
		/* data on a listening socket means a new client connection */
		assert(events & EPOLLIN);

		/* accept new connection from a remote client */
		struct sockaddr_in clientaddr;
    	socklen_t clientaddr_size = sizeof(clientaddr);
		int newClientSD = accept(sd,  (struct sockaddr *)&clientaddr, &clientaddr_size);

			
		int len=20;
		char ip_add[len];

		inet_ntop(AF_INET, &(clientaddr.sin_addr), ip_add, len);


		pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
						"Client connected on server with address : %s ", ip_add	);


		/* now register this new socket so we know when its ready */
		memset(&ev, 0, sizeof(struct epoll_event));
		ev.events = EPOLLIN|EPOLLOUT;
		ev.data.fd = newClientSD;
		epoll_ctl(pcapReplay->ed, EPOLL_CTL_ADD, newClientSD, &ev);

	} else {
		/* A client is communicating with us over an existing connection */
		if((events & EPOLLIN) && (events & EPOLLOUT)) {
			/* EPOLLIN && EPOLLOUT activated :
		 	 * The server have packets to send and receive */
			numBytes = send_packet(pckt_to_send, sd);

			/* log result */
			if(numBytes > 0) {
				pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
						"Successfully sent a '%d' (bytes) packet to the client", numBytes);
			} else if(numBytes == 0) {
			/* The client doesn't recreate TCP ACK messages */
			pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
						"The last packet to send was an ACK. Skipped sending.", numBytes);
			} else {
				pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
							"Unable to send message");
			}

			/*  Prepare to receive message */
			memset(receivedPacket, 0, (size_t)MTU);
			numBytes = recv(sd, receivedPacket, (size_t)MTU, 0);
			pcapReplay->isFirstPacketReceived=TRUE;
			/* log result */
			if(numBytes > 0) {
				pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
							"Successfully received a packet from the client", numBytes);
			} else if(numBytes == 0) {
				/* Client closed the remote connection
				 * Restart the server & wait for a new connection */
				pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
						"Client closed connection? Restarting..");
				if(restart_server(pcapReplay)) {
					pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__, 
								"Successfully restarted the server !");
					return;
				} else{
					deinstanciate(pcapReplay,sd);
					return;
				}
			} else {
				pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
						"Unable to receive message");
			}
		/* End EPOLLIN & EPOLLOUT */
		} else if(events & EPOLLIN) {
			/* A message is ready to be received */
			memset(receivedPacket, 0, (size_t)MTU);
			numBytes = recv(sd, receivedPacket, (size_t)MTU, 0);

			/* log result */
			if(numBytes > 0) {
				pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
						"Successfully received a message for the client", numBytes);
			} else if(numBytes == 0) {
				/* Client closed the remote connection
				 * Restart the server & wait for a new connection */
				if(restart_server(pcapReplay)) {
					pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__, "Successfully restarted the server !");
					return;
				} else{
					deinstanciate(pcapReplay,sd);
					return;
				}
			} else {
				pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
						"Unable to receive message");
			}
			pcapReplay->isFirstPacketReceived=TRUE;
			/* tell epoll we want to read/write now */
			memset(&ev, 0, sizeof(struct epoll_event));
			ev.events = EPOLLIN|EPOLLOUT;
			ev.data.fd = sd;
			epoll_ctl(pcapReplay->ed, EPOLL_CTL_MOD, sd, &ev);
		/* End EPOLLIN */
		} else if(events & EPOLLOUT) {
			/* The server can send packet to the client */
			/* send the pcap packet */
			numBytes = send_packet(pckt_to_send, sd);

			/* log result */
			if(numBytes >= 0) {
				pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
							"Successfully sent a '%d' (bytes) packet to the client", numBytes);
			} else if(numBytes == 0) {
				/* The client doesn't recreate TCP control messages */
				pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
							"The last packet to send was an ACK. Skipped sending.", numBytes);
			} else {
				pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
							"Unable to send message");
			}
		} /* End EPOLLOUT */
	} /* End process events */

	free(pckt_to_send);
	/* Sleep timetowait */
	pcapReplay->slogf(G_LOG_LEVEL_INFO, __FUNCTION__, 
				"The server is gonna sleep before sending the next packet : Sleep time : %d.%.9ld",timeToWait.tv_sec,timeToWait.tv_nsec);
	nanosleep((const struct timespec*)&timeToWait,NULL); 

	/* If timeout expired, close connection and exit plugin */
	GDateTime* dt = g_date_time_new_now_local();
	if(g_date_time_to_unix(dt) >= pcapReplay->timeout) {
		/* tell epoll we want to write the response now */
		pcapReplay->slogf(G_LOG_LEVEL_INFO, __FUNCTION__,  "Timeout reached!");
		deinstanciate(pcapReplay,sd);
	}
}

gboolean pcap_StartClient(Pcap_Replay* pcapReplay) {
	g_assert(pcapReplay && (pcapReplay->magic == MAGIC));

	/* use epoll to asynchronously watch events for all of our sockets */
	if(pcapReplay->isRestarting==FALSE) {
		pcapReplay->ed = epoll_create(1);
		if(pcapReplay->ed == -1) {
			pcapReplay->slogf(G_LOG_LEVEL_CRITICAL, __FUNCTION__, 
						"Error in main epoll_create");
			close(pcapReplay->ed);
			return FALSE;
		}
	}

	/* create the client socket and get a socket descriptor */
	pcapReplay->client.sd = socket(AF_INET, (SOCK_STREAM | SOCK_NONBLOCK), 0);
	if(pcapReplay->client.sd == -1) {
		pcapReplay->slogf(G_LOG_LEVEL_ERROR, __FUNCTION__,
					"Unable to start control socket: error in socket");
		return FALSE;
	}

	/* Set TCP_NODELAY option to avoid Nagle algo */
	int optval = 1;
	setsockopt(pcapReplay->server.sd, IPPROTO_TCP, TCP_NODELAY, (char *) &optval, sizeof(optval));
	if(pcapReplay->client.sd == -1){
		pcapReplay->slogf(G_LOG_LEVEL_ERROR, __FUNCTION__,
					"Unable to set options to the socket !");
		return FALSE;
	}
	/* get the server ip address */
	if(g_ascii_strncasecmp(pcapReplay->serverHostName->str, "localhost", 9) == 0) {
		pcapReplay->serverIP = htonl(INADDR_LOOPBACK);
	} else {
		struct addrinfo* info;
		int ret = getaddrinfo(pcapReplay->serverHostName->str, NULL, NULL, &info);
		if(ret < 0) {
			pcapReplay->slogf(G_LOG_LEVEL_ERROR, __FUNCTION__,
					"Unable to getaddrinfo() on hostname \"%s\"", pcapReplay->serverHostName->str);
			return FALSE;
		}

		pcapReplay->serverIP = ((struct sockaddr_in*)(info->ai_addr))->sin_addr.s_addr;
		freeaddrinfo(info);
	}

	/* our client socket address information for connecting to the server */
	struct sockaddr_in serverAddress;
	memset(&serverAddress, 0, sizeof(serverAddress));
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.s_addr = pcapReplay->serverIP;
	
	int new_port = pcapReplay->serverPortInt + pcapReplay->nmb_conn;
	pcapReplay->serverPort = (in_port_t) htons(new_port) ;
	serverAddress.sin_port =pcapReplay->serverPort;
	pcapReplay->nmb_conn = pcapReplay->nmb_conn+1;

	/* connect to server. since we are non-blocking, we expect this to return EINPROGRESS */
	gint res = connect(pcapReplay->client.sd, (struct sockaddr *) &serverAddress, sizeof(serverAddress));
	if (res == -1 && errno != EINPROGRESS) {
		pcapReplay->slogf(G_LOG_LEVEL_ERROR, __FUNCTION__,
					"Unable to start control socket: error in connect");
		return FALSE;
	}

	/* specify the events to watch for on this socket.
	 * to start out, the client wants to know when it can send a message. */
	_pcap_client_epoll(pcapReplay, EPOLL_CTL_ADD, EPOLLOUT);

	return TRUE;
}

gboolean pcap_StartClientTor(Pcap_Replay* pcapReplay) {
	g_assert(pcapReplay && (pcapReplay->magic == MAGIC));

	/* use epoll to asynchronously watch events for all of our sockets */
	if(pcapReplay->isRestarting==FALSE) {
		pcapReplay->ed = epoll_create(1);
		if(pcapReplay->ed == -1) {
			pcapReplay->slogf(G_LOG_LEVEL_CRITICAL, __FUNCTION__, "Error in main epoll_create");
			close(pcapReplay->ed);
			return FALSE;
		}
	}
	
	/* create the client socket and get a socket descriptor */
	pcapReplay->client.sd = socket(AF_INET, SOCK_STREAM, 0);
	if(pcapReplay->client.sd == -1) {
		pcapReplay->slogf(G_LOG_LEVEL_ERROR, __FUNCTION__,
				"unable to start control socket: error in socket");
		return FALSE;
	}

	/* Set TCP_NODELAY option to avoid Nagle algo */
	int optval = 1;
	setsockopt(pcapReplay->client.sd, IPPROTO_TCP, TCP_NODELAY, (char *) &optval, sizeof(optval));
	if(pcapReplay->server.sd == -1){
		pcapReplay->slogf(G_LOG_LEVEL_ERROR, __FUNCTION__,
					"Unable to set options to the socket !");
		return FALSE;
	}

	/* our client socket address information for connecting to the Tor proxy */
	struct sockaddr_in proxyAddress;
	memset(&proxyAddress, 0, sizeof(proxyAddress));
	proxyAddress.sin_family = AF_INET;
	proxyAddress.sin_addr.s_addr = pcapReplay->proxyIP;
	proxyAddress.sin_port = pcapReplay->proxyPort;

	pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
				"Trying to connect to Tor socket (port 9000)");
	/* connect to server. since we are non-blocking, we expect this to return EINPROGRESS */
	gint res = connect(pcapReplay->client.sd, (struct sockaddr *) &proxyAddress, sizeof(proxyAddress));
	if (res == -1 && errno != EINPROGRESS) {
		pcapReplay->slogf(G_LOG_LEVEL_ERROR, __FUNCTION__,
				"unable to start control socket: error in connect");
		return FALSE;
	}	
	pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
				"Connected to Tor socket port 9000 !");

	/* Initiate the connection to the Tor proxy.
	 * The client needs to do the Socks5 handshake to communicate
	 * through Tor using the proxy */
	initiate_conn_to_proxy(pcapReplay);

	/* specify the events to watch for on this socket.
	 * to start out, the client wants to know when it can send a message. */
	_pcap_client_epoll(pcapReplay, EPOLL_CTL_ADD, EPOLLOUT);

	return TRUE;
}

gboolean pcap_StartServer(Pcap_Replay* pcapReplay) {
	g_assert(pcapReplay && (pcapReplay->magic == MAGIC));

	/* use epoll to asynchronously watch events for all of our sockets */
	if(pcapReplay->isRestarting==FALSE) {
		/* Don't close&restart epoll descriptor when isRestarting!*/
		pcapReplay->ed = epoll_create(1);
		if(pcapReplay->ed == -1) {
			pcapReplay->slogf(G_LOG_LEVEL_CRITICAL, __FUNCTION__, "Error in main epoll_create");
			close(pcapReplay->ed);
			return FALSE;
		}
	}

	/* Create the server socket and get a socket descriptor */
	pcapReplay->server.sd = socket(AF_INET, (SOCK_STREAM | SOCK_NONBLOCK), 0);
	if(pcapReplay->server.sd == -1) {
		pcapReplay->slogf(G_LOG_LEVEL_ERROR, __FUNCTION__,
					"Unable to start control socket: error in socket");
		return FALSE;
	}

	/* Set TCP_NODELAY option to avoid Nagle algo */
	int optval = 1;
	setsockopt(pcapReplay->server.sd, IPPROTO_TCP, TCP_NODELAY, (char *) &optval, sizeof(optval));
	if(pcapReplay->server.sd == -1){
		pcapReplay->slogf(G_LOG_LEVEL_ERROR, __FUNCTION__,
					"Unable to set options to the socket !");
		return FALSE;
	}

	// if(setsockopt(pcapReplay->server.sd, IPPROTO_TCP, TCP_NODELAY, (char *) &optval, sizeof(optval)) == -1){
		// pcapReplay->slogf(G_LOG_LEVEL_ERROR, __FUNCTION__,
		// 			"Unable to set options to the socket !");
		// return FALSE;
	// }

	/* Setup the socket address info, client has outgoing connection to server */
	struct sockaddr_in bindAddress;
	memset(&bindAddress, 0, sizeof(bindAddress));
	bindAddress.sin_family = AF_INET;
	bindAddress.sin_addr.s_addr = INADDR_ANY;
	int new_port = pcapReplay->serverPortInt + pcapReplay->nmb_conn;

	/* Since the server cannot rebind instantly after restarting,
	 * we increment the port number by one. The client does the same thing 
	 * NOW : The client/server doesn't deconnect themselves anymore. Only one startServer now*/
	pcapReplay->serverPort = htons(new_port) ;
	bindAddress.sin_port =pcapReplay->serverPort;
	pcapReplay->nmb_conn = pcapReplay->nmb_conn+1;

	/* Bind the socket to the server port */
	gint res = bind(pcapReplay->server.sd, (struct sockaddr *) &bindAddress, sizeof(bindAddress));
	if (res == -1) {
		pcapReplay->slogf(G_LOG_LEVEL_CRITICAL, __FUNCTION__,
				"unable to start server: error in bind");
		return FALSE;
	}

	/* set as server socket that will listen for clients */
	res = listen(pcapReplay->server.sd, 100);
	if (res == -1) {
		pcapReplay->slogf(G_LOG_LEVEL_CRITICAL, __FUNCTION__,
				"unable to start server: error in listen");
		return FALSE;
	}

	/* Specify the events to watch for on this socket.
	 * To start out, the server wants to know when a client is connecting. */
	_pcap_server_epoll(pcapReplay, EPOLL_CTL_ADD, EPOLLIN);

	return TRUE;
}

/* The pcap_replay_new() function creates a new instance of the pcap replayer plugin 
 * The instance can either be a server waiting for a client or a client connecting to the pcap server. */
Pcap_Replay* pcap_replay_new(gint argc, gchar* argv[], PcapReplayLogFunc slogf) {
	g_assert(slogf);
	gboolean is_instanciation_done = FALSE; 
	gint arg_idx = 1;

	Pcap_Replay* pcapReplay = g_new0(Pcap_Replay, 1);

	pcapReplay->magic = MAGIC;
	pcapReplay->slogf = slogf;
	pcapReplay->slogf(G_LOG_LEVEL_CRITICAL, __FUNCTION__,
					"Creating a new instance of the pcap replayer plugin:");
	

	const GString* nodeType = g_string_new(argv[arg_idx++]); // client or server ?
	const GString* client_str = g_string_new("client");
	const GString* clientTor_str = g_string_new("client-tor");
	const GString* server_str = g_string_new("server");

	if(g_string_equal(nodeType,clientTor_str)) {
		/* If tor client, get SocksPort */
		pcapReplay->proxyPort = htons(atoi(argv[arg_idx++]));
		/* use loopback addr to connect to the Socks Tor proxy */
		pcapReplay->proxyIP = htonl(INADDR_LOOPBACK); 

	}

	/* Get the remote server name & port */
	pcapReplay->serverHostName = g_string_new(argv[arg_idx++]);
	pcapReplay->serverPort = (in_port_t) htons((in_port_t)atoi(argv[arg_idx]));
	pcapReplay->serverPortInt = atoi(argv[arg_idx++]);
	pcapReplay->nmb_conn = 0;

	pcapReplay->isAllowedToSend = TRUE;
	pcapReplay->isFirstPacketReceived=TRUE;
	pcapReplay->isRestarting = FALSE;

	// Get client IP addr used in the pcap file
	if(inet_aton(argv[arg_idx++], &pcapReplay->client_IP_in_pcap) == 0) {
		pcapReplay->slogf(G_LOG_LEVEL_CRITICAL, __FUNCTION__,
					"Cannot get the client IP used in pcap file : Err in the arguments ");
		pcap_replay_free(pcapReplay);
		return NULL;
	}
	// Get client port used in pcap file
	pcapReplay->client_port_in_pcap = (gushort) atoi(argv[arg_idx++]);

	// Get server IP addr used in the pcap file
	if(inet_aton(argv[arg_idx++], &pcapReplay->server_IP_in_pcap) == 0) {
		pcapReplay->slogf(G_LOG_LEVEL_CRITICAL, __FUNCTION__,
					"Cannot get the server IP used in pcap file : Err in the arguments ");
		pcap_replay_free(pcapReplay);
		return NULL;
	}
	// Get server port used in pcap file
	pcapReplay->server_port_in_pcap = (gushort) atoi(argv[arg_idx++]);

	// Get the timeout of the experiment
	GDateTime* dt = g_date_time_new_now_local();
	pcapReplay->timeout = atoi(argv[arg_idx++]) + g_date_time_to_unix(dt);

	// Get pcap paths and then open the file using pcap_open()
	pcapReplay->nmb_pcap_file = argc-arg_idx;
	// We open all the pcap file here in order to know directly if there is an error ;)
	// The paths of the pcap file are stored in a queue as well as the pcap_t pointers.
	// Path & pcap_t pointers are stored in the same order !
	pcapReplay->pcapFilePathQueue = g_queue_new();
	pcapReplay->pcapStructQueue = g_queue_new();

	for(gint i=arg_idx; i < arg_idx+pcapReplay->nmb_pcap_file ;i++) {
		// Open the pcap file 
		pcap_t *pcap = NULL;
		char ebuf[PCAP_ERRBUF_SIZE];
		if ((pcap = pcap_open_offline(argv[i], ebuf)) == NULL) {
			pcapReplay->slogf(G_LOG_LEVEL_CRITICAL, __FUNCTION__,
					"Unable to open the pcap file :");
			return NULL;
		} else {
			pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
					"Pcap file opened (%s) ",argv[i]);
		}
		//Add the file paths & pcap_t pointer to the queues
		g_queue_push_tail(pcapReplay->pcapFilePathQueue, g_string_new(argv[i]));
		g_queue_push_tail(pcapReplay->pcapStructQueue, pcap);
	}

	// Attach the first pcap_t struct to the instance state
	// The pcap files are used in the order the appear in arguments
	pcapReplay->pcap = (pcap_t*) g_queue_peek_head(pcapReplay->pcapStructQueue);

	/* If the first argument is equal to "client" 
	 * Then create a new client instance of the  pcap replayer plugin */
	if(g_string_equal(nodeType,client_str)) {
		pcapReplay->isClient = TRUE;
		pcapReplay->isTorClient = FALSE;
		// Start the client (socket,connect)
		if(!pcap_StartClient(pcapReplay)) {
			pcap_replay_free(pcapReplay);
			return NULL;
		} 
	} 
	/* If the first argument is equal to "client-tor" 
	 * Then create a new tor client instance of the pcap replayer plugin */
	else if(g_string_equal(nodeType,clientTor_str)) {
		pcapReplay->isClient = TRUE;
		pcapReplay->isTorClient = TRUE;
		// Start the client (socket,connect)
		if(!pcap_StartClientTor(pcapReplay)) {
			pcap_replay_free(pcapReplay);
			return NULL;
		}
	} 
	/* If the first argument is equal to "server" 
	 * Then create a new server instance of the pcap replayer plugin */
	else if(g_string_equal(nodeType,server_str)) {
		pcapReplay->isClient = FALSE;
		// Start the server (socket, bind, listen)
		if(!pcap_StartServer(pcapReplay)) {
			pcap_replay_free(pcapReplay);
			return NULL;
		} 	
	} else{
		pcapReplay->slogf(G_LOG_LEVEL_CRITICAL, __FUNCTION__,
					"First argument is not equals to either 'client'|'client-tor|'server'. Exiting !");
		pcap_replay_free(pcapReplay);
		return NULL;
	}
	is_instanciation_done = TRUE;

	/* Get first the first pcap packet matching the IP:PORT received in argv 
	 * Example : 
	 * If in the pcap file the client have the IP:Port address 192.168.1.2:5555 
	 * and the server have the IP:Port address 192.168.1.3:80. 
	 * Then, if the plugin is instanciated as a client, the client needs to resend
	 * the packet with ip.source=192.168.1.2 & ip.destination=192.168.1.3 & port.dest=80
	 * to the remote server.
	 * On the contrary, if the plugin is instanciated as a server, the server needs to wait
	 * for a client connection. When the a client is connected, it starts to resend packets 
	 * with ip.source=192.168.1.3 & ip.dest=192.168.1.2 & port.dest=5555 */
	if(get_next_packet(pcapReplay)==FALSE) {
		// If there is no packet matching the IP.source & IP.dest & port.dest, then exits !
		is_instanciation_done=FALSE;
		pcap_replay_free(pcapReplay);
		pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
				"Cannot find one packet (in the pcap file) matching the IPs/Ports arguments ");
		return NULL;
	}

	// Free the Strings used for comparaison
	g_string_free((GString*)nodeType, TRUE);
	g_string_free((GString*)client_str, TRUE);
	g_string_free((GString*)server_str, TRUE);

	if(!is_instanciation_done) {
		//pcap_replay_free(pcapReplay);
		pcapReplay->slogf(G_LOG_LEVEL_CRITICAL, __FUNCTION__,
				"Cannot instanciate the pcap plugin ! Exiting plugin");
		pcap_replay_free(pcapReplay);
		return NULL;
	}
	/* Everything went OK !
	 * pcapReplay is now a client connected to the server
	 * Or a server waiting for a client connection */
	return pcapReplay;
}

void pcap_replay_ready(Pcap_Replay* pcapReplay) {
	g_assert(pcapReplay && (pcapReplay->magic == MAGIC));

	/* Collect the events that are ready 
	 * Then activate client or server with corresponding events (EPOLLIN &| EPOLLOUT)*/
	struct epoll_event epevs[100];
	gint nfds = epoll_wait(pcapReplay->ed, epevs, 100, 0);
	if(nfds == -1) {
		pcapReplay->slogf(G_LOG_LEVEL_CRITICAL, __FUNCTION__, "error in epoll_wait");
	} else {
		for(gint i = 0; i < nfds; i++) {
			gint d = epevs[i].data.fd;
			uint32_t e = epevs[i].events;
			if(d == pcapReplay->client.sd) {
				_pcap_activateClient(pcapReplay, d, e);
			} else {
				_pcap_activateServer(pcapReplay, d, e);
			}
		}
	}
}

void _pcap_client_epoll(Pcap_Replay* pcapReplay, gint operation, guint32 events) {
	g_assert(pcapReplay && (pcapReplay->magic == MAGIC));

	struct epoll_event ev;
	memset(&ev, 0, sizeof(struct epoll_event));
	ev.events = events;
	ev.data.fd = pcapReplay->client.sd;

	gint res = epoll_ctl(pcapReplay->ed, operation, pcapReplay->client.sd, &ev);
	if(res == -1) {
		pcapReplay->slogf(G_LOG_LEVEL_ERROR, __FUNCTION__, "error in client_epoll_ctl");
	}
}

void _pcap_server_epoll(Pcap_Replay* pcapReplay, gint operation, guint32 events) {
	g_assert(pcapReplay && (pcapReplay->magic == MAGIC));

	struct epoll_event ev;
	memset(&ev, 0, sizeof(struct epoll_event));
	ev.events = events;
	ev.data.fd = pcapReplay->server.sd;

	gint res = epoll_ctl(pcapReplay->ed, operation, pcapReplay->server.sd, &ev);
	if(res == -1) {
		pcapReplay->slogf(G_LOG_LEVEL_ERROR, __FUNCTION__, "error in server_epoll_ctl");
	}
}

gint pcap_replay_getEpollDescriptor(Pcap_Replay* pcapReplay) {
	g_assert(pcapReplay && (pcapReplay->magic == MAGIC));
	return pcapReplay->ed;
}

gboolean pcap_replay_isDone(Pcap_Replay* pcapReplay) {
	g_assert(pcapReplay && (pcapReplay->magic == MAGIC));
	return pcapReplay->isDone;
}

void pcap_replay_free(Pcap_Replay* pcapReplay) {
	g_assert(pcapReplay && (pcapReplay->magic == MAGIC));

	if(pcapReplay->ed) {
		close(pcapReplay->ed);
	}
	if(pcapReplay->client.sd) {
		close(pcapReplay->client.sd);
	}
	if(pcapReplay->server.sd) {
		close(pcapReplay->server.sd);
	}
	if(pcapReplay->serverHostName) {
		g_string_free(pcapReplay->serverHostName, TRUE);
	}
	while(!g_queue_is_empty(pcapReplay->pcapStructQueue)) {
		pcap_t * pcap = g_queue_pop_head(pcapReplay->pcapStructQueue);
		if(pcap) {
			pcap_close(pcap);
		}
	}
	while(!g_queue_is_empty(pcapReplay->pcapFilePathQueue)) {
		GString * s = g_queue_pop_head(pcapReplay->pcapFilePathQueue);
		if(s) {
			g_string_free(s,TRUE);
		}
	}
	g_queue_free(pcapReplay->pcapStructQueue);
	g_queue_free(pcapReplay->pcapFilePathQueue);
	pcapReplay->magic = 0;
	g_free(pcapReplay);
}

gboolean get_next_packet(Pcap_Replay* pcapReplay) {
	/* Get first the first pcap packet matching the IP:PORT received in argv 
	 * Example : 
	 * If in the pcap file the client have the IP:Port address 192.168.1.2:5555 
	 * and the server have the IP:Port address 192.168.1.3:80. 
	 * Then, if the plugin is instanciated as a client, the client needs to resend
	 * the packet with ip.source=192.168.1.2 & ip.destination=192.168.1.3 & port.dest=80
	 * to the remote server.
	 * On the contrary, if the plugin is instanciated as a server, the server needs to wait
	 * for a client connection. When the a client is connected, it starts to resend packets 
	 * with ip.source=192.168.1.3 & ip.dest=192.168.1.2 & port.dest=5555 */

	struct pcap_pkthdr *header;
	const u_char *pkt_data;
	int size = 0;
	gboolean exists = FALSE;

	//tcp info
	const struct sniff_ethernet *ethernet; /* The ethernet header */
	const struct sniff_ip *ip; /* The IP header */
	const struct sniff_tcp *tcp; /* The TCP header */
	u_int size_ip;
	u_int size_tcp;



	while((size = pcap_next_ex(pcapReplay->pcap, &header, &pkt_data)) >= 0) {
		// There exists a next packet in the pcap file
		// Retrieve header information
		ethernet = (struct sniff_ethernet*)(pkt_data);
		ip = (struct sniff_ip*)(pkt_data + SIZE_ETHERNET);
		size_ip = IP_HL(ip)*4;
		tcp = (struct sniff_tcp*)(pkt_data + SIZE_ETHERNET + size_ip);
		size_tcp = TH_OFF(tcp)*4;
		char* payload = (char *)(pkt_data + SIZE_ETHERNET + size_ip + size_tcp);
		int size_payload = ntohs(ip->ip_len) - (size_ip + size_tcp);

		// Client scenario
		if(pcapReplay->isClient) {
			// check if this packet srcIP,destIP,destPort correspond to the arguments
			if(pcapReplay->client_IP_in_pcap.s_addr == ip->ip_src.s_addr) {
				if(pcapReplay->server_IP_in_pcap.s_addr == ip->ip_dst.s_addr) {
					//if((ntohs(tcp->th_dport) == pcapReplay->server_port_in_pcap)) {
						pcapReplay->nextPacket = g_new0(Custom_Packet_t, 1);
						pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__, "Found at least one matching packet in next_packet");
						exists = TRUE;
						pcapReplay->nextPacket->timestamp.tv_sec = header->ts.tv_sec;
						pcapReplay->nextPacket->timestamp.tv_usec = header->ts.tv_usec;
						pcapReplay->nextPacket->payload_size = size_payload;
						pcapReplay->nextPacket->payload = payload;
						break;
					//}
				}
			}	
		} 
		// Server scenario 
		else {
			if(pcapReplay->server_IP_in_pcap.s_addr == ip->ip_src.s_addr) {
				if(pcapReplay->client_IP_in_pcap.s_addr == ip->ip_dst.s_addr) {
					//if((ntohs(tcp->th_dport) == pcapReplay->client_port_in_pcap)) {
						pcapReplay->nextPacket = g_new0(Custom_Packet_t, 1);
						pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__, "Found at least one matching packet in next_packet");
						exists = TRUE;
						pcapReplay->nextPacket->timestamp.tv_sec = header->ts.tv_sec;
						pcapReplay->nextPacket->timestamp.tv_usec = header->ts.tv_usec;
						pcapReplay->nextPacket->payload_size = size_payload;
						pcapReplay->nextPacket->payload = payload;
						break;
					//}
				}
			}		
		}
	} 
	return exists;
}

void deinstanciate(Pcap_Replay* pcapReplay, gint sd) {
	epoll_ctl(pcapReplay->ed, EPOLL_CTL_DEL, sd, NULL);
	close(sd);
	pcapReplay->client.sd = 0;
	pcapReplay->isDone = TRUE;
	pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
					"Plugin deinstanciated, exiting plugin !");
}

int timeval_subtract (struct timespec *result, struct timeval *y, struct timeval *x)
{
	/* Perform the carry for the later subtraction by updating y. */
	if (x->tv_usec < y->tv_usec) {
		int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
		y->tv_usec -= 1000000 * nsec;
		y->tv_sec += nsec;
	}
	if (x->tv_usec - y->tv_usec > 1000000) {
		int nsec = (x->tv_usec - y->tv_usec) / 1000000;
		y->tv_usec += 1000000 * nsec;
		y->tv_sec -= nsec;																																																																		
	}

	/* Compute the time remaining to wait.
	 * tv_usec is certainly positive. */
	result->tv_sec = x->tv_sec - y->tv_sec;
	result->tv_nsec = (x->tv_usec - y->tv_usec)*1000;

	/* Return 1 if result is negative. */
	return x->tv_sec < y->tv_sec;
}

gboolean change_pcap_file_to_send(Pcap_Replay* pcapReplay) {
	pcap_t * pcap = g_queue_pop_head(pcapReplay->pcapStructQueue);
	pcap_close(pcap);

	pcap= NULL;
	char ebuf[PCAP_ERRBUF_SIZE];
	GString * pcap_to_reset = g_queue_peek_head(pcapReplay->pcapFilePathQueue);

	if ((pcap = pcap_open_offline(pcap_to_reset->str, ebuf)) == NULL) {
		pcapReplay->slogf(G_LOG_LEVEL_CRITICAL, __FUNCTION__,
				"Unable to re-open the pcap file : %s",pcap_to_reset->str);
		return FALSE;
	} else{
		pcapReplay->slogf(G_LOG_LEVEL_CRITICAL, __FUNCTION__,
				"Successfully reset pcap file : %s",pcap_to_reset->str);

		g_queue_push_tail(pcapReplay->pcapFilePathQueue,  g_queue_pop_head(pcapReplay->pcapFilePathQueue));
		g_queue_push_tail(pcapReplay->pcapStructQueue, pcap);																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																																				

		pcapReplay->pcap =  g_queue_peek_head(pcapReplay->pcapStructQueue);
		return TRUE;
	}
}

gboolean restart_server(Pcap_Replay* pcapReplay) {
	/* The remote connection has been closed OR
	 * The server have finished sending the current pcap file.
	 * In these two cases, the server needs to restart and bind a new port. */

	/* Time to wait before the server restart sending the next pcap file */
	struct timespec ts; 
	ts.tv_sec=50;
	ts.tv_nsec=0;


	/* UNCOMMENT IF YOU WANT THE CONNECTION TO BE CLOSED 
	 * AND RESTARTED AFTER EACH PCAP FILE */

	/*
	// Finish the connection if not already done 
	shutdown(pcapReplay->server.sd,2);
	close(pcapReplay->server.sd);
	*/

	// renew pcap descriptor in use
	if(!change_pcap_file_to_send(pcapReplay)) {
		pcapReplay->slogf(G_LOG_LEVEL_CRITICAL, __FUNCTION__, "Cannot change pcap file to send ! Exiting");
		return FALSE;
	};
	// Load the first packet into the current state
	if(!get_next_packet(pcapReplay)) {
		pcapReplay->slogf(G_LOG_LEVEL_CRITICAL, __FUNCTION__, "Cannot find a matching packet in the pcap file ! Exiting");
		return FALSE;
	}
	// Sleep for ts seconds to make sure connection/port have been released
	nanosleep((const struct timespec*)&ts,NULL); 
	pcapReplay->isAllowedToSend = FALSE; // Need to wait for the first packet of the client !
	pcapReplay->isFirstPacketReceived=FALSE;

	/* UNCOMMENT IF YOU WANT THE CONNECTION TO BE CLOSED 
	 * AND RESTARTED AFTER SENDING EACH PCAP FILE */

	/*
	pcapReplay->isRestarting = TRUE;
	if(!pcap_StartServer(pcapReplay)) {
		pcapReplay->slogf(G_LOG_LEVEL_CRITICAL, __FUNCTION__,
				"Unable to restart the server ! Exiting ");
		return FALSE;
	} else{
		return TRUE;
	}
	*/

	return TRUE;

}

gboolean restart_client(Pcap_Replay* pcapReplay) {
	struct timespec timewait; // Time to wait before isRestarting
	timewait.tv_sec=60;
	timewait.tv_nsec=0;

	/* UNCOMMENT IF YOU WANT THE CONNECTION TO BE CLOSED 
	 * AND RESTARTED AFTER SENDING EACH PCAP FILE */

	/*
	// Finish the connection if not already done 
	shutdown(pcapReplay->client.sd,2);
	close(pcapReplay->client.sd);
	*/

	// renew pcap descriptor in use
	if(!change_pcap_file_to_send(pcapReplay)) {
		pcapReplay->slogf(G_LOG_LEVEL_CRITICAL, __FUNCTION__, "Cannot change pcap file to send ! Exiting");
		return FALSE;
	};

	if(!get_next_packet(pcapReplay)) {
		pcapReplay->slogf(G_LOG_LEVEL_CRITICAL, __FUNCTION__, "Cannot find a matching packet in the pcap file ! Exiting");
		return FALSE;
	}

	// Sleep for timeToWait before isRestarting the client 
	nanosleep((const struct timespec*)&timewait,NULL); 

	/* UNCOMMENT IF YOU WANT THE CONNECTION TO BE CLOSED 
	 * AND RESTARTED AFTER EACH PCAP FILE */
	/*

	pcapReplay->isRestarting = TRUE;
	if(pcapReplay->isTorClient==FALSE) {
		// The current instance is a normal client, then restart as a normal client
		if(!pcap_StartClient(pcapReplay)) {
			pcapReplay->slogf(G_LOG_LEVEL_CRITICAL, __FUNCTION__,
					"Unable to restart the client (re-connect to server failed) ! Exiting ");
			return FALSE;
		} else{
			return TRUE;
		}
	} else {
		// The current instance is a Tor client, then restart as a Tor client
		if(!pcap_StartClientTor(pcapReplay)) {
			pcapReplay->slogf(G_LOG_LEVEL_CRITICAL, __FUNCTION__,
					"Unable to restart the Tor client (re-connect to server failed) ! Exiting ");
			return FALSE;
		} else{
			return TRUE;
		}
	}
	*/

	return TRUE;
}

gboolean initiate_conn_to_proxy(Pcap_Replay* pcapReplay) {
	/* This function is used to connect to the Tor proxy.
	 * The client needs to respect the Socks5 protocol to create 
	 * a remote connection through Tor. The negociation between the client 
	 * and the proxy is done in 4 steps (described hereafter).
	 *
	 * - Note that this function is called right after the client connect() 
	 *   to the Tor proxy. Then the client is already abble to communicate with the proxy
	 * - This function will ask the Tor proxy to create a remote connection to the server.
	 *   When the negociation is over, the client will be able to send data to the proxy 
	 *   and the proxy will be in charge of "forwarding" the data to the remote server.
	 * - Also note that is a *VERY VERY BASIC* implementation of the Socks negociation protocol. 
	 *   This means that if something goes wrong when connecting to the proxy, the shadow experiment will fail!
	 *   See shd-tgen-transport.c for more information about the negociation protocol !
	 **/

	/* Step 1)
	* Send authentication (5,1,0) to Tor proxy (Socks V.5) */
	pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
				"Start Socks5 negociation protocol : Send Auth message to the Tor proxy.");

	gssize bytesSent = send_to_proxy(pcapReplay, "\x05\x01\x00", 3);
	g_assert(bytesSent == 3);
	pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
				"Step 1 : Finished : Authentication to Tor proxy succeeded.");


	/* Step 2) 
	 * socks choice client <-- server
	 *
	 * \x05 (version 5)
	 * \x00 (auth method choice - \xFF means none supported)
	 */
	gchar step2_buffer[8];
	memset(step2_buffer, 0, 8);
	gssize bytesReceived = recv_from_proxy(pcapReplay, step2_buffer, 2);
	g_assert(bytesReceived == 2);

	// Verify answer (2)
	if(step2_buffer[0] == 0x05 && step2_buffer[1] == 0x00) {
		pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
				"Step 2 : Finished : Socks choice supported by the proxy");
	} else {
		pcapReplay->slogf(G_LOG_LEVEL_ERROR, __FUNCTION__,
				"Step 2 : Failed :Socks choice unsupported by the proxy ! Exiting ");
		return FALSE;
	}

	/* Step 3)
	 * socks request sent in (1) client --> server
	 * \x05 (version 5)
	 * \x01 (tcp stream)
	 * \x00 (reserved) 
	 *
	 *	--> the client asks the server to connect to a remote server 
	 * 3a) ip address client --> server
	 *  \x01 (ipv4)
	 * in_addr_t (4 bytes)
	 * in_port_t (2 bytes)

	 * 3b) hostname client --> server
	 * \x03 (domain name)
	 * \x__ (1 byte name len)
	 * (name)
	 * in_port_t (2 bytes)
	 * 
	 * We use method 3a !
	 */

	struct addrinfo* info;
	int ret = getaddrinfo(pcapReplay->serverHostName->str, NULL, NULL, &info);
	if(ret >= 0) {
		pcapReplay->serverIP = ((struct sockaddr_in*)(info->ai_addr))->sin_addr.s_addr;
	}
	freeaddrinfo(info);

	/* case 3a - IPv4 */
	in_addr_t ip = pcapReplay->serverIP;
	int new_port = pcapReplay->serverPortInt + pcapReplay->nmb_conn;
	in_addr_t port = htons(new_port);
	pcapReplay->nmb_conn = pcapReplay->nmb_conn+1;

	gchar step3_buffer[16];
	memset(step3_buffer, 0, 16);

	g_memmove(&step3_buffer[0], "\x05\x01\x00\x01", 4);
	g_memmove(&step3_buffer[4], &ip, 4);
	g_memmove(&step3_buffer[8], &port, 2);

	bytesSent = send_to_proxy(pcapReplay, step3_buffer, 10);
	g_assert(bytesSent == 10);

	pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
				"Step 3 : Finished : Send TCP create connection to remote server to the proxy.");

	/*
	* Step 4 : socks response client <-- server
	* \x05 (version 5)
	* \x00 (request granted)
	* \x00 (reserved)
	*
	* --> the server can tell us that we need to reconnect elsewhere
	*
	* 4a) ip address client <-- server
	* \x01 (ipv4)
	* in_addr_t (4 bytes)
	* in_port_t (2 bytes)
	*
	* 4b hostname client <-- server
	* \x03 (domain name)
	* \x__ (1 byte name len)
	* (name)
	* in_port_t (2 bytes)
	*/

	gchar step4_buffer[256];
	memset(step4_buffer, 0, 256);
   	bytesReceived = recv_from_proxy(pcapReplay, step4_buffer, 256);
	g_assert(bytesReceived >= 4);

	if(step4_buffer[0] == 0x05 && step4_buffer[1] == 0x00) {
		// Request Granted by the proxy !
		pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
				"Step 4 : Finished : TCP connection to remote server created.");
 	} else {
		pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
				"Step 4 : Error : TCP connection to remote server cannot be created.");
		return FALSE;
	}
	return TRUE;
}


ssize_t send_packet(Custom_Packet_t* cp, gint sd) {
	// Send the payload of the custom packet through the socket sd
	char message[cp->payload_size];
	memset(message, 0, (size_t)cp->payload_size);
	snprintf(message,(size_t)cp->payload_size, "%s",(const char*) cp->payload);
	return send(sd, message, (size_t)cp->payload_size, 0);
}

gssize send_to_proxy(Pcap_Replay* pcapReplay, gpointer buffer, gsize length) {
	/* This function is used to send commands to the proxy 
	 * Used during the negociation phase */
	gssize bytes = write(pcapReplay->client.sd, buffer, length);

	if(bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
		pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
				"unable to send command to the Tor proxy ! Exiting");
		return -1;
	}
	if(bytes >= 0) {
		pcapReplay->slogf(G_LOG_LEVEL_DEBUG, __FUNCTION__,
				"Command sent to proxy : %s ",buffer);
	}
	return bytes;
}

gssize recv_from_proxy(Pcap_Replay* pcapReplay, gpointer buffer, gsize length) {
	/* This function is used to receive commands from the proxy 
	 * Used during the negociation phase */
	gssize bytes = read(pcapReplay->client.sd, buffer, length);

	if(bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
		pcapReplay->slogf(G_LOG_LEVEL_ERROR, __FUNCTION__,
				"unable to receive command from the Tor proxy ! Exiting");
	}
	if(bytes >= 0) {
		pcapReplay->slogf(G_LOG_LEVEL_MESSAGE, __FUNCTION__,
				"Command received from the proxy");
	}
	return bytes;
}
