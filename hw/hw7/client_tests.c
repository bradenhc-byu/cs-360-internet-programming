/*
 * Example simple client code
 *
 */

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>

#define BUFFER_MAX  200

void print_bytes(unsigned char *bytes, int byteslen);
int connect_to_host(char* host, char* port, int protocol);

int main(int argc, char* argv[]) {
    int i;
    int sflag;
    int protocol;

    if(argc < 3 || argc > 4){
        fprintf(stderr,"usage: %s <SOCK_DGRAM | SOCK_STREAM> <send_flag>  [<send_data]\n",argv[0]);
        exit(EXIT_FAILURE);
    }

    sflag = atoi(argv[2]);
    if(strcmp(argv[1],"SOCK_DGRAM") == 0){
        printf("Establishing UDP connection...\n");
        protocol = SOCK_DGRAM;
    }
    else if(strcmp(argv[1],"SOCK_STREAM") == 0){
        printf("Establishing TCP connection...\n");
        protocol = SOCK_STREAM;
    }
    else{
        fprintf(stderr,"ERROR: invalid protocol - must use SOCK_DGRAM or SOCK_STREAM");
        exit(EXIT_FAILURE);
    }

    // Use SOCK_STREAM for TCP, and SOCK_DGRAM for UDP
    int sock = connect_to_host("localhost", "8080", protocol);
    
    if(sflag){
        printf("Send flag set, sending data to server...\n");
        printf("Program arguments: %d\n",argc);
        unsigned char send_buffer[BUFFER_MAX];
        if(argc != 4){
            for (i = 0; i < 10; i++) {
                // Put a bunch of As (0x41) into the buffer and send
                memset(send_buffer, 'A', BUFFER_MAX);
                if (send(sock, send_buffer, BUFFER_MAX, 0) == -1) {
                    perror("send");
                }

                // Put a bunch of Bs (0x42) into the buffer and send
                memset(send_buffer, 'B', BUFFER_MAX);
                if (send(sock, send_buffer, BUFFER_MAX, 0) == -1) {
                    perror("send");
                }
            }
        }
        else{
            // put the user input into the buffer and send
            memset(send_buffer,0,BUFFER_MAX);
            strncpy(send_buffer,argv[3],strlen(argv[3]));
            if(send(sock, send_buffer, strlen(argv[3]), 0) == -1){
                perror("send");
            }
        }
    }

    // receive data from the server
    printf("Waiting to receive (max 10 times)\n");
    unsigned char recv_buffer[6];
    for(i = 0; i < 10; i++){
        printf("%d requests remaining...\n",10 - i);
        memset(recv_buffer,0,6); 
        if(recv(sock, recv_buffer, 5, 0) < 0){
            perror("receive");
        }
        else{
            printf("%s\n",recv_buffer);
        }
    }
    
    sleep(10);

    // try to receive some data
    memset(recv_buffer,0,6);
    if(recv(sock, recv_buffer, 5, 0) < 0){
        perror("receive");
    }
    else{
        printf("Received data after host closed: %s\n",recv_buffer);
    }

    printf("Closing client...\n");
    close(sock);

    return 0;

}

int connect_to_host(char* host, char* service, int protocol) {
    int sock;
    int ret;
    struct addrinfo hints;
    struct addrinfo* addr_ptr;
    struct addrinfo* addr_list;

    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = protocol;
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

void print_bytes(unsigned char *bytes, int byteslen) {
    int i, j, byteslen_adjusted;
    unsigned char c;

    if (byteslen % 8) {
        byteslen_adjusted = ((byteslen / 8) + 1) * 8;
    } else {
        byteslen_adjusted = byteslen;
    }
    for (i = 0; i < byteslen_adjusted + 1; i++) {
        if (!(i % 8)) {
            if (i > 0) {
                for (j = i - 8; j < i; j++) {
                    if (j >= byteslen_adjusted) {
                        printf("  ");
                    } else if (j >= byteslen) {
                        printf("  ");
                    } else if (bytes[j] >= '!' && bytes[j] <= '~') {
                        printf(" %c", bytes[j]);
                    } else {
                        printf(" .");
                    }
                }
            }
            if (i < byteslen_adjusted) {
                printf("\n%02X: ", i);
            }
        } else if (!(i % 4)) {
            printf(" ");
        }
        if (i >= byteslen_adjusted) {
            continue;
        } else if (i >= byteslen) {
            printf("   ");
        } else {
            printf("%02X ", bytes[i]);
        }
    }
    printf("\n");
}

