#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
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

