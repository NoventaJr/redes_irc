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
#define MUTE_CMD "/mute"
#define MUTE_CMD "/mute"
#define UNMUTE_CMD "/unmute"
#define KICK_CMD "/kick"

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
    int muted[MAX_CLIENTS];
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
void send_message(char *msg, t_client *client, int receive, int ch, int alert){
	pthread_mutex_lock(&clients_mutex);
    int i, j;

    if(!alert){
        for(i = 0;i < MAX_CHANNELS;i++){
            if(channels[i]){
                if(channels[i]->cid == client->current_channel){
                    for(j = 0;j < MAX_CLIENTS;j++){
                        if(channels[i]->muted[j] == client->uid){
                            send(client->sockfd, "Voce nao tem permissao para enviar mensagens neste canal\n", MAX_MSG_SIZE, 0);
                            sleep(1);
                            bzero(msg, MAX_MSG_SIZE);
                            pthread_mutex_unlock(&clients_mutex);
                            return;
                        }
                    }
                }
            }
        }
    }

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

    bzero(msg, MAX_MSG_SIZE);
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

int remove_channel(t_channel *channel){
    int i;

    for(i = 0;i < MAX_CHANNELS;i++){
        if(channels[i]){
            if(channels[i]->cid == channel->cid){
                bzero(channel->ch_name, MAX_CHANNEL_SIZE);
                channel->cid = 0;
                channel->adm = -1;
                channels[i] = NULL;
                free(channel);
                return 1;
            }
        }
    }

    return 0;
}

void remove_from_channel(t_client *client, int ch_id, int kicked){
    int i, j;

    //percorre canais
    for(i = 0;i < MAX_CHANNELS;i++){
        if(channels[i]){
            if(channels[i]->cid == ch_id){
                if(kicked){
                    strcpy(msg, "O usuario ");
                    strcat(msg, client->nick);
                    strcat(msg, " foi kickado do canal pelo admnistrador\n");
                    send_message(msg, client, 0, client->current_channel, 1);
                }else{
                    strcpy(msg, "O usuario ");
                    strcat(msg, client->nick);
                    strcat(msg, " se desconectou do canal\n");
                    send_message(msg, client, 0, client->current_channel, 1);
                }
                channels[i]->n_users--;
                client->current_channel = -1;
                if(channels[i]->adm == client->uid && channels[i]->n_users > 0){
                    for(j = 0;j < MAX_CLIENTS;j++){
                        if(clients[j]){
                            if(clients[j]->uid != client->uid && clients[j]->current_channel == channels[i]->cid){
                                channels[i]->adm = clients[j]->uid;
                                send(clients[j]->sockfd, "Voce e o novo admnistrador do canal\n", MAX_MSG_SIZE, 0);
                            }
                        }
                    }
                }else{
                    break;
                }
            }
        }
    }

    if(channels[i]->n_users == 0){
        if(remove_channel(channels[i]))     channel_count--;
    }
}

//remove determinado cliente da lista
void remove_client(t_client *client){
	pthread_mutex_lock(&clients_mutex);
    int i;

    //verifica o cliente esta conectado em algum canal e chama função de remoção
    if(client->current_channel != -1){
        remove_from_channel(client, client->current_channel, 0);
    }

    for(i = 0;i < MAX_CLIENTS;i++){
        if(clients[i]){
            if(clients[i]->uid == client->uid){
                bzero(client->nick, MAX_NICK_SIZE);
                client->uid = -1;
                client->current_channel = -1;
                clients[i] = NULL;
                break;
            }
        }
    }

	pthread_mutex_unlock(&clients_mutex);
}

int add_channel(char *name, t_client *client){
    int i, j;

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
    channel_count++;    

    //altera canal ativo do cliente
    client->current_channel = new_channel->cid;

    //adiciona na lista de canais
    for(i = 0;i < MAX_CHANNELS;i++){
        if(!channels[i]){
            channels[i] = new_channel;
            for(j = 0;j < MAX_CLIENTS;j++)  channels[i]->muted[j] = -1;
            break;
        }
    }

    //confirmacao de criacao
    // printf("Canal %s criado\n", new_channel->ch_name);

    strcpy(msg, new_channel->ch_name);
    strcat(msg, " criado com sucesso!\n\n");
    send(client->sockfd, msg, MAX_MSG_SIZE, 0);

    //free(new_channel);
    bzero(msg, MAX_MSG_SIZE);
    return 1;
}

int join_channel(char *name, t_client *client){
    // pthread_mutex_lock(&channels_mutex);
    int i, j;
    int joined = 0;
    int success = 0;
    // char msg[MAX_MSG_SIZE] = {0};

    if(client->current_channel != -1){
        remove_from_channel(client, client->current_channel, 0);
    }

    //verificando se canal existe
    for(i = 0;i < MAX_CHANNELS;i++){
        if(channels[i]){
            if(strcmp(name, channels[i]->ch_name) == 0){
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
                send_message(msg, client, 0, client->current_channel, 1);
                success = 1;

                // printf("%s se conectou ao canal %s\n", client->nick, channels[i]->ch_name);
                break;
            }
        }
    }

    if(!joined){
        //canal não encontrado
        send(client->sockfd, "Canal inexistente, tentando criar canal...\n", MAX_MSG_SIZE, 0);
        bzero(msg, MAX_MSG_SIZE);
        sleep(1);
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
            send_message(msg, client, 1, 0, 1);
        }else{
            //mensagem avisando conexao (para outros usuarios)
            strcpy(msg, nick);
            strcat(msg, " se conectou ao servidor!\n");
            send_message(msg, client, 0, 0, 1);
            //mensagem de boas vindas (para o usuario)
            strcpy(msg, "Bem vindo ");
            strcat(msg, nick);
            strcat(msg, "!\nPara se conectar a um canal, utilize o comando /join CANAL\n\n");
            send(client->sockfd, msg, MAX_MSG_SIZE, 0);
        }
		strcpy(client->nick, nick);

        printf("%s CONECTOU\n\n", client->nick);
	}

    bzero(msg, MAX_MSG_SIZE);
	bzero(nick, MAX_NICK_SIZE);
    return;
}

