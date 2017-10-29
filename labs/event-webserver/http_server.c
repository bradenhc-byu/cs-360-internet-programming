/** 
 * TCP Socket Server
 * Handles HTTP requests
 *
 * @author: Braden Hitchcock
 * @date: 3.31.2017
 * @class: CS 360 (CS 324 Systems Programming)
 * Brigham Young University
 *
 **/

#include "http_server.h"

#define SERVER_ROOT_DIR		"www"

// Setup from global variables
int vflag;
bool server_running = FALSE;


int main(int argc, char* argv[]) {

	char* port = NULL;
	char* config_path = NULL;

	vflag = 0;
	port = DEFAULT_PORT;

	int c;
	while ((c = getopt(argc, argv, "vp:c:t:q:")) != -1) {
		switch (c) {
			case 'v':
				vflag = 1;
		 		break;
			case 'p':
				port = optarg;
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

	// change state of server
	server_running = TRUE;

	// setup handling SIGINT to free memory
	struct sigaction sa;
	memset(&sa,0,sizeof(sa));
	sa.sa_handler = signal_handler;
	if (sigaction(SIGINT, &sa, NULL) == -1) {
		perror("sigaction:");
		exit(EXIT_FAILURE);
	}
	
	if(vflag) printf("Starting the server...\n");

	// create a list of clients
	client_t* clients[MAX_CLIENTS];
	int n;
	for(n = 0; n < MAX_CLIENTS; n++){
		clients[n] = NULL;
	}

	// start the server
	int error = http_server_run(config_path,port,clients);
	if(error < 0){
		fprintf(stderr,"http_server_run: fatal error\n");
		// any other cleanup we may need
		exit(EXIT_FAILURE);
	}

	if(vflag) printf("\nFreeing resources...\n");
	for(n = 0; n < MAX_CLIENTS; n++){
		if(clients[n] != NULL){
			close(clients[n]->fd);
			free_client(clients[n]);
			clients[n] = NULL;
		}
	}
	if(vflag) printf("Exiting....\n");
	return 0;
}



/**********************************************************************************
*********************************************************************************** 
** Signal handler for SIGINT to terminate all loops and
** clean up memory
**/
void signal_handler(int signum){
	if(signum == SIGINT){
		server_running = FALSE;
	}
	return;
}


/**********************************************************************************
*********************************************************************************** 
** Prints the correct usage of the program to the user
**/
void usage(char* name) {
	printf("Usage: %s [-v] [-p port]\n", name);
	printf("Example:\n");
        printf("\t%s -v -p 8080\n", name);
	return;
}


/**********************************************************************************
*********************************************************************************** 
** Starts the HTTP server, accepting new clients and forking them into
** seperate processes.
**/
int http_server_run(char* config_path, char* port, client_t* clients[]){

	// This is an EPOLL server...set up some variables
	int n;
	int nfds;
	int epoll_fd;
	struct epoll_event ev;
	struct epoll_event events[MAX_EVENTS];

	// create the server socket
	int server_sock = create_server_socket(port, SOCK_STREAM);

	// create the epoll socket
	if(vflag) printf("Creating epoll file descriptor...\n");
	if((epoll_fd = epoll_create1(0)) == -1){
		perror("epoll_create1");
		return -1;
	}
	// set the server socket to non-blocking
	set_blocking(server_sock,0);

	// create a new server struct
	server_t server;
	server.fd = server_sock;

	// set up the events for the server socket
	ev.events = EPOLLIN;
	ev.data.ptr = (void*)&server;

	if(vflag) printf("Registering server socket epoll file descriptor...\n");
	// register the server with the epoll controller
	if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_sock, &ev) == -1){
		perror("epoll_ctl: server_sock");
		return -1;
	}

	while (server_running) {

		// wait for an event to occur on one of the registered sockets
		if(vflag) printf("Waiting for events...\n");
		nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, EPOLL_TIMEOUT);
		if(nfds == -1){
			if(errno == EINTR){
				if(!server_running){
					return 0;
				}
			}
			else{
				perror("epoll_wait");
				return -1;
			}
		}

		// time to handle each of the events
		for(n = 0; n < nfds; n++){

			// check to see if we need to handle the server
			if(events[n].data.ptr == &server){
				if(vflag) printf("Handeling event on server socket...\n");
				client_t* new_client = get_new_client(server.fd);
				if(new_client == NULL) continue;

				ev.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
				ev.data.ptr = (void*)new_client;

				if(vflag) printf("Registering Client[%d]\n",new_client->fd);
				if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_client->fd, &ev) == -1){
					perror("epoll_ctl: new_client");
					return -1;
				}
				// add the client to our client list
				int i;
				for(i = 0; i < MAX_CLIENTS; i++){
					if(clients[i] == NULL){
						clients[i] = new_client;
						break;
					}
				}
				continue;
			}

			// we are looking at a client, handle it
			client_t* client = (client_t*)events[n].data.ptr;
			if(vflag) printf("Handling Client[%d] event...\n",client->fd);
			if(events[n].events & EPOLLIN){
				if(vflag) printf("Client[%d] - Receiving headers...\n",client->fd);
				if(receive_requests(client) == 0){
					// we only get here if the state has changed
					// to SENDING
					ev.events = EPOLLOUT | EPOLLRDHUP | EPOLLET;
					ev.data.ptr = (void*)client;
					if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, client->fd, &ev) == -1) {
						perror("epoll_ctl: switching to output events");
						return -1;
					}
				}
			}
			if(events[n].events & EPOLLOUT){
				if(vflag) printf("Client[%d] - Sending response...\n",client->fd);
				if(send_responses(client) == 0){
					// we only get here if the state has changed back
					// to RECEIVING
					ev.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
					ev.data.ptr = (void*)client;
					if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, client->fd, &ev) == -1) {
						perror("epoll_ctl: switching to input events");
						return -1;
					}
				}
			}
			if(events[n].events & EPOLLRDHUP){
				if(client->state == RECEIVING_HEADERS || client->state == RECEIVING_BODY){
					client->state = DISCONNECTED;
				}
			}
			if(client->state == DISCONNECTED){
				if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client->fd, NULL) == -1) {
					perror("epoll_ctl: removing client");
					exit(EXIT_FAILURE);
				}
				printf("client disconnected\n");
				// remove from our list of clients
				int i;
				for(i = 0; i < MAX_CLIENTS; i++){
					if(clients[i] == client){
						clients[i] = NULL;
						break;
					}
				}
				close(client->fd);
				free_client(client);
			}
		}

		// sweep the list of clients
		int i;
		for(i = 0; i < MAX_CLIENTS; i++){
			if(clients[i] != NULL){
				// get the time difference for the client
				time_t now;
				time(&now);
				time_t diff = (now - clients[i]->last_active) * 1000;
				if(diff >= EXPIRE_TIME){
					if(vflag) printf("Client[%d] - idle too long, disconnecting...\n",clients[i]->fd);
					close(clients[i]->fd);
					free_client(clients[i]);
					clients[i] = NULL;
				}
			}
		}
	}
	return 0;
}

