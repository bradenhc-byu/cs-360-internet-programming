#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <arpa/inet.h>

#include "dns.h"


// PROGRAM CONSTANTS AND TYPES --------------------------------
#define MAX_ENTRIES 		1024
#define DNS_MSG_MAX 		4096
#define LISTEN_QUEUE_SIZE	1024

typedef struct {
	dns_rr rr;
	time_t expires;
} dns_db_entry;

dns_db_entry cachedb[MAX_ENTRIES];
time_t cachedb_start;
char *cachedb_file;

// FUNCTION DEFINITIONS ---------------------------------------
void init_db();
int is_valid_request(unsigned char* request);
dns_rr get_question(unsigned char *request);
int get_response(unsigned char *request, int len, unsigned char *response);
void serve_udp(char* port);
void serve_tcp(char* port);
int create_server_socket(char* port, int protocol);
void printCache();

void init_db() {
	/* 
	 * Import the cache database from the file specified by cachedb_file.
	 * Reset cachedb_start to the current time.  Zero the expiration
	 * for all unused cache entries to invalidate them.  Read each line in
	 * the file, create a dns_rr structure for each, set the expires time
	 * based on the current time and the TTL read from the file, and create
	 * an entry in the cachedb.  Zero the expiration for all unused cache
	 * entries to invalidate them.
	 *
	 * INPUT:  None
	 * OUTPUT: None
	 */
	 if(DEBUG_MODE){printf("INITIALIZING CACHE...\n");}
	 // initialize the start time 
	 time(&cachedb_start);

	 // set up buffers
	 char line[BUFFER_MAX];
	 int rr_num = 0;
	 char host[BUFFER_MAX];
	 char class[BUFFER_MAX];
	 char type[BUFFER_MAX];
	 char data[BUFFER_MAX];
	 int t;
	 // open the file and read
	 FILE* db_file = fopen(cachedb_file,"r");
	 while(fgets(line,sizeof(line),db_file)){
		memset(host,0,BUFFER_MAX);
		memset(class,0,BUFFER_MAX);
		memset(type,0,BUFFER_MAX);
		memset(data,0,BUFFER_MAX);
		t = 0;
		sscanf(line,"%s %d %s %s %s",host,&t,class,type,data);
		// enter rr name
		char* name = (char*)malloc(sizeof(char)*strlen(host));
		memcpy(name,host,strlen(host));
		cachedb[rr_num].rr.name = name;
		// enter rr ttl
		cachedb[rr_num].rr.ttl = (dns_rr_ttl) t;
		// enter rr class
		if(strcmp(class,"IN") == 0){
			cachedb[rr_num].rr.class = 1;
		}else{
			cachedb[rr_num].rr.class = 0;
		}
		// enter rr type
		if(strcmp(type,"A") == 0){
			cachedb[rr_num].rr.type = TYPE_A;
		}else if(strcmp(type,"CNAME") == 0){
			cachedb[rr_num].rr.type = CNAME;
		}
		else{
			cachedb[rr_num].rr.type = 0;
		}
		// enter rr rdata_len and rdata (for TYPE_A and CNAME)
		if(cachedb[rr_num].rr.type == TYPE_A){
			cachedb[rr_num].rr.rdata_len = sizeof(int);
			cachedb[rr_num].rr.rdata = (char*)malloc(sizeof(int));;
			memset(cachedb[rr_num].rr.rdata,0,sizeof(int));
			inet_pton(AF_INET,data,cachedb[rr_num].rr.rdata);
		}
		else if(cachedb[rr_num].rr.type == CNAME){
			cachedb[rr_num].rr.rdata = (char*)malloc(sizeof(char)*BUFFER_MAX);
			memset(cachedb[rr_num].rr.rdata,0,BUFFER_MAX);
			int datalen = name_ascii_to_wire(data,cachedb[rr_num].rr.rdata);
			cachedb[rr_num].rr.rdata_len = datalen;
		}
		// set the cache entry expire time
		cachedb[rr_num].expires = (cachedb_start + (time_t)t);
		rr_num++;
		memset(line,0,BUFFER_MAX);
	 }
	 // close the file
	 fclose(db_file);
	 if(DEBUG_MODE){printCache();}
	 return;
}

