CC = gcc
CFLAGS = -g

all: echoserveri echoserverp echoservers

echoserveri: echoserveri.c echo.c csapp.c
	$(CC) $(CFLAGS) -o echoserveri echoserveri.c echo.c csapp.c -lpthread -lm

echoserverp: echoserverp.c echo.c csapp.c
	$(CC) $(CFLAGS) -o echoserverp echoserverp.c echo.c csapp.c -lpthread -lm

echoservers: echoservers.c echo.c csapp.c
	$(CC) $(CFLAGS) -o echoservers echoservers.c echo.c csapp.c -lpthread -lm