/**********************************************************************************
*********************************************************************************** 
** Receive a New Client
** Handles a new client socket created from the accept() call and initializes
** the client_t structure
**/
client_t* get_new_client(int sock){
	struct sockaddr_storage addr;
	socklen_t addr_len = sizeof(struct sockaddr_storage);

	// get the new client socket file descriptor
	int new_fd = accept(sock, (struct sockaddr*)&addr, &addr_len);
	if(new_fd == -1){
		perror("accept");
		return NULL;
	}

	// make the new socket descriptor nonblocking
	set_blocking(new_fd, 0);

	// get some information about the connecting client
	char client_hostname[NI_MAXHOST];
	char client_port[NI_MAXSERV];
	int ret = getnameinfo((struct sockaddr*)&addr, addr_len, client_hostname,
		       NI_MAXHOST, client_port, NI_MAXSERV, 0);
	if (ret != 0) {
		fprintf(stderr, "Failed in getnameinfo: %s\n", gai_strerror(ret));
		close(new_fd);
		return NULL;
	}
	printf("Got a connection from %s:%s\n", client_hostname, client_port);

	// create the new client_t structure
	client_t* client = (client_t*)calloc(1,sizeof(client_t));
	client->fd = new_fd;

	// initialize the client state
	client->state = RECEIVING_HEADERS;

	// set up the send and receive buffers
	client->recv_buf.data = (char*)calloc(1,BUFFER_MAX);
	client->recv_buf.max_length = BUFFER_MAX;
	client->send_buf.data = (char*)calloc(1,BUFFER_MAX);
	client->send_buf.max_length = BUFFER_MAX;

	// initialize memory for requests
	client->requests = (http_request*)calloc(MAX_REQUESTS,sizeof(http_request));

	// stamp the timer
	time(&client->last_active);
	return client;
}


