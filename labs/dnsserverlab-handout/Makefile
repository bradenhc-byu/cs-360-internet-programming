#
# Student makefile for DNS resolver lab
# Note: requires a 64-bit x86-64 system 
#
CC = gcc
CFLAGS = -g

all: server

server: dns.c server.c
	$(CC) $(CFLAGS) -o server dns.c server.c -lm 

clean:
	rm -f server
