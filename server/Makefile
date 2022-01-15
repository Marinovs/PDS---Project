CC = gcc
LIBSDIR = ../libs
CFLAGS	= -c

serverStart: server.o basicTools.o threadPool.o
	$(CC) server.o basicTools.o threadPool.o -o serverStart -pthread
	
server.o: server.c
	$(CC) $(CFLAGS) server.c
	
basicTools.o: $(LIBSDIR)/basicTools.c  $(LIBSDIR)/basicTools.h	
	$(CC) $(CFLAGS) $(LIBSDIR)/basicTools.c
	
threadPool.o: threadPool.c threadPool.h
	$(CC) $(CFLAGS) threadPool.c
	

clean:
	rm -rf *.o serverStart