/**********************************************************************************
*********************************************************************************** 
** Frees struct data associated with a client
**/
void free_client(client_t* client){
	if(client->recv_buf.data) free(client->recv_buf.data);
	if(client->send_buf.data) free(client->send_buf.data);
	int i;
	for(i = 0; i < client->num_requests; i++){
		freeRequestStruct(client->requests[i]);
	}
	if(client->requests) free(client->requests);
	free(client);
	return;
}


/**********************************************************************************
*********************************************************************************** 
** Client Request Handler
** Receives requests from a client while the server is running
** Returns either when the server is terminated or the client disconnects
**/
int receive_requests(client_t* client){
	if(recv_data(client) != 0){
		client->state = DISCONNECTED;
		return 1;
	}
	// check to see if we have read a full request
	if( strstr(client->recv_buf.data,"\r\n\r\n") != NULL){
		// if we get here, we have received a full request. Parse it
		client->status = request_to_struct(client);
		if(vflag) printf("RECEIVED REQUEST:\n%s\n", client->recv_buf.data);
		// clear the send buffer in preparation
		memset(client->send_buf.data,0,client->send_buf.max_length);
		client->send_buf.position = 0;
		client->send_buf.length = 0;
		if(client->status == STATUS_OK){
			// build the response header
			build_ok_header(client);
		}
		else{
			build_error_header(client);
		}
		client->state = SENDING_HEADERS;
		// return 0 to indicate state has changed
		return 0;
	}
	return 1;
}

/**********************************************************************************
*********************************************************************************** 
** Client Receive Data Handler
** Reads data from the buffer of a client socket descriptor that has data in it
** It will read data until the data buffer for this event is empty
**/
int recv_data(client_t* client) {
	char temp_buffer[BUFFER_MAX];
	int bytes_read;
	while (1) {
		bytes_read = recv(client->fd, temp_buffer, BUFFER_MAX, 0);
		if (bytes_read == -1) {
			if (errno == EWOULDBLOCK || errno == EAGAIN) {
				break; /* We've read all we can for the moment, stop */
			}
			else if (errno == EINTR) {
				if(!server_running){
					break;
				}
				else{
					continue;
				}
			}
			else {
				/* some other error occurred, let's print it */
				perror("recv");
				return 1;
			}
		}
		else if (bytes_read == 0) {
			/* the client won't be sending us any more data (ever)
			 * but it may still be wanting us to send things back */
			break;
		}
		// stamp the timer
		time(&client->last_active);

		/* now append the new data recieved to the client's recv buffer */
		memcpy(&(client->recv_buf.data)[client->recv_buf.length], temp_buffer, bytes_read);
		client->recv_buf.length += bytes_read;

	}
	return 0;
}



/**********************************************************************************
*********************************************************************************** 
** Client Response Handler
** Receives requests from a client while the server is running
** Returns either when the server is terminated or the client disconnects
**/
int send_responses(client_t* client){
	// first thing we should do is check the status of the client request
	if(client->state == SENDING_HEADERS){
		if(send_data(client) == 0){
			// clear the send buffer in preparation
			memset(client->send_buf.data,0,client->send_buf.max_length);
			client->send_buf.position = 0;
			client->send_buf.length = 0;
			// build the response body
			if(client->status == STATUS_OK){
				build_ok_body(client);
			}
			else{
				build_error_body(client);
			}
			client->state = SENDING_BODY;
		}
	}
	if(client->state == SENDING_BODY){
		if(send_data(client) == 0){
			client->cur_request++;
			if(client->cur_request < client->num_requests){
				// there is another request header
				client->state = SENDING_HEADERS;
				// prep the buffer again by clearing it
				memset(client->send_buf.data,0,client->send_buf.max_length);
				client->send_buf.position = 0;
				if(client->status == STATUS_OK){
					build_ok_header(client);
				}
				else{
					build_error_header(client);
				}
			}
			else{
				// we have finished sending all the responses
				// clear the receive buffer and request array
				int i;
				for(i = 0; i < client->num_requests; i++){
					freeRequestStruct(client->requests[i]);
				}
				client->cur_request = 0;
				client->num_requests = 0;
				memset(client->recv_buf.data,0,BUFFER_MAX);
				client->recv_buf.length = 0;
				memset(client->requests,0,sizeof(http_request)*MAX_REQUESTS);
				// change the state
				client->state = RECEIVING_HEADERS;
				return 0;
			}
		}
	}
	return 1;
}


