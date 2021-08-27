#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "basicTools.h"

#define MAX 256
#define COMMUNICATION_BUF_SIZE 10512
#define SA struct sockaddr

// Function designed for chat between client and server.
void comunicate(int sockfd, unsigned long int T_c_i, unsigned long int T_s, char *command)
{
    char buff[MAX];
    int response_auth = beginAuth(sockfd, T_c_i, T_s);

    //Check if the authentication succeeded
    if (response_auth != 200)
    {
        printf("Authentication failed\n");
        close(sockfd);
        return;
    }
    else
        printf("Authentication succeded\n");

    //If no command is provided by the user
    if (strcmp(command, "NONE") == 0)
    {
        printf("no command is specified, ending...\n");
        close(sockfd);
        return;
    }
    else
        beginCommand(sockfd, command);
}

//Function that handle the client side of the auth protocol
int beginAuth(int sockfd, unsigned long int T_c_i, unsigned long int T_s)
{
    char buff[MAX];
    unsigned long int received_challenge, challenge_response, enc1, enc2;
    int response_code = 0;

    //Begin the authentication phase by sending HELO

    sprintf(buff, "HELO");
    printf("sending %s\n", buff);
    send(sockfd, buff, sizeof(buff), 0);
    memset(buff, 0, strlen(buff) + 1);

    receiveNumberL(sockfd, &response_code);

    if (response_code != 300)
        return;

    //Get the challenge
    receiveNumberL(sockfd, &received_challenge);

    printf("Received response from server %d\n", response_code);
    printf("Received challenge from server\n");

    //Extract the challenge using T_s and calculate enc1 and enc2
    challenge_response = received_challenge ^ T_s;

    enc1 = T_s ^ challenge_response ^ T_c_i;
    enc2 = T_c_i ^ challenge_response;

    //Send the challenge back to the server and wait for the results
    sprintf(buff, "AUTH");
    send(sockfd, buff, sizeof(buff), 0);

    sendNumberL(sockfd, enc1);
    sendNumberL(sockfd, enc2);

    receiveNumberL(sockfd, &response_code);
    printf("Receiver response from server %d\n", response_code);
    return response_code;
}

//Function that handle the client side of the command
int beginCommand(int sockfd, char *command)
{
    int size = 0;

    char **commandAr = splitString(command, &size);

    if (size < 2)
    {
        printf("Invalid command, ending process...\n");
        return 0;
    }

    printf("\nsending command -%s- \n", commandAr[0]);

    //Set-up the right comunication protocol
    if (strcmp(commandAr[0], "EXEC") == 0)
    {
        //Executing exec command
        execC(sockfd, command);
    }
    else if (strcmp(commandAr[0], "LSF") == 0)
    {
        //Executing lsf command
        lsfC(sockfd, command);
    }
    else if (strcmp(commandAr[0], "UPLOAD") == 0)
    {
        uploadC(sockfd, commandAr, size);
    }
    else if (strcmp(commandAr[0], "DOWNLOAD") == 0)
    {
        downloadC(sockfd, commandAr, size);
    }
    else
    {
        printf("command not supported, ending process...\n");
        return 0;
    }

    free(commandAr);
    close(sockfd);
    return 1;
}

/* ////COMMAND FUNCTIONS\\\\ */

int execC(int sockfd, char *command)
{
    int response_code;
    char out_buff[COMMUNICATION_BUF_SIZE];
    int msgSize;

    //Sending the command to the server
    send(sockfd, command, strlen(command), 0);

    //Waiting for server response
    receiveNumberL(sockfd, &response_code);
    if (response_code != 300)
    {
        printf("Some error with the server has occurred ( %d ) \n", response_code);
        return 0;
    }
    printf("resp : %d\n", response_code);

    printf("command results \n\n");
    do
    {
        memset(out_buff, 0, COMMUNICATION_BUF_SIZE);
        msgSize = recv(sockfd, out_buff, COMMUNICATION_BUF_SIZE, 0);
        if (msgSize > 0)
        {
            printf("%s", out_buff);

            char *msg = subString(out_buff, 0, msgSize);
            //End the communication if the server sends the terminator string
            //If the output is over, msg should match " \r\n.\r\n "
            if (strcmp(msg, "\r\n.\r\n") == 0)
            {
                free(msg);
                break;
            }
            free(msg);
        }

    } while (msgSize > 0);

    printf("Done\n");
    return 1;
}

