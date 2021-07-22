#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "basicTools.h"
#include "threadPool.h"

#define MAX 256
#define COMMUNICATION_BUF_SIZE 512
#define SA struct sockaddr

#define DEFAULT_MAX_THREADS 10
#define DEFAULT_TCP_PORT 8888
#define DEFAULT_LOG_PATH "tmp/server.log"

pthread_mutex_t lock;

typedef struct threadArgs
{
    int sock;
    unsigned long int T_s;
    char **client_info;
};

//Send 400 through the socker
int sendResponse(int sockfd, int response)
{
    return write(sockfd, &response, sizeof(int));
}

// Function designed for chat between client and server.
void *handleConnection(void *p_args)
{

    pthread_mutex_lock(&lock);

    struct threadArgs *args = (struct threadArgs *)p_args;
    unsigned long int T_s = args->T_s;
    int sockfd = args->sock;
    pid_t pid;
    pthread_t tid;

    char buff[COMMUNICATION_BUF_SIZE];
    char *command;

    //Prepare server to begin the auth phase
    int response_code = handleAuth(T_s, sockfd);

    //Exit if the authentication has failed
    if (response_code == 400)
    {
        printf("authentication has failed\n");
        close(sockfd);
        return;
    }

    printf("authentication has succeded\n");

    //Getting the command, if no command is sent, close the connection
    bzero(buff, COMMUNICATION_BUF_SIZE);

    int reader = recv(sockfd, buff, COMMUNICATION_BUF_SIZE, 0);

    if (reader <= 0)
    {
        sendResponse(sockfd, 400);
        close(sockfd);
        return;
    }
    else
        command = subString(buff, 0, reader);

    printf("User sent: %s\n", command);

    // WRITE INTO THE LOG

    int comSize = 0;
    char **commandAr = splitString(command, &comSize);

    if (comSize < 2)
    {
        printf("Invalid command, ending process...\n");
        sendResponse(sockfd, 400);
        close(sockfd);
        return;
    }

    //Set-up the right comunication protocol
    if (strcmp(commandAr[0], "EXEC") == 0)
    {

        printf("User sent an EXEC command\n");
        handleExec(sockfd, commandAr, comSize);
    }
    else if (strcmp(commandAr[0], "LSF") == 0)
    {

        printf("User sent an LSF command\n");
    }
    else if (strcmp(commandAr[0], "SIZE") == 0)
    {

        printf("User sent an SIZE command\n");
    }
    else if (strcmp(commandAr[0], "UPLOAD") == 0)
    {

        printf("User sent an UPLOAD command\n");
    }
    else if (strcmp(commandAr[0], "DOWNLOAD") == 0)
    {

        printf("User sent an DOWNLOAD command\n");
    }
    else
    {
        printf("command not supported, ending process...");
        sendResponse(sockfd, 400);
        pthread_mutex_unlock(&lock);
        close(sockfd);
        return;
    }

    pid = getpid();
    tid = pthread_self();
    writeToLog(commandAr[0], args->client_info, DEFAULT_LOG_PATH, pid, tid);

    // After the command, close the socket
    pthread_mutex_unlock(&lock);
    close(sockfd);
}

//Function that handle the authentication phases ( AUTH )
int handleAuth(unsigned long int T_s, int sockfd)
{
    char buff[MAX];
    int response_code = 400;

    //Waiting for client to send the HELO request to init the Authentication phase
    recv(sockfd, buff, MAX, 0);

    printf("received from client :  %s\n", buff);

    //Check the first message, must be HELO
    if (strcmp(buff, "HELO") == 0)
    {
        unsigned long int challenge = getRandom();
        unsigned long int response = T_s ^ challenge;

        bzero(buff, strlen(buff) + 1);

        printf("Invio 300 e challenge: %lu\n", response);

        //After preparing the challenge the server sends the challeng encoded with his private token
        sendResponse(sockfd, 300);
        write(sockfd, &response, sizeof(response));

        //Wait for the client to begin the AUTH phase
        recv(sockfd, buff, MAX, 0);
        puts(buff);

        if (strcmp("AUTH", buff) == 0)
        {
            unsigned long int enc1, enc2, T_c_i, received_challenge;
            recv(sockfd, &enc1, sizeof(enc1), 0);
            recv(sockfd, &enc2, sizeof(enc2), 0);
            //Calculate the client key with his key
            T_c_i = enc1 ^ T_s ^ challenge;
            received_challenge = T_c_i ^ enc2;

            //If the challenge is correct, the authentication is complete
            if (received_challenge == challenge)
                response_code = 200;
            else
                response_code = 400;

            sendResponse(sockfd, response_code);
        }
        return response_code;
    }
    return 400;
}

