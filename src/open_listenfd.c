/*
 * This function is from example 11.19 in the text
 * It creates a server side socket ready to listen for and accept connection requests
 *
 * This function requires the following header files:
 * stdlib.h
 * stdio.h
 * string.h
 * sys/socket.h
 * sys/types/h
 * netdb.h
*/

int open_listenfd(char* port){
    
    struct addrinfo hints, *listp, *p;
    int listenfd, optval = 1;

    // Get a list of potential server addresses
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_STREAM;                // accept connections
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;    // ... on any IP address
    hints.ai_flags |= AI_NUMERICSERV;               // ... using any port number
    getaddrinfo(NULL,port, &hints, &listp);
    
    // walk the list for one that we can bind to
    for(p = listp; p; p = p->ai_next){
        // create a socket descriptor
        if((listenfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0){
            continue;   // the socket failed, try the next one
        }
        
        // eliminates "address already in use" error from bind
        setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));

        // bind the descriptor to the address
        if(bind(listenfd, p->ai_addr, p->ai_addrlen) == 0){
            break; // SUCCESS
        }
        close(listenfd);    // the bind failed, try the next
    }

    // cleanup
    freeaddrinfo(listp);
    if(!p) return -1;   // no address worked

    // make it a listening socket ready to accept connection requests
    if(listen(listenfd, LISTENQ) < 0){
        close(listenfd);
        return -1;  // failed to create a listening socket
    }
    
    // OVERALL SUCCESS! return the file descriptor
    return listenfd;
    
}




