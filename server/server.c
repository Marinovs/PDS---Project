#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#define MAX 256
#define COMMUNICATION_BUF_SIZE 512
#define SA struct sockaddr

#define DEFAULT_MAX_THREADS 10
#define DEFAULT_TCP_PORT 8888
#define DEFAULT_LOG_PATH "/tmp/server.log"

//Simple function for generating an hash token from a string
unsigned long int generateToken(const char *passphrase)
{
    unsigned long int hash = 69681;
    int c;

    while (c = *passphrase++)
        hash = ((hash << 5) + hash) + c;

    return hash;
}

//Return a random value
unsigned long int getRandom()
{
    srand(time(NULL));
    return (unsigned long int)rand();
}

//Return the substring between indexstart and indexend
char *subString(char *input, int indexstart, int indexend)
{
    char *start = input + indexstart;
    char *substring = malloc(indexend - indexstart + 1);
    memcpy(substring, start, indexend);
    return substring;
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

//Split a string by spaces, save the size in finalSize and return the bidimensional array containing all the words
char **splitString(char *originalString,int *finalSize){
   
   //Calculate how many words would be created
   int k = 0;
   for(int z = 0; z < strlen(originalString); z++)
   	if(isspace(originalString[z])) k++;
   k++;
   *finalSize = k;
   
   //Creating a bidimensional array with K rows, each rows is at best larger as the originalString ( no delimiter at all )
   char **res = (char **)malloc(k+1 * sizeof(char *));
   for (int i=0; i<k+1; i++)
         res[i] = (char *)malloc(strlen(originalString) * sizeof(char));
   
  
   //if(!k) return res;      
         
   //Get a substring of the original string each time with strtok, and store it in the 2D array
   int i = 0;
   char * token = strtok(originalString, " ");
   while( k > 0 ) {
      res[i] = token;
      token = strtok(NULL, " ");
      i++;
      k--;
   }
   return res; 
}

//Send 400 through the socker
int sendResponse(int sockfd, int response){
    return write(sockfd, &response, sizeof(int), 0);
}


////////////END UTILIY FUNCTION///////////////////




// Function designed for chat between client and server.
void func(int sockfd, unsigned long int T_s)
{
    char buff[COMMUNICATION_BUF_SIZE];
    char *command;
    int reader;

    //Prepare server to begin the auth phase
    int response_code = handleAuth(T_s, sockfd);

    //Exit if the authentication has failed
    if (response_code != 200)
        return;


    //Getting the command
    reader = recv(sockfd, buff, COMMUNICATION_BUF_SIZE, 0);
    if(reader == 0) sendResponse(sockfd, 400);
    else{
        command = subString(buff, 0, reader);
    }
    printf("User sent: %s\n", command);

    
    
    int comSize = 0;
    char **commandAr = splitString(command, &comSize);

    if(comSize < 2)
    {
        printf("Invalid command, ending process...\n");
        sendResponse(sockfd, 400);
        return;
    }
    for(int i = 0; i < comSize;i++){
        printf("%s\n", commandAr[i]);
    }


    //Set-up the right comunication protocol
    if(strcmp(commandAr[0], "EXEC") == 0){

        printf("User sent an EXEC command\n");

    }
    else if(strcmp(commandAr[0], "LSF") == 0){
        
        printf("User sent an LSF command\n");

    }
    else if(strcmp(commandAr[0], "SIZE") == 0){
        
        printf("User sent an SIZE command\n");

    }
    else if(strcmp(commandAr[0], "UPLOAD") == 0){
        
        printf("User sent an UPLOAD command\n");

    }
    else if(strcmp(commandAr[0], "DOWNLOAD") == 0){
        
        printf("User sent an DOWNLOAD command\n");

    }
    else{
        printf("command not supported, ending process...");
        sendResponse(sockfd, 400);
        return;
    }
}


//Function that handle the authentication phases
int handleAuth(unsigned long int T_s, int sockfd)
{
    char buff[MAX];
    int response_code;

    //Waiting for client to send the HELO request to init the Authentication phase
    recv(sockfd, buff, MAX, 0);

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
    return 0;
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