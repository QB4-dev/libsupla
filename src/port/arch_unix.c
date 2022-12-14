/*
 * Copyright (c) 2022 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "net.h"

#if (LIBSUPLA_ARCH == LIBSUPLA_ARCH_UNIX)

#ifndef NOSSL
#include "../supla-common/supla-socket.h"

void *supla_link_init(const char host[], int port, unsigned char secure)
{
	return ssocket_client_init(host,port,secure);
}

int supla_link_connect(void *link)
{
	return ssocket_client_connect(link,NULL,NULL);
}

int supla_link_read(void *link, void *buf, int count)
{
	return ssocket_read(link,NULL,buf,count);
}

int supla_link_write(void *link, void *buf, int count)
{
	return ssocket_write(link,NULL,buf,count);
}

void supla_link_close(void *link)
{
	return ssocket_close(link);
}

void supla_link_free(void *link)
{
	return ssocket_free(link);
}

#else

typedef struct {
	int port;
	char *host;
	int sfd;
} backend_data_t;

void *supla_link_init(const char host[], int port, unsigned char secure)
{
	backend_data_t *ssd = malloc(sizeof(backend_data_t));
	if(!ssd)
		return NULL;

	memset(ssd, 0, sizeof(backend_data_t));
	if(secure){
		supla_log(LOG_WARNING,"WARNING: basic SUPLA Cloud backend doesn't support encryption");
		port = (!port || port == 2016) ? 2015 : port; //switch to unencrypted port
	}

	ssd->port = port;
	ssd->sfd = -1;
	if(host)
		ssd->host = strdup(host);

	return ssd;
}

void supla_link_close(void *ssd)
{
	backend_data_t *socket_data = ssd;
	if(socket_data->sfd != -1){
		close(socket_data->sfd);
		socket_data->sfd = -1;
	}
}

void supla_link_free(void *ssd)
{
	backend_data_t *socket_data = ssd;
	if(socket_data){
		supla_link_close(ssd);
		free(socket_data->host);
		socket_data->host = NULL;
		free(ssd);
	}
}

int supla_link_connect(void *ssd)
{
	backend_data_t *socket_data = ssd;
	supla_link_close(ssd);

	struct in6_addr serveraddr;
	struct addrinfo hints, *res = NULL;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_NUMERICSERV;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	socket_data->sfd = -1;
	char port[15];
	snprintf(port, sizeof(port), "%i", socket_data->port);

	char empty[] = "";
	char *server = empty;

	if(socket_data->host != NULL && strnlen(socket_data->host,1024) > 0) {
		server = socket_data->host;
	}

	if (inet_pton(AF_INET, server, &serveraddr) == 1) {
		hints.ai_family = AF_INET;
		hints.ai_flags |= AI_NUMERICHOST;
	} else if (inet_pton(AF_INET6, server, &serveraddr) == 1) {
		hints.ai_family = AF_INET6;
		hints.ai_flags |= AI_NUMERICHOST;
	}

	if (getaddrinfo(server, port, &hints, &res) != 0)
		return -1;

	socket_data->sfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if(socket_data->sfd == -1){
		freeaddrinfo(res);
		return -1;
	}

	if(connect(socket_data->sfd, res->ai_addr, res->ai_addrlen) != 0) {
		close(socket_data->sfd);
		socket_data->sfd = -1;
		freeaddrinfo(res);
		return -1;
	}
	freeaddrinfo(res);
	return 1;
}

int supla_link_read(void *ssd, void *buf, int count)
{
	backend_data_t *socket_data = ssd;
	return recv(socket_data->sfd, buf, count, MSG_DONTWAIT);
}

int supla_link_write(void *ssd, void *buf, int count)
{
	backend_data_t *socket_data = ssd;
	return send(socket_data->sfd, buf, count, MSG_NOSIGNAL);
}
#endif

#endif


