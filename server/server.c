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

typedef struct threadArgs
{
    int sock;
    unsigned long int T_s;
    char **client_info;
    threadpool_t *pool;
};

//Send 400 through the socker
int sendResponse(int sockfd, int response)
{
    return write(sockfd, &response, sizeof(int));
}

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
    return write(sockfd, responseC, strlen(responseC));
}

// Function designed for chat between client and server.
void *handleConnection(void *p_args)
{

    struct threadArgs *args = (struct threadArgs *)p_args;

    threadpool_t *pool = (threadpool_t *)args->pool;

    unsigned long int T_s = args->T_s;
    int sockfd = args->sock;
    pid_t pid;
    pthread_t tid;
    char buff[COMMUNICATION_BUF_SIZE];
    char *command;
    int comSize = 0;

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

    char **commandAr = splitString(command, &comSize);
    if (comSize < 2)
    {
        printf("Invalid command, ending process...\n");
        sendResponse(sockfd, 400);
        close(sockfd);
        return;
    }

    // WRITE INTO THE LOG
    pid = getpid();
    tid = pthread_self();
    writeToLog(commandAr[0], args->client_info, pid, tid, (pool->logLock));

    //Set-up the right comunication protocol
    if (strcmp(commandAr[0], "EXEC") == 0)
    {
        printf("User sent an EXEC command\n");
        handleExec(sockfd, commandAr, comSize);
    }
    else if (strcmp(commandAr[0], "LSF") == 0)
    {
        printf("User sent an LSF command\n");
        handleLSF(sockfd, commandAr, comSize);
    }
    else if (strcmp(commandAr[0], "SIZE") == 0)
    {
        printf("User sent an SIZE command\n");
        handleSize(sockfd, commandAr, comSize);
    }
    else if (strcmp(commandAr[0], "UPLOAD") == 0)
    {

        printf("User sent an UPLOAD command\n");
        handleUpload(sockfd, commandAr, comSize);
    }
    else if (strcmp(commandAr[0], "DOWNLOAD") == 0)
    {

        printf("User sent an DOWNLOAD command\n");
        handleDownload(sockfd, commandAr, comSize);
    }
    else
    {
        printf("command not supported, ending process...");
        sendResponse(sockfd, 400);
        close(sockfd);
        return;
    }

    // After the command, close the socket
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

        //After preparing the challenge the server sends the challeng encoded with his private token
        sendResponse(sockfd, 300);
        write(sockfd, &response, sizeof(response));

        //Wait for the client to begin the AUTH phase
        recv(sockfd, buff, MAX, 0);

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
    bzero(command, 256);

    printf("size is %d\n", size);

    //Composing the command with the args
    strcat(command, commandArr[1]);
    for (int i = 2; i < size; i++)
    {
        strcat(command, " ");
        strcat(command, commandArr[i]);
    }

    //Executing the command
    FILE *f = popen(command, "r");

    //If some error occurred
    if (!f)
    {
        int err = errno;
        printf("error during the command execution - %d \n", err);
        write(sockfd, err, strlen(err));
        return err;
    }

    //Prepare client for receive the output
    sendResponse(sockfd, 300);

    //get the output and send it back to the client
    while (1)
    {
        bzero(buff, COMMUNICATION_BUF_SIZE);
        usleep(2000);
        if (fgets(buff, COMMUNICATION_BUF_SIZE, f) == NULL)
        {

            sendStatus(sockfd, pclose(f));

            //End comunication
            char *endOfOut = "\r\n.\r\n";
            write(sockfd, endOfOut, sizeof(endOfOut));
            printf("communication ended\n");
            break;
        }
        if (strlen(buff) > 0)
        {
            printf("sending : %s", buff);
            write(sockfd, buff, strlen(buff));
        }
    }

    //Close the stdout of the popen
    close(f);
    free(command);
    return 1;
}

//Function that handle LSF command
int handleLSF(int sockfd, char **commandArr, int size)
{
    printf("handling LSF\n");
    char buff[256], *command = calloc(0, 256), result[256], *path = calloc(0, 256);

    strcat(path, commandArr[1]);
    strcat(command, "ls -s --block-size=KB ");
    strcat(command, path);

    printf("command is %s \n", command);

    FILE *f = popen(command, "r");
    if (!f)
    {
        int err = errno;
        write(sockfd, err, strlen(err));
        return err;
    }

    //Prepare client for receive the output
    sendResponse(sockfd, 300);

    fgets(buff, 256, f);
    while (fgets(buff, 256, f) != NULL)
    {
        strcat(result, buff);
        strcat(result, "\r\n");
    }

    strcat(result, "\r\n.\r\n");

    printf("Sending: %s", result);
    write(sockfd, result, sizeof(result));
    fclose(f);
    printf("communication ended\n");

    return 1;
}

