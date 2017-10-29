/** 
 * Example event-driven echo server
 * Using epoll() with edge triggering for client socket, level triggering for server socket
 *
 * Note that this code is meant for demonstration purposes only. Feel free to modify
 * it in any way that makes sense for your particular project.
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
#include <sys/epoll.h>
#include <sys/types.h>
#include <fcntl.h>

#define BUFFER_MAX	1024
/* Experiment with different values of MAX_EVENTS to determine best one */
#define MAX_EVENTS	100


/* Note that in this example, as compared with other examples,
 * we have dropped the _t and typedefs in our structs. This is
 * to give you some familiarity with POSIX coding standards, which
 * (unfortunately) reserve all _t identifiers */

/* this enumeration is used to keep track of what
 * state a client is in so we know how to handle it
 * when we get an event for it */
enum state {
	/* these values are a bit simplistic for the example
 	 * you may want more than these in your server */
	SENDING,
	RECEIVING,
	DISCONNECTED,
};

/* this struct just encapsulates all of the variables
 * required to properly manage a variable-sized buffer */
struct buffer {
	unsigned char* data;
	int length; /* current length of valid information in buffer */
	int max_length; /* the maximum possible length (currently) */
	int position; /* buffer location currently being looked at */
};

/* this struct associates a client socket descriptor with all of the 
 * other state we need to keep for each client. In event-driven
 * systems, we need to seperate functions from the data on which they 
 * operate. */
struct client {
	int fd; /* socket descriptor for client */
	enum state state; /* state enumeration */
	struct buffer send_buf; /* holds the data we're supposed to send to client */
	struct buffer recv_buf; /* holds the data we've received from the client */
	/* You'll probably want to add things to the client class.
	 * For example, a file descriptor, list of parsed requests, etc.*/
};

/* this struct associates a server socket descriptor with all of the
 * other state we need for the server. So far it's just one thing,
 * but we can easily extend it later if needed */
struct server {
	int fd; /* listening socket */
};

int send_responses(struct client* client);
int recv_requests(struct client* client);
int create_server_socket(char* port, int protocol);
int set_blocking(int sock, int blocking);
int send_data(struct client* client);
int recv_data(struct client* client);
struct client* get_new_client(int sock);
void free_client(struct client* client);

int main() {
	int n;
	int nfds;
	int epoll_fd;
	struct epoll_event ev;
	struct epoll_event events[MAX_EVENTS];

	/* start up listen socket on port 8080, TCP */	
	int listen_sock = create_server_socket("8080", SOCK_STREAM);
	/* create epoll file descriptor */
	if ((epoll_fd = epoll_create1(0)) == -1) {
		perror("epoll_create1");
		exit(EXIT_FAILURE);
	}

	/* make server socket nonblocking */
	set_blocking(listen_sock, 0);
	/* package the server FD up so we can use a pointer to it
 	 * in our event struct and add to it later if we need */
	struct server server = { .fd = listen_sock };

	ev.events = EPOLLIN; /* register incoming data event (level-triggered) with the listen FD */
	ev.data.ptr = (void*)&server; /* associate the server struct with this event */
	/* use epoll_ctl to add the listen sock to the epoller, and give it the events we want to listen
 	 * for and the data we want back when we get those events by passing &ev */
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_sock, &ev) == -1) {
		perror("epoll_ctl: listen_sock");
		exit(EXIT_FAILURE);
	}

	/* Main event loop start.  For a clean shutdown you should make this depent on a "running" booean */
	while (1) {
		/* Wait for events, indefinitely (-1 for last param means wait forever).
		 * Your server will want to change this timeout value to be able to return
		 * early from epoll_wait() and clean up idle connections */
		nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
		if (nfds == -1) {
			perror("epoll_wait");
			exit(EXIT_FAILURE);
		}

		/* if we got here, we have events to handle (if we set a timeout the timeout expires, then nfds is 0) */

		/* loop through all the events we got */
		for (n = 0; n < nfds; n++) {

			/* events[n].data.ptr is the same data we gave to epoll_ctl earlier with our ev struct */
			if (events[n].data.ptr == &server) { /* if this is an event for the server socket...*/
				/* Incoming event for server listening socket, meaning a new client connected */

				/* this function calls accept.  If you want this to be edge-triggered, you should call accept multiple times*/
				struct client* new_client = get_new_client(server.fd);
				if (new_client == NULL) continue;

				/* Add new client to epoll, waiting for input events, read hangups, and using edge-triggering */
				ev.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
				ev.data.ptr = (void*)new_client; /* associate pointer to new client with the event */
				/* add new client to epoller */
				if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_client->fd, &ev) == -1) {
					perror("epoll_ctl: new_client");
					exit(EXIT_FAILURE);
				}
				continue;
			}


			/* if we got this far, it means we have an event from a client */
			struct client* client = (struct client*)events[n].data.ptr;
			if (events[n].events & EPOLLIN) { /* input event happened */
				if (recv_requests(client) == 0) {
					/* if we got here we have left the receiving state.
					* register the client for EPOLLOUT now */
					ev.events = EPOLLOUT | EPOLLRDHUP | EPOLLET;
					ev.data.ptr = (void*)client;
					if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, client->fd, &ev) == -1) {
						perror("epoll_ctl: switching to output events");
						exit(EXIT_FAILURE);
					}
				}
			}
			if (events[n].events & EPOLLOUT) { /* output event happened (our send buffer is no longer full) */
				if (send_responses(client) == 0) {
					/* if we got here we have left the sending state
					 * register the client for EPOLLIN now */
					ev.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
					ev.data.ptr = (void*)client;
					if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, client->fd, &ev) == -1) {
						perror("epoll_ctl: switching to input events");
						exit(EXIT_FAILURE);
					}
				}
			}
			if (events[n].events & EPOLLRDHUP) { /* client shut down the writing side of its socket */
				if (client->state == RECEIVING) {
					/* only treat this as a full disconnect if it happened during receiving state */
					client->state = DISCONNECTED;
				}
			}
			if (client->state == DISCONNECTED) { /* cleanup client if its now in the disconnected state */
				if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client->fd, NULL) == -1) {
					perror("epoll_ctl: removing client");
					exit(EXIT_FAILURE);
				}
				printf("client disconnected\n");
				close(client->fd); /* technically this will also remove the client from epoll, so the CTL_DEL isn't necessary*/
				free_client(client);
				/* TODO: Note that this is the only place we're removing client connections and freeing them.
				 * You will want to implement a timeout as well to close and clean up idle clients */
			}
		}
	}
	return 0;
}

