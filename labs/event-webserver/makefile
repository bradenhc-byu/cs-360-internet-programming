HEADERS = http_server.h
OBJECTS = http_server.o

default: server

%.o: %.c $(HEADERS)
	gcc -pthread -g -c $< -o $@

server: $(OBJECTS)
	gcc -pthread -g $(OBJECTS) -o $@
	-rm -f $(OBJECTS)

clean:
	-rm -f $(OBJECTS)
	-rm -f server
