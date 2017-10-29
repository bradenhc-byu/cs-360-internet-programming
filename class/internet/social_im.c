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

#define TEMP_BUF_LEN    1024
#define BUFFER_MAX      1024

// Program functions
//
int main(int argc, char* argv[]);
void signal_handler(int signum);
void receiver(int sock);
void sender(int sock, char* name);
int connect_to_host(char* host, char* sender);

// MAIN FUNCTIONS
//
int main(int argc, char* argv[]){
    struct sigaction sigact;
    memset(&sigact, 0, sizeof(sigact));
    sigact.sa_handler = signal_handler;
    sigaction(SIGINT, &sigact, NULL);

    int sock = connect_to_host(argv[1], argv[2]);
    receiver(sock);
    sender(sock,argv[3]);
    return 0;
}

// MODIFIED SIGNAL HANDLER FOR FORKED CHILD 
//
void signal_handler(int signum){
    int status;
    if(signum == SIGINT){
        
    }
}

// RECEIVER FUNCTION
//
void receiver(int sock){
    pid = fork();
    if(pid < 0){
        fprintf(stderr, "Fork failed!\n");
        exit(EXIT_FAILURE);
    }
    if(pid == 0){
        close(STDIN_FILENO);
        unsigned char* recv_buffer;
        while(1){
            recv(sock, buffer, 1024);
            recv_comm_until(sock, &recv_buffer, "\n");
            free(recv_buffer);
            recv_buffer = 0;
        }
    }
    return;
}

// SENDER FUNCTION
//
void sender(int sock, char* name){
    char send_buffer[BUFFER_MAX];
    char name_buffer[BUFFER_MAX];
    sprintf(name_buffer, "[%s]: ", name);
    while(1){
        fgets(send_buffer, BUFFER_MAX, stdin);
        // using strlen() to send the length is only useful if you are using
        // human readable ASCII characters
        send_comm(sock, name_buffer, strlen(name_buffer));
        send_comm(sock, send_buffer, strlen(send_buffer)); // lots of assumptions here... 
        memset(send_buffer, 0, BUFFER_MAX);
    }
    close(sock);
    return;
}

/*
 * The god function for connecting to any port via TCP
 * Created by Mark O'Neil (CS 360 instructor)
*/

int connect_to_host(char* host, char* service){
    int sock;
    int ret;
    struct addrinfo hints;
    struct addrinfo* addr_ptr;
    struct addrinfo* addr_list;

    // Structs initialized on the stack are not initialized to all zeros...so we 
    // need to do that manually here
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;    // typically TCP (change to SOCK_DEGRAM for VDN)
    hints.ai_family = AF_UNSPEC;        // IP4 or IP6, we don't care
    ret = getaddrinfo(host, service, &hints, &addr_list);   // returning multiple things in c can be done by passing pointer pointers
    if(ret != 0){
        fprintf(stderr, "Failed in getaddrinfo: %s\n",gai_strerror(ret));
        exit(EXIT_FAILURE);
    }

    // Loop through all the answers we received from getaddrinfo()
    for(addr_ptr = addr_list; addr_ptr != NULL; addr_ptr){
        sock = socket(addr_ptr->ai_family, addr_ptr->ai_sockettype, addr_ptr->ai_protocol);
        if(sock == -1){
            // perror() or strerror() - when encountering an error from a system call
            //                          we can call these to get all the details
            //                          (need to include errno.h)
            perror("socket");
            continue;
        }
        if(connect(sock, addr_ptr->ai_addr, addr_ptr->ai_addrlen) == -1){
            perror("connect");
            close(sock);
            continue;
        }
        break;
    }

    if(addr_ptr == NULL){
        fprintf(stderr, "Failed to find a suitable address for connection\n");
        exit(EXIT_FAILURE);
    }
    freeaddrinfo(addr_list);
    return sock;
}
