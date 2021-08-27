gcc -c basicTools.c
gcc -c threadPool.c
gcc -c server.c
gcc basicTools.o threadPool.o server.o -o serverStart -lws2_32