/**********************************************************************************
*********************************************************************************** 
** Parses an HTTP request and stores its values in the provided http_request struct
** Will return the status of the response after parse
**/
int request_to_struct(client_t* client){

	char* request = client->recv_buf.data;
	http_request* httpr;

	char* cur_request = strstr(request,"\r\n\r\n");

	while(cur_request != NULL){

		*cur_request++ = '\0';
		int first = 1;
		http_header headers[HEADER_MAX];
		memset(headers,0,HEADER_MAX*sizeof(http_header));
		int header_index = 0;
		httpr = &(client->requests[client->num_requests]);

		// read in line by line
		char* request_line = strtok(request,"\r\n");

		while(request_line != NULL){
			// Parse based on line
			if(first){
				// read in the request method, URI, and version
				char* method = (char*)malloc(16);
				char* uri = (char*)malloc(BUFFER_MAX);
				char* version = (char*)malloc(32);
				sscanf(request_line,"%s %s %s",method,uri,version);
				if(strcmp(method,"GET") == 0
					|| strcmp(method,"POST") == 0
					|| strcmp(method,"HEAD") == 0){
					httpr->method = method;
				}else{
					free(method);
					free(uri);
					free(version);
					return STATUS_NOT_IMPLEMENTED; // invalid method
				}
				httpr->uri = uri;
				if(strcmp(version,"HTTP/1.1") == 0){
					httpr->version = version;
				}else{
					free(method);
					free(uri);
					free(version);
					return STATUS_INTERNAL_ERROR; // aren't handling other request types
				}
				if(strcmp(uri,"/") == 0){
					uri = "/index.html";
				}
				char* ext = get_filename_ext(uri);
				char* type;
				if(strcmp(ext,"html") == 0){
					type = (char*)malloc(strlen(HTML)+1);
					memset(type,0,strlen(HTML)+1);
					memcpy(type,HTML,strlen(HTML));
				}else if(strcmp(ext,"txt") == 0){
					type = (char*)malloc(strlen(TEXT)+1);
					memset(type,0,strlen(TEXT)+1);
					memcpy(type,TEXT,strlen(TEXT));
				}else if(strcmp(ext,"jpg") == 0){
					type = (char*)malloc(strlen(JPEG)+1);
					memset(type,0,strlen(JPEG)+1);
					memcpy(type,JPEG,strlen(JPEG));
				}else if(strcmp(ext,"gif") == 0){
					type = (char*)malloc(strlen(GIF)+1);
					memset(type,0,strlen(GIF)+1);
					memcpy(type,GIF,strlen(GIF));
				}else if(strcmp(ext,"png") == 0){
					type = (char*)malloc(strlen(PNG)+1);
					memset(type,0,strlen(PNG)+1);
					memcpy(type,PNG,strlen(PNG));
				}else if(strcmp(ext,"pdf") == 0){
					type = (char*)malloc(strlen(PDF)+1);
					memset(type,0,strlen(PDF)+1);
					memcpy(type,PDF,strlen(PDF));
				}else{
					type = (char*)malloc(strlen(DEFAULT)+1);
					memset(type,0,strlen(DEFAULT)+1);
					memcpy(type,DEFAULT,strlen(DEFAULT));
				}
				httpr->filetype = type;
				first = 0;
			}
			else{
				char* name = (char*)malloc(BUFFER_MAX);
				char* value = (char*)malloc(BUFFER_MAX);
				memset(name,0,BUFFER_MAX);
				memset(value,0,BUFFER_MAX);
				sscanf(request_line,"%s %s",name,value);
				if(strcmp(name,"Host:") == 0){
					httpr->host = value;
					free(name);
				}
				else{
					str_replace(name,":","\0");
					headers[header_index].name = name;
					headers[header_index].value = value;
					header_index++;
				}
			}
			request_line = strtok(NULL,"\r\n");
		}

		// Set the httpr header pointer to the array of HTTP headers
		http_header* httpheaders = (http_header*)malloc(sizeof(http_header)*HEADER_MAX);
		memcpy(httpheaders,headers,HEADER_MAX*sizeof(http_header));
		httpr->headers = httpheaders;

		// increment the number of request headers in the client
		client->num_requests++;

		// get the next request
		request = cur_request + 3;
		cur_request = strstr(request,"\r\n\r\n");

	}

	return STATUS_OK;
}


