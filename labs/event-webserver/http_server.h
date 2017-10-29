/*
 * Header file for http_server.c
 * Includes functions, macros, and constants for
 * simplified programming
 *
 * @author: Braden Hitchcock
 * @date: 3.31.2017
 * @class: CS 360 (CS 324 Systems Programming)
*/

#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/epoll.h>

#define SERVER_NAME     "Prestige/CS360 (Ubuntu 64-bit)"

#define DEFAULT_PORT	"8080"
#define DEFAULT_CONFIG	"http.conf"

#define DEFAULT_NUM_THREADS     8
#define DEFAULT_QUEUE_SIZE      10

#define ROOT_DOC        "/"

#define BUFFER_MAX	    2048
#define HEADER_MAX      12
#define MAX_EVENTS      100
#define MAX_REQUESTS    10
#define MAX_CLIENTS     100

#define FALSE       0
#define TRUE        1

#define EPOLL_TIMEOUT     2500
#define EXPIRE_TIME       5000

// HTTP Request Types
//
#define GET         1
#define POST        2
#define HEAD        3

// HTTP Versions
//
#define HTTP_1_1    1

// HTTP Error Codes
//
#define STATUS_OK                   200     // http request ok
#define STATUS_BAD_REQUEST          400     // bad http request (cannot parse)
#define STATUS_FORBIDDEN            403     // no access permision
#define STATUS_NOT_FOUND            404     // file not found
#define STATUS_NOT_IMPLEMENTED      501     // request method not supported
#define STATUS_INTERNAL_ERROR       500     // other error while serving request

// HTTP MIME types
//
#define HTML            "text/html"
#define TEXT            "text/plain"
#define JPEG            "image/jpeg"
#define GIF             "image/gif"
#define PNG             "image/png"
#define PDF             "application/pdf"
#define DEFAULT         "text/plain"

// Parsing constants
//
#define CRLF            "\r\n"

// Structs for parsed HTTP requests
//
typedef struct{
    char* name;
    char* value;
} http_header;

typedef struct{
    char* method;
    char* uri;
    char* filetype;
    char* version;
    char* host;
    http_header* headers;
} http_request;

// I/O Multiplexing Structs and Types
//
enum state {
    RECEIVING_HEADERS,
    RECEIVING_BODY,
    SENDING_HEADERS,
    SENDING_BODY,
    DISCONNECTED
};

typedef struct buffer {
    unsigned char* data;    
    int length;             // current length of info in buffer
    int max_length;         // the maximum possible length (currently)
    int position;           // current buffer pointer
} buffer_t;

typedef struct client {
    int fd;                         // socket file descriptor
    enum state state;               // current state of client
    buffer_t send_buf;              // buffer for send data
    buffer_t recv_buf;              // buffer for receive data
    int status;                     // the status of the request
    time_t last_active;             // timestamp of last active moment
    http_request* requests;         // request this client has received
    int num_requests;               // total number of requests
    int cur_request;                // current request being handled
} client_t;

typedef struct server {
    int fd;
} server_t;

// Structs for threading
//
typedef unsigned int bool;

typedef struct queue_item{
    int client;
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len;
} queue_item_t;

typedef struct world{
    pthread_mutex_t w_mutex;
    sem_t q_not_empty;
    sem_t q_not_full;
    int* q_size;
    queue_item_t* queue;
} world_t;

typedef struct param{
    int id;
    world_t* world;
} param_t;


// Function declarations
// Setup functions
void usage(char* name);
int create_server_socket(char* port, int protocol);
int set_blocking(int sock, int blocking);
int http_server_run(char* config_path, char* port, client_t* clients[]);
client_t* get_new_client(int sock);
void signal_handler(int signum);
// queue functions
void push(queue_item_t* queue, int* q_size, queue_item_t item);
queue_item_t pop(queue_item_t* queue, int* q_size);
// http request handler functions
int receive_requests(client_t* client);
int send_responses(client_t* client);
int request_to_struct(client_t* client);
int build_ok_header(client_t* client);
int build_ok_body(client_t* client);
int build_error_header(client_t* client);
int build_error_body(client_t* client);
int recv_data(client_t* client);
int send_data(client_t* client);
int set_date_header(char* response, int* length);
int set_servername_header(char* response, int* length, char* name);
int set_content_type_header(char* response, int* length, char* type);
int set_content_length_header(char* response, int* length, int content_length);
int set_modified_date_header(char* response, int* length, time_t date);
int set_header(char* name, char* value, char* response, int* offset);
void freeRequestStruct(http_request request);
void free_client(client_t* client);
void str_replace(char *target, const char *needle, const char *replacement);
char* concat(const char *s1, const char *s2);
char* get_filename_ext(char* filename);
// Debugging methods
void printRequestStruct(http_request request);
void printQueue(queue_item_t* queue, int q_size);


#endif /* HTTP_SERVER_H */