#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#define MAX 256
#define SA struct sockaddr

#define DEFAULT_MAX_THREADS 10
#define DEFAULT_TCP_PORT 8888
#define DEFAULT_LOG_PATH "/tmp/server.log"

int is_a_number(char *input);
unsigned long int generateToken(const char *passphrase);
int handleAuth(unsigned long int T_s, int sockfd);
unsigned long int getRandom();
char *subString(char *input, int indexstart, int indexend);

// Function designed for chat between client and server.
void func(int sockfd, unsigned long int T_s)
{
    char buff[MAX];

    //Prepare server to begin the auth phase
    int response_code = handleAuth(T_s, sockfd);

    //Exit if the authentication has failed
    if (response_code != 200)
        return;

    for (;;)
    {
        recv(sockfd, buff, MAX, 0);
        printf("User sent: %s", buff);

        if (buff == NULL || strlen(buff) < 4)
        {
            printf("Invalid command format");
            continue;
        }

        if (buff[0] == '-' && buff[2] == ' ')
        {
            if (buff[1] == 'e')
            {
                char *command = subString(buff, 3, strlen(buff));
                puts(command);

                system(command);
            }

            else if (buff[1] == 'l')
            {
            }
            else if (buff[1] == 'd')
            {
            }
            else if (buff[1] == 'u')
            {
            }
            else
            {
                puts("Command not found");
            }
        }
        else
        {
            puts("Invalid command format");
        }

        bzero(buff, MAX);
    }
}

int main(int argc, char *argv[])
{

    int sockfd, connfd, len, max_thread, port, print_token;
    char *config_path, *log_path;
    char passphrase[256];
    unsigned long int T_s;
    struct sockaddr_in servaddr, cli;

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
    connfd = accept(sockfd, (SA *)&cli, &len);
    if (connfd < 0)
    {
        printf("server acccept failed...\n");
        exit(0);
    }
    else
        printf("server acccept the client...\n");

    // Function for chatting between client and server
    func(connfd, T_s);

    // After chatting close the socket
    close(sockfd);
}

//Simple function for check if a string is a number
int is_a_number(char *input)
{
    int length = strlen(input);
    for (int i = 0; i < length; i++)
    {
        if (!isdigit(input[i]))
            return 1;
    }
    return 0;
}

//Simple function for generating an hash token from a string
unsigned long int generateToken(const char *passphrase)
{
    unsigned long int hash = 69681;
    int c;

    while (c = *passphrase++)
        hash = ((hash << 5) + hash) + c;

    return hash;
}

int handleAuth(unsigned long int T_s, int sockfd)
{
    char buff[MAX];
    int response_code;

    //Waiting for client to send the HELO request to init the Authentication phase
    recv(sockfd, buff, MAX, 0);

    //Check the message
    if (strcmp(buff, "HELO") == 0)
    {
        unsigned long int challenge = 900;
        unsigned long int response = T_s ^ challenge;

        bzero(buff, strlen(buff) + 1);

        printf("Invio 300 e challenge: %lu\n", response);

        //After preparing the challenge the server sends the challeng encoded with his private token
        response_code = 300;
        write(sockfd, &response_code, sizeof(int));
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
            T_c_i = enc1 ^ T_s;
            received_challenge = T_c_i ^ enc2;

            //If the challenge is correct, the authentication is complete
            if (received_challenge == challenge)
            {
                response_code = 200;
                write(sockfd, &response_code, sizeof(int), 0);
            }
            else
            {
                response_code = 400;
                write(sockfd, &response_code, sizeof(int), 0);
            }
        }
        return response_code;
    }

    return 0;
}

unsigned long int getRandom()
{
    srand(time(NULL));
    return (unsigned long int)rand();
}

char *subString(char *input, int indexstart, int indexend)
{
    char *start = input + indexstart;
    char *substring = malloc(indexend - indexstart + 1);
    memcpy(substring, start, indexend);
    return substring;
}