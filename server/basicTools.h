#ifndef BASICTOOLS_H
#define BASICTOOLS_H

unsigned long int generateToken(const char *passphrase);
unsigned long int getRandom();
char *subString(char *input, int indexstart, int indexend);
int is_a_number(char *input);
char **splitString(char *originalString, int *finalSize);
void writeToLog(char *type, char **client_info, DWORD tid, CRITICAL_SECTION lock);
int readConfig(int *port, int *max_thread, char *path);
int checkDirectory(char *dir);
int receiveNumberL(int sockfd, unsigned long int *number);
int sendNumberL(int sockfd, unsigned long int number);

#endif
