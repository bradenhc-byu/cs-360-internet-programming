#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include <arpa/inet.h>

#define MAX_BUFFER_SIZE	1024

#define BITS_IN_CHAR	8
#define IS_POINTER(c)	(c & 0xc0)
#define IS_QUERY(c)		!(c & 0x80)
#define IS_RESPONSE(c)	(s & 0x80)
#define OPCODE(c)		((c & 0x78)>>3)
#define FLAGS(s)		(s & 0x07f0)
#define AA_SET(f)		(f & 0x0400)
#define TC_SET(f)		(f & 0x0200)
#define RD_SET(f)		(f & 0x0100)
#define RA_SET(f)		(f & 0x0080)
#define Z_ST(f)			(f & 0x0040)
#define AD_SET(f)		(f & 0x0020)
#define CD_SET(f)		(f & 0x0010)
#define RCODE(c)		(c & 0x0f)

#define SWAP_ENDIAN_32(num)	(((num>>24)&0xff) | ((num<<8)&0xff0000) | ((num>>8)&0xff00) | ((num<<24)&0xff000000)) // byte 0 to byte 3

#define TYPE_A			1
#define CNAME			5

// Set this to 1 to see debug messages, 0 to hide them
#define DEBUG_MODE 1

typedef unsigned int dns_rr_ttl;
typedef unsigned short dns_rr_type;
typedef unsigned short dns_rr_class;
typedef unsigned short dns_rdata_len;
typedef unsigned short dns_rr_count;
typedef unsigned short dns_query_id;
typedef unsigned short dns_flags;

typedef struct {
	char *name;
	dns_rr_type type;
	dns_rr_class class;
	dns_rr_ttl ttl;
	dns_rdata_len rdata_len;
	unsigned char *rdata;
} dns_rr;

// the query for a connectino to www.example.com 
unsigned char example_msg[] = {
0x27, 0xd6, 0x01, 0x00,
0x00, 0x01, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00,
0x03, 0x77, 0x77, 0x77,
0x07, 0x65, 0x78, 0x61,
0x6d, 0x70, 0x6c, 0x65,
0x03, 0x63, 0x6f, 0x6d,
0x00, 0x00, 0x01, 0x00,
0x01
};

/*******************************************************************************
 * Function definitions
*/
void print_bytes(unsigned char *bytes, int byteslen);
void canonicalize_name(char *name);
int name_ascii_to_wire(char *name, unsigned char *wire);
char *name_ascii_from_wire(unsigned char *wire, int *indexp);
dns_rr rr_from_wire(unsigned char *wire, int *indexp, int query_only);
int rr_to_wire(dns_rr rr, unsigned char *wire, int query_only);
unsigned short create_dns_query(char *qname, dns_rr_type qtype, unsigned char *wire);
char *get_answer_address(char *qname, dns_rr_type qtype, unsigned char *wire);
int send_recv_message(unsigned char *request, int requestlen, unsigned char *response, char *server, unsigned short port);
char *resolve(char *qname, char *server);
int create_udp_socket(char* server, short port);
void flushBuffer(char* buffer);





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

void canonicalize_name(char *name) {
	/*
	 * Canonicalize name in place.  Change all upper-case characters to
	 * lower case and remove the trailing dot if there is any.  If the name
	 * passed is a single dot, "." (representing the root zone), then it
	 * should stay the same.
	 *
	 * INPUT:  name: the domain name that should be canonicalized in place
	 */
	
	int namelen, i;

	// leave the root zone alone
	if (strcmp(name, ".") == 0) {
		return;
	}

	namelen = strlen(name);
	// remove the trailing dot, if any
	if (name[namelen - 1] == '.') {
		name[namelen - 1] = '\0';
	}

	// make all upper-case letters lower case
	for (i = 0; i < namelen; i++) {
		if (name[i] >= 'A' && name[i] <= 'Z') {
			name[i] += 32;
		}
	}
}

void flushBuffer(char* buffer){
	int i;
	for(i = 0; i < MAX_BUFFER_SIZE; i++){
		buffer[i] = 0x00;
	}
}

