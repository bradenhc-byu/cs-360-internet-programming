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

#define SERVER_NAME     "Prestige/CS360 (Ubuntu 64-bit)"

#define DEFAULT_PORT	"8080"
#define DEFAULT_CONFIG	"http.conf"

#define ROOT_DOC        "/"

#define BUFFER_MAX	1024
#define HEADER_MAX  12

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

// Function declarations
//
void usage(char* name);
int create_server_socket(char* port, int protocol);
void handle_client(int sock, struct sockaddr_storage client_addr, socklen_t addr_len);
void http_server_run(char* config_path, char* port, int verbose_flag);
void reap();

int handle_request(char* request);
int request_to_struct(char* request, http_request* httpr);
int send_response(int sock,http_request request, int* status);
int get_ascii_file_contents(FILE* file, char* contents);
int set_header(char* name, char* value, char* response, int* offset);
int set_body(FILE* file, char* response, int* offset);
void freeRequestStruct(http_request request);
void str_replace(char *target, const char *needle, const char *replacement);
char* concat(const char *s1, const char *s2);
char* get_filename_ext(char* filename);
int add_error_html(char* response, char* message, int* length);

// Debugging methods
void printRequestStruct(http_request request);



// Definitions for body content for HTTP Errors
//


#endif /* HTTP_SERVER_H */