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
int beginAuth(int sockfd, unsigned long int T_c_i, unsigned long int T_s);

// Function designed for chat between client and server.
void func(int sockfd, unsigned long int T_c_i, unsigned long int T_s, char *command)
{
    char buff[MAX];
    int response_auth = beginAuth(sockfd, T_c_i, T_s);

    //Check if the authentication succeeded
    if (response_auth != 200)
        printf("errore");

    int beginCommand(sockfd, command);
}

// Driver function
int main(int argc, char *argv[])
{

    int sockfd, connfd, len, port = -1;
    char client_passphrase[256], server_passphrase[256], *server_address = "0.0.0.0", *command = malloc(256);
    unsigned long int T_c_i, T_s;
    struct sockaddr_in servaddr, cli;

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
            if (strcmp("-e", argv[i]) == 0)
            {

                strcat(command, "EXEC ");
                strcat(command, argv[i + 1]);
                puts(command);
            }
        }
    }
    else
    {
        printf("missing arguments ( min 2 )\n");
        exit(0);
    }

    //Check if server address and port was setted
    if (strcmp(server_address, "0.0.0.0") == 0 || port == -1)
    {
        printf("Server address and port must be specified \n");
        exit(0);
    }

    //asking user to insert client-passphrase
    printf("Starting client...\nPlease insert a client passhprase: ");
    fgets(client_passphrase, sizeof(client_passphrase), stdin);

    printf("\n");

    //generation of token
    T_c_i = generateToken(client_passphrase);

    //asking user to insert server-passphrase
    printf("Please insert the server passhprase: ");
    fgets(server_passphrase, sizeof(server_passphrase), stdin);
    printf("\n");

    //generation of token
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
    bzero(&servaddr, sizeof(servaddr));

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

    // function for chat
    func(sockfd, T_c_i, T_s, command);

    // close the socket
    close(sockfd);
}

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

unsigned long int generateToken(const char *passphrase)
{
    unsigned long int hash = 69681;
    int c;

    while (c = *passphrase++)
        hash = ((hash << 5) + hash) + c;

    return hash;
}

int beginAuth(int sockfd, unsigned long int T_c_i, unsigned long int T_s)
{
    char buff[MAX];
    unsigned long int received_challenge, challenge_response, enc1, enc2;
    int response_code = 0;

    //Begin the authentication phase by sending HELO
    sprintf(buff, "HELO");
    write(sockfd, buff, sizeof(buff));
    bzero(buff, strlen(buff) + 1);

    recv(sockfd, &response_code, sizeof(response_code), 0);

    if (response_code != 300)
        return;

    //Get the challenge
    recv(sockfd, &received_challenge, sizeof(received_challenge), 0);

    printf("\nRicevuto response_code e challenge: %d, %lu\n", response_code, received_challenge);

    //Extract the challenge using T_s and calculate enc1 and enc2
    challenge_response = received_challenge ^ T_s;
    enc1 = T_s ^ T_c_i;
    enc2 = T_c_i ^ challenge_response;

    //Send the challenge back to the server and wait for the results
    sprintf(buff, "AUTH");
    write(sockfd, buff, sizeof(buff));

    write(sockfd, &enc1, sizeof(enc1));
    write(sockfd, &enc2, sizeof(enc2));

    recv(sockfd, &response_code, sizeof(response_code), 0);
    printf("\nRicevuto response_code: %d\n", response_code);
    return response_code;
}

int beginCommand(int sockfd, char *command)
{
}
