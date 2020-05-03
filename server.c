#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <converter.h>

#define MAX_SIZE 4096
#define ENTER 10

struct in_addr{
    unsigned long s_addr;
};

struct sockaddr_in{
    //__uint8_t sin_len;
    short sin_family;
    unsigned short sin_port;
    struct in_addr sin_addr;
    char sin_zero[8];
};

//argv[1] = port
int main(int argc, char *argv[]){
    int server_fd, new_socket;
    int port;
    struct sockaddr_in server_addr;
    int addr_len = sizeof(server_addr);
    int c;

    //verificando porta
    if(argc < 2){
        fprintf(stderr, "Porta nao especificada!\n");
        exit(-1);
    }
    port = atoi(argv[1]);
   
    //criando socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd < 0){
        fprintf(stderr, "Nao foi possivel criar socket!\n");
        exit(-2);
    }

    bzero(&server_addr, sizeof(server_addr));   //ao contrario do cliente.c, memset da erro
    //memset(&server_addr, '0', sizeof(server_addr));
    server_addr.sin_addr.s_addr = 0;            //usar convert_to_net pra ip
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = __bswap_16(port);

    //iniciando
    if(bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0){
        fprintf(stderr, "Bind falhou!\n");
        exit(-3);
    };

    if(listen(server_fd, 50) < 0) { 
        printf("Falha ao escutar\n"); 
        exit(-4); 
    }

    //conectado
    printf("Servidor conectado!!\n");
    while(1){
        char buffer[MAX_SIZE] = {0};

        printf("Aguardando conexao...\n");
        if ((new_socket = accept(server_fd, (struct sockaddr *)&server_addr, (socklen_t *)&addr_len)) < 0){
            printf("Falha ao aceitar conexao\n");            
            exit(-5);        
        }

        send(new_socket, "Conectado! Envie 'exit' para desconectar.", strlen("Conectado!"), 0);

        //rotina de recebimento
        while(recv(new_socket, buffer, MAX_SIZE, 0) > 0){
            printf("%s\n", buffer);
            memset(buffer, 0, sizeof(buffer));
        }

        printf("Usuario desconectou...\n");
        close(new_socket);
    }

    return 0;
}