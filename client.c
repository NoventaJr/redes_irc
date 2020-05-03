#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <converter.h>

#define ENTER 10
#define MAX_SIZE 4096

struct in_addr{
    unsigned s_addr;
};

struct sockaddr_in{
    short sin_family;
    unsigned short sin_port;
    struct in_addr sin_addr;
    char sin_zero[8];
};

//argv[1] = port
int main(int argc, char *argv[]){
    int server_fd;
    int port;
    long valread;
    char msg_rec[MAX_SIZE], msg_send[MAX_SIZE];
    int msg_len;
    struct sockaddr_in server_addr;
    char c;
    int i;

    //verificando porta
    if(argc < 2){
        fprintf(stderr, "Porta nao especificada!\n");
        exit(-1);
    }
    port = atoi(argv[1]);

    //criando socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd == -1){
        fprintf(stderr, "Nao foi possivel criar socket!\n");
        exit(-2);
    }

    memset(&server_addr, '0', sizeof(server_addr));
    server_addr.sin_addr.s_addr = convert_to_net("127.0.0.1");
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = __bswap_16(port);

    //tentando conectar
    if(connect(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0){
        fprintf(stderr, "Erro de conexao!\n");
        exit(-3);
    }

    valread = recv(server_fd, msg_rec, MAX_SIZE, 0);
    printf("%s\n", msg_rec);

    do{
        char *msg = NULL;
        msg_len = 0;

        do{
            c = getc(stdin);
            msg = (char *) realloc(msg, sizeof(char) * (msg_len + 1));
            msg[msg_len++] = c;
        }while(c != ENTER);
        msg[--msg_len] = '\0';
        fflush(stdin);
        printf("%d\n", msg_len);

        //condicao de saida
        if(strcmp(msg, "exit\0") == 0){
            printf("Desconectando...\n");
            close(server_fd);
            return 0;
        }

        //rotina de envio
        while(msg_len > MAX_SIZE){
            strcpy(msg_send, msg);
            send(server_fd, msg_send, MAX_SIZE, 0);
            msg_len -= MAX_SIZE;
            for(i = 0;i < msg_len;i++){
                msg[i] = msg[i + MAX_SIZE];
            }
        }

        //enviando final da mensagem
        strcpy(msg_send, msg);
        send(server_fd, msg, msg_len, 0);
    
        free(msg);
    }while(msg_send != "exit\0");

    return 0;
}