int lsfC(int sockfd, char *command)
{
    int response_code;
    char out_buff[COMMUNICATION_BUF_SIZE];
    memset(out_buff, 0, COMMUNICATION_BUF_SIZE);

    //Sending the command to the server
    send(sockfd, command, strlen(command), 0);

    //Waiting for server response
    receiveNumberL(sockfd, &response_code);
    if (response_code != 300)
    {
        printf("Some error with the server has occurred ( %d ) \n", response_code);
        return 0;
    }
    printf("resp : %d\n", response_code);

    recv(sockfd, out_buff, COMMUNICATION_BUF_SIZE, 0);
    puts(out_buff);

    memset(out_buff, 0, COMMUNICATION_BUF_SIZE);
    printf("Done\n");
    return 1;
}

unsigned long int sizeC(int sockfd, char *command)
{
    int response_code;
    unsigned long int out_buff;

    //Sending the command to the server
    send(sockfd, command, strlen(command), 0);

    //Waiting for server response
    receiveNumberL(sockfd, &response_code);
    if (response_code != 300)
    {
        printf("Some error with the server has occurred ( %d ) \n", response_code);
        return 0;
    }
    printf("resp : %d\n", response_code);

    receiveNumberL(sockfd, &out_buff);
    printf("SIZE RECEIVED: %li\n", out_buff);

    printf("Done\n");
    return out_buff;
}

int downloadC(int sockfd, char **commandArr, int arSize)
{
    if (arSize != 3)
        return 0;

    unsigned long int file_size;
    int response_code;
    char *path_src = malloc(strlen(commandArr[1]));
    char *path_dest = malloc(strlen(commandArr[2]));
    char *buff = (char *)malloc(COMMUNICATION_BUF_SIZE);
    char *command = malloc(512);

    memset(path_dest, 0, sizeof(path_dest));
    memset(path_src, 0, sizeof(path_src));
    memset(command, 0, 512);

    strcat(path_src, commandArr[1]);
    strcat(path_dest, commandArr[2]);
    printf("opening the file %s \n", path_src);

    //Preparing the DOWNLOAD command
    FILE *f = fopen(path_src, "rb");
    if (f != NULL)
    {
        /* 1) Get the filesize */
        fseek(f, 0, SEEK_END);
        file_size = ftell(f);
        fseek(f, 0, SEEK_SET);

        /* 2) Build the command and send it to the server */
        sprintf(command, "DOWNLOAD %s %li", path_dest, file_size);
        printf("Command is %s -\n", command);

        send(sockfd, command, strlen(command), 0);

        //Wait for the server for accepting the download
        receiveNumberL(sockfd, &response_code);
        if (response_code == 200)
        {

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

            //Waiting for server to confirm the reception
            receiveNumberL(sockfd, &response_code);

            if (response_code != 200)
            {
                printf("The operation hasn't succeded properly\n");
            }

            printf("Upload complete\n");
        }
        else
        {
            printf("Some error with the server has occurred ( %d ) \n", response_code);
        }
        fclose(f);
    }
    else
    {
        printf("cannot access the file\n");
    }
    free(buff);
    free(path_dest);
    free(path_src);
    free(command);
    return response_code;
}