/**********************************************************************************
*********************************************************************************** 
** Build a response to an HTTP request in the given char pointer
** Will return the length of the response in bytes
** Also can update and use the status int pointer
**/
int build_ok_header(client_t* client){

	int sock = client->fd;
	http_request request = client->requests[client->cur_request];
	int* status = &client->status;
	char* response = client->send_buf.data;

	int length = 0;
	
	// verify the requested uri exists
	char filepath[BUFFER_MAX];
	if(strcmp("/",request.uri) == 0){
		sprintf(filepath,"%s%s",SERVER_ROOT_DIR,"/index.html");
	}else{
		sprintf(filepath,"%s%s",SERVER_ROOT_DIR,request.uri);
	}
	int file_descriptor = open(filepath,O_RDONLY);
	if(file_descriptor < 0){
		int error_code = errno;
		// There was an error reading the file, determine what it was
		if(error_code == ENOENT){ // file not found
			(*status) = STATUS_NOT_FOUND;
		}
		else if(error_code == EACCES){ // permission denied
			(*status) = STATUS_FORBIDDEN;
		}
		else{
			(*status) = STATUS_INTERNAL_ERROR;
		}
		length = build_error_header(client);
		return length;
	}

	// the file has been opened, now lets set the response headers 
	// before adding the file contents
	// add the version
	length += sprintf(response,"%s ",request.version);
	// add the response code
	length += sprintf(response+length,"200 OK\r\n");
	// add the date
	set_date_header(response,&length);
	// add the server name
	set_servername_header(response,&length,SERVER_NAME);
	// add the content type
	set_content_type_header(response,&length,request.filetype);
	// get some info about the file
	struct stat attrib;
	fstat(file_descriptor, &attrib);
	// add the content length
	set_content_length_header(response,&length,(int)(attrib.st_size));	
	// add date last modified
	set_modified_date_header(response,&length,attrib.st_ctime);
	// add extra CRLF
	length += sprintf(response+length,"\r\n");

	// set the client parameters
	client->send_buf.length = length;
	// stamp the timer
	time(&client->last_active);

	if(vflag) printf("RESPONSE HEAD:\n%s\n",response);

	close(file_descriptor);
	
	return length;
}


