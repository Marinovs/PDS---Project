#ifndef BASICTOOLS_H
#define BASICTOOLS_H

unsigned long int generateToken(const char *passphrase);
unsigned long int getRandom();
char *subString(char *input, int indexstart, int indexend);
int is_a_number(char *input);
char **splitString(char *originalString,int *finalSize);

#endif 
