CLIENT_OBJS = chatc.o lib/chat-display.o
SERVER_OBJS = chatd.o lib/linkedlist.o
CC = gcc
DEBUG = -g
CFLAGS = -Wall -c $(DEBUG)
LFLAGS = -Wall $(DEBUG)

all : server client

server : $(SERVER_OBJS)
	$(CC) $(LFLAGS) $(SERVER_OBJS) -o chatd

client : $(CLIENT_OBJS)
	$(CC) $(LFLAGS) $(CLIENT_OBJS) -o chat-client -lcurses

chatd.o : config.h lib/linkedlist.h
	$(CC) $(CFLAGS) chatd.c

chatc.o : config.h lib/chat-display.o
	$(CC) $(CFLAGS) chatc.c

lib/linkedlist.o : lib/linkedlist.h
	cd lib; $(CC) $(CFLAGS) linkedlist.c

lib/chat-display.o :
	

clean:
	    \rm *.o lib/linkedlist.o chatd chat-client

srctar:
	tar cjvf cscorley_src.tar.bz2 *.h *.c lib/*.h lib/*.c lib/chat-display.o makefile

tar: server client
	tar cjvf cscorley_build.tar.bz2 chatd chatc