/**********************************************************************************
*********************************************************************************** 
** Uses the client send buffer to send the response back to the client
** based on the file in the request
**/
int build_ok_body(client_t* client){

	int sock = client->fd;
	char* response = client->send_buf.data;
	http_request request = client->requests[client->cur_request];

	// open up the file
	char filepath[BUFFER_MAX];
	if(strcmp("/",request.uri) == 0){
		sprintf(filepath,"%s%s",SERVER_ROOT_DIR,"/index.html");
	}else{
		sprintf(filepath,"%s%s",SERVER_ROOT_DIR,request.uri);
	}
	int file_descriptor = open(filepath,O_RDONLY);

	// stamp the timer
	time(&client->last_active);

	if(vflag) printf("Reading file...\n");
	// read in and send the file contents
	int bytes_read = 0;
	char temp_buffer[BUFFER_MAX];
	memset(temp_buffer,0,BUFFER_MAX);
	while ( (bytes_read = read(file_descriptor, temp_buffer, BUFFER_MAX)) > 0){
		// Realloc the recv buffer of the client if needed
		int new_length = client->send_buf.length + bytes_read;
		if (client->send_buf.max_length < new_length) {
			/* since we just reached the max size of the buffer, let's double it 
			 * so we are less likely to run into this situation again */
			client->send_buf.data = realloc(client->send_buf.data, new_length * 2);
			int added_length = new_length * 2 - client->send_buf.length;
			memset(&(client->send_buf.data)[client->send_buf.length],0,added_length);
			client->send_buf.max_length = new_length * 2;
		}
		/* now append the new data recieved to the client's recv buffer */
		memcpy(&(client->send_buf.data)[client->send_buf.length], temp_buffer, bytes_read);
		client->send_buf.length += bytes_read;
		// clear the temp buffer for cleanliness
		memset(temp_buffer,0,BUFFER_MAX);
	}
	close(file_descriptor);
	if(vflag) printf("File contents read!\n\n");

	return client->send_buf.length;
}


/**********************************************************************************
*********************************************************************************** 
** Attempts to send all of the data in the client's send buffer. Returns 0 on
** success, or 1 if there is still data in the buffer to send
**/
int send_data(struct client* client) {
	char* buffer = (char*)client->send_buf.data;
	int bytes_sent;
	int bytes_left = client->send_buf.length - client->send_buf.position;
	while (bytes_left > 0) {
		bytes_sent = send(client->fd, &buffer[client->send_buf.position], bytes_left, 0);
		if (bytes_sent == -1) {
			if (errno == EWOULDBLOCK || errno == EAGAIN) {
				return 1; /* We've sent all we can for the moment */
			}
			else if (errno == EINTR) {
				if(!server_running){
					break;
				}
				continue; /* continue upon interrupt */
			}
			else if (errno == EPIPE || errno == ECONNRESET) {
				/* client disconnected, we can't send any more data */
				if(vflag) printf("EPIPE or ECONNRESET received during send\n");
				client->state = DISCONNECTED;
				return 1;
			}
			else {
				perror("send");
				client->state = DISCONNECTED;
				return 1;
			}
		}
		bytes_left -= bytes_sent;
		client->send_buf.position += bytes_sent;
	}
	return 0;
}


/**********************************************************************************
*********************************************************************************** 
** Function to set the date header
**/
int set_date_header(char* response, int* length){
	time_t raw_time;
	time(&raw_time);
	struct tm* info = localtime(&raw_time);
	char buffer[80];
	strftime(buffer,80,"%a, %d %b %Y %H:%M:%S %Z",info);
	int len = set_header("Date",buffer,response,length);
	return len;
}


/**********************************************************************************
*********************************************************************************** 
** Function to set the server name
**/
int set_servername_header(char* response, int* length, char* name){
	int len = set_header("Server",name,response,length);
	return len;
}


/**********************************************************************************
*********************************************************************************** 
** Function to set the content type in header
**/
int set_content_type_header(char* response, int* length, char* type){
	int len = set_header("Content-Type",type,response,length);
	return len;
}


/**********************************************************************************
*********************************************************************************** 
** Function to set the content length in header
**/
int set_content_length_header(char* response, int* length, int content_length){
	char num[10];
	sprintf(num,"%d",content_length);
	int len = set_header("Content-Length",num,response,length);
	return len;
}


/**********************************************************************************
*********************************************************************************** 
** Function to set the date last modified for the file
**/
int set_modified_date_header(char* response, int* length, time_t date){
	char buffer[80];
	memset(buffer,0,80);
	strftime(buffer,80,"%a, %d %b %Y %H:%M:%S %Z",localtime(&date));
	int len = set_header("Last-Modified",buffer,response,length);
	return len;
}


