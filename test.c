#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>

char *subString(char *input, int indexstart, int indexend)
{
    char *start = input + indexstart;
    char *substring = malloc(indexend - indexstart + 1);
    memcpy(substring, start, indexend);
    return substring;
}

int main(int argc, char *argv[])
{
    char buff[200];
    fgets(buff, 200, stdin);

    if (buff[0] == 'c')
        printf("%c\n", buff[0]);
}