int is_valid_request(unsigned char *request) {
	/* 
	 * Check that the request received is a valid query.
	 *
	 * INPUT:  request: a pointer to the array of bytes representing the
	 *                  request received by the server
	 * OUTPUT: a boolean value (1 or 0) which indicates whether the request
	 *                  is valid.  0 should be returned if the QR flag is
	 *                  set (1), if the opcode is non-zero (not a standard
	 *                  query), or if there are no questions in the query
	 *                  (question count != 1).
	 */
	 int index = 2;			// we need to start by skipping the ID 
	 short code_n_flags;	// we will copy the QR, Opcode, flags, and Rcode 
	 memcpy(&code_n_flags,request+index,sizeof(short));
	 code_n_flags = ntohs(code_n_flags);
	 // verify query flag is set
	 if(!IS_QUERY(code_n_flags)){
		 if(DEBUG_MODE){printf("NOT QUERY\n");}
		 return 0;
	 }
	 // verify opcode is 0
	 if(OPCODE(code_n_flags) != 0){
		 if(DEBUG_MODE){printf("OPCODE: %d\n",OPCODE(code_n_flags));}
		 return 0;
	 }
	 index += sizeof(short);
	 // verify only one question was sent
	 short total_questions;
	 memcpy(&total_questions,request+index,sizeof(short));
	 total_questions = ntohs(total_questions);
	 if(total_questions != 1){
		 if(DEBUG_MODE){printf("NO QUESTIONS\n");}
		 return 0;
	 }

	 return 1;
}

dns_rr get_question(unsigned char *request) {
	/* 
	 * Return a dns_rr for the question, including the query name and type.
	 *
	 * INPUT:  request: a pointer to the array of bytes representing the
	 *                  request received by the server.
	 * OUTPUT: a dns_rr structure for the question.
	 */

	 int index;
	 dns_rr question;
	 memset(&question,0,sizeof(dns_rr));
	 // we need to start extracting the record after the id, QR, opcode, flags, and
	 // other information
	 index = sizeof(int) * 3;		// based on DNS structure format
	 question = rr_from_wire(request,&index,QUESTION);

	 return question;
}

