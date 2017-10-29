HEADERS = http_server.h
OBJECTS = http_server.o

default: server

%.o: %.c $(HEADERS)
	gcc -c $< -o $@

server: $(OBJECTS)
	gcc $(OBJECTS) -o $@

clean:
	-rm -f $(OBJECTS)
	-rm -f server
