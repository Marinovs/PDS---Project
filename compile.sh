rm ./server/server.o
rm ./client/client.o

cd client
gcc -c client.c
gcc -c basicTools.c
gcc client.o basicTools.o -o clientStart

cd ../server
gcc -c server.c
gcc -c basicTools.c
gcc -c threadPool.c
gcc server.o basicTools.o threadPool.o -o serverStart -pthread

clear
