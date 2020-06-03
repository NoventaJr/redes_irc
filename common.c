#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <common.h>

char *readLine(int *len){
    char c;
    char *string = NULL;
    int count = 0;

    do{
        c = getc(stdin);
        string = (char *) realloc(string, sizeof(char) * (count + 1));
        string[count++] = c;
    }while(c != ENTER);
    string[--count] = '\0';
    fflush(stdin);

    *len = count;

    return string;
}