int name_ascii_to_wire(char *name, unsigned char *wire) {
	/* 
	 * Convert a DNS name from string representation (dot-separated labels)
	 * to DNS wire format, using the provided byte array (wire).  Return
	 * the number of bytes used by the name in wire format.
	 *
	 * INPUT:  name: the string containing the domain name
	 * INPUT:  wire: a pointer to the array of bytes where the
	 *              wire-formatted name should be constructed
	 * OUTPUT: the length of the wire-formatted name.
	 */

	 char* wire_ptr = wire;
	 char name_buffer[MAX_BUFFER_SIZE];
	 flushBuffer(name_buffer);
	 strncpy(name_buffer,name,strlen(name));
	 char delim[2] = ".";

	 char* cur_label;
	 size_t len, total_len = 0;

	 // get the first token
	 cur_label = strtok(name_buffer,delim);

	 // get the remaining tokenized labels
	 while(cur_label != NULL){
		 len = strlen(cur_label);
		 const char clen = (char)len;
		 strncpy(wire_ptr,&clen,sizeof(char));
		 wire_ptr++;
		 strncpy(wire_ptr,cur_label,len);
		 wire_ptr += len;
		 cur_label = strtok(NULL,delim);
		 total_len += len;
	 }
	 // add the terminating 00 to the end
	 char c = 0x00;
	 memcpy(wire_ptr,&c,sizeof(char));
	 total_len += 4;
	 
	 return total_len;

}

char *name_ascii_from_wire(unsigned char *wire, int *indexp) {
	/* 
	 * Extract the wire-formatted DNS name at the offset specified by
	 * *indexp in the array of bytes provided (wire) and return its string
	 * representation (dot-separated labels) in a char array allocated for
	 * that purpose.  Update the value pointed to by indexp to the next
	 * value beyond the name.
	 *
	 * INPUT:  wire: a pointer to an array of bytes
	 * INPUT:  indexp, a pointer to the index in the wire where the
	 *              wire-formatted name begins
	 * OUTPUT: a string containing the string representation of the name,
	 *              allocated on the heap.
	 */
	
	int compressed = 0;
	unsigned char* wire_ptr = wire;
	unsigned char* name = (char*)malloc(sizeof(char)*MAX_BUFFER_SIZE);
	char* name_ptr = name;
	char* delim = ".";
	wire_ptr += *indexp;
	int label_len = (int)*wire_ptr;
	while(label_len != 0){
		if(label_len >= 192){
			// we are looking at a pointer, get the offset and move the pointer
			if(DEBUG_MODE){printf("Found a pointer!\n");}
			wire_ptr++;
			if(!compressed){(*indexp)++;}
			int name_offset = (int)*wire_ptr;
			wire_ptr = (wire + name_offset);
			label_len = (int)(*wire_ptr); 
			compressed = 1;
		}else{
			wire_ptr++;								// increment to start of label
			if(!compressed){(*indexp)++;}
			memcpy(name_ptr,wire_ptr,label_len);	// read the label
			name_ptr += label_len;		
			wire_ptr += label_len;					// increment to next length
			if(!compressed){(*indexp) += label_len;}
			label_len = (int)*wire_ptr;				// get the length of the label
			if(label_len != 0){
				memcpy(name_ptr,delim,sizeof(char));	// add the period
				name_ptr++;
			}
		}
	}
	// increment past the trailing 0x00 or the pointer value
	(*indexp)++;

	return name;
}

