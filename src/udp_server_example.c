/** 
 * Example *very* basic socket server (UDP)
 *
 **/

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>

// Set this to the maximum number of clients you want waiting to be serviced
#define LISTEN_QUEUE_SIZE	1024
#define BUFFER_MAX	1024

int create_server_socket(char* port, int protocol);

int main() {
	int sock = create_server_socket("8080", SOCK_DGRAM);
	char message[BUFFER_MAX];
	char client_hostname[NI_MAXHOST];
	char client_port[NI_MAXSERV];
	while (1) {
		struct sockaddr_storage client_addr;
		int msg_length;
		socklen_t client_addr_len = sizeof(client_addr);

		// Receive a message from a client
		if ((msg_length = recvfrom(sock, message, BUFFER_MAX, 0, (struct sockaddr*)&client_addr, &client_addr_len)) < 0) {
			fprintf(stderr, "Failed in recvfrom\n");
			continue;
		}

		// Get and print the address of the peer (for fun)
		int ret = getnameinfo((struct sockaddr*)&client_addr, client_addr_len, 
			client_hostname, BUFFER_MAX, client_port, BUFFER_MAX, 0);
		if (ret != 0) {
			fprintf(stderr, "Failed in getnameinfo: %s\n", gai_strerror(ret));
		}
		printf("Got a message from %s:%s\n", client_hostname, client_port);

		// Just echo the message back to the client
		sendto(sock, message, msg_length, 0, (struct sockaddr*)&client_addr, client_addr_len);
	}
	return 0;
}

int create_server_socket(char* port, int protocol) {
	int sock;
	int ret;
	int optval = 1;
	struct addrinfo hints;
	struct addrinfo* addr_ptr;
	struct addrinfo* addr_list;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = protocol;
	/* AI_PASSIVE for filtering out addresses on which we
	 * can't use for servers
	 *
	 * AI_ADDRCONFIG to filter out address types the system
	 * does not support
	 *
	 * AI_NUMERICSERV to indicate port parameter is a number
	 * and not a string
	 *
	 * */
	hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG | AI_NUMERICSERV;
	/*
	 *  On Linux binding to :: also binds to 0.0.0.0
	 *  Null is fine for TCP, but UDP needs both
	 *  See https://blog.powerdns.com/2012/10/08/on-binding-datagram-udp-sockets-to-the-any-addresses/
	 */
	ret = getaddrinfo(protocol == SOCK_DGRAM ? "::" : NULL, port, &hints, &addr_list);
	if (ret != 0) {
		fprintf(stderr, "Failed in getaddrinfo: %s\n", gai_strerror(ret));
		exit(EXIT_FAILURE);
	}
	
	for (addr_ptr = addr_list; addr_ptr != NULL; addr_ptr = addr_ptr->ai_next) {
		sock = socket(addr_ptr->ai_family, addr_ptr->ai_socktype, addr_ptr->ai_protocol);
		if (sock == -1) {
			perror("socket");
			continue;
		}

		// Allow us to quickly reuse the address if we shut down (avoiding timeout)
		ret = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
		if (ret == -1) {
			perror("setsockopt");
			close(sock);
			continue;
		}

		ret = bind(sock, addr_ptr->ai_addr, addr_ptr->ai_addrlen);
		if (ret == -1) {
			perror("bind");
			close(sock);
			continue;
		}
		break;
	}
	freeaddrinfo(addr_list);
	if (addr_ptr == NULL) {
		fprintf(stderr, "Failed to find a suitable address for binding\n");
		exit(EXIT_FAILURE);
	}

	if (protocol == SOCK_DGRAM) {
		return sock;
	}
	// Turn the socket into a listening socket if TCP
	ret = listen(sock, LISTEN_QUEUE_SIZE);
	if (ret == -1) {
		perror("listen");
		close(sock);
		exit(EXIT_FAILURE);
	}

	return sock;
}


