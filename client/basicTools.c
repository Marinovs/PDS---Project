#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "basicTools.h"

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
char **splitString(char *originalString, int *finalSize)
{

    char *x = malloc(strlen(originalString) + 1);
    strcpy(x, originalString);
    strcat(x, "\0");

    //Calculate how many words would be created
    int k = 0;
    for (int z = 0; z < strlen(x); z++)
        if (isspace(x[z]))
            k++;
    k++;
    *finalSize = k;

    //Creating a bidimensional array with K rows, each rows is at best larger as the originalString ( no delimiter at all )
    char **res = (char **)malloc(k + 1 * sizeof(char *));
    for (int i = 0; i < k + 1; i++)
        res[i] = (char *)malloc(strlen(x) * sizeof(char));

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

int receiveNumberL(int sockfd, unsigned long int *number)
{

    char numberStr[64];
    memset(numberStr, 0, 64);
    recv(sockfd, numberStr, 64, 0);
    *number = strtoul(numberStr, NULL, 10);
    return 1;
}

int sendNumberL(int sockfd, unsigned long int number)
{

    char numberStr[64];
    memset(numberStr, 0, 64);
    sprintf(numberStr, "%lu", number);
    send(sockfd, numberStr, 64, 0);
    return 1;
}