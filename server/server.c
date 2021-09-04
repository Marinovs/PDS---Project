#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <math.h>
#include <signal.h>
#include <fcntl.h>

#include <sys/types.h>
#include <dirent.h>

#include "basicTools.h"
#include "threadPool.h"

#define MAX 256
#define COMMUNICATION_BUF_SIZE 512
#define SA struct sockaddr

#define DEFAULT_MAX_THREADS 10
#define DEFAULT_TCP_PORT 8888
#define DEFAULT_LOG_PATH "tmp/server.log"

int isRestarted = 0;

typedef struct threadArgs
{
    int sock;
    unsigned long int T_s;
    char **client_info;
    threadpool_t *pool;
};

void handle_sighup(int signal)
{

    if (signal == SIGHUP)
    {
        printf("SIGHUP RECEIVED");
        isRestarted = 1;
    }
}

void reloadServer()
{
    struct sigaction sa;

    // Setup the sighub handler
    sa.sa_handler = &handle_sighup;

    // Block every signal during the handler
    sigfillset(&sa.sa_mask);

    // Intercept SIGHUP and SIGINT
    if (sigaction(SIGHUP, &sa, NULL) == -1)
    {
        perror("Error: cannot handle SIGHUP"); // Should not happen
    }
}

//Send 400 through the socker
int sendResponse(int sockfd, int response)
{
    return sendNumberL(sockfd, response);
}

//Send the popen status to che client
int sendStatus(int sockfd, int status)
{
    int response = status;
    char *responseC = calloc(0, 10);

    if (status == 0)
        return 1;

    if (status == -1)
    {
        perror("pclose");
    }
    else if (WIFSIGNALED(status))
    {
        response = WTERMSIG(status);
        printf("terminating signal: %d\n", WTERMSIG(status));
    }
    else if (WIFEXITED(status))
    {
        response = WEXITSTATUS(status);
        printf("exit with status: %d\n", WEXITSTATUS(status));
    }
    else
    {
        response = -1;
        printf("unexpected: %d\n", status);
    }

    sprintf(responseC, "An error occurred during the execution ( %d )\n", response);
    return send(sockfd, responseC, strlen(responseC), 0);
}

// Function designed for chat between client and server.
void *handleConnection(void *p_args)
{
    struct threadArgs *args = (struct threadArgs *)p_args;
    threadpool_t *pool = (threadpool_t *)args->pool;
    unsigned long int T_s = args->T_s;
    int sockfd = args->sock;
    pthread_t tid;
    char buff[COMMUNICATION_BUF_SIZE];

    char **commandAr;
    int comSize = 0;

    //Prepare server to begin the auth phase
    int response_code = handleAuth(T_s, sockfd);

    //Exit if the authentication has failed
    if (response_code == 400)
    {
        printf("authentication has failed\n");
        close(sockfd);
        free(args->client_info);
        return;
    }

    printf("authentication has succeded\n");
    do
    {
        char *command;
        printf("\nListening for command...\n");

        //Getting the command, if no command is sent, close the connection
        memset(buff, 0, COMMUNICATION_BUF_SIZE);

        int reader = recv(sockfd, buff, COMMUNICATION_BUF_SIZE, 0);

        if (reader <= 0)
        {
            sendResponse(sockfd, 400);
            close(sockfd);
            free(args->client_info);
            return;
        }
        else
            command = subString(buff, 0, reader);

        printf("User sent : %s\n", command);

        commandAr = splitString(command, &comSize);

        if (comSize >= 2)
        {
            // send INTO THE LOG
            tid = pthread_self();
            writeToLog(commandAr[0], args->client_info, tid, (pool->logLock));

            //Set-up the right comunication protocol
            if (strcmp(commandAr[0], "EXEC") == 0)
            {
                printf("User sent an EXEC command\n");
                handleExec(sockfd, commandAr, comSize);
                break;
            }
            else if (strcmp(commandAr[0], "LSF") == 0)
            {
                printf("User sent a LSF command\n");
                handleLSF(sockfd, commandAr, comSize);
                break;
            }
            else if (strcmp(commandAr[0], "SIZE") == 0)
            {
                printf("User sent a SIZE command\n");
                if (handleSize(sockfd, commandAr, comSize) == 0)
                {
                    free(command);
                    free(commandAr);
                    break;
                }
            }
            else if (strcmp(commandAr[0], "UPLOAD") == 0)
            {
                printf("User sent an UPLOAD command\n");
                handleUpload(sockfd, commandAr, comSize);
                break;
            }
            else if (strcmp(commandAr[0], "DOWNLOAD") == 0)
            {

                printf("User sent a DOWNLOAD command\n");
                handleDownload(sockfd, commandAr, comSize);
                break;
            }
            else
            {
                printf("command not supported, ending process...");
                sendResponse(sockfd, 400);
                break;
            }
        }
        else
        {
            printf("Invalid command, ending process...\n");
            sendResponse(sockfd, 400);
            break;
        }
        free(command);
        free(commandAr);
    } while (1);

    printf("THREAD : %lu ending listening\n", tid);

    free(args->client_info);
    close(sockfd);
}

