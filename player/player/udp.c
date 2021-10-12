/**
 * Copyright
 *
 * This file is used to send/receiv the RRC message through UDP.
 *
 **/

/* =======================    Include Files    ====================== */
#include "udp.h"

bool _select(uint32_t msec, _udp_t skt)
{
	fd_set fs;
	FD_ZERO(&fs);
	FD_SET(skt.sockfd, &fs);
	struct timeval waiting = {};
	waiting.tv_sec = msec / 1000;
	waiting.tv_usec = msec % 1000 * 1000;
	int val = select(skt.sockfd + 1, &fs, NULL, NULL, &waiting);
	return val > 0;
}

/**===================================================================
 @Name:udp_client_init
 @Function:initialize the client udp socket
 @Input Parameters: skt, the udp socket handle, defined in rrc_context.h
 	 	 	 	 	UDP_SERVER_IP, the IP of the server you want to connect
 	 	 	 	 	UDP_PORT, the server's port number
 @Output Parameters:
 @Note: should be called by the client before message send/receive
 ==================================================================== **/
void udp_client_init(_udp_t* skt, char* UDP_SERVER_IP, int UDP_PORT)
{
	skt->addr_len = sizeof(struct sockaddr_in);
    if ((skt->sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    	perror("socket");
    	exit(1);
    }

    bzero(&skt->addr, sizeof(skt->addr));
	skt->addr.sin_family = AF_INET;
	skt->addr.sin_port = htons(UDP_PORT);
	skt->addr.sin_addr.s_addr = inet_addr(UDP_SERVER_IP);
}

/**===================================================================
 @Name:udp_server_init
 @Function:initialize the server udp socket
 @Input Parameters: skt, the udp socket handle, defined in rrc_contest.h
 	 	 	 	 	UDP_SERVER_IP, the server's IP address
 	 	 	 	 	UDP_PORT the port number the server wants to bind with
 @Output Parameters:
 @Note: should be called by the server betore message send/receive.
 ==================================================================== **/
void udp_server_init(_udp_t* skt, char* UDP_SERVER_IP, int UDP_PORT)
{
	skt->addr_len = sizeof(struct sockaddr_in);

	if ((skt->sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror ("socket");
		exit(1);
	}

	bzero(&skt->addr, sizeof(skt->addr));
	skt->addr.sin_family = AF_INET;
	skt->addr.sin_port = htons(UDP_PORT);
	skt->addr.sin_addr.s_addr = htonl(INADDR_ANY) ;

	if (bind(skt->sockfd, (struct sockaddr *)&(skt->addr), sizeof(skt->addr))<0) {
		perror("connect");
		exit(1);
	}
}

/**===================================================================
 @Name:udp_send
 @Function:send a message by udp
 @Input Parameters: skt, the udp socket handle, defined in rrc_contest.h
 	 	 	 	 	buffer, a pointer to the start of the message to be sent
 	 	 	 	 	len, length of the message to be sent
 @Output Parameters:
 @Note: should be called by the server after server/client init.
 ==================================================================== **/
int udp_send(_udp_t* skt, char* buffer, int len)
{
	return sendto(skt->sockfd, buffer, len, 0, (struct sockaddr *)&(skt->addr), skt->addr_len);
}

/**===================================================================
 @Name:udp_receive
 @Function:receive an udp message
 @Input Parameters: skt, the udp socket handle, defined in rrc_contest.h
 	 	 	 	 	buffer, the pointer to a buffer to store the received message
 @Output Parameters: len, length of the received message
 @Note: should be called by the server after server/client init.
 ==================================================================== **/
int udp_receive(_udp_t* skt, char* buffer)
{
	return recvfrom(skt->sockfd, buffer, UDP_BUFFER_MAX_SIZE, 0, (struct sockaddr *)&(skt->addr), (socklen_t *)&(skt->addr_len));
}
