/**
 * Copyright
 **/

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>

#define UDP_BUFFER_MAX_SIZE 1000

typedef struct udp_t {
	struct sockaddr_in addr;
	int sockfd;
	int addr_len;
} _udp_t;

bool _select(uint32_t msec, _udp_t skt);
void udp_client_init(_udp_t* skt, char* UDP_SERVER_IP, int UDP_PORT);
void udp_server_init(_udp_t* skt, char* UDP_SERVER_IP, int UDP_PORT);
int udp_send(_udp_t* skt, char* buffer, int len);
int udp_receive(_udp_t* skt, char* buffer);

