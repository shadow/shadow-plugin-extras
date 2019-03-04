/* Note: This example TCP server code was taken
 * from TCP/IP Sockets in C - Practical Guide
 * for Programmers (2nd edition) - Donahoo, 
 * Calvert.
 *
 * Some slight modifications have been made.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFSIZE 1024

static const int MAXPENDING = 5;

/* Simple function to print message and exit */
void die(char *str) {
	printf("%s", str);
	exit(0);
}

/* For each new TCP client, print what they send
 * and just send it back to them. */
void HandleTCPClient(int clntSocket)
{
	char buffer[BUFSIZE];
	
	ssize_t numBytesRcvd = recv(clntSocket, buffer, BUFSIZE, 0);
	if (numBytesRcvd < 0)
		die("recv() failed\n");
	

	while (numBytesRcvd > 0) {
		printf("Received: %s\n", buffer);
		ssize_t numBytesSent = send(clntSocket, buffer, numBytesRcvd, 0);
		if (numBytesSent < 0)
			die("send() failed\n");
		
		else if (numBytesSent != numBytesRcvd)
			die("send() sent unexpected num of bytes\n");

		numBytesRcvd = recv(clntSocket, buffer, BUFSIZE, 0);
		if (numBytesRcvd < 0)
			die("recv() failed\n");

	}

	memset(buffer, 0, BUFSIZE);
	close(clntSocket);

}


int main(int argc, char *argv[]) {
	if (argc != 2)
		die("Usage: ./tor-echo-server <port>\n");

	printf("[+] Starting tor-echo-server on port: %d\n", atoi(argv[1]));
	in_port_t servPort = atoi(argv[1]);

	int servSock;
	if ((servSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
		die("socket() failed\n");

	struct sockaddr_in servAddr;
	memset(&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servAddr.sin_port = htons(servPort);

	if (bind(servSock, (struct sockaddr*)&servAddr, sizeof(servAddr)) < 0)
		die("bind() failed\n");
	
	if (listen(servSock, MAXPENDING) < 0)
		die("listen() failed\n");
	
	for (;;) {
		struct sockaddr_in clntAddr;
		socklen_t clntAddrLen = sizeof(clntAddr);
			
		
		int clntSock = accept(servSock, (struct sockaddr *)&clntAddr, &clntAddrLen);
		if (clntSock < 0)
			die("accept() failed\n");
		
		char clntName[INET_ADDRSTRLEN];
		if (inet_ntop(AF_INET, &clntAddr.sin_addr.s_addr, clntName, sizeof(clntName)) != NULL) {
			printf("Handling client: %s:%d\n", clntName, ntohs(clntAddr.sin_port));
		} else {
			printf("Unable to get client address\n");
		}

		HandleTCPClient(clntSock);

	}
}
