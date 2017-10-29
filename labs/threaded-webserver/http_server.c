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
	int num_threads = DEFAULT_NUM_THREADS;
	int max_q_size = DEFAULT_QUEUE_SIZE;
	port = DEFAULT_PORT;
	config_path = DEFAULT_CONFIG;

	int c;
	while ((c = getopt(argc, argv, "vp:c:t:q:")) != -1) {
		switch (c) {
			case 'v':
				vflag = 1;
		 		break;
			case 'p':
				port = optarg;
				break;
			case 'c':
				config_path = optarg;
				break;
			case 't':
				num_threads = atoi(optarg);
				break;
			case 'q':
				max_q_size = atoi(optarg);
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

	// setup the queue
	int q_size = 0;
	queue_item_t* queue = (queue_item_t*)malloc(max_q_size * sizeof(queue_item_t));
	world_t world = {
		.q_size = &q_size,
		.queue = queue
	};

	// set up mutex, semaphores, and threads
	pthread_mutex_init(&world.w_mutex,NULL);
	sem_init(&world.q_not_full,0,max_q_size);
	sem_init(&world.q_not_empty,0,0);

	pthread_t* response_threads = (pthread_t*)malloc(sizeof(pthread_t)*num_threads);
	param_t* response_params = (param_t*)malloc(sizeof(param_t)*num_threads);
	int i;
	for(i = 0; i < num_threads; i++){
		if(vflag) printf("Creating Thread[%d]...\n",i);
		response_params[i].id = i;
		response_params[i].world = &world;
		if(pthread_create(&response_threads[i],NULL,worker,(void*)&response_params[i])){
			fprintf(stderr,"Failed to create all threads\n");
			exit(EXIT_FAILURE);
		}
	}
	
	if(vflag) printf("Starting the server...\n");
	// start the server
	http_server_run(config_path,port,&world);

	// free all threads and shared resources
	for(i = 0; i < num_threads; i++){
		if(vflag) printf("\nSending SIGINT to Thread[%d]",i);
		pthread_kill(response_threads[i],SIGINT);
	}
	for(i = 0; i < num_threads; i++){
		if(vflag) printf("\nJoining Thread[%d]",i);
		pthread_join(response_threads[i],NULL);
	}
	if(vflag) printf("\nFreeing resources...\n");
	free(response_threads);
	free(response_params);
	free(world.queue);
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
	printf("Usage: %s [-v] [-p port] [-c config-file]\n", name);
	printf("Example:\n");
        printf("\t%s -v -p 8080 -c http.conf \n", name);
	return;
}


/**********************************************************************************
*********************************************************************************** 
** Thread function
** Handles requests from a client until that client disconnects, then
** it looks for another client in the shared queue
**/
void* worker(void* args){

	param_t* params = (param_t*)args;
	int id = params->id;
	world_t* world = params->world;

	while(server_running){

			// wait for there to be something in the queue
			if(vflag) printf("Thread[%d] - Waiting for item in queue...\n",id);
			if(sem_wait(&world->q_not_empty) == -1){
				if(errno == EINTR){
					if(!server_running){
						return NULL;
					}
				}
			}
			pthread_mutex_lock(&world->w_mutex);
			// get an item from the queue here
			queue_item_t item = pop(world->queue,world->q_size);
			if(vflag) printf("Thread[%d] got a client\n",id);
			sem_post(&world->q_not_full);
			pthread_mutex_unlock(&world->w_mutex);

			handle_client(item.client,item.client_addr,item.client_addr_len);

	}

}


/**********************************************************************************
*********************************************************************************** 
** Queue Function - Push
** Adds a client to the shared queue for a thread to handle
**/
void push(queue_item_t* queue, int* q_size, queue_item_t item){
	queue[(*q_size)].client = item.client;
	queue[(*q_size)].client_addr = item.client_addr;
	queue[(*q_size)].client_addr_len = item.client_addr_len;
	(*q_size)++;
	return;
}

/** Queue Function - Pop
 ** Removes client information from the shared queue for
 ** a thread to process
**/
queue_item_t pop(queue_item_t* queue, int* q_size){
	queue_item_t item = {
		.client = queue[0].client,
		.client_addr = queue[0].client_addr,
		.client_addr_len = queue[0].client_addr_len
	};
	int i;
	for(i = 1; i < *q_size; i++){
		queue[i-1].client = queue[i].client;
		queue[i-1].client_addr = queue[i].client_addr;
		queue[i-1].client_addr_len = queue[i].client_addr_len;
	}
	(*q_size)--;
	return item;
}


/**********************************************************************************
*********************************************************************************** 
** Starts the HTTP server, accepting new clients and forking them into
** seperate processes.
**/
void http_server_run(char* config_path, char* port, world_t* world){
	// create the server socket
	int sock = create_server_socket(port, SOCK_STREAM);

	while (server_running) {
		struct sockaddr_storage client_addr;
		socklen_t client_addr_len = sizeof(client_addr);
		// wait for the queue to open up
		if(vflag) printf("[Main Thread] - Waiting for spot in queue...\n");
		if(sem_wait(&world->q_not_full) == -1){
			if(errno == EINTR){
				if(!server_running){
					return;
				}
			}
		}
		if(vflag) printf("[Main Thread] - Waiting for Connection...\n");
		int client = accept(sock, (struct sockaddr*)&client_addr, &client_addr_len);
		if (client == -1) {
			if(errno == EINTR && !server_running){
				close(sock);
				return;
			}
			perror("accept");
			continue;
		}
		// here is where we will insert file descriptors into the
		// queue for the threads to handle
		queue_item_t item = {
			.client = client,
			.client_addr = client_addr,
			.client_addr_len = client_addr_len
		};
		pthread_mutex_lock(&world->w_mutex);
		// insert into the queue here
		if(vflag) printf("[Main Thread] - Adding client to the queue\n");
		push(world->queue,world->q_size,item);
		sem_post(&world->q_not_empty);
		pthread_mutex_unlock(&world->w_mutex);
	}
	return;
}


/**********************************************************************************
*********************************************************************************** 
** Client Handler
** Receives requests from a client while the server is running
** Returns either when the server is terminated or the client disconnects
**/
void handle_client(int sock, struct sockaddr_storage client_addr, socklen_t addr_len) {


	unsigned char request[BUFFER_MAX];
	char client_hostname[NI_MAXHOST];
	char client_port[NI_MAXSERV];
	int ret = getnameinfo((struct sockaddr*)&client_addr, addr_len, client_hostname,
		       NI_MAXHOST, client_port, NI_MAXSERV, 0);
	if (ret != 0) {
		fprintf(stderr, "Failed in getnameinfo: %s\n", gai_strerror(ret));
		return;
	}
	printf("Got a connection from %s:%s\n", client_hostname, client_port);
	// Here is where we read responses from the client
	int bytes_read = 0;
	while(1){
		bytes_read = recv(sock, request+bytes_read, BUFFER_MAX-1, 0);
		if (bytes_read == 0) {
			printf("Peer disconnected\n");
			return;
		}
		else if (bytes_read < 0) {
			if(errno == EINTR && !server_running){
				break;
			}
			else{
				perror("recv");
				return;
			}
		}
		// wait until a full response is received
		if(strstr(request,"\r\n\r\n") == NULL){
			continue;
		}
		request[bytes_read] = '\0';
		if(vflag) printf("RECEIVED REQUEST:\n%s\n", request);
		bytes_read = 0;
		// This is where we receive an HTTP Request, so we need to parse it
		// and handle it before sending the response
		http_request httpr;
		memset(&httpr,0,sizeof(http_request));
		int status = 0;
		status = request_to_struct(request,&httpr);
		if(status == STATUS_OK){
			// handle the request and send a response
			send_response(sock,httpr,&status);
			// Don't forget to free the request to clean memory
			freeRequestStruct(httpr);
		}
		else{
			send_error_response(sock,status);
		}
		memset(request,0,BUFFER_MAX);
	}

	close(sock);

	return;
}


/**********************************************************************************
*********************************************************************************** 
** Parses an HTTP request and stores it values in the provided http_request struct
** Will return the status of the response after parse
**/
int request_to_struct(char* request, http_request* httpr){

	char buffer[BUFFER_MAX];
	memset(buffer,0,BUFFER_MAX);
	memcpy(buffer,request,BUFFER_MAX);
	char* request_line;
	int first = 1;
	http_header headers[HEADER_MAX];
	memset(headers,0,HEADER_MAX*sizeof(http_header));
	int header_index = 0;
	// read in the line by line
	request_line = strtok(buffer,"\r\n");
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

	return STATUS_OK;
}


/**********************************************************************************
*********************************************************************************** 
** Build a response to an HTTP request in the given char pointer
** Will return the length of the response in bytes
** Also can update and use the status int pointer
**/
int send_response(int sock,http_request request, int* status){

	char response[BUFFER_MAX];
	memset(response,0,BUFFER_MAX);
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
			if(vflag) printf("\n404 Not Found\n");
			(*status) = STATUS_NOT_FOUND;
		}
		else if(error_code == EACCES){ // permission denied
			if(vflag) printf("\n403 Forbidden\n");
			(*status) = STATUS_FORBIDDEN;
		}
		else{
			(*status) = STATUS_INTERNAL_ERROR;
			printf("Unknown error opening file\n");
		}
		length = send_error_response(sock,*status);
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

	if(vflag) printf("RESPONSE HEAD:\n%s\n",response);
	// send the header
	if(send(sock,response,length,0) < 0){
		perror("send:");	
	}

	if(vflag) printf("Reading and sending file...\n");
	// read in and send the file contents
	// if the file exists and is readable
	char buf[BUFFER_MAX];
	memset(buf,0,BUFFER_MAX);
	size_t nbytes = 0;
	while ( (nbytes = read(file_descriptor, buf, BUFFER_MAX)) > 0){
		length += nbytes;
		send(sock, buf, nbytes, 0);
		memset(buf,0,BUFFER_MAX);
	}

	close(file_descriptor);

	if(vflag) printf("File contents sent!\n\n");

	return length;
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
int send_error_response(int sock, int status){

	char response[BUFFER_MAX];
	memset(response,0,BUFFER_MAX);
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

	if(vflag) printf("RESPONSE HEAD:\n%s\n",response);
	// send the header
	if(send(sock,response,length,0) < 0){
		perror("send:");	
	}

	return length;
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