int uploadC(int sockfd, char **commandArr, int arSize)
{
    if (arSize != 3)
        return 0;

    unsigned long int file_size;
    int response_code;
    char *path_src = malloc(strlen(commandArr[1]));
    char *path_dest = malloc(strlen(commandArr[2]));
    char *command = malloc(512);
    char *buff = malloc(COMMUNICATION_BUF_SIZE);

    memset(path_dest, 0, strlen(commandArr[1]));
    memset(path_src, 0, strlen(commandArr[2]));
    memset(buff, 0, COMMUNICATION_BUF_SIZE + 1);
    memset(command, 0, 512);

    strcat(path_src, commandArr[1]);
    strcat(path_dest, commandArr[2]);

    //Preparing the SIZE command
    sprintf(command, "SIZE %s", path_src);

    //Calling the right executioner for the SIZE command
    file_size = sizeC(sockfd, command);
    if (file_size == 0)
        return;

    /* ____________________ */

    memset(command, 0, 512);
    //Preparing the UPLOAD command
    sprintf(command, "UPLOAD %s %li", path_src, file_size);

    FILE *f = fopen(path_dest, "w");
    if (f != NULL)
    {
        send(sockfd, command, strlen(command), 0);

        //Wait for the server sending 300
        receiveNumberL(sockfd, &response_code);

        if (response_code == 300)
        {
            int readed_bytes = 0;
            while (readed_bytes < file_size)
            {
                int recSize = recv(sockfd, buff, COMMUNICATION_BUF_SIZE + 1, 0);

                if (recSize <= 0)
                    break;

                fwrite(buff, strlen(buff), 1, f);
                fflush(f);

                memset(buff, 0, COMMUNICATION_BUF_SIZE + 1);
                readed_bytes += recSize;
            }
            fclose(f);
            printf("Download complete\n");
        }
        else
        {
            printf("Some error with the server has occurred ( %d ) \n", response_code);
        }
    }
    else
    {
        printf("cannot access the file\n");
    }

    free(path_dest);
    free(path_src);
    free(command);
    free(buff);
    return response_code;
}

/////////////////////////////////////////////////
int main(int argc, char *argv[])
{

    int sockfd, connfd, len, port = -1;
    char client_passphrase[256], server_passphrase[256], *server_address = "0.0.0.0", *command = malloc(512);
    unsigned long int T_c_i, T_s;
    struct sockaddr_in servaddr, cli;

    memset(command, 0, 512);
    strcat(command, "");

    if (argc > 2)
    {
        for (int i = 0; i < argc; i++)
        {
            if (strcmp("-p", argv[i]) == 0)
            {
                if (is_a_number(argv[i + 1]) == 0)
                    port = atoi(argv[i + 1]);
            }
            if (strcmp("-h", argv[i]) == 0)
            {
                server_address = argv[i + 1];
            }

            //START THE PARSE OF THE COMMAND

            //Exec command
            if (strcmp("-e", argv[i]) == 0)
            {
                strcat(command, "EXEC ");
                strcat(command, argv[i + 1]);
            }
            //Path command
            else if (strcmp("-l", argv[i]) == 0)
            {

                strcat(command, "LSF ");
                strcat(command, argv[i + 1]);
            }
            //Download command
            else if (strcmp("-d", argv[i]) == 0)
            {

                strcat(command, "DOWNLOAD ");
                strcat(command, argv[i + 1]);
                strcat(command, " ");
                strcat(command, argv[i + 2]);
            }
            //Upload command
            else if (strcmp("-u", argv[i]) == 0)
            {

                strcat(command, "UPLOAD ");
                strcat(command, argv[i + 1]);
                strcat(command, " ");
                strcat(command, argv[i + 2]);
            }
        }
    }
    else
    {
        printf("missing arguments ( min 2 )\n");
        exit(0);
    }

    if (strlen(command) == 0)
        strcat(command, "NONE");
    strcat(command, "\0");

    //Check if server address and port was setted
    if (strcmp(server_address, "0.0.0.0") == 0 || port == -1)
    {
        printf("Server address and port must be specified \n");
        exit(0);
    }

    /* TOKEN GENERATION PHASE */

    //asking user to insert client-passphrase
    printf("Starting client...\nPlease insert a client passhprase: ");
    fgets(client_passphrase, sizeof(client_passphrase), stdin);
    //generation of client token
    T_c_i = generateToken(client_passphrase);

    //asking user to insert server-passphrase
    printf("\nPlease insert the server passhprase: ");
    fgets(server_passphrase, sizeof(server_passphrase), stdin);
    printf("\n");
    //generation of server token
    T_s = generateToken(server_passphrase);

    // socket create and varification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        printf("socket creation failed...\n");
        exit(0);
    }
    else
        printf("Socket successfully created..\n");

    memset(&servaddr, 0, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(server_address);
    servaddr.sin_port = htons(port);

    // connect the client socket to server socket
    if (connect(sockfd, (SA *)&servaddr, sizeof(servaddr)) != 0)
    {
        printf("connection with the server failed...\n");
        exit(0);
    }
    else
        printf("connected to the server..\n");

    comunicate(sockfd, T_c_i, T_s, command);

    // close the socket
    close(sockfd);
}
