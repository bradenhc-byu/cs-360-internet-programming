#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <signal.h>
#include <sys/wait.h>

#define TEMP_BUF_LEN	1024
#define BUFFER_MAX	1024

int connect_to_host(char* host, char* port);
int recv_comm(int socket, unsigned char* buffer, int length);
int recv_comm_until(int socket, unsigned char** buffer, char* needle);
int send_comm(int socket, unsigned char* buffer, int length);
void signal_handler(int signum);
void receiver(int sock);
void sender(int sock, char* username);

pid_t pid;

int main(int argc, char* argv[]) {

	struct sigaction sigact;
	memset(&sigact, 0, sizeof(sigact));
	sigact.sa_handler = signal_handler;
	sigaction(SIGINT, &sigact, NULL);

	int sock = connect_to_host(argv[1], argv[2]);
	receiver(sock);
	sender(sock, argv[3]);
	return 0;
}

void signal_handler(int signum) {
	int status;
	if (signum == SIGINT) {
		if (pid != 0) {
			kill(pid, SIGINT);
			waitpid(-1, &status, 0);
		}
	}
	_exit(EXIT_SUCCESS);
	return;
}

void receiver(int sock) {
	pid = fork();
	if (pid < 0) {
		fprintf(stderr, "Fork failed\n");
		exit(EXIT_FAILURE);
	}
	if (pid == 0) {
		close(STDIN_FILENO);
		unsigned char* recv_buffer;
		while (1) {
			recv_comm_until(sock, &recv_buffer, "\n");
			printf("%s", recv_buffer);
			free(recv_buffer);
			recv_buffer = 0;
		}
	}
	return;
}

void sender(int sock, char* name) {
	char send_buffer[BUFFER_MAX];
	char name_buffer[BUFFER_MAX];
	sprintf(name_buffer, "[%s]: ", name);
	while (1) {
		fgets(send_buffer, BUFFER_MAX, stdin);
		send_comm(sock, name_buffer, strlen(name_buffer));
		send_comm(sock, send_buffer, strlen(send_buffer)); // we're making a lot assumptions here
		memset(send_buffer, 0, BUFFER_MAX);
	}
	close(sock);
	return;
}

int connect_to_host(char* host, char* service) {
	int sock;
	int ret;
	struct addrinfo hints;
	struct addrinfo* addr_ptr;
	struct addrinfo* addr_list;

	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM; // Typically TCP
	hints.ai_family = AF_UNSPEC; // IP4 or IP6, we don't care
	ret = getaddrinfo(host, service, &hints, &addr_list);
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
		if (connect(sock, addr_ptr->ai_addr, addr_ptr->ai_addrlen) == -1) {
			perror("connect");
			close(sock);
			continue;
		}
		break;
	}
	if (addr_ptr == NULL) {
		fprintf(stderr, "Failed to find a suitable address for connection\n");
		exit(EXIT_FAILURE);
	}
	freeaddrinfo(addr_list);
	return sock;
}

int recv_comm(int socket, unsigned char* buffer, int length) {
	unsigned char* ptr;
	int bytes_left;
	int bytes_read;
	ptr = buffer;
	bytes_left = length;
	while (bytes_left) {
		bytes_read = recv(socket, ptr, bytes_left, 0);
		if (bytes_read < 0) {
			if (errno == EINTR) {
				continue; // continue upon interrupt
			}
			else {
				perror("recv");
			}
		}
		else if (bytes_read == 0) {
			return -1;
		}
		ptr += bytes_read;
		bytes_left -= bytes_read;
	}
	return 0;
}

int send_comm(int socket, unsigned char* buffer, int length) {
	unsigned char* ptr;
	int bytes_left;
	int bytes_sent;
	ptr = buffer;
	bytes_left = length;
	while (bytes_left) {
		bytes_sent = send(socket, ptr, bytes_left, 0);
		if (bytes_sent < 0) {
			if (errno == EINTR) {
				continue; // continue upon interrupt
			}
			else {
				perror("send");
			}
		}
		else if (bytes_sent == 0) {
			return -1;
		}
		ptr += bytes_sent;
		bytes_left -= bytes_sent;
	}
	return 0;
}

int recv_comm_until(int socket, unsigned char** buffer, char* needle) {
	int total_bytes_read;
	int bytes_read;
	int buffer_length = TEMP_BUF_LEN;
	char temp_buffer[TEMP_BUF_LEN];
	total_bytes_read = 0;
	*buffer = (unsigned char*)calloc(sizeof(unsigned char),buffer_length);
	while (strstr((char*)*buffer, needle) == NULL) {
		bytes_read = recv(socket, temp_buffer, TEMP_BUF_LEN, 0);
		if (bytes_read < 0) {
			if (errno == EINTR) {
				continue; // continue upon interrupt
			}
			else {
				perror("recv");
			}
		}
		else if (bytes_read == 0) {
			return -1;
		}

		if ((total_bytes_read + bytes_read) > buffer_length) {
			*buffer = (unsigned char*)realloc(*buffer, (total_bytes_read + bytes_read) * 2);
		}
		memcpy(&(*buffer)[total_bytes_read], temp_buffer, bytes_read);
		total_bytes_read += bytes_read;
	}
	return 0;
}

