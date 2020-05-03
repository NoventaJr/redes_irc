#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define ENTER 10

int countDots(char *str){
    int i = 0;
    int count = 0;

    while(str[i] != '\n'){
        if(str[i] == '.')   count++;
        i++;
    }

    return count;   
}

char *int_to_binary(int value){
    char *binary;
    int i, aux;

    binary = (char *) calloc(0, sizeof(char));
    for(i = 0;i < 8;i++){
        aux = pow(2, 7 - i);
        if(value >= aux){
            binary[i] = '1';
            value -= aux;
        }else   binary[i] = '0';
    }

    return binary;
}

unsigned int binary_address(int *parts, int n_dots){
    int i, j, k = 0;
    unsigned int binary = 0;
    char *temp;

    temp = (char *) calloc(8, sizeof(char));

    switch(n_dots){
        case 3:
            for(i = 0;i < 4;i++){
                temp = int_to_binary(parts[3 - i]);
                for(j = 0;j < 8;j++){
                    if(temp[j] == '1')    binary += pow(2, 31 - k);
                    k++;
                }
            }
            break;
        case 2:
            temp = int_to_binary(parts[2]);
            for(j = 0;j < 8;j++){
                if(temp[j] == '1')    binary += pow(2, 31 - k);
                k++;
            }
            k = 0;
            temp = int_to_binary(parts[1]);
            for(j = 0;j < 8;j++){
                if(temp[j] == '1')    binary += pow(2, 15 - k);
                k++;
            }
            temp = int_to_binary(parts[0]);
            for(j = 0;j < 8;j++){
                if(temp[j] == '1')    binary += pow(2, 15 - k);
                k++;
            }
            break;
        case 1:
            temp = int_to_binary(parts[1]);
            for(j = 0;j < 8;j++){
                if(temp[j] == '1')    binary += pow(2, 31 - k);
                k++;
            }
            k = 0;
            temp = int_to_binary(parts[0]);
            for(j = 0;j < 8;j++){
                if(temp[j] == '1')    binary += pow(2, 7 - k);
                k++;
            }
            break;
        case 0:
            temp = int_to_binary(parts[0]);
            for(j = 0;j < 8;j++){
                if(temp[j] == '1')     binary += pow(2, 31 - k);
                k++;
            }
            break;
    }

    free(temp);
    return binary;
}

unsigned int convert_to_net(char *ip){
    char temp[3];
    char c;
    int *parts;
    int n_dots;
    int i, j, k = 0;
    unsigned int binary;

    n_dots = countDots(ip);
    parts = calloc(n_dots + 1, sizeof(int));

    for(i = 0;i <= n_dots;i++){
        //splitting
        for(j = 0;j < 3;j++)    temp[j] = ' ';
        j = 0;
        do{
            if(ip[k] == '.' || ip[k] == '\0'){
                k++;
                break;            
            }else{
                temp[j++] = ip[k++];
            }
        }while(1);
        parts[i] = atoi(temp);
        //To do: verificar se 0 <= part < 256 

        //printf("%s\n", int_to_binary(parts[i], 8));
    }

    binary = binary_address(parts, n_dots);
    //printf("%u\n", binary);

    //free(ip);
    free(parts);
    //printf("\n");
    return binary;
}