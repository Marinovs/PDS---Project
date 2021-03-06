#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>

#include "basicTools.h"

//Simple function for generating an hash token from a string
char *subString(char *input, int indexstart, int indexend)
{
    char *start = input + indexstart;
    char *substring = myMalloc(0, indexend - indexstart + 1, 1);
    memcpy(substring, start, indexend - indexstart);
    strcat(substring, "\0");
    return substring;
}

//Simple function for generating an hash token from a string
unsigned long int generateToken(const char *passphrase)
{
    unsigned long int hash = 12;
    int c;

    while (c = *passphrase++)
    {
        hash = hash << 2;
        hash += hash + c;
    }

    return hash;
}

unsigned long int createToken(char *passphrase)
{
    int chunk = 8;
    int reminder = strlen(passphrase) % chunk;
    int pass = floor(strlen(passphrase) / chunk);
    unsigned long int finalHash = 1;

    for (int i = 0; i < pass; i++)
    {
        int start = i * chunk;
        int end = start + 8;
        char *nextStr = subString(passphrase, start, end);
        unsigned long int tmpHash = generateToken(nextStr);
        finalHash ^= tmpHash;
        finalHash += finalHash / strlen(passphrase);
    }
    finalHash ^= generateToken(subString(passphrase, pass * chunk, (pass * chunk) + reminder));
    return finalHash;
}

int checkDirectory(char *dir)
{
    FILE *file;
    if ((file = fopen(dir, "r")))
        fclose(file);
    else
    {
        return 0;
    }
    return 1;
}

//Return a random value
unsigned long int getRandom()
{
    srand(time(NULL));
    return (unsigned long int)rand();
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
char **splitString(char *originalString, int *finalSize)
{

    char *x = myMalloc(0, strlen(originalString) + 1, 0);
    strcpy(x, originalString);
    strcat(x, "\0");

    // Calculate how many words would be created
    int k = 0;
    for (int z = 0; z < strlen(x); z++)
        if (isspace(x[z]))
            k++;
    k++;
    *finalSize = k;

    // Creating a bidimensional array with K rows, each rows is at best larger as the originalString ( no delimiter at all )
    char **res = (char **)myMalloc(0, k + 1 * sizeof(char *), 0);
    for (int i = 0; i < k + 1; i++)
        res[i] = (char *)myMalloc(0, strlen(x) * sizeof(char), 0);

    //Get a substring of the original string each time with strtok, and store it in the 2D array
    int i = 0;
    char *token = strtok(x, " ");
    while (k > 0)
    {
        if (token == NULL)
        {
            i++;
            k--;
            continue;
        }
        strcpy(res[i], token);
        token = strtok(NULL, " ");
        i++;
        k--;
    }
    free(x);
    return res;
}

//Write to log thread_id, client_ip, client_port, type of request and timestamp
void writeToLog(char *type, char **client_info, pthread_t tid, pthread_mutex_t lock)
{
    //Get the lock for write on the file
    char *logpath = client_info[2];

    char *row = myMalloc(0, 256, 0);
    char *tid_s = myMalloc(0, 256, 0);
    char *time_s = myMalloc(0, 256, 0);

    sprintf(tid_s, "%lu", tid);
    sprintf(time_s, "%lu", time(NULL));

    strcat(row, "THREAD_ID: ");
    strcat(row, tid_s);

    strcat(row, "\tIP_ADDRESS: ");
    strcat(row, client_info[0]);

    strcat(row, "\tPORT: ");
    strcat(row, client_info[1]);

    strcat(row, "\tREQUEST_TYPE: ");
    strcat(row, type);

    strcat(row, "\tTIMESTAMP: ");
    strcat(row, time_s);
    strcat(row, "\n");

    //Get the lock on the file
    pthread_mutex_lock(&lock);

    //Write down the log onto the file
    FILE *f = fopen(logpath, "a");
    if (f == NULL)
    {
        perror("Error opening file!");
        free(row);
        free(tid_s);
        free(time_s);
        exit(1);
    }

    fprintf(f, "%s", row);
    fclose(f);

    free(row);
    free(tid_s);
    free(time_s);
    pthread_mutex_unlock(&lock);
}

//Read the file config
int readConfig(int *port, int *max_thread, char *path)
{
    char *buff = myMalloc(0, 256, 0);
    FILE *f = fopen(path, "r");

    if (f == NULL)
    {
        perror("Error");
    }
    while (fgets(buff, 256, f) != NULL)
    {
        char **row;
        int rowSize = 0;
        row = splitString(buff, &rowSize);
        if (rowSize >= 3)
        {
            if (strcmp(row[0], "TCP_PORT") == 0)
            {
                if (atoi(row[2]) > 0)
                    *port = atoi(row[2]);
            }
            else if (strcmp(row[0], "MAX_THREAD") == 0)
            {
                if (atoi(row[2]) > 0)
                    *max_thread = atoi(row[2]);
            }
        }
        else
        {
            puts("Config file format wrong.");
            return 0;
        }

        bzero(buff, 256);
    }

    fclose(f);
    return 1;
}

int receiveNumberL(int sockfd, unsigned long int *number)
{

    char *numberStr = myMalloc(0, 64, 0);
    recv(sockfd, numberStr, 64, 0);
    *number = strtoul(numberStr, NULL, 10);
    free(numberStr);
    return 1;
}

int sendNumberL(int sockfd, unsigned long int number)
{

    char *numberStr = myMalloc(0, 64, 0);
    sprintf(numberStr, "%lu", number);
    send(sockfd, numberStr, 64, 0);
    free(numberStr);
    return 1;
}

char *myMalloc(int fill, int size, int initString)
{

    char *str;

    if ((str = malloc(size)) == NULL)
    {
        perror("Error allocating memory");
        exit(0);
    }
    memset(str, fill, size);

    if (initString == 1 && size > 0)
        strcat(str, "");

    return str;
}
