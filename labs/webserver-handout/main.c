#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>


#define DEFAULT_PORT	"8080"
#define DEFAULT_CONFIG	"http.conf"
#define BUFFER_MAX	1024
#define PATH_MAX        128
#define ROOT            "www"
#define RESPONSE_MAX    999999
#define HEADER_MAX      256
#define HTML_404        "<html><head><title>404 - Not Found</title></head><body><h1>404 - Not Found</h1><p>Could not find the file specified</p></body></html>"
#define HTML_403        "<html><head><title>403 - Forbidden</title></head><body><h1>403 - Forbidden</h1><p>Insufficient permissions to read requested file</p></body></html>"



// explain usage of executable to user
void usage(char* name);

// create server socket
int create_server_socket(char* port, int protocol);

// handle client
void handle_client(int sock, struct sockaddr_storage client_addr, socklen_t addr_len);

//run server
void run_http_server(char* config_path, char* port);

//read request from socket
int getRequest(int sock, char* request);

//parse request and respond to the socket 
void respond(int sock, char* request);

void sendStatus(int sock, int code);
void sendDate(int sock);
void sendServer(int sock);
void sendContentType(int sock, char* file_ext);
void sendStaticContent(int sock, char* content);
void sendFile(int sock, char* path);


int verbose_flag;

int main(int argc, char* argv[]) 
{
	char* port = NULL;
	char* config_path = NULL;

	verbose_flag = 0;
	port = DEFAULT_PORT;
	config_path = DEFAULT_CONFIG;

	int c;
	while ((c = getopt(argc, argv, "vp:c:")) != -1) 
        {
		switch (c) {
			case 'v':
				verbose_flag = 1;
		 		break;
			case 'p':
				port = optarg;
				break;
			case 'c':
				config_path = optarg;
				break;
			case '?':
				if (optopt == 'p' || optopt == 'c') {
					fprintf(stderr, "Option -%c requires an argument\n", optopt);
					usage(argv[0]);
					exit(EXIT_FAILURE);
				}
			default:
				fprintf(stderr, "Unknown option encountered\n");
				usage(argv[0]);
				exit(EXIT_FAILURE);
		}
	}

	run_http_server(config_path, port);
	
	return EXIT_SUCCESS;
}

void usage(char* name) 
{
	printf("Usage: %s [-v] [-p port] [-c config-file]\n", name);
	printf("Example:\n");
        printf("\t%s -v -p 8080 -c http.conf \n", name);
	return;
}

void handle_client(int sock, struct sockaddr_storage client_addr, socklen_t addr_len) 
{
        int optval = 1;
        if(setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) < 0) 
        {
            perror("setsockopt()");
            close(sock);
            exit(EXIT_FAILURE);
        }
    
	unsigned char request[BUFFER_MAX];
	char client_hostname[NI_MAXHOST];
	char client_port[NI_MAXSERV];
	int ret = getnameinfo((struct sockaddr*)&client_addr, addr_len, client_hostname, NI_MAXHOST, client_port, NI_MAXSERV, 0);
	if (ret != 0) 
        {
		fprintf(stderr, "Failed in getnameinfo: %s\n", gai_strerror(ret));
	}
	
	if(verbose_flag) printf("Got a connection from %s:%s\n", client_hostname, client_port);
	
        while (1) 
        {
                int ret = getRequest(sock, request);
		if( ret == 2)
                {
                        break;
                }
                else if(ret == 1)
                {
                        continue;
                }
                else //ret == 0
                {
                        respond(sock, request);
                }
	}
	return;
}