/* recv_requests performs a simple echo service to a connected client.
 * Your server should be much more advanced than this, reading all possible data,
 * parsing it into requests (you might receive more than one or peices
 * ofjust one). If you don't get at least one full request during
 * receiving, that client should remain in the receiving state. If
 * you have received at least one request, you should create response(s)
 * and enter the sending state.
 *
 * Returns 0 when there is no more reading to do 
 * Returns 1 when we still need more data to send a response
 * 	^--- this never happens, in this example, but may in
 * 	your server
 * */
int recv_requests(struct client* client) {
	/* drain all data available and put it in client recv buffer */
	if (recv_data(client) != 0) {
		/* if an unhandled error occurred, stop */
		client->state = DISCONNECTED;
		return 0;
	}

	if (client->recv_buf.length > 0) {
		printf("client buffer: %.*s", client->recv_buf.length, client->recv_buf.data);
	}

	/* We pretend anything we got is just one, whole message. 
	 * It's time to switch to sending */
	client->state = SENDING;

	/* create a send buffer for the client. We're just echoing
	 * what it sent to us */
	int response_length = client->recv_buf.length;
	client->send_buf.length = response_length;
	/* create space for the response, if it doesn't exist already */
	if (client->send_buf.max_length < response_length) {
		client->send_buf.data = realloc(client->send_buf.data, response_length);
		client->send_buf.max_length = response_length;
	}
	client->send_buf.position = 0; /* start sending at first byte */

	/* fill the send buffer with the data we want to send */
	memcpy(client->send_buf.data, client->recv_buf.data, response_length);
	return 0;
}

/* send_responses performs a simple echo service to a connected client.
 * Your server should be much more advanced than this, sending all responses
 * to the requests previously received, and then going back to the receieve
 * state
 *
 * Returns 0 when all responses have been sent
 * Returns 1 when there is still data to be sent
 * */
int send_responses(struct client* client) {
	/* send all the data we possibly can */
	if (send_data(client) == 1) {
		/* this means we weren't able to send everything.
		 * return 1 to indicate there's still more to send */
		return 1;
	}

	/* if we got here then the send buffer has all been sent
	 * let's go ahead and clear it. */
	client->send_buf.length = 0;
	client->send_buf.position = 0;
	/* notice we're just invalidating the current send buffer
	 * by setting length and position to 0. we're going to keep
	 * the same memory for efficiency purposes */

	/* Go back to recieve state */
	client->state = RECEIVING;

	/* reset the client's receive buffer, because we satisfied all
	 * the current requests */

	client->recv_buf.length = 0;
	client->recv_buf.position = 0;

	return 0;
}

/* get_new_client gets a new client connection using accept(),
 * makes the client socket nonblocking, and places that socket
 * descriptor into a freshly initialized client struct
 *
 * returns a pointer to the new client struct and NULL on error */
struct client* get_new_client(int sock) {
	struct sockaddr_storage addr;
	socklen_t addr_len = sizeof(struct sockaddr_storage);

	/* get a new client socket descriptor */
	int new_fd = accept(sock, (struct sockaddr*)&addr, &addr_len);
	if (new_fd == -1) {
		perror("accept");
		return NULL;
	}

	/* make the new socket descriptor nonblocking */
	set_blocking(new_fd, 0);

	/* create new client structure */
	struct client* client = (struct client*)calloc(1, sizeof(struct client));
	client->fd = new_fd;

	/* initialize client state to receiving */
	client->state = RECEIVING;
	
	/* the recv and send buffers for the client were all 
	 * zeroed out by the previouc calloc() */
	printf("got a connection\n");
	return client;
}

/* free_client frees a heap-allocated client structure
 * and corresponding buffer data, if present */
