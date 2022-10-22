/*
 * Copyright (c) 2022 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "net.h"
#include "arch.h"

#if LIBSUPLA_ARCH == LIBSUPLA_ARCH_UNIX
#include "../supla-common/supla-socket.h"
#elif LIBSUPLA_ARCH == LIBSUPLA_ARCH_ESP8266
#endif

#if LIBSUPLA_ARCH == LIBSUPLA_ARCH_ESP8266 || LIBSUPLA_ARCH == LIBSUPLA_ARCH_ESP32
typedef struct {
	int sfd;
}esp_socket_t;

typedef struct {
  unsigned char secure;
  int port;
  char *host;

  esp_socket_t supla_socket;
} esp_socket_data_t;



void *esp_link_init(const char host[], int port, unsigned char secure)
{
	esp_socket_data_t *ssd = malloc(sizeof(esp_socket_data_t));
	if(!ssd)
		return NULL;
	memset(ssd, 0, sizeof(esp_socket_data_t));

	ssd->port = port;
	ssd->secure = secure;
	ssd->supla_socket.sfd = -1;
	if(host)
	  ssd->host = strdup(host);

	//TODO SSL stuff
	return ssd;
}

void esp_link_close(esp_socket_data_t *ssd)
{
	esp_socket_t *esp_socket = &ssd->supla_socket;

	if(esp_socket->sfd != -1){
		close(esp_socket->sfd);
		esp_socket->sfd = -1;
	}
}

void esp_link_free(esp_socket_data_t *ssd)
{
	if(ssd){
		esp_link_close(ssd);
		free(ssd->host);
		ssd->host = NULL;
		free(ssd);
	}
}

int esp_link_connect(esp_socket_data_t *ssd)
{
	esp_link_close(ssd);

	struct in6_addr serveraddr;
	struct addrinfo hints, *res = NULL;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_NUMERICSERV;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	ssd->supla_socket.sfd = -1;
	char port[15];
	snprintf(port, sizeof(port), "%i", ssd->port);

	char empty[] = "";
	char *server = empty;

	if(ssd->host != NULL && strnlen(ssd->host, 1024) > 0) {
		server = ssd->host;
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

	ssd->supla_socket.sfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if(ssd->supla_socket.sfd == -1){
		freeaddrinfo(res);
		return -1;
	}

	if(connect(ssd->supla_socket.sfd, res->ai_addr, res->ai_addrlen) != 0) {
		close(ssd->supla_socket.sfd);
		ssd->supla_socket.sfd = -1;
		freeaddrinfo(res);
		return -1;
	}
	freeaddrinfo(res);
	return 1;
	//TODO SSL stuff
}

int esp_link_read(esp_socket_data_t *ssd, void *buf, int count)
{
	esp_socket_t *supla_socket = &ssd->supla_socket;
	return recv(supla_socket->sfd, buf, count, MSG_DONTWAIT);
}

int esp_link_write(esp_socket_data_t *ssd, void *buf, int count)
{
	esp_socket_t *supla_socket = &ssd->supla_socket;
	return send(supla_socket->sfd, buf, count, MSG_NOSIGNAL);
}
#endif

//supla_net_adapter_t esp_net_adapter = {
//	.init = esp_link_init,
//	.connect = esp_link_connect,
//	.read = esp_link_read,
//	.write = esp_link_write,
//	.close = esp_link_close,
//	.free = esp_link_free
//};

void *supla_link_init(const char host[], int port, unsigned char secure)
{
#if LIBSUPLA_ARCH == LIBSUPLA_ARCH_UNIX
	return ssocket_client_init(host,port,secure);
#elif LIBSUPLA_ARCH == LIBSUPLA_ARCH_ESP8266 || LIBSUPLA_ARCH == LIBSUPLA_ARCH_ESP32
	return esp_link_init(host,port,secure);
#else
  return NULL;
#endif
}

int supla_link_connect(void *link)
{
#if LIBSUPLA_ARCH == LIBSUPLA_ARCH_UNIX
	return ssocket_client_connect(link,NULL,NULL);
#elif LIBSUPLA_ARCH == LIBSUPLA_ARCH_ESP8266 || LIBSUPLA_ARCH == LIBSUPLA_ARCH_ESP32
	return esp_link_connect(link);
#else
  return 0;
#endif
}

int supla_link_read(void *link, void *buf, int count)
{
#if LIBSUPLA_ARCH == LIBSUPLA_ARCH_UNIX
	return ssocket_read(link,NULL,buf,count);
#elif LIBSUPLA_ARCH == LIBSUPLA_ARCH_ESP8266 || LIBSUPLA_ARCH == LIBSUPLA_ARCH_ESP32
	return esp_link_read(link,buf,count);
#else
  return 0;
#endif
}

int supla_link_write(void *link, void *buf, int count)
{
#if LIBSUPLA_ARCH == LIBSUPLA_ARCH_UNIX
	return ssocket_write(link,NULL,buf,count);
#elif LIBSUPLA_ARCH == LIBSUPLA_ARCH_ESP8266 || LIBSUPLA_ARCH == LIBSUPLA_ARCH_ESP32
	return esp_link_write(link,buf,count);
#else
  return 0;
#endif
}

void supla_link_close(void *link)
{
#if LIBSUPLA_ARCH == LIBSUPLA_ARCH_UNIX
	return supla_link_close(link);
#elif LIBSUPLA_ARCH == LIBSUPLA_ARCH_ESP8266 || LIBSUPLA_ARCH == LIBSUPLA_ARCH_ESP32
	return esp_link_close(link);
#else
  return 0;
#endif
}

void supla_link_free(void *link)
{
#if LIBSUPLA_ARCH == LIBSUPLA_ARCH_UNIX
	return ssocket_free(link);
#elif LIBSUPLA_ARCH == LIBSUPLA_ARCH_ESP8266 || LIBSUPLA_ARCH == LIBSUPLA_ARCH_ESP32
	return esp_link_free(link);
#else
  return 0;
#endif
}