dns_rr rr_from_wire(unsigned char *wire, int *indexp, int query_only) {
	/* 
	 * Extract the wire-formatted resource record at the offset specified by
	 * *indexp in the array of bytes provided (wire) and return a 
	 * dns_rr (struct) populated with its contents. Update the value
	 * pointed to by indexp to the next value beyond the resource record.
	 *
	 * INPUT:  wire: a pointer to an array of bytes
	 * INPUT:  indexp: a pointer to the index in the wire where the
	 *              wire-formatted resource record begins
	 * INPUT:  query_only: a boolean value (1 or 0) which indicates whether
	 *              we are extracting a full resource record or only a
	 *              query (i.e., in the question section of the DNS
	 *              message).  In the case of the latter, the ttl,
	 *              rdata_len, and rdata are skipped.
	 * OUTPUT: the resource record (struct)
	 */

	unsigned char* wire_ptr = wire;
	dns_rr rr;
	wire_ptr += (*indexp);
	// extract the name (but check for pointer)
	if(((int)(*wire_ptr) < 192) && !query_only){
		(*indexp) += 11;
	}
	rr.name = name_ascii_from_wire(wire,indexp);
	wire_ptr = (wire + *indexp);
	if(DEBUG_MODE){printf("RR Name: %s\n",rr.name);}
	// extract the type
	memcpy(&rr.type,wire_ptr,sizeof(dns_rr_type));
	rr.type = ntohs(rr.type);
	wire_ptr += sizeof(short);
	(*indexp) += sizeof(short);
	// extract the class
	
	memcpy(&rr.class,wire_ptr,sizeof(dns_rr_type));
	rr.class = ntohs(rr.class);
	wire_ptr += sizeof(short);
	(*indexp) += sizeof(short);
	// stop here unless the query only flag is not set
	if(query_only == 0){
		// extract the ttl
		memcpy(&(rr.ttl),wire_ptr,sizeof(int));
		wire_ptr += sizeof(int);
		(*indexp) += sizeof(int);
		// extract the record data length
		memcpy(&rr.rdata_len,wire_ptr,sizeof(dns_rdata_len));
		rr.rdata_len = ntohs(rr.rdata_len);
		wire_ptr += sizeof(dns_rdata_len);
		(*indexp) += sizeof(dns_rdata_len);
		// extract a pointer to the record data
		rr.rdata = wire_ptr;
		// increment the index past the rdata 
		(*indexp) += (int)rr.rdata_len;
	}
	
	return rr;

}


int rr_to_wire(dns_rr rr, unsigned char *wire, int query_only) {
	/* 
	 * Convert a DNS resource record struct to DNS wire format, using the
	 * provided byte array (wire).  Return the number of bytes used by the
	 * name in wire format.
	 *
	 * INPUT:  rr: the dns_rr struct containing the rr record
	 * INPUT:  wire: a pointer to the array of bytes where the
	 *             wire-formatted resource record should be constructed
	 * INPUT:  query_only: a boolean value (1 or 0) which indicates whether
	 *              we are constructing a full resource record or only a
	 *              query (i.e., in the question section of the DNS
	 *              message).  In the case of the latter, the ttl,
	 *              rdata_len, and rdata are skipped.
	 * OUTPUT: the length of the wire-formatted resource record.
	 *
	 */

}

unsigned short create_dns_query(char *qname, dns_rr_type qtype, unsigned char *wire) {
	/* 
	 * Create a wire-formatted DNS (query) message using the provided byte
	 * array (wire).  Create the header and question sections, including
	 * the qname and qtype.
	 *
	 * INPUT:  qname: the string containing the name to be queried
	 * INPUT:  qtype: the integer representation of type of the query (type A == 1)
	 * INPUT:  wire: the pointer to the array of bytes where the DNS wire
	 *               message should be constructed
	 * OUTPUT: the length of the DNS wire message
	 */

	 unsigned char* wire_ptr = wire;
	 unsigned short wire_len = 0;
	 // create the unique id 
	 unsigned short rand_id = (unsigned short) rand();
	 memcpy(wire_ptr,&rand_id,sizeof(short));
	 wire_ptr += sizeof(short);
	 wire_len += sizeof(short);

	 // create the default query options
	 unsigned char preset_data[] = {0x01,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00};
	 size_t data_len = 10;
	 memcpy(wire_ptr,preset_data,data_len);
	 wire_ptr += data_len;
	 wire_len += (short)data_len;

	 // add the name to the query
	 int name_length = name_ascii_to_wire(qname,wire_ptr);
	 wire_ptr += name_length;
	 wire_len += (short)name_length;

	 // add the type and class
	 dns_rr_class class = htons(0x0001);
	 dns_rr_type type = htons(qtype);
	 memcpy(wire_ptr,&type,sizeof(short));
	 wire_ptr += sizeof(short);
	 wire_len += (short)sizeof(short);
	 memcpy(wire_ptr,&class,sizeof(short));
	 wire_ptr += sizeof(short);
	 wire_len += (short) sizeof(short);

	 return wire_len;

}

