int create_dup_socket(char * server, short port){

    struct cockaddr_in resolver_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(53),
        .sin_addr = inet_addr(server)
    };
    int sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(sock = -1){
        fprintf(stderr, "Failed to create socket\n");
        exit(EXIT_FAILURE);
    }
    if(connect(sock, (struct sockaddr*) &resolver_addr, sizeof(resovler_addr)) == -1){
        fprintf(stderr, "Failed to conect\n");
        exit(EXIT_FAILURE);
    }
    return sock;
}
