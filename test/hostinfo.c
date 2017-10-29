#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/types.h>
#include <string.h>


#define BUFFER_SIZE 1024

int main(int argc, char* argv[]){

    struct addrinfo *p, *listp, hints;
    char buf_host[BUFFER_SIZE];
    char buf_service[BUFFER_SIZE];
    int rc, flags;

    if(argc != 2){
        fprintf(stderr, "usage: %s <domain name>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Get a list of addrinfo records
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;          // IPv4 only
    hints.ai_socktype = SOCK_STREAM;    // connections only
    if((rc = getaddrinfo(argv[1],NULL, &hints, &listp)) != 0){
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(rc));
        exit(EXIT_FAILURE);
    }

    // walk the list and display each IP address
    flags = NI_NUMERICHOST | NI_NUMERICSERV;     // display address string instead of domain name
    for(p = listp; p; p = p->ai_next){
        getnameinfo(p->ai_addr, p->ai_addrlen, buf_host, BUFFER_SIZE, buf_service, BUFFER_SIZE, flags);
        printf("%s => %s\n",buf_host,buf_service);
    }

    // clean up
    freeaddrinfo(listp);

    exit(EXIT_SUCCESS);
}
