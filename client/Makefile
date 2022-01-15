CC = gcc
LIBSDIR = ../libs
CFLAGS	= -c

clientStart: client.o basicTools.o 
	$(CC) client.o basicTools.o -o clientStart
	
client.o: client.c
	$(CC) $(CFLAGS) client.c
	
basicTools.o: $(LIBSDIR)/basicTools.c  $(LIBSDIR)/basicTools.h	
	$(CC) $(CFLAGS) $(LIBSDIR)/basicTools.c
	

clean:
	rm -rf *.o clientStart

