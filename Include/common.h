#ifndef COMMON_H_
#define COMMON_H_

//defines comuns para server e cliente
#define ENTER 10
#define MAX_MSG_SIZE 4096
#define MAX_NICK_SIZE 32
#define MAX_CMD_SIZE 16
#define QUIT_CMD "/quit"
#define PING_CMD "/ping"

// //estruturas para conexao
// struct in_addr{
//     unsigned long s_addr;
// };

// struct sockaddr_in{
//     short sin_family;
//     unsigned short sin_port;
//     struct in_addr sin_addr;
//     char sin_zero[8];
// };

char *readLine(int *);

#endif
