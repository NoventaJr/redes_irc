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
#define MAX_CHANNELS 16
#define MAX_NICK_SIZE 50
#define MAX_CHANNEL_SIZE 200
#define MIN_NAME_SIZE 4
#define SERVER_FULL_MSG "Numero maximo de clientes no momento.\nTente novamente mais tarde.\n\n"
#define LIST_CLIENTS_CMD "/list_clients"
#define LIST_CHANNELS_CMD "/list_channels"
#define PING_CMD "/ping"
#define NICK_CMD "/nickname"
#define JOIN_CMD "/join"

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t channels_mutex = PTHREAD_MUTEX_INITIALIZER;

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
    int current_channel;
} t_client; 

//estrutura para gerenciar canais
typedef struct channel{
    int cid;
    char ch_name[MAX_CHANNEL_SIZE];
    //int users[MAX_CLIENTS];
    int n_users;
    int adm;
} t_channel;

//variaveis globais
int client_count = 0;
int channel_count = 0;
t_client *clients[MAX_CLIENTS];
t_channel *channels[MAX_CHANNELS];
int cid = 10;
int flag;
char msg[MAX_MSG_SIZE] = {0};

//envia mensagens para todos os clientes conectados
void send_message(char *msg, t_client *client, int receive, int ch){
	pthread_mutex_lock(&clients_mutex);
    int i, j;

    //passa por todos os clientes
    for(i = 0;i < MAX_CLIENTS;i++){
        //verifica se o cliente existe
        if(clients[i]){
            //verifica se o cliente esta no canal ou se é aberto a todos (ch == 0)
            if(clients[i]->current_channel == ch || ch == 0){
                //verifica se é o cliente que enviou a mensagem (caso receive == 0 ela não é enviada para ele)
                if(clients[i]->uid != client->uid || receive == 1){
                    //enfim envia a mensagem (verificando se ocorreu erro)
                    if(send(clients[i]->sockfd, msg, MAX_MSG_SIZE, 0) < 0){
                        fprintf(stderr, "ERRO: Nao foi possivel enviar mensagem!\t(%s)\n", clients[i]->nick);
                        break;
                    }
                }
            }
        }
    }

	pthread_mutex_unlock(&clients_mutex);
}

