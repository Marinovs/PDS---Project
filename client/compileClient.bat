gcc -c basicTools.c
gcc -c client.c
gcc basicTools.o client.o -o clientStart -lws2_32