char *get_answer_address(char *qname, dns_rr_type qtype, unsigned char *wire) {
	/* 
	 * Extract the IPv4 address from the answer section, following any
	 * aliases that might be found, and return the string representation of
	 * the IP address.  If no address is found, then return NULL.
	 *
	 * INPUT:  qname: the string containing the name that was queried
	 * INPUT:  qtype: the integer representation of type of the query (type A == 1)
	 * INPUT:  wire: the pointer to the array of bytes representing the DNS wire message
	 * OUTPUT: a string representing the IP address in the answer; or NULL if none is found
	 */

	 char* qname_ptr = qname;
	 char* wire_ptr = wire;
	 int offset = 0;
	 dns_rr question_rr;
	 dns_rr rr;
	 // increment past the id and the flags and opcode
	 wire_ptr += sizeof(int);
	 offset += sizeof(int);
	 // get the quantity info
	 // get the number of questions
	 short num_questions;
	 memcpy(&num_questions,wire_ptr,sizeof(short));
	 num_questions = ntohs(num_questions);
	 if(DEBUG_MODE){printf("Num Questions: %d\n",num_questions);}
	 wire_ptr += sizeof(dns_rr_count);
	 offset += sizeof(dns_rr_count);
	 // get the number of answer rr
	 dns_rr_count num_answer_rr;
	 memcpy(&num_answer_rr,wire_ptr,sizeof(dns_rr_count));
	 num_answer_rr = ntohs(num_answer_rr);
	 if(DEBUG_MODE){printf("Num Answer RR: %d\n",num_answer_rr);}
	 wire_ptr += sizeof(dns_rr_count);
	 offset += sizeof(dns_rr_count);
	 // get the number of authority rr
	 dns_rr_count num_auth_rr;
	 memcpy(&num_auth_rr,wire_ptr,sizeof(dns_rr_count));
	 num_auth_rr = ntohs(num_auth_rr);
	 if(DEBUG_MODE){printf("Num Authority RR: %d\n",num_auth_rr);}
	 wire_ptr += sizeof(dns_rr_count);
	 offset += sizeof(dns_rr_count);
	 // get the number of additional rr
	 dns_rr_count num_add_rr;
	 memcpy(&num_add_rr,wire_ptr,sizeof(dns_rr_count));
	 num_add_rr = ntohs(num_add_rr);
	 if(DEBUG_MODE){printf("Num Additional RR: %d\n",num_add_rr);}
	 wire_ptr += sizeof(dns_rr_count);
	 offset += sizeof(dns_rr_count);
	 // extract and increment past the question section 
	 if(DEBUG_MODE){printf("Getting Question RR\n");}
	 question_rr = rr_from_wire(wire,&offset,1);
	 if(DEBUG_MODE){printf("Quesiton Type: %d\n",question_rr.type);}
	 // now start extracting resource records
	 int rr_left = num_answer_rr + num_auth_rr + num_add_rr;
	 while(rr_left){
		 if(DEBUG_MODE){printf("Records Left: %d\n",rr_left);}
		 if(DEBUG_MODE){printf("'Before' Offset: %d\n",offset);}
		 rr = rr_from_wire(wire,&offset,0);
		 if(DEBUG_MODE){printf("'After' Offset: %d\n'",offset);}
		 rr_left--;
		 if(strcmp(rr.name,qname_ptr) == 0){
			 if(DEBUG_MODE){
				 printf("Current RR Type: %d\n",rr.type);
			 }
			if(rr.type == qtype){
				// extract the ip address and return
				unsigned char* ip = (char*)malloc(sizeof(char)*rr.rdata_len);
				if(DEBUG_MODE){
					printf("IP Found!\n");
					printf("Length: %d\n",rr.rdata_len);
					printf("Byte representation of IP Address: %s\n",rr.rdata);
				}
				struct in_addr addr;
				memcpy(&(addr.s_addr),rr.rdata,rr.rdata_len);
				if(DEBUG_MODE){printf("Struct Value for IP: %d\n",addr.s_addr);}
				//inet_ntop(AF_INET6,&addr,ip,rr.rdata_len);
				ip = inet_ntoa(addr);
				return ip;
			}
			else if(rr.type == CNAME){
				// handle the alias
				int tmp = offset - rr.rdata_len;
				char* next_qname = name_ascii_from_wire(wire,&tmp);
				canonicalize_name(next_qname);
				qname_ptr = next_qname;
				//offset--;
				if(DEBUG_MODE){
					printf("CNAME: looking up alias - %s\n",qname_ptr);
				}
			}
		 }
		 else{
			printf("Breaking Loop...\n");
			break;
		}
	 }

	 return NULL;

}