/**********************************************************************************
*********************************************************************************** 
** Sends an error response head to the client
**/
int build_error_header(client_t* client){

	int sock = client->fd;
	int status = client->status;
	char* response = client->send_buf.data;

	int length = 0;

	// add the version
	length += sprintf(response,"%s ","HTTP/1.1");

	// add the response code
	switch(status){
		case STATUS_BAD_REQUEST:
			length += sprintf(response+length,"400 Bad Request\r\n");
			break;
		case STATUS_FORBIDDEN:
			length += sprintf(response+length,"403 Forbidden\r\n");
			break;
		case STATUS_NOT_FOUND:
			length += sprintf(response+length,"404 Not Found\r\n");
			break;
		case STATUS_NOT_IMPLEMENTED:
			length += sprintf(response+length,"501 Not Implemented\r\n");
			break;
		case STATUS_INTERNAL_ERROR:
			length += sprintf(response+length,"500 Internal Server Error\r\n");
			break;
	}

	// set the date header
	set_date_header(response,&length);
	// set the server name header
	set_servername_header(response,&length,SERVER_NAME);
	// set the content type header
	set_content_type_header(response,&length,"text/html");
	// set the content length header
	char* data;
	switch(status){
		case STATUS_BAD_REQUEST:
				data = "<h1>400 - Bad Request</h1>";
				break;
		case STATUS_FORBIDDEN:
				data = "<h1>403 - Forbidden</h1>";
				break;
		case STATUS_NOT_FOUND:
				data = "<h1>404 - Not Found</h1>";
				break;
		case STATUS_NOT_IMPLEMENTED:
				data = "<h1>501 - Not Implemented</h1>";
				break;
		case STATUS_INTERNAL_ERROR:
				data = "<h1>500 - Internal Server Error</h1>";
				break;
	}
	int content_len = strlen(data);
	set_content_length_header(response,&length,content_len);
	// set the date last modified
	set_modified_date_header(response,&length,time(NULL));
	// add extra CRLF
	length += sprintf(response+length,"\r\n");
	// add the body
	length += sprintf(response+length,"%s",data);

	// stamp the timer
	time(&client->last_active);
	// set the length
	client->send_buf.length = length;

	if(vflag) printf("RESPONSE HEAD:\n%s\n",response);

	return length;
}


/**********************************************************************************
*********************************************************************************** 
** This function would normally send back all the data necessary
** to inform the user of the error the server experienced (HTML)
** However, since we are already sending that small string in the header,
** for now we will just change the state and return
**/
int build_error_body(client_t* client){
	client->send_buf.length = 0;
	return 0;
}


/**********************************************************************************
*********************************************************************************** 
** Function to set the header value in an HTTP response
** Updates the given offset pointer
** returns the length of the header
**/
int set_header(char* name, char* value, char* response,int* offset){
	int len = sprintf(response+(*offset),"%s: %s\r\n",name,value);
	(*offset) += len;
	return len;
}


/**********************************************************************************
*********************************************************************************** 
** Creates a simple server socket based on the specified protocol
**/
int create_server_socket(char* port, int protocol) {
	int sock;
	int ret;
	int optval = 1;
	struct addrinfo hints;
	struct addrinfo* addr_ptr;
	struct addrinfo* addr_list;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = protocol;
	/* AI_PASSIVE for filtering out addresses on which we
	 * can't use for servers
	 *
	 * AI_ADDRCONFIG to filter out address types the system
	 * does not support
	 *
	 * AI_NUMERICSERV to indicate port parameter is a number
	 * and not a string
	 *
	 * */
	hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG | AI_NUMERICSERV;
	/*
	 *  On Linux binding to :: also binds to 0.0.0.0
	 *  Null is fine for TCP, but UDP needs both
	 *  See https://blog.powerdns.com/2012/10/08/on-binding-datagram-udp-sockets-to-the-any-addresses/
	 */
	ret = getaddrinfo(protocol == SOCK_DGRAM ? "::" : NULL, port, &hints, &addr_list);
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

		// Allow us to quickly reuse the address if we shut down (avoiding timeout)
		ret = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
		if (ret == -1) {
			perror("setsockopt");
			close(sock);
			continue;
		}

		ret = bind(sock, addr_ptr->ai_addr, addr_ptr->ai_addrlen);
		if (ret == -1) {
			perror("bind");
			close(sock);
			continue;
		}
		break;
	}
	freeaddrinfo(addr_list);
	if (addr_ptr == NULL) {
		fprintf(stderr, "Failed to find a suitable address for binding\n");
		exit(EXIT_FAILURE);
	}

	if (protocol == SOCK_DGRAM) {
		return sock;
	}
	// Turn the socket into a listening socket if TCP
	ret = listen(sock, SOMAXCONN);
	if (ret == -1) {
		perror("listen");
		close(sock);
		exit(EXIT_FAILURE);
	}

	return sock;
}