void free_client(struct client* client) {
	if (client->recv_buf.data) free(client->recv_buf.data);
	if (client->send_buf.data) free(client->send_buf.data);
	free(client);
	return;
}

/* recv_data receives all the data currently in the receieve buffer.
 * It returns when:
 * 	there is no data left
 * 	an error other than EINTR is returned by recv.
 * 	the client shuts down the writing half of its socket (or disconnects)
 *
 * 	returns 0 when there is no more data to read
 * 	returns 1 when an error occurred
 */
int recv_data(struct client* client) {
	char temp_buffer[BUFFER_MAX];
	int bytes_read;
	while (1) {
		bytes_read = recv(client->fd, temp_buffer, BUFFER_MAX, 0);
		if (bytes_read == -1) {
			if (errno == EWOULDBLOCK || errno == EAGAIN) {
				break; /* We've read all we can for the moment, stop */
			}
			else if (errno == EINTR) {
				/* continue upon interrupt
				 * this doesn't necessarily have to be what you do */
				continue;
			}
			else {
				/* some other error occurred, let's print it */
				perror("recv");
				return 1;
			}
		}
		else if (bytes_read == 0) {
			/* the client won't be sending us any more data (ever)
			 * but it may still be wanting us to send things back */
			break;
		}

		/* Realloc the recv buffer of the client if needed */
		int new_length = client->recv_buf.length + bytes_read;
		if (client->recv_buf.max_length < new_length) {
			/* since we just reached the max size of the buffer, let's double it 
			 * so we are less likely to run into this situation again */
			client->recv_buf.data = realloc(client->recv_buf.data, new_length * 2);
			client->recv_buf.max_length = new_length * 2;
		}

		/* now append the new data recieved to the client's recv buffer */
		memcpy(&(client->recv_buf.data)[client->recv_buf.length], temp_buffer, bytes_read);
		client->recv_buf.length += bytes_read;
	}
	return 0;
}

/* send_data attempts to send all of the data left in a client's
 * send buffer to the client socket. It starts at the position of
 * the send buffer and tries to send all remaining bytes from that
 * point. send_data returns when:
 * 
 * 	all data has been sent
 * 	an error other than EINTR occurs
 * 	the OS send buffer is full (EAGAIN/EWOULDBLOCK)
 *
 *  Returns 0 if all data in the send buffer was sent
 *  Returns 1 if there was still data left in the buffer */
int send_data(struct client* client) {
	char* buffer = (char*)client->send_buf.data;
	int bytes_sent;
	int bytes_left = client->send_buf.length - client->send_buf.position;
	while (bytes_left > 0) {
		bytes_sent = send(client->fd, &buffer[client->send_buf.position], bytes_left, 0);
		if (bytes_sent == -1) {
			if (errno == EWOULDBLOCK || errno == EAGAIN) {
				return 1; /* We've sent all we can for the moment */
			}
			else if (errno == EINTR) {
				continue; /* continue upon interrupt */
			}
			else if (errno == EPIPE || errno == ECONNRESET) {
				/* client disconnected, we can't send any more data */
				client->state = DISCONNECTED;
				return 1;
			}
			else {
				perror("send");
				client->state = DISCONNECTED;
				return 1;
			}
		}
		bytes_left -= bytes_sent;
		client->send_buf.position += bytes_sent;
	}
	return 0;
}

/* Create server socket establishes a server socket on
 * the given port for the given protocol. The resulting
 * socket is bound to all local addresses, both IPv4 and
 * IPv6, and has the REUSEADDR option set. If SOCK_STREAM
 * is specified,the socket is turned into a listening
 * socket.
 *
 * port can be any string representing a numeric port
 * protocol can be either SOCK_DGRAM or SOCK_STREAM
 *
 * returns a server socket descriptor on success
 * exits on failure
 * */
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
	ret = listen(sock, SOMAXCONN);
	if (ret == -1) {
		perror("listen");
		close(sock);
		exit(EXIT_FAILURE);
	}

	return sock;
}

/* set_blocking turns a socket into a blocking
 * or nonblocking socket, if it is not already
 * set to the indicated mode.
 *
 * returns 0 on success
 * exits on failure
 *
 * sock is a valid, open socket descriptor
 * blocking is either 0 or 1
 * 	0 for nonblocking
 * 	1 for blocking
 */
int set_blocking(int sock, int blocking) {
	int flags;
	/* Get flags for socket */
	if ((flags = fcntl(sock, F_GETFL)) == -1) {
		perror("fcntl get");
		exit(EXIT_FAILURE);
	}
	/* Only change flags if they're not what we want */
	if (blocking && (flags & O_NONBLOCK)) {
		if (fcntl(sock, F_SETFL, flags & ~O_NONBLOCK) == -1) {
			perror("fcntl set block");
			exit(EXIT_FAILURE);
		}
		return 0;
	}
	/* Only change flags if they're not what we want */
	if (!blocking && !(flags & O_NONBLOCK)) {
		if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
			perror("fcntl set nonblock");
			exit(EXIT_FAILURE);
		}
		return 0;
	}
	return 0;
}