int create_server_socket(char* port, int protocol) 
{
	int sock;
	int ret;
	int optval = 1;
	struct addrinfo hints;
	struct addrinfo* addr_ptr;
	struct addrinfo* addr_list;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = protocol;
	
        //
        // AI_PASSIVE for filtering out addresses on which we
	// can't use for servers
	//
	// AI_ADDRCONFIG to filter out address types the system
	// does not support
	//
	// AI_NUMERICSERV to indicate port parameter is a number
	// and not a string
	//
	hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG | AI_NUMERICSERV;
	
        //
	//  On Linux binding to :: also binds to 0.0.0.0
	//  Null is fine for TCP, but UDP needs both
	//  See https://blog.powerdns.com/2012/10/08/on-binding-datagram-udp-sockets-to-the-any-addresses/
	//
	ret = getaddrinfo(protocol == SOCK_DGRAM ? "::" : NULL, port, &hints, &addr_list);
	if (ret != 0) {
		fprintf(stderr, "Failed in getaddrinfo: %s\n", gai_strerror(ret));
		exit(EXIT_FAILURE);
	}
	
	for (addr_ptr = addr_list; addr_ptr != NULL; addr_ptr = addr_ptr->ai_next) 
        {
		sock = socket(addr_ptr->ai_family, addr_ptr->ai_socktype, addr_ptr->ai_protocol);
		if (sock == -1) 
                {
			perror("socket");
			continue;
		}

		// Allow us to quickly reuse the address if we shut down (avoiding timeout)
		ret = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
		if (ret == -1) 
                {
			perror("setsockopt");
			close(sock);
			continue;
		}

		ret = bind(sock, addr_ptr->ai_addr, addr_ptr->ai_addrlen);
		if (ret == -1) 
                {
			perror("bind");
			close(sock);
			continue;
		}
		break;
	}
	freeaddrinfo(addr_list);
	if (addr_ptr == NULL) 
        {
		fprintf(stderr, "Failed to find a suitable address for binding\n");
		exit(EXIT_FAILURE);
	}

	if (protocol == SOCK_DGRAM) 
        {
		return sock;
	}
	// Turn the socket into a listening socket if TCP
	ret = listen(sock, SOMAXCONN);
	if (ret == -1) 
        {
		perror("listen");
		close(sock);
		exit(EXIT_FAILURE);
	}

	return sock;
}

void run_http_server(char* config_path, char* port)
{
        int sock = create_server_socket(port, SOCK_STREAM);
	while (1) 
        {
		struct sockaddr_storage client_addr;
		socklen_t client_addr_len = sizeof(client_addr);
		int client = accept(sock, (struct sockaddr*)&client_addr, &client_addr_len);
		if (client == -1) 
                {
			perror("accept");
			continue;
		}
		handle_client(client, client_addr, client_addr_len);
	}
}

int getRequest(int sock, char* request)
{
        int bytes_read = recv(sock, request, BUFFER_MAX-1, 0);
        if (bytes_read == 0) 
        {
                if(verbose_flag) printf("Peer disconnected!\n");
                
                close(sock);
                return 2;
        }
        if (bytes_read < 0) 
        {
                perror("recv");
                return 1;
        }
        request[bytes_read] = '\0';
        
        if(verbose_flag) printf("Received Request:\n%s\n", request);
        
        return 0;
}

void respond(int sock, char* request)
{
        char original_request[BUFFER_MAX];
        strcpy(original_request, request);
    
        //parse request
        char* method = strtok(request, " ");
        char* uri = strtok(NULL, " ");
        char* http_version = strtok(NULL, "\r\n");
        
        if(verbose_flag) printf("Method: [%s] URI: [%s] Version [%s]\n", method, uri, http_version);
        
        //what are we supposed to do with the rest of the header?
        
        //generate response
        
        if(strcmp(method, "GET\0") == 0)
        {
                if(uri[strlen(uri)-1] == '/')
                {
                        uri = "/index.html";
                }
                char path[PATH_MAX];
                strcpy(path, ROOT);
                strcpy(&path[strlen(ROOT)], uri);
                
                if(verbose_flag) printf("Requested Path: [%s]\n", path);
                
                
                if(access(path, F_OK) == -1) // file doesn't exist
                {
                        //send header
                        sendStatus(sock, 404);
                        sendDate(sock);
                        sendServer(sock);
                        sendContentType(sock, ".html");
                        sendStaticContent(sock, HTML_404);
                }
                else  if(access(path, R_OK) == -1) // we don't have permission to read the file
                {
                        //send header
                        sendStatus(sock, 403);
                        sendDate(sock);
                        sendServer(sock);
                        sendContentType(sock, ".html");
                        sendStaticContent(sock, HTML_403);
                }
                else //the file exists and we can read it
                {
                        sendStatus(sock, 200);
                        sendDate(sock);
                        sendServer(sock);   
                        sendFile(sock, path);
                }
        }
        else if(strcmp(method, "POST") == 0)
        {
            
        }
        else if(strcmp(method, "HEAD") == 0)
        {
            
        }
        else
        {
            
        }
        
        //send(sock, original_request, strlen(original_request)+1, 0);
        
        
}