//Function that handle the exec command ( EXEC )
int handleExec(int sockfd, char **command, int size)
{
    char buff[COMMUNICATION_BUF_SIZE];
    char *commandToExec = malloc(256);

    //Composing the command with the args
    strcat(commandToExec, command[1]);
    for (int i = 2; i < size; i++)
    {
        strcat(commandToExec, " ");
        strcat(commandToExec, command[i]);
    }

    //Executing the command
    FILE *f = popen(commandToExec, "r");
    //If some error occurred
    if (!f)
    {
        int err = errno;
        write(sockfd, err, strlen(err));
        return err;
    }

    //Prepare client for receive the output
    sendResponse(sockfd, 300);

    //get the output and send it back to the client
    bzero(buff, COMMUNICATION_BUF_SIZE);
    while (fgets(buff, COMMUNICATION_BUF_SIZE, f) != NULL)
    {
        if (strlen(buff) > 0)
        {

            printf("sending : %s", buff);
            write(sockfd, buff, strlen(buff));
            bzero(buff, COMMUNICATION_BUF_SIZE);

            usleep(2000);
        }
    }

    //Close the stdout of the popen
    close(f);

    //End comunication
    char *endOfOut = "\r\n.\r\n";
    write(sockfd, endOfOut, sizeof(endOfOut));

    printf("end\n");
    return 1;
}

int main(int argc, char *argv[])
{

    int sockfd, client_sock, len, max_thread, port, print_token;
    char *config_path, *log_path, client_ip[12], client_port[6];
    char passphrase[256];
    unsigned long int T_s;
    struct sockaddr_in servaddr, cli;
    threadpool_t *pool;

    //Parsing args
    if (argc > 1)
    {
        for (int i = 0; i < argc; i++)
        {
            if (strcmp("-p", argv[i]) == 0)
            {
                if (is_a_number(argv[i + 1]) == 0)
                    port = atoi(argv[i + 1]);
            }
            if (strcmp("-n", argv[i]) == 0)
            {
                if (is_a_number(argv[i + 1]) == 0)
                    max_thread = atoi(argv[i + 1]);
            }
            if (strcmp("-c", argv[i]) == 0)
            {
                config_path = argv[i + 1];
            }
            if (strcmp("-s", argv[i]) == 0)
            {
                print_token = 1;
            }

            if (strcmp("-l", argv[i]) == 0)
            {
                log_path = argv[i + 1];
            }
        }
    }

    //asking user to insert passphrase
    printf("Starting server...\nPlease insert a passhprase: ");
    fgets(passphrase, sizeof(passphrase), stdin);

    //generation of token
    T_s = generateToken(passphrase);

    if (print_token == 1)
        printf("\nServer generates token T_s: %lu\n\n", T_s);

    //THREAD POOL GENERATION
    printf("Creation of %i threads...\n", max_thread);
    pool = threadpool_create(max_thread, 256, 0);

    //END OF POOL GENERATION

    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        printf("socket creation failed...\n");
        exit(0);
    }
    else
        printf("Socket successfully created..\n");
    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(DEFAULT_TCP_PORT);

    // Binding newly created socket to given IP and verification
    if ((bind(sockfd, (SA *)&servaddr, sizeof(servaddr))) != 0)
    {
        printf("socket bind failed...\n");
        exit(0);
    }
    else
        printf("Socket successfully binded..\n");

    // Now server is ready to listen and verification
    if ((listen(sockfd, 5)) != 0)
    {
        printf("Listen failed...\n");
        exit(0);
    }
    else
        printf("Server listening..\n");

    len = sizeof(cli);

    // Accept the data packet from client and verification
    while ((client_sock = accept(sockfd, (SA *)&cli, &len)))
    {
        printf("New user connected, spawning a thread...\n");

        // Getting client ip and client port
        strcpy(client_ip, inet_ntoa(cli.sin_addr));
        sprintf(client_port, "%u", ntohs(cli.sin_port));

        pthread_t sniffer_thread;

        struct threadArgs p_args;
        p_args.sock = client_sock;
        p_args.T_s = T_s;
        p_args.client_info = malloc(strlen(client_ip) * strlen(client_port));
        p_args.client_info[0] = client_ip;
        p_args.client_info[1] = client_port;

        if (threadpool_add(pool, &handleConnection, &p_args, 0) == 0)
        {
            pthread_mutex_lock(&lock);
            pthread_mutex_unlock(&lock);
        }
        else
        {
            perror("could not create thread");
            return 1;
        }
    }

    close(sockfd);
}