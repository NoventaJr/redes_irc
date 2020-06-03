#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <pthread.h>
#include <converter.h>
#include <common.h>

//defines proprios para servidor
#define MAX_CLIENTS 16
#define WELCOME_MSG_1 "Bem vindo, "
#define WELCOME_MSG_2 "!\n"
#define SERVER_FULL_MSG "Numero maximo de clientes no momento.\nTente novamente mais tarde.\n\n"
#define LIST_CMD "/list"
#define LIST_IP_CMD "/list_ip"

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

//estruturas para conexao
struct in_addr{
    unsigned long s_addr;
};

struct sockaddr_in{
    short sin_family;
    unsigned short sin_port;
    struct in_addr sin_addr;
    char sin_zero[8];
};

//estrutura para gerenciar clientes
typedef struct client{
    struct sockaddr_in address;
	int sockfd;
	int uid;
	char nick[MAX_NICK_SIZE];
} t_client; 

//variaveis globais
int client_count = 0;
t_client *clients[MAX_CLIENTS];
int flag;

//adiciona um novo cliente na lista
void add_client(t_client *new_client){
	pthread_mutex_lock(&clients_mutex);

	for(int i = 0; i < MAX_CLIENTS; ++i){
		if(!clients[i]){
			clients[i] = new_client;
            //printf("Cliente adicionado\n");
			break;
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

//remove determinado cliente da lista
void remove_client(int uid){
	pthread_mutex_lock(&clients_mutex);

	for(int i = 0; i < MAX_CLIENTS; ++i){
		if(clients[i]){
			if(clients[i]->uid == uid){
				clients[i] = NULL;
                //printf("Cliente removido\n");
				break;
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

//envia mensagens para todos os clientes conectados
void send_message(char *msg){
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i<MAX_CLIENTS; ++i){
		if(clients[i]){
            if(send(clients[i]->sockfd, msg, MAX_MSG_SIZE, 0) < 0){
                fprintf(stderr, "ERRO: Nao foi possivel enviar mensagem!\t(%s)\n", clients[i]->nick);
                break;
            }
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

//gerencia os clientes via thread
void *handle_client(void *arg){
	char msg_send[MAX_MSG_SIZE];
    char buffer[MAX_MSG_SIZE];
	char nick[32] = {0};
	int leave_flag = 0;

	client_count++;
	t_client *client = (t_client *) arg;

	//da o apelido ao cliente
    if (recv(client->sockfd, nick, MAX_NICK_SIZE, 0) < 0){
        fprintf(stderr, "ERRO: Nao foi possivel pegar o apelido!\n");
        leave_flag = 1;
    }
    
    //verifica se o apelido é valido
	if(strlen(nick) <  2 || strlen(nick) >= MAX_NICK_SIZE-1){
		printf("ERRO: Apelido invalido!\n");
		leave_flag = 1;
	} else{
		strcpy(client->nick, nick);

        //cria mensagem de boas vindas
        strcpy(msg_send, WELCOME_MSG_1);
        strcat(msg_send, client->nick);
        strcat(msg_send, WELCOME_MSG_2);

        send(client->sockfd, msg_send, MAX_MSG_SIZE, 0);
        printf("%s CONECTOU\n\n", client->nick);
	}

	bzero(msg_send, MAX_MSG_SIZE);

	while(1){
		if (leave_flag) {
			break;
		}

        //caso cliente tenha se desconectado por algum erro
		if(recv(client->sockfd, buffer, MAX_MSG_SIZE, 0) < 0){
            fprintf(stderr, "%s DESCONECTOU (por inatividade)\n\n", client->nick);
            break;
        }

        //caso cliente envie /quit
        if(strcmp(buffer, QUIT_CMD) == 0){
            fprintf(stderr, "%s DESCONECTOU\n", client->nick);
            break;
        }

        //caso cliente envie /ping
        if(strcmp(buffer, PING_CMD) == 0){
            send(client->sockfd, "pong\n", sizeof("pong\n"), 0);
        }else{
            //formatando mensagem e enviando
            strcpy(msg_send, client->nick);
            strcat(msg_send, ": ");
            strcat(msg_send, buffer);
            strcat(msg_send, "\n");
            memset(buffer, 0, sizeof(buffer));

            printf("%s", msg_send);
            send_message(msg_send);
        }

        //limpa buffer
		bzero(msg_send, MAX_MSG_SIZE);
	}

    //retira o cliente
	close(client->sockfd);
    remove_client(client->uid);
    free(client);
    client_count--;

    pthread_detach(pthread_self());

	return NULL;
}

//alguns comandos basicos para o servidor
void *admServer(){
    char c;
    int count;
    int i;

    while(1){
        //pega comando
        char *cmd = readLine(&count);
        
        //lista apelido de quem esta conectado
        if(strcmp(cmd, LIST_CMD) == 0){
            printf("(%d/%d)\n", client_count, MAX_CLIENTS);
            if(client_count == 0)   printf("Nao ha clientes conectados no momento\n\n");
            else{
                for(i = 0;i < client_count;i++){
                    if(clients[i])  printf("%s\n", clients[i]->nick);
                }
                printf("\n");
            }
        }

        //lista apelido e ip de quem esta conectado
        if(strcmp(cmd, LIST_IP_CMD) == 0){
            printf("(%d/%d)\n", client_count, MAX_CLIENTS);
            if(client_count == 0)   printf("Nao ha clientes conectados no momento\n\n");
            else{
                for(i = 0;i < client_count;i++){
                    if(clients[i])  printf("%s\t%ld\n", clients[i]->nick, clients[i]->address.sin_addr.s_addr);
                }
                printf("\n");
            }
        }

        //funcao para fechar o server (nao funcionando ainda)
        // if(strcmp(cmd, QUIT_CMD) == 0){
        //     printf("Fechando servidor\n");
        //     for(i = 0;i < client_count;i++){
        //         if(clients[i]){
        //             close(clients[i]->sockfd);
        //             remove_client(clients[i]->uid);
        //         }
        //     }
        //     flag = 1;
        //     printf("flag: %d\n", flag);
        //     break;
        // }

        free(cmd);
    }
}

//argv[1] = port
int main(int argc, char *argv[]){
    int server_fd, new_socket;
    int port;
    struct sockaddr_in client_addr;
    struct sockaddr_in server_addr;
    int cli_addr_len = sizeof(client_addr);
    int serv_addr_len = sizeof(server_addr);
    int c;
    int uid = 0;    //id para definir um cliente unico

    flag = 0;
    client_count = 0;

    //usar comandos no server
    pthread_t adm;
    pthread_create(&adm, NULL, &admServer, NULL);

    //verificando porta
    if(argc < 2){
        fprintf(stderr, "ERRO: Porta nao especificada!\n");
        exit(-1);
    }
    port = atoi(argv[1]);
   
    //criando socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd < 0){
        fprintf(stderr, "ERRO: Nao foi possivel criar socket!\n");
        exit(-1);
    }

    bzero(&server_addr, sizeof(server_addr));   //ao contrario do cliente.c, memset da erro
    //memset(&server_addr, '0', sizeof(server_addr));
    server_addr.sin_addr.s_addr = convert_to_net("127.0.0.1");
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = __bswap_16(port);

    //iniciando
    if(bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0){
        fprintf(stderr, "ERRO: Bind!\n");
        exit(-1);
    };

    if(listen(server_fd, 50) < 0) { 
        fprintf(stderr, "ERRO: Listen!\n"); 
        exit(-1); 
    }

    //conectado
    printf("Servidor conectado!!\n--+--+--+--+--+--\n\n");

    while(!flag){
        if(client_count == 0)   printf("Aguardando conexao...\n");
        else                    printf("Aguardando nova conexao...\n");

        if ((new_socket = accept(server_fd, (struct sockaddr *)&client_addr, (socklen_t *)&cli_addr_len)) < 0){
            fprintf(stderr, "ERRO: Accept!\n");            
            exit(-1);        
        }

        if(client_count + 1 > MAX_CLIENTS){
            //nao aceita novos clientes quando o server esta cheio
            printf("ERRO: Numero maximo de clientes conectados!\n");
            send(new_socket, SERVER_FULL_MSG, sizeof(SERVER_FULL_MSG), 0);
            sleep(1);
            close(new_socket);
        }else{
            //criando novo cliente para adicionar na lista
            t_client *new_client = (t_client *) malloc(sizeof(t_client));
            new_client->address = client_addr;
            new_client->sockfd = new_socket;
            new_client->uid = ++uid;
            add_client(new_client);

            //thread para gerenciar o cliente
            pthread_t tid;
            pthread_create(&tid, NULL, &handle_client, (void *) new_client);
            sleep(1);
        }
    }

    //printf("out of while\n");

    return 0;
}