//Function that handle size command
int handleSize(int sockfd, char **commandArr, int size)
{
    printf("handling Size\n");
    char buff[256], *command = calloc(0, 256), *result = calloc(0, 256), *path = calloc(0, 256);
    int i = 0;
    unsigned long int path_size;

    strcat(path, commandArr[1]);
    strcat(command, "du -sb ");
    strcat(command, path);

    FILE *f = popen(command, "r");

    if (f == NULL)
    {
        int err = errno;
        write(sockfd, err, strlen(err));
        return err;
    }

    //Prepare client for receive the output
    sendResponse(sockfd, 300);

    while (fgets(buff, 256, f) != NULL)
    {
        strcat(result, buff);
    }

    fclose(f);

    while (isspace(result[i]) == 0)
        i++;

    result = subString(result, 0, i);
    path_size = atoi(result);

    printf("Sending: %li bytes\n", path_size);
    write(sockfd, &path_size, sizeof(path_size));
    printf("communication ended\n");

    return 1;
}

int handleDownload(int sockfd, char **commandArr, int size)
{
    int response_code, n;
    char out_buff[COMMUNICATION_BUF_SIZE], *path_src = malloc(256), *path_dest = malloc(256);

    strcat(path_src, commandArr[1]);
    strcat(path_dest, commandArr[2]);

    printf("Received file %s from client that will be stored in %s\n", commandArr[1], commandArr[2]);
    //sending path
    write(sockfd, path_src, 512);
    write(sockfd, path_dest, 512);

    //receive response
    recv(sockfd, &response_code, sizeof(response_code), 0);

    if (response_code != 300)
    {
        // we need to send response code 400?
        perror("File not found on client.");
        return 0;
    }

    FILE *f = fopen(commandArr[2], "w");

    if (f == NULL)
    {
        // we need to send response code 400?

        perror("File not found on server.");
        return 0;
    }

    while (1)
    {
        n = recv(sockfd, out_buff, sizeof(out_buff), 0);

        if (n <= 0)
        {
            break;
            return;
        }
        fprintf(f, "%s", out_buff);
        bzero(out_buff, COMMUNICATION_BUF_SIZE);
    }

    fclose(f);
    puts("Download done.");

    free(path_src);
    free(path_dest);
    return 1;
}

//Function that handle upload command
int handleUpload(int sockfd, char **commandArr, int size)
{
    int response_code = 300, n;
    char data[COMMUNICATION_BUF_SIZE] = {0}, *path_src = malloc(256), *path_dest = malloc(256);
    struct stat path_stat;

    strcat(path_src, commandArr[1]);
    strcat(path_dest, commandArr[2]);

    stat(path_src, &path_stat);

    if (S_ISREG(path_stat.st_mode) != 1)
    {
        perror("Not a file.");
        response_code = 400;
        write(sockfd, &response_code, sizeof(response_code));
        return 0;
    }

    printf("Sending file %s that will be stored in %s\n", path_src, path_dest);
    //sending path
    FILE *f = fopen(path_src, "r");

    if (f == NULL)
    {
        // we need to send response code 400?
        response_code = 400;
        write(sockfd, &response_code, sizeof(response_code));
        perror("File not found on server.");
        return 0;
    }

    //send response
    write(sockfd, &response_code, sizeof(response_code));

    //sending dest path
    write(sockfd, path_src, sizeof(path_src));
    write(sockfd, path_dest, sizeof(path_dest));

    while (fgets(data, COMMUNICATION_BUF_SIZE, f) != NULL)
    {
        if (send(sockfd, data, sizeof(data), 0) == -1)
        {
            perror("Error sending file.");

            response_code = 400;
            write(sockfd, &response_code, sizeof(response_code));
            return 0;
        }
        bzero(data, COMMUNICATION_BUF_SIZE);
    }

    fclose(f);
    puts("Upload done.");

    free(path_src);
    free(path_dest);
    return 1;
}


int checkDirectory(char *dir)
{
    FILE *file;
    if ((file = fopen(dir, "w")))
        fclose(file);
    else
    {
        return 0;
    }
    return 1;
}

int main(int argc, char *argv[])
{

    int sockfd, client_sock, len, max_thread = MAX_THREADS, port, print_token;
    char *config_path, *log_path = DEFAULT_LOG_PATH, client_ip[12], client_port[6];
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
                //Check if the file is valid
                if (!checkDirectory(log_path))
                {
                    log_path = DEFAULT_LOG_PATH;
                    printf("The log directory is not valid, using the default one\n");
                }
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

    close(sockfd);
}