//Function that handle the authentication phases ( AUTH )
int handleAuth(unsigned long int T_s, int sockfd)
{
    char buff[COMMUNICATION_BUF_SIZE];
    int response_code = 400;

    //Waiting for client to send the HELO request to init the Authentication phase
    recv(sockfd, buff, MAX, 0);

    printf("received from client :  %s\n", buff);

    //Check the first message, must be HELO
    if (strcmp(buff, "HELO") == 0)
    {
        unsigned long int challenge = getRandom();
        unsigned long int response = T_s ^ challenge;

        memset(buff, 0, strlen(buff) + 1);

        //After preparing the challenge the server sends the challeng encoded with his private token
        sendResponse(sockfd, 300);
        sendNumberL(sockfd, response);

        //Wait for the client to begin the AUTH phase
        recv(sockfd, buff, MAX, 0);

        if (strcmp("AUTH", buff) == 0)
        {
            unsigned long int enc1, enc2, T_c_i, received_challenge;
            receiveNumberL(sockfd, &enc1);
            receiveNumberL(sockfd, &enc2);

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
    else
        sendResponse(sockfd, 400);
    return 400;
}

//Function that handle the exec command ( EXEC )
int handleExec(int sockfd, char **commandArr, int size)
{
    printf("handling Exec\n");

    char *command = malloc(256);
    char buff[COMMUNICATION_BUF_SIZE];
    memset(command, 0, 256);

    //Composing the command with the args
    strcat(command, commandArr[1]);
    for (int i = 2; i < size; i++)
    {
        strcat(command, " ");
        strcat(command, commandArr[i]);
    }

    //Executing the command
    FILE *f = popen(command, "r");
    if (f != NULL)
    {
        //Prepare client for receive the output
        sendResponse(sockfd, 300);

        //get the output and send it back to the client
        while (1)
        {
            memset(buff, 0, COMMUNICATION_BUF_SIZE);
            usleep(2000);
            if (fgets(buff, COMMUNICATION_BUF_SIZE, f) == NULL)
            {
                //send the exit status in needed
                sendStatus(sockfd, pclose(f));

                //End comunication
                char *endOfOut = "\r\n.\r\n";
                send(sockfd, endOfOut, sizeof(endOfOut), 0);
                printf("communication ended\n");
                break;
            }
            if (strlen(buff) > 0)
            {
                printf("sending : %s", buff);
                send(sockfd, buff, strlen(buff), 0);
            }
        }
    }
    //If some error occurred
    else
    {
        int err = errno;
        printf("error during the command execution - %d \n", err);
        send(sockfd, err, strlen(err), 0);
    }
    free(command);
    return 1;
}

//Function that handle LSF command
int handleLSF(int sockfd, char **commandArr, int size)
{
    printf("handling LSF\n");
    char *result = malloc(256), *path = malloc(256);
    int reSize = 256;
    struct dirent *entry;

    memset(result, 0, 256);
    memset(path, 0, 256);

    strcat(path, commandArr[1]);

    DIR *dir = opendir(path);
    if (dir != NULL)
    {

        sendResponse(sockfd, 300);

        //Read all the entry in the dir
        while ((entry = readdir(dir)))
        {

            FILE *f;
            char *name = entry->d_name;
            long fileSize = 0;

            if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
                continue;

            //If the entry is a dir
            if (entry->d_type == 4)
            {
                strcat(result, "DIR ");
            }
            //if the entry is a file, get the size
            else if (entry->d_type == 8)
            {

                f = fopen(name, "r");
                if (f == NULL)
                    continue;

                //get file size
                char *size = malloc(32);

                fseek(f, 0, SEEK_END);
                sprintf(size, "%ld ", ftell(f));
                strcat(result, size);

                free(size);
                fclose(f);
            }

            //Increase the memory accordignly
            if (reSize < strlen(result) + strlen(name) + 32)
            {
                reSize += strlen(name) + 32;
                result = realloc(result, reSize);
            }

            strcat(result, name);
            strcat(result, "\r\n");
        }
        strcat(result, "\r\n.\r\n");

        send(sockfd, result, strlen(result), 0);

        closedir(dir);
    }
    else
    {
        int err = errno;
        sendResponse(sockfd, err);
    }

    free(result);
    free(path);
    return 1;
}

//Function that handle size command
int handleSize(int sockfd, char **commandArr, int size)
{
    printf("handling Size\n");

    char *path_src = malloc(strlen(commandArr[1]) + 1);
    memset(path_src, 0, sizeof(path_src));
    strcat(path_src, commandArr[1]);
    strcat(path_src, "\0");

    FILE *f = fopen(path_src, "rb");

    if (f != NULL)
    {
        //Prepare client for receive the output
        sendResponse(sockfd, 300);

        /* 1) Get the filesize */
        fseek(f, 0, SEEK_END);
        int file_size = ftell(f);
        fseek(f, 0, SEEK_SET);

        fclose(f);

        printf("Sending: %li bytes\n", file_size);
        sendNumberL(sockfd, &file_size);
        printf("communication ended\n");
    }
    else
    {
        printf("cannot access the file\n");
        sendResponse(sockfd, 400);
        return 0;
    }
    free(path_src);
    return 1;
}

//Function that handle the download of a file, uploaded from the client
int handleDownload(int sockfd, char **commandArr, int size)
{
    printf("handling Download\n");

    unsigned long int file_size = strtol(commandArr[2], NULL, 10);

    char *buff = (char *)malloc(COMMUNICATION_BUF_SIZE + 1);
    char *path_dest = malloc(strlen(commandArr[1] + 1));

    memset(path_dest, 0, strlen(path_dest));

    strcat(path_dest, commandArr[1]);

    FILE *f = fopen(path_dest, "wb");
    if (f != NULL)
    {
        sendResponse(sockfd, 200);

        //Server getting ready to receive the file
        int readed_bytes = 0;
        while (readed_bytes < file_size)
        {
            memset(buff, 0, COMMUNICATION_BUF_SIZE + 1);

            int recSize = recv(sockfd, buff, COMMUNICATION_BUF_SIZE + 1, 0);

            if (recSize <= 0)
                break;

            fwrite(buff, strlen(buff), 1, f);
            fflush(f);

            readed_bytes += recSize;
        }
        sendResponse(sockfd, 200);
        free(buff);
        fclose(f);
    }
    else
    {
        //If the directory doesn't exist
        printf("cannot access file %s\n", path_dest);
        sendResponse(sockfd, 400);
    }
    free(path_dest);
}

//Function that handle the uploaded of a file, from the server to the client
int handleUpload(int sockfd, char **commandArr, int size)
{
    printf("handling Upload\n");

    unsigned long int file_size = strtol(commandArr[2], NULL, 10);

    char *buff = (char *)malloc(COMMUNICATION_BUF_SIZE + 1);
    char *path_src = malloc(strlen(commandArr[1]));

    memset(path_src, 0, strlen(path_src));
    memset(buff, 0, COMMUNICATION_BUF_SIZE + 1);

    strcat(path_src, commandArr[1]);

    FILE *f = fopen(path_src, "r");
    if (f != NULL)
    {
        sendResponse(sockfd, 300);

        //Sending the file
        int sz = 0;
        memset(buff, 0, COMMUNICATION_BUF_SIZE + 1);
        while ((sz = fread(buff, COMMUNICATION_BUF_SIZE, 1, f)) > -1)
        {
            strcat(buff, "\0");
            //send the chunk to the server
            send(sockfd, buff, strlen(buff), 0);

            //If it didn't read all BUF SIZE bytes, the eof has come
            if (sz != 1)
                break;

            memset(buff, 0, COMMUNICATION_BUF_SIZE + 1);
        }

        fclose(f);
    }
    free(buff);
    free(path_src);
}

int main(int argc, char *argv[])
{
    int sockfd, client_sock, len, max_thread = MAX_THREADS, port = DEFAULT_TCP_PORT, print_token, forcePrint = 0;
    char *config_path, *log_path = DEFAULT_LOG_PATH, client_ip[12], client_port[6];
    char passphrase[256];
    unsigned long int T_s;
    struct sockaddr_in servaddr, cli;
    threadpool_t *pool;

    //asking user to insert passphrase
    printf("Starting server...\nPlease insert a passhprase: ");
    fgets(passphrase, sizeof(passphrase), stdin);
    //generation of token
    T_s = createToken(passphrase);

    if (print_token == 1)
        printf("\nServer generates token T_s: %lu\n\n", T_s);

    //DEAMONIZATION PHASE
    pid_t pid;
    /* Fork off the parent process */
    pid = fork();

    if (pid < 0)
        exit(EXIT_FAILURE);

    /* Success: Let the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);

    /* On success: The child process becomes session leader */
    if (setsid() < 0)
        exit(EXIT_FAILURE);

    /* Fork off for the second time*/
    pid = fork();

    /* An error occurred */
    if (pid < 0)
        exit(EXIT_FAILURE);

    /* Success: Let the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);

    //END OF DEAMONIZATION PHASE

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
            if (strcmp("-fp", argv[i]) == 0)
            {
                forcePrint = 1;
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
                //Check if the file is valid
                if (!checkDirectory(log_path))
                {
                    log_path = DEFAULT_LOG_PATH;
                    printf("The log directory is not valid, using the default one\n");
                }
            }
        }
    }

    //If the user doesn't specifically request it, close the output to fully run the server as a daemon
    if (forcePrint == 0)
    {
        fclose(stdout);
    }

startup:
    isRestarted = 0;
    if (config_path != NULL && strlen(config_path) > 0 && checkDirectory(config_path))
    {
        //printf("Config file found: %s, reading...\n", config_path);
        readConfig(&port, &max_thread, config_path);
    }

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
        printf("Socket successfully created...\n");

    memset(&servaddr, 0, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    printf("SERVER PORT IS : %d\n", ntohs(servaddr.sin_port));

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &isRestarted, sizeof(int)) < 0)
        error("setsockopt(SO_REUSEADDR) failed");

    //set socked not blocking
    fcntl(sockfd, F_SETFL, O_NONBLOCK);

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

    reloadServer();
    // Accept the data packet from client and verification
    while (1)
    {

        if ((client_sock = accept(sockfd, (SA *)&cli, &len)) > 0)
        {

            printf("\n\nNew user connected, spawning a thread...\n");

            // Getting client ip and client port
            strcpy(client_ip, inet_ntoa(cli.sin_addr));
            sprintf(client_port, "%u", ntohs(cli.sin_port));

            struct threadArgs p_args;

            p_args.sock = client_sock;
            p_args.T_s = T_s;
            p_args.client_info = malloc(strlen(client_ip) * strlen(client_port));

            p_args.client_info[0] = client_ip;
            p_args.client_info[1] = client_port;
            p_args.client_info[2] = log_path;
            p_args.pool = pool;

            if (!threadpool_add(pool, &handleConnection, &p_args, 0) == 0)
            {
                perror("could not add the task");
                return 1;
            }
        }

        if (isRestarted)
        {
            //threadpool_destroy(pool, 0);
            close(client_sock);
            close(sockfd);
            goto startup;
        }
    }

    close(sockfd);
}
