#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <pthread.h>
#include <signal.h>
#include <converter.h>
#include <common.h>

//defines proprios para cliente
#define CONNECT_CMD "/connect"

//estruturas para conexao
struct in_addr{
    unsigned s_addr;
};

struct sockaddr_in{
    short sin_family;
    unsigned short sin_port;
    struct in_addr sin_addr;
    char sin_zero[8];
};

//variaveis globais
int flag;

//rotina de envio
void *send_msg(void *arg){
    int server_fd = *(int *) arg;
    char msg_send[MAX_MSG_SIZE] = {0};
    int msg_len = 0;
    int i;

    while(1) {
        //pega mensagem
        char *msg = readLine(&msg_len);
        // msg_len = readLine(msg);
        
        //comando de saida
        if (strcmp(msg, QUIT_CMD) == 0) {
            send(server_fd, msg, MAX_MSG_SIZE, 0);
            free(msg);
            printf("Desconectando...\n");
            close(server_fd);
            flag = 1;
            break;
        }

        while(msg_len > MAX_MSG_SIZE){  //caso maior que 4096 caracteres
            strcpy(msg_send, msg);
            if(send(server_fd, msg_send, MAX_MSG_SIZE, 0) < 0){
                if(!flag)   fprintf(stderr, "ERRO: Nao foi possivel enviar mensagem!\nDesconectando...\n");
                close(server_fd);
                flag = 1;
                break;
            }
            //copia resto da mensagem para o buffer de envio
            msg_len -= MAX_MSG_SIZE;
            for(i = 0;i < msg_len;i++){
                msg[i] = msg[i + MAX_MSG_SIZE];
            }
        }

        //enviando final da mensagem
        strcpy(msg_send, msg);
        if(send(server_fd, msg_send, msg_len, 0) < 0){
            if(!flag)   fprintf(stderr, "ERRO: Nao foi possivel enviar mensagem!\nDesconectando...\n");
            close(server_fd);
            flag =-1;
            break;
        }

        //limpa buffer de envio
        bzero(msg_send, MAX_MSG_SIZE);
        free(msg);
    }
}

//rotina de recebimento
void *recv_msg(void *arg) {
	char msg_rec[MAX_MSG_SIZE] = {0};
    int server_fd = *(int *) arg;

    while (1) {
		if(recv(server_fd, msg_rec, MAX_MSG_SIZE, 0) < 0){
            if(!flag)   fprintf(stderr, "ERRO: Nao foi possivel receber mensagem!\n");
            close(server_fd);
            flag = 1;
            break;
        }
        
        //imprime mensagem e limpa buffer
        printf("%s", msg_rec);
		memset(msg_rec, 0, sizeof(msg_rec));
    }
}

//argv[1] = port
int main(int argc, char *argv[]){
    struct sockaddr_in server_addr;
    int server_fd;
    int port;
    char msg_rec[MAX_MSG_SIZE], msg_send[MAX_MSG_SIZE];
    int msg_len, cmd_len;
    int i;

    flag = 0;

    //verificando porta
    if(argc < 2){
        fprintf(stderr, "ERRO: Porta nao especificada!\n");
        exit(-1);
    }
    port = atoi(argv[1]);

    //comando para conectar
    while(1){
        printf("/connect\t Para se CONECTAR ao servidor.\n/quit\t\t Para FECHAR a aplicacao.\n\n");

        char *cmd = readLine(&cmd_len);
        
        //cliente digita /quit
        if(strcmp(cmd, QUIT_CMD) == 0){
            printf("Fechando aplicacao...\n");
            free(cmd);
            return 0;
        }

        //cliente digita /connect
        if(strcmp(cmd, CONNECT_CMD) == 0){
            free(cmd);
            break;
        }
    }

    printf("Conectando ao servidor...\n");

    //criando socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd == -1){
        fprintf(stderr, "ERRO: Nao foi possivel criar socket!\n");
        exit(-1);
    }

    //server info
    memset(&server_addr, '0', sizeof(server_addr));
    server_addr.sin_addr.s_addr = convert_to_net("127.0.0.1");
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = __bswap_16(port);

    //tentando conectar
    if(connect(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0){
        fprintf(stderr, "ERRO: Connect!\n");
        exit(-1);
    }

    //criacao das threads
    //thread para envio de mensagens
    pthread_t send_msg_thread;
    if(pthread_create(&send_msg_thread, NULL, &send_msg, (void *) &server_fd) != 0){
		fprintf(stderr, "ERRO: Nao foi possivel criar THREAD DE ENVIO!\n");
        return(-1);
	}

    //thread para recebimento de mensagens
	pthread_t recv_msg_thread;
    if(pthread_create(&recv_msg_thread, NULL, &recv_msg, (void *) &server_fd) != 0){
		fprintf(stderr, "ERRO: Nao foi possivel criar THREAD DE RECEBIMENTO!\n");
		return(-5);
	}

    //while para ignorar SIGINT e manter threads abertas ate cliente desconectar
	while (1){
        (void) signal(SIGINT, SIG_IGN);

		if(flag == 1){
			printf("\nVolte sempre!\n--+--+--+--+--\n\n");
			break;
        }
	}

	close(server_fd);

    return 0;
}