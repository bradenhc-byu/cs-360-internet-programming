/*
 * Function that allows us to connect to any host given the host name and the service
 * Protocol independent
 * Very similar to the example (11.18) provided in the book for this class - open_clientfd()
 *
 * must include the following header files:
 * stdlib.h
 * stdio.h
 * string.h
 * sys/socket.h
 * sys/type.h
 * netdb.h
 *
 * This function will return the socket address of the successful connection for the client
*/

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

