rm ./server/server.o
rm ./client/client.o

gcc ./server/server.c -o ./server/server.o
gcc ./client/client.c -o ./client/client.o

clear
gnome-terminal -- ./server/server.o

gnome-terminal -- ./client/client.o -p 8888 -h 127.0.0.1