int send_recv_message(unsigned char *request, int requestlen, unsigned char *response, char *server, unsigned short port) {
	/* 
	 * Send a message (request) over UDP to a server (server) and port
	 * (port) and wait for a response, which is placed in another byte
	 * array (response).  Create a socket, "connect()" it to the
	 * appropriate destination, and then use send() and recv();
	 *
	 * INPUT:  request: a pointer to an array of bytes that should be sent
	 * INPUT:  requestlen: the length of request, in bytes.
	 * INPUT:  response: a pointer to an array of bytes in which the
	 *             response should be received
	 * OUTPUT: the size (bytes) of the response received
	 */
	 if(DEBUG_MODE){printf("Creating socket...\n");}
	 int sock = create_udp_socket(server,port);
	 if(DEBUG_MODE){
		 printf("Socket created, socket id: %d\n",sock);
	 	 printf("Request =");
	 	 print_bytes(request,requestlen);
	 	 printf("Sending request...\n");
	 }
	 send(sock,request,requestlen, 0);
	 if(DEBUG_MODE){printf("Request sent, getting response....\n");}
	 int bytes_received = recv(sock,response,MAX_BUFFER_SIZE, 0);
	 if(DEBUG_MODE){printf("Information received!\n");}
	 close(sock);

	 return bytes_received;
}

char *resolve(char *qname, char *server) {
	short port = 8080;
	unsigned char query_msg[MAX_BUFFER_SIZE]; 
	unsigned char response[MAX_BUFFER_SIZE];

	// build the query
	dns_rr_type type = 1;
	canonicalize_name(qname);
	int query_len = create_dns_query(qname,type,query_msg);

	// connect to the host
	int response_size = send_recv_message(query_msg,query_len,response,server,port);
	if(DEBUG_MODE){
		printf("BYTES RECEIVED: %d\n",response_size);
		printf("RESPONSE =");
		print_bytes(response,response_size);
	}

	// extract the ip address from the response
	char* ip_addr = get_answer_address(qname,type,response);

	return ip_addr;
}

int create_udp_socket(char* server, short port) {
	struct sockaddr_in resolver_addr = {
		.sin_family = AF_INET, // Internet Address Family
		.sin_port = htons(53), // DNS port, converted to big endian if host is little endian
		.sin_addr = inet_addr(server) // Converting the dot notation (e.g., 8.8.8.8) to binary
	};
	// PF_INET for Internet Protocol family
	// SOCK_DGRAM for datagram socket type
	// IPPROTO_UDP for explicitly indicating UDP protocol
	int sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock == -1) {
		perror("socket");
		exit(EXIT_FAILURE);
	}

	// Note that connect() on a UDP socket isn't required since it's
	// a connectionless protocol. However, doing this will make future send()
	// write(), read(), and recv() calls use the address we "connected" to. 
	// That way you no longer use sendto() or recvfrom(). If this is
	// not useful for your purposes, remove the connect() call.
	if (connect(sock, (struct sockaddr*)&resolver_addr, sizeof(resolver_addr)) == -1) {
		perror("connect");
		exit(EXIT_FAILURE);
	}
	return sock;
}


int main(int argc, char *argv[]) {
	char *ip;
	if (argc < 3) {
		fprintf(stderr, "Usage: %s <domain name> <server>\n", argv[0]);
		exit(1);
	}
	ip = resolve(argv[1], argv[2]);
	printf("%s => %s\n", argv[1], ip == NULL ? "NONE" : ip);
}