int get_response(unsigned char *request, int len, unsigned char *response) {
	/* 
	 * Handle a request and produce the appropriate response.
	 *
	 * Start building the response:
	 *   copy ID from request to response;
	 *   clear all flags and codes;
	 *   set QR bit to 1;
	 *   copy RD flag from request;
	 *
	 * If the request is not valid:
	 *   set the response code to 1 (FORMERR).
	 *   set all section counts to 0
	 *   return 12 (length of the header only)
	 *
	 * Otherwise:
	 *
	 *   Continue building the response:
	 *     set question count to 1;
	 *     set authority and additional counts to 0
	 *     copy question section from request;
	 *
	 *   Search through the cache database for an entry with a matching
	 *   name and type which has not expired
	 *   (expiration - current time > 0).
	 *
	 *     If no match is found:
	 *       the answer count will be 0, the response code will
	 *       be 3 (NXDOMAIN or name does not exist).
	 *
	 *     Otherwise (a match is found):
	 *       the answer count will be 1 (positive), the response code will
	 *       be 0 (NOERROR), and the appropriate RR will be added to the
	 *       answer section.  The TTL for the RR in the answer section
	 *       should be updated to expiration - current time.
	 *
	 *   Return the length of the response message.
	 *
	 * INPUT:  request: a pointer to the array of bytes representing the
	 *                  request received by the server.
	 * INPUT:  len: the length (number of bytes) of the request
	 * INPUT:  response: a pointer to the array of bytes where the response
	 *                  message should be constructed.
	 * OUTPUT: the length of the response message.
	 */

	 int index = 0;
	 // copy the request id
	 memcpy(response,request,sizeof(short));
	 index += sizeof(short);
	 // set QR to 1, copy RD flag, clear rest
	 short req_code_n_flags = 0;
	 memcpy(&req_code_n_flags,request+index,sizeof(short));
	 req_code_n_flags = ntohs(req_code_n_flags);
	 unsigned short code_n_flags = 0;
	 code_n_flags = SET_RESPONSE(code_n_flags);
	 if(RD_SET(req_code_n_flags)){
		code_n_flags = SET_RD(code_n_flags);
	 }
	 // verify it is a valid request
	 if(!is_valid_request(request)){
		 code_n_flags = SET_FORMERR(code_n_flags);
		 memcpy(response+index,&code_n_flags,sizeof(short));
		 if(DEBUG_MODE){printf("INVALID QUESTION!!\n");}
		 return 12;
	 }
	 code_n_flags = htons(code_n_flags);
	 memcpy(response+index,&code_n_flags,sizeof(short));
	 int cnf_index = index;
	 index += sizeof(short);
	 // set the total questions to 1
	 short total_questions = htons((short)1);
	 memcpy(response+index,&total_questions,sizeof(short));
	 index += sizeof(short);
	 // set authority and additional rr counts to 0 (leave answerr rr)
	 memset(response+index,0,sizeof(short)*3);
	 int answer_rr_count_index = index;
	 index += sizeof(short)*3;
	 // build the question name
	 dns_rr question = get_question(request);
	 int name_len = name_ascii_to_wire(question.name,(response+index));
	 index += name_len;
	 // copy the question type
	 short n_type = htons(question.type);
	 memcpy(response+index,&n_type,sizeof(short));
	 index += sizeof(short);
	 // copy the question class
	 short n_class = htons(question.class);
	 memcpy(response+index,&n_class,sizeof(short));
	 index += sizeof(short);

	 // find unexpired entry with matching name and type in cache  
	 int i;
	 short num_answers = 0;
	 dns_db_entry cur;
	 time_t remaining;
	 int found = 0;
	 char* qname = question.name;
	 for(i = 0; i < MAX_ENTRIES && cachedb[i].rr.name != NULL; i++){
		 cur = cachedb[i];
		 // update the ttl of the entry
		 remaining =  cur.expires - time(NULL);
		 cur.rr.ttl = remaining;
		 if(DEBUG_MODE){
			 printf("CACHE ENTRY: %s %d %d %d %s\n",
			 	cur.rr.name,cur.rr.ttl,cur.rr.class,cur.rr.type,cur.rr.rdata);
		 }
		 // check whether the name matches
		 if(strcmp(cur.rr.name,qname) == 0 && (remaining > 0)){
			 if(cur.rr.type == question.type){
				// found a match, update the time 
				found = 1;
				num_answers++;
				// add the resource record
				int rr_len = rr_to_wire(cur.rr,(response+index),RESOURCE_RECORD);
				index += rr_len;
				break;
			 }
			 else if(cur.rr.type == CNAME){
				// found a CNAME record, append to response and keep looking
				if(DEBUG_MODE){printf("FOUND CNAME...\n");}
				num_answers++;
				int rr_len = rr_to_wire(cur.rr,(response+index),RESOURCE_RECORD);
				index += rr_len;
				int tmp = 0;
				qname = name_ascii_from_wire(cur.rr.rdata,&(tmp));
				i = 0;
			 }
		 }
	 }
	 // if no record found return NXDOMAIN in the RCode
	 if(!found){
		code_n_flags = ntohs(code_n_flags);
		code_n_flags = SET_NXDOMAIN(code_n_flags);
		code_n_flags = htons(code_n_flags);
		memcpy((response+cnf_index),&code_n_flags,sizeof(short));
	 }

	 // update the answer count
	 if(num_answers != 0){
		num_answers = htons(num_answers);
	 	memcpy((response+answer_rr_count_index),&num_answers,sizeof(short));
	 }

	 // return the length of the response
	 return index;
}