void mute(t_client *adm, char *nick){
    int i, j;
    t_client *client = (t_client *) malloc(sizeof(t_client));

    for(i = 0;i < MAX_CLIENTS;i++){
        if(clients[i]){
            if(strcmp(clients[i]->nick, nick) == 0){
                client = clients[i];
                break;
            }
        }
    }

    if(client->current_channel != -1){
        for(i = 0;i < MAX_CHANNELS;i++){
            if(channels[i]){
                if(channels[i]->cid == client->current_channel){
                    if(client->uid != channels[i]->adm){
                        for(j = 0;j < MAX_CLIENTS;j++){
                            if(channels[i]->muted[j] == -1){
                                channels[i]->muted[j] = client->uid;
                                break;
                            }
                        }
                        send(client->sockfd, "Voce foi mutado pelo admnistrador do canal\n\n", MAX_MSG_SIZE, 0);
                    }else{
                        send(adm->sockfd, "Nao e possivel mutar o adm\n\n", MAX_MSG_SIZE, 0);
                    }
                    free(client);
                    return;
                }
            }
        }
    }

    free(client);
    send(adm->sockfd, "Usuario nao esta conectado no canal\n", MAX_MSG_SIZE, 0);
}

void unmute(t_client *adm, char *nick){
    int i, j;
    t_client *client = (t_client *) malloc(sizeof(t_client));

    for(i = 0;i < MAX_CLIENTS;i++){
        if(clients[i]){
            if(strcmp(clients[i]->nick, nick) == 0){
                client = clients[i];
                break;
            }
        }
    }

    if(client->current_channel != -1){
        for(i = 0;i < MAX_CHANNELS;i++){
            if(channels[i]){
                if(channels[i]->cid == client->current_channel){
                    for(j = 0;j < MAX_CLIENTS;j++){
                        if(channels[i]->muted[j] == client->uid){
                            channels[i]->muted[j] = -1;
                            send(client->sockfd, "Voce foi desmutado\n\n", MAX_MSG_SIZE, 0);
                            break;
                        }
                    }
                    free(client);
                    return;
                }
            }
        }
    }

    free(client);
    send(adm->sockfd, "Usuario nao esta conectado no canal\n", MAX_MSG_SIZE, 0);
}

