#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>


#include "dns.h"

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
	 char name_buffer[BUFFER_MAX];
	 memset(name_buffer,0,BUFFER_MAX);
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
		 total_len++;
		 strncpy(wire_ptr,cur_label,len);
		 wire_ptr += len;
		 cur_label = strtok(NULL,delim);
		 total_len += len;
	 }
	 // add the terminating 00 to the end
	 char c = 0x00;
	 memcpy(wire_ptr,&c,sizeof(char));
	 total_len++;
	 
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
	unsigned char* name = (char*)malloc(sizeof(char)*BUFFER_MAX);
	memset(name,0,BUFFER_MAX);
	char* name_ptr = name;
	char* delim = ".";
	wire_ptr += *indexp;
	int label_len = (int)*wire_ptr;
	while(label_len != 0){
		if(label_len >= COMPRESSED_VAL){
			// we are looking at a pointer, get the offset and move the pointer
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
	if(((int)(*wire_ptr) < COMPRESSED_VAL) && !query_only){
		(*indexp) += 11;
	}
	rr.name = name_ascii_from_wire(wire,indexp);
	wire_ptr = (wire + *indexp);
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
		rr.ttl = SWAP_ENDIAN_32(rr.ttl);
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

	 int len = 0;
	 // insert the name
	 int name_len = name_ascii_to_wire(rr.name,wire);
	 len += name_len;
	 if(name_len == strlen(rr.name)){len++;}
	 // insert the type 
	 short type = htons(rr.type);
	 memcpy((wire+len),&type,sizeof(short));
	 len += sizeof(short);
	 // insert the class 
	 short class = htons(rr.class);
	 memcpy((wire+len),&class,sizeof(short));
	 len += sizeof(short);
	 // insert the time to live
	 int ttl = SWAP_ENDIAN_32(rr.ttl);
	 memcpy((wire+len),&ttl,sizeof(int));
	 len += sizeof(int);
	 // insert the record data length
	 short length = htons(rr.rdata_len);
	 memcpy((wire+len),&length,sizeof(short));
	 len += sizeof(short);
	 // insert the record data
	 memcpy((wire+len),rr.rdata,rr.rdata_len);
	 len += rr.rdata_len;

	 return len; 

}
