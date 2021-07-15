rm ./server/server.o
rm ./client/client.o

gcc ./server/server.c -o ./server/server.o -pthread
gcc ./client/client.c -o ./client/client.o

clear
