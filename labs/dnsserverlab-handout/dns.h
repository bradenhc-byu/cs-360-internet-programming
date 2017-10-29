/*
 * Helper functions for the DNS Server lab - CS 360
 * Implemented by: Braden Hitchcock
 * Date: 3.11.2017
 * 
*/

#define BUFFER_MAX			1024
#define COMPRESSED_VAL		192
#define QUESTION 			1		// use when extracting a question rr with rr_from_wire
#define RESOURCE_RECORD		0		// use when extracting a resource record in full

#define BITS_IN_CHAR	8
#define IS_POINTER(s)	((s)& 0xc0)
#define SET_QUERY(s)	((s) & 0x7FFF)
#define IS_QUERY(s)		!((s) & 0x8000)
#define SET_RESPONSE(s)	((s) | 0x8000)
#define IS_RESPONSE(s)	((s) & 0x8000)
#define OPCODE(s)		(((s) & 0x7800)>>11)
#define FLAGS(s)		((s) & 0x07f0)
#define AA_SET(f)		((f) & 0x0400)
#define TC_SET(f)		((f) & 0x0200)
#define RD_SET(f)		((f) & 0x0100)
#define SET_RD(f)		((f) | 0x0100)
#define RA_SET(f)		((f) & 0x0080)
#define Z_ST(f)			((f) & 0x0040)
#define AD_SET(f)		((f) & 0x0020)
#define CD_SET(f)		((f) & 0x0010)
#define RCODE(c)		((c) & 0x000f)
#define SET_FORMERR(c)	((c)| 0x0001)
#define SET_NXDOMAIN(c)	((c) | 0x0003)

#define SWAP_ENDIAN_32(num)	(((num>>24)&0xff) | ((num<<8)&0xff0000) | ((num>>8)&0xff00) | ((num<<24)&0xff000000)) 

#define TYPE_A			1
#define CNAME			5

#define DEBUG_MODE		0

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

void print_bytes(unsigned char *bytes, int byteslen);
void canonicalize_name(char *name);
int name_ascii_to_wire(char *name, unsigned char *wire);
char *name_ascii_from_wire(unsigned char *wire, int *indexp);
dns_rr rr_from_wire(unsigned char *wire, int *indexp, int query_only);
int rr_to_wire(dns_rr rr, unsigned char *wire, int query_only);