//adiciona um novo cliente na lista
void add_client(t_client *new_client){
	pthread_mutex_lock(&clients_mutex);
    int i;

	for(i = 0; i < MAX_CLIENTS; i++){
		if(!clients[i]){
			clients[i] = new_client;
            //printf("Cliente adicionado\n");
			break;
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

//remove determinado cliente da lista
void remove_client(t_client *client){
	pthread_mutex_lock(&clients_mutex);
    int j, k;

    //verifica o cliente esta conectado em algum canal
    if(client->current_channel != -1){
        //procura canal em que o cliente esta conectado
        for(j = 0;j < MAX_CHANNELS;j++){
            //verifica se existe
            if(channels[j]){
                //verifica se é o correto
                if(channels[j]->cid == client->current_channel){
                    //diminui a qtd de usuarios ativos
                    channels[j]->n_users--;
                    //elimina o canal caso não tenha mais ngm
                    if(channels[j]->n_users == 0){
                        //remove_channel(channel[i]);
                    }else{
                        //caso tenha mais gente, verifica se o cliente desconectado é o adm
                        if(channels[j]->adm == client->uid){
                            //se for roda os clientes do canal
                            for(k = 0;k < MAX_CLIENTS;k++){
                                //verifica se o cliente existe
                                if(clients[k]){
                                    //verifica se nao é o cliente desconectado e se pertence ao canal
                                    if(clients[k]->uid != client->uid && clients[k]->current_channel == channels[j]->cid){
                                        //transfere adm
                                        channels[j]->adm = clients[k]->uid;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

	pthread_mutex_unlock(&clients_mutex);
}

int add_channel(char *name, t_client *client){
    int i;

    if(name[0] != '#'){
        send(client->sockfd, "O nome do canal deve comecar com #\nTente novamente.\n\n", MAX_MSG_SIZE, 0);
        return 0;
    }
    //verifica se o nome é valido (tamanho)
	if(strlen(name) < MIN_NAME_SIZE || strlen(name) >= MAX_CHANNEL_SIZE - 1){
		//printf("Alerta: Nome invalido! Nomes devem ter entre %d e %d caracteres.\n", MIN_NAME_SIZE, MAX_NICK_SIZE);
        send(client->sockfd, "Nome invalido! Nomes de canal devem ter entre 4 e 32 caracteres!\nPor favor envie o comando com um novo nome.\n\n", MAX_MSG_SIZE, 0);
		return 0;
	}

    //cria o canal
    t_channel *new_channel = (t_channel *) malloc(sizeof(t_channel));
    new_channel->cid = ++cid;
    strcpy(new_channel->ch_name, name);
    new_channel->n_users = 1;
    new_channel->adm = client->uid;

    //altera canal ativo do cliente
    client->current_channel = new_channel->cid;

    //adiciona na lista de canais
    for(i = 0;i < MAX_CHANNELS;i++){
        if(!channels[i]){
            channels[i] = new_channel;
        }
    }

    //confirmacao de criacao
    strcpy(msg, new_channel->ch_name);
    strcat(msg, " criado com sucesso!\n\n");
    send(client->sockfd, msg, MAX_MSG_SIZE, 0);

    bzero(msg, MAX_MSG_SIZE);
    return 1;
}

int join_channel(char *name, t_client *client){
    // pthread_mutex_lock(&channels_mutex);
    int i, j;
    int joined = 0;
    int success = 0;
    // char msg[MAX_MSG_SIZE] = {0};

    //verificando se canal existe
    for(i = 0;i < MAX_CHANNELS;i++){
        if(channels[i]){
            if(strcmp(name, channels[i]->ch_name) == 0){
                printf("%s\t%s\t%d\n", name, channels[i]->ch_name, channels[i]->n_users);
                //altera numero de usuarios
                channels[i]->n_users++;
                //altera canal ativo do cliente
                client->current_channel = channels[i]->cid;
                joined = 1;
                //envia confirmação de entrada
                strcpy(msg, "Conectando ao canal ");
                strcat(msg, name);
                strcat(msg, "\n");
                send(client->sockfd, msg, MAX_MSG_SIZE, 0);
                //envia aviso de entrada ao canal
                strcpy(msg, client->nick);
                strcat(msg, " se conectou ao canal!\n");
                send_message(msg, client, 0, client->current_channel);
                success = 1;
                break;
            }
        }
    }

    if(!joined){
        //canal não encontrado
        send(client->sockfd, "Canal inexistente, tentando criar canal...\n", MAX_MSG_SIZE, 0);
        success = add_channel(name, client);
        // pthread_mutex_unlock(&channels_mutex);
    }

    bzero(msg, MAX_MSG_SIZE);
    return success;
}

void change_nick(char *nick, t_client *client){
    // char msg[MAX_MSG_SIZE] = {0};
    int i;
    
    //verifica se o apelido é valido (tamanho)
	if(strlen(nick) <  MIN_NAME_SIZE || strlen(nick) >= MAX_NICK_SIZE - 1){
		printf("Alerta: Apelido invalido! Apelidos devem ter entre %d e %d caracteres.\n", MIN_NAME_SIZE, MAX_NICK_SIZE);
        send(client->sockfd, "Apelido invalido! Apelidos devem ter entre 4 e 32 caracteres!\nPor favor reenvie o comando com um novo apelido.\n\n", MAX_MSG_SIZE, 0);
		return;
	} else{
        for(i = 0;i < MAX_CLIENTS;i++){
            //verifica se o apelido é valido (repetido)
            if(clients[i]){
                if(strcmp(nick, clients[i]->nick) == 0){
                    // printf("Alerta: Apelido invalido! Ja existe alguem com esse apelido.\n");
                    send(client->sockfd, "Apelido invalido! Ja existe alguem com esse apelido.\nPor favor reenvie o comando com um novo apelido.\n\n", MAX_MSG_SIZE, 0);
                    return;
                }
            }
        }

        //verifica se é o primeiro apelido
        if(strlen(client->nick) > 0){
            //mensagem avisando a troca de apelido (para todos)
            strcpy(msg, client->nick);
            strcat(msg, " mudou seu apelido para ");
            strcat(msg, nick);
            strcat(msg, "!\n");
            send_message(msg, client, 1, 0);
        }else{
            //mensagem avisando conexao (para outros usuarios)
            strcpy(msg, nick);
            strcat(msg, " se conectou ao servidor!\n");
            send_message(msg, client, 0, 0);
            //mensagem de boas vindas (para o usuario)
            strcpy(msg, "Bem vindo ");
            strcat(msg, nick);
            strcat(msg, "!\n");
            send(client->sockfd, msg, MAX_MSG_SIZE, 0);
        }
		strcpy(client->nick, nick);

        printf("%s CONECTOU\n\n", client->nick);
	}

    bzero(msg, MAX_MSG_SIZE);
	bzero(nick, MAX_NICK_SIZE);
    return;
}

//gerencia os clientes via thread
void *handle_client(void *arg){
	char msg_send[MAX_MSG_SIZE];
    char buffer[MAX_MSG_SIZE];
	int leave_flag = 0;
    char c;
    char channel_name[MAX_CHANNEL_SIZE] = {0};

	client_count++;
	t_client *client = (t_client *) arg;

    // printf("handle client %d\n", client->uid);

	while(1){
		if(leave_flag){
			break;
		}

        //caso cliente tenha se desconectado por algum erro
		if(recv(client->sockfd, buffer, MAX_MSG_SIZE, 0) < 0){
            fprintf(stderr, "%s DESCONECTOU (por inatividade)\n\n", client->nick);
            break;
        }

        //verifica se é comando
        if(buffer[0] == '/'){            
            //caso cliente envie /quit
            if(strcmp(buffer, QUIT_CMD) == 0){
                strcpy(msg_send, client->nick);
                strcat(msg_send, " desconectou!\n");
                send_message(msg_send, client, 0, 0);
                fprintf(stderr, "%s DESCONECTOU\n", client->nick);
                break;
            }

            //caso cliente envie /ping
            if(strcmp(buffer, PING_CMD) == 0)   send(client->sockfd, "pong\n", sizeof("pong\n"), 0);
            else{
                char *cmd;
                
                cmd = strtok(buffer, " ");
                // printf("%s\n", cmd);

                //caso cliente envie /nickname xxxxx
                if(strcmp(cmd, NICK_CMD) == 0){
                    cmd = strtok(NULL, " ");
                    // printf(".%s.\n", cmd);
                    change_nick(cmd, client);
                }else{
                    //caso cliente envie /join xxxxx
                    if(strcmp(cmd, JOIN_CMD) == 0){
                        cmd = strtok(NULL, " ");
                        if(join_channel(cmd, client)){
                            strcpy(channel_name, cmd);
                        }
                    }
                }
            }
        }else{
            //verifica se usuario ja colocou um apelido
            if(strlen(client->nick) > 0 && client->current_channel != -1){
                //formatando mensagem e enviando
                strcpy(msg_send, client->nick);
                strcat(msg_send, " > ");
                strcat(msg_send, channel_name);
                strcat(msg_send, ": ");
                strcat(msg_send, buffer);
                strcat(msg_send, "\n");
                memset(buffer, 0, sizeof(buffer));

                printf("%s", msg_send);
                send_message(msg_send, client, 1, client->current_channel);
            }else{
                if(strlen(client->nick) <= 0)   send(client->sockfd, "Por favor, especifique um apelido atraves do comando /nickname APELIDO antes de enviar uma mensagem\n\n", MAX_MSG_SIZE, 0);
                else                            send(client->sockfd, "Por favor, conecte-se a um canal atraves do comando /join canal antes de enviar uma mensagem\n\n", MAX_MSG_SIZE, 0);
            }
        }

        //limpa buffer
		bzero(msg_send, MAX_MSG_SIZE);
        bzero(buffer, MAX_MSG_SIZE);
	}

    //retira o cliente
	close(client->sockfd);
    remove_client(client);
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
        if(strcmp(cmd, LIST_CLIENTS_CMD) == 0){
            printf("(%d/%d)\n", client_count, MAX_CLIENTS);
            if(client_count == 0)   printf("Nao ha clientes conectados no momento\n\n");
            else{
                for(i = 0;i < client_count;i++){
                    if(clients[i])  printf("%s\n", clients[i]->nick);
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
    int uid = 10;    //id para definir um cliente unico

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
            new_client->current_channel = -1;
            add_client(new_client);

            //envia mensagem de boas vindas + aviso de apelido
            send(new_client->sockfd, "\nBem vindo! Vamos comecar escolhendo um apelido!\n/nickname APELIDO\tPara DEFINIR apelido.\n/quit\t\t\tPara DESCONECTAR do servidor e FECHAR a aplicacao.\n\n", MAX_MSG_SIZE, 0);

            //thread para gerenciar o cliente
            pthread_t tid;
            pthread_create(&tid, NULL, &handle_client, (void *) new_client);
            sleep(1);
        }
    }

    //printf("out of while\n");

    return 0;
}