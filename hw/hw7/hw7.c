#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>

// FUNCTION DEFINITIONS
int create_udp_socket(char* server, short port);

int main(int argc, char* argv[]){

    unsigned char* server;
    short port;
    int sockfd;
    
    if(argc != 3){
        fprintf(stderr, "usage: %s <server> <port>\n",argv[0]);
        exit(EXIT_FAILURE);
    }

    server = argv[1];
    port = (short)atoi(argv[2]);
    port = htons(port);

    sockfd = create_udp_socket(server,port);

    printf("Socket FD: %d\n",sockfd);

    close(sockfd);

    exit(EXIT_SUCCESS);

}


int create_udp_socket(char* server, short port) {
    struct sockaddr_in resolver_addr = {
        .sin_family = AF_INET, // Internet Address Family
        .sin_port = htons(53), // DNS port, converted to big endian if host is little endian
        .sin_addr = inet_addr(server) // Converting the dot notation (e.g., 8.8.8.8) to binary
    };
    // PF_INET for Internet Protocol family
    // SOCK_DGRAM for datagram socket type
    // IPPROTO_UDP for explicitly indicating UDP protocol
    int sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Note that connect() on a UDP socket isn't required since it's
    // a connectionless protocol. However, doing this will make future send()
    // write(), read(), and recv() calls use the address we "connected" to. 
    // That way you no longer use sendto() or recvfrom(). If this is
    // not useful for your purposes, remove the connect() call.
    if (connect(sock, (struct sockaddr*)&resolver_addr, sizeof(resolver_addr)) == -1) {
        perror("connect");
        exit(EXIT_FAILURE);
    }
    return sock;
}