void serve_udp(char* port) {
	/* 
	 * Listen for and respond to DNS requests over UDP.
	 * Initialize the cache.  Initialize the socket.  Receive datagram
	 * requests over UDP, and return the appropriate responses to the
	 * client.
	 *
	 * INPUT:  port: a numerical port on which the server should listen.
	*/
	unsigned char request[BUFFER_MAX];
	unsigned char response[BUFFER_MAX];
	struct sockaddr_storage addr;
	socklen_t addr_len = sizeof(struct sockaddr_storage);
	int request_len, response_len;

	// initialize the database cache
	init_db();

	// create the listening socket
	int sock = create_server_socket(port,SOCK_DGRAM);

	while(1){
		// reset the buffers
		memset(request,0,BUFFER_MAX);
		memset(response,0,BUFFER_MAX);
		addr_len = sizeof(struct sockaddr_storage);
		// wait for data
		request_len = recvfrom(sock,request,BUFFER_MAX,MSG_WAITALL,
									(struct sockaddr *)&addr, &addr_len);
	    if(DEBUG_MODE){printf("REQUEST--------");print_bytes(request,request_len);}
		// read the request and put it into the response
		response_len = get_response(request,request_len,response);
		if(DEBUG_MODE){printf("RESPONSE-------");print_bytes(response,response_len);}
		// send the response back to the client
		if(sendto(sock,response,response_len,0,(struct sockaddr*)&addr, addr_len) < 0){
			perror("sendto");
		}
	}

	close(sock);

}

void serve_tcp(char* port) {
	/* 
	 * Listen for and respond to DNS requests over TCP.
	 * Initialize the cache.  Initialize the socket.  Receive requests
	 * requests over TCP, ensuring that the entire request is received, and
	 * return the appropriate responses to the client, ensuring that the
	 * entire response is transmitted.
	 *
	 * Note that for requests, the first two bytes (a 16-bit (unsigned
	 * short) integer in network byte order) read indicate the size (bytes)
	 * of the DNS request.  The actual request doesn't start until after
	 * those two bytes.  For the responses, you will need to similarly send
	 * the size in two bytes before sending the actual DNS response.
	 *
	 * In both cases you will want to loop until you have sent or received
	 * the entire message.
	 *
	 * INPUT:  port: a numerical port on which the server should listen.
	 */
}

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
	ret = listen(sock, LISTEN_QUEUE_SIZE);
	if (ret == -1) {
		perror("listen");
		close(sock);
		exit(EXIT_FAILURE);
	}

	return sock;
}

int main(int argc, char *argv[]) {
	unsigned short port;
	int argindex = 1, daemonize = 0;
	if (argc < 3) {
		fprintf(stderr, "Usage: %s [-d] <cache file> <port>\n", argv[0]);
		exit(1);
	}
	if (argc > 3) {
	       	if (strcmp(argv[1], "-d") != 0) {
			fprintf(stderr, "Usage: %s [-d] <cache file> <port>\n", argv[0]);
			exit(1);
		}
		argindex++;
		daemonize = 1;
	}
	cachedb_file = argv[argindex++];
	port = atoi(argv[argindex]);

	// daemonize, if specified, and start server(s)
	// ...
	if(daemonize){
		pid_t pid = fork();
		if(pid < 0){
			fprintf(stderr,"Failed to daemonize the process\n");
			exit(EXIT_FAILURE);
		}
		if(pid == 0){
			// child
			printf("\nChild PID: %d\n",getpid());
			// start udp server
			serve_udp(argv[argindex]);
			_exit(EXIT_SUCCESS);
		}
		else{
			// parent
			exit(EXIT_SUCCESS);
		}
	}
	else{
		// start with UDP...
		serve_udp(argv[argindex]);
	}
	return 0;
}

void printCache(){
	int i;
	 for(i = 0; i < BUFFER_MAX && cachedb[i].rr.name != NULL; i++){
		 printf("DATABASE CACHE ENTRY: %s %d %d %d %d %s\n",
			 	cachedb[i].rr.name,cachedb[i].rr.ttl,cachedb[i].rr.class,
				cachedb[i].rr.type,cachedb[i].rr.rdata_len,cachedb[i].rr.rdata);
	 }
}
