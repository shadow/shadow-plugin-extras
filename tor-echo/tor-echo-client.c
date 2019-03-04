#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SOCKS_PORT 9000
#define MAX_BUF 1024


/* Simple function to print message and exit */
void die(char *msg)
{
	printf("%\n",  msg);
	exit(0);
}


/* Craft a SOCKS request for a particular hidden service */
void craft_request(char *request, char *onion)
{
	/* Per the SOCKS5 spec, we send:
	 *  \x05\x01\x00\x03(len(onion))(onion)\x00\x50
	 */

	request[0] = 0x05;
	request[1] = 0x01;
	request[2] = 0x00;
	request[3] = 0x03;
	request[4] = strlen(onion);
	
	int i = 0;
	for (i = 0; i < strlen(onion); i++) {
		request[5+i] = onion[i];
	}

	i = 5 + i;
	request[i] = 0x00;
	request[i+1] = 0x50;
	
	
}

int main(int argc, char *argv[]) {

	
	if (argc < 3) {
		die("Usage: ./tor-echo-client <onion> <msg>");	
	}

	char *onion = argv[1];	
	char *msg = argv[2];

	printf("[+] Connecting to SOCKS proxy %s\n", onion);

	// Client's SOCKS proxy IP/PORT
	char *servIP = "127.0.0.1";
	in_port_t servPort = SOCKS_PORT;
	
	int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0)
		die("Could not create socket\n");


	struct sockaddr_in servAddr;
	memset(&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = AF_INET;


	int rtnVal = inet_pton(AF_INET, servIP, &servAddr.sin_addr.s_addr);
	if (rtnVal == 0)
		die("inet_pton failed\n");

	servAddr.sin_port = htons(servPort);

	if (connect(sock, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0)
		die("connect() failed\n");

	char data[3];
	data[0] = 0x05;
	data[1] = 0x01;
	data[2] = 0x00;
	
	printf("[+] Sending SOCKS client handshake\n");
	ssize_t numBytes = send(sock, data, 3, 0);
	if (numBytes < 0)
		die("send() failed\n");

	unsigned int totalBytesRcvd = 0;
	int buffer[2];
	
	numBytes = recv(sock, buffer, 2, 0);
	if (numBytes < 0)
		die("recv() failed\n");

	printf("[+] SOCKS proxy response\n");

	char request[29];
	craft_request(request, onion);

	printf("[+] Sending Tor hiddenserver request\n");
	numBytes = send(sock, request, sizeof(request), 0);
	if (numBytes < 0)
		die("send() failed\n");

	totalBytesRcvd = 0;
	int buffer2[10];
	
	numBytes = recv(sock, buffer2, 12, 0);
	if (numBytes < 0)
		die("recv() failed\n");

	numBytes = send(sock, msg, strlen(msg), 0);
	if (numBytes < 0)
		die("send() failed\n");


	totalBytesRcvd = 0;
	char serverResponse[MAX_BUF];
	
	numBytes = recv(sock, serverResponse, MAX_BUF, 0);
	if (numBytes < 0)
		die("recv() failed\n");

	printf("[+] Server's response: %s\n", serverResponse);
	
	close(sock);
	exit(0);
	
}
