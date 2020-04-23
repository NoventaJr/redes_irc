#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

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

// struct sockaddr{
//     unsigned short sa_family;
//     char sa_data[14];
// };

__uint32_t convert_long(){

}

__uint16_t convert_short(){

}

//argv[1] = port
int main(int argc, char *argv[]){
    int server_fd;
    int port;
    struct sockaddr_in server_addr;

    //verificando porta
    if(argc < 2){
        fprintf(stderr, "Porta nao especificada!\n");
        exit(-1);
    }

    port = atoi(argv[1]);
    //printf("%d\n", port);

    //criando socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd == -1){
        fprintf(stderr, "Nao foi possivel criar socket!\n");
        exit(-2);
    }

    server_addr.sin_addr.s_addr = inet_addr("74.125.235.20");
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = __bswap_16(port);

    //printf("%d\n", server_addr.sin_port);

    //tentando conectar
    if(connect(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
        fprintf(stderr, "Erro de conexao!\n");
        exit(-3);
    }

    printf("Conectado!\n");

    return 0;
}