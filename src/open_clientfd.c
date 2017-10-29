/*
 * This code comes from example 11.18 of the textbook
 * It opens up a client side socket
 *
 * This function requires the following header files:
 * stdlib.h
 * stdio.h
 * string.h
 * sys/socket.h
 * sys/types.h
 * netdb.h
*/

int open_clientfd(char* hostname, char* port){

    int clientfd;
    struct addrinfo hints, *listp, *p;

    // Get a list of potential server addresses
    memset(&hints,0,sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_STREAM;        // open a connection
    hints.ai_flags = AI_NUMERICSERV;        // ... using a numeric port arg
    hints.ai_flags |= AI_ADDRCONFIG;        // recommended for connections
    getaddrinfo(hostname,port, &hints, &listp);

    // walk the list for ont that we can successfully connect to
    for(p = listp; p; p = ai_next){
        // create socket descriptor
        if((clientfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0){
            continue;   // socket failed, try the next
        }

        // connect to the server
        if(connect(clientfd, p->ai_addr, p->ai_addrlen) != -1){
            break;  // SUCCESS
        }

        close(clientfd);    // connection failed, try another
    }

    // clean up
    freeaddrinfo(listp);
    if(!p) return -1;   // all connects failed

    // OVERALL SUCCESS!! return the client file descriptor
    return clientfd;

}