void kick(t_client *adm, char *nick){
    int i;
    t_client *client = (t_client *) malloc(sizeof(t_client));

    for(i = 0;i < MAX_CLIENTS;i++){
        if(clients[i]){
            if(strcmp(clients[i]->nick, nick) == 0){
                client = clients[i];
                break;
            }
        }
    }

    if(client->current_channel != -1){
        for(i = 0;i < MAX_CHANNELS;i++){
            if(channels[i]){
                if(channels[i]->cid == client->current_channel){
                    if(client->uid != channels[i]->adm){
                        send(client->sockfd, "Voce foi kickado do canal pelo admnistrador\n\n", MAX_MSG_SIZE, 0);
                        remove_from_channel(client, channels[i]->cid, 1);
                    }else{
                        send(adm->sockfd, "Nao e possivel kickar o adm\n\n", MAX_MSG_SIZE, 0);
                    }
                    bzero(msg, MAX_MSG_SIZE);
                    free(client);
                    return;
                }
            }
        }
    }

    free(client);
    send(adm->sockfd, "Usuario nao esta conectado no canal\n", MAX_MSG_SIZE, 0);
}

int isAdm(t_client *client){
    int i;

    for(i = 0;i < MAX_CHANNELS;i++){
        if(channels[i]){
            if(channels[i]->adm == client->uid)    return 1;
        }
    }

    return 0;
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
                send_message(msg_send, client, 0, 0, 0);
                fprintf(stderr, "%s DESCONECTOU\n", client->nick);
                break;
            }

            //caso cliente envie /ping
            if(strcmp(buffer, PING_CMD) == 0)   send(client->sockfd, "pong\n", sizeof("pong\n"), 0);
            else{
                char *cmd;
                
                cmd = strtok(buffer, " ");
                // printf("%s\n", cmd);

                if(isAdm(client)){
                    //caso cliente seja adm do canal
                    //caso cliente envie /mute xxxxx
                    if(strcmp(cmd, MUTE_CMD) == 0){
                        cmd = strtok(NULL, " ");
                        mute(client, cmd);
                    }else{
                        //caso cliente envie /unmute xxxxx
                        if(strcmp(cmd, UNMUTE_CMD) == 0){
                            cmd = strtok(NULL, " ");
                            unmute(client, cmd);
                        }else{
                            //caso cliente envie /kick xxxxx
                            if(strcmp(cmd, KICK_CMD) == 0){
                                cmd = strtok(NULL, " ");
                                kick(client, cmd);
                            }
                        }
                    }
                }
                    
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
                send_message(msg_send, client, 1, client->current_channel, 0);
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
                for(i = 0;i < MAX_CLIENTS;i++){
                    if(clients[i])  printf("%d\t%s\t%d\t\n", clients[i]->uid, clients[i]->nick, clients[i]->current_channel);
                }
                printf("\n");
            }
        }

        if(strcmp(cmd, LIST_CHANNELS_CMD) == 0){
            printf("(%d/%d)\n", channel_count, MAX_CHANNELS);
            if(channel_count == 0)  printf("Nao ha canais criados no momento\n\n");
            else{
                for(i = 0;i < MAX_CHANNELS;i++){
                    if(channels[i]) printf("%d\t%s\t%d\t%d\n", channels[i]->cid, channels[i]->ch_name, channels[i]->n_users, channels[i]->adm);
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