void sendStatus(int sock, int code)
{
        
        char* status;
        
        if(code == 200)
        {
                status = "HTTP/1.1 200 OK\n";
        }
        else if (code == 403)
        {
                status = "HTTP/1.1 403 Forbidden\n";
        }
        else if (code == 404)
        {
                status = "HTTP/1.1 404 Not Found\n";
        }
        else
        {
                status = "HTTP/1.1 500 Internal Server Error\n";
        }
        
        send(sock, status, strlen(status), 0);
        
        if(verbose_flag) printf("%s", status);
}

void sendDate(int sock)
{
        time_t rawtime;
        struct tm * timeinfo;

        time ( &rawtime );
        timeinfo = localtime ( &rawtime );
        
        char time_str[HEADER_MAX];
        
        strftime(time_str, HEADER_MAX, "%a, %d %b %Y %H:%M:%S %Z", timeinfo);
        
        char date_header[HEADER_MAX];
        
        sprintf(date_header, "Date: %s\n", time_str);
        
        send(sock, date_header, strlen(date_header), 0);
        
        if(verbose_flag) printf("%s", date_header);
}

void sendServer(int sock)
{
        char* server_header = "Server: Multiprocess Server\n";
        send(sock, server_header, strlen(server_header), 0);
        
        if(verbose_flag) printf("%s", server_header);
}

void sendContentType(int sock, char* file_ext)
{
        char* mime_type;
        if(strcmp(file_ext, ".txt") == 0)
        {
                mime_type = "text/plain";
        }
        else if(strcmp(file_ext, ".jpg") == 0)
        {
                mime_type = "image/jpeg";
        }
        else if(strcmp(file_ext, ".gif") == 0)
        {
                mime_type = "image/gif";
        }
        else if(strcmp(file_ext, ".png") == 0)
        {
                mime_type = "image/png";
        }
        else if(strcmp(file_ext, ".pdf") == 0)
        {
                mime_type = "application/pdf";
        }
        else if(strcmp(file_ext, ".html") == 0)
        {
                mime_type = "text/html";
        }
        else //default
        {
                mime_type = "text/plain";
        }
        
        char type_header[HEADER_MAX];

        sprintf(type_header, "Content-Type: %s\n", mime_type);
        send(sock, type_header, strlen(type_header), 0);
        
        if(verbose_flag) printf("%s", type_header);
}

void sendStaticContent(int sock, char* content)
{
        char length_header[HEADER_MAX];
        sprintf(length_header, "Content-Length %zd\r\n\r\n", strlen(content));
        
        send(sock, length_header, strlen(length_header), 0);
        if(verbose_flag) printf("%s", length_header);
        
        //send content
        send(sock, content, strlen(content), 0);
        if(verbose_flag) printf("%s", content); 
}


void sendFile(int sock, char* path)
{
        sendContentType(sock, strrchr(path, '.'));
        
        int file = open(path, O_RDONLY);
        struct stat fileinfo;
        fstat(file, &fileinfo);
               
        char length_header[HEADER_MAX];
        sprintf(length_header, "Content-Length: %zd\r\n\r\n", fileinfo.st_size);
        
        send(sock, length_header, strlen(length_header), 0);
        if(verbose_flag) printf("%s", length_header);
        
        int bytes_read;
        char buffer[BUFFER_MAX];
        
        //send file
        while ((bytes_read = read(file, buffer, BUFFER_MAX)) > 0)
        {
                send(sock, buffer, bytes_read, 0);
                if(verbose_flag) printf("%s", buffer);
        }
        
}