/**********************************************************************************
*********************************************************************************** 
** Sets the given socket file descriptor to be blocking or non-blocking
**/
int set_blocking(int sock, int blocking) {
        int flags;
        /* Get flags for socket */
        if ((flags = fcntl(sock, F_GETFL)) == -1) {
                perror("fcntl get");
                exit(EXIT_FAILURE);
        }
        /* Only change flags if they're not what we want */
        if (blocking && (flags & O_NONBLOCK)) {
                if (fcntl(sock, F_SETFL, flags & ~O_NONBLOCK) == -1) {
                        perror("fcntl set block");
                        exit(EXIT_FAILURE);
                }
                return 0;
        }
        /* Only change flags if they're not what we want */
        if (!blocking && !(flags & O_NONBLOCK)) {
                if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
                        perror("fcntl set nonblock");
                        exit(EXIT_FAILURE);
                }
                return 0;
        }
        return 0;
}

/**********************************************************************************
*********************************************************************************** 
** Frees the pointers inside of a http_request struct
**/
void freeRequestStruct(http_request request){
	free(request.method);
	free(request.uri);
	free(request.filetype);
	free(request.host);
	free(request.version);
	int i;
	for(i = 0; i < HEADER_MAX && request.headers[i].name != NULL; i++){
		free(request.headers[i].name);
		free(request.headers[i].value);
	}
	free(request.headers);
	return;
}


/**********************************************************************************
*********************************************************************************** 
** Simple helper function to replace a substring with the given string
** Used to replace the ':' with '\0' when creating the http_request struct
**/
void str_replace(char *target, const char *needle, const char *replacement)
{
    char buffer[1024] = { 0 };
    char *insert_point = &buffer[0];
    const char *tmp = target;
    size_t needle_len = strlen(needle);
    size_t repl_len = strlen(replacement);

    while (1) {
        const char *p = strstr(tmp, needle);

        // walked past last occurrence of needle; copy remaining part
        if (p == NULL) {
            strcpy(insert_point, tmp);
            break;
        }

        // copy part before needle
        memcpy(insert_point, tmp, p - tmp);
        insert_point += p - tmp;

        // copy replacement string
        memcpy(insert_point, replacement, repl_len);
        insert_point += repl_len;

        // adjust pointers, move on
        tmp = p + needle_len;
    }

    // write altered string back to target
    strcpy(target, buffer);
}


/**********************************************************************************
*********************************************************************************** 
** Function to concatonate two strings
** Remember to free the resulting string after using it!
**/
char* concat(const char *s1, const char *s2)
{
    char *result = malloc(strlen(s1)+strlen(s2)+1);//+1 for the zero-terminator
    //in real code you would check for errors in malloc here
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}


/**********************************************************************************
*********************************************************************************** 
** Returns the file extension given a string representation
** of a file or file path
**/
char* get_filename_ext(char* filename) {
    char *dot = strrchr(filename, '.');
    if(!dot || dot == filename) return "";
    return dot + 1;
}


/**********************************************************************************
*********************************************************************************** 
/**********************************************************************************
*********************************************************************************** 
** DUBUGGING HELPER FUNCTIONS
**/
void printRequestStruct(http_request request){
	printf("Method: %s\n",request.method);
	printf("URI: %s\n",request.uri);
	printf("Version: %s\n",request.version);
	printf("Host: %s\n",request.host);
	int i;
	printf("Headers....\n");
	for(i = 0; i < HEADER_MAX && request.headers[i].name != NULL; i++){
		printf("%s: %s\n",request.headers[i].name,request.headers[i].value);
	}
	return;
}

void printQueue(queue_item_t* queue, int q_size){
	printf("Queue Contents (File Descriptors):\n");
	int i;
	for(i = 0; i < q_size; i++){
		printf("Queue[%d] = %d\n",i,queue[i].client);
	}
	return;
}