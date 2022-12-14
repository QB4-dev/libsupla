/*
 * Copyright (c) 2022 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef SRC_PORT_NET_H_
#define SRC_PORT_NET_H_

#include "arch.h"

typedef void* supla_link_t;

int supla_cloud_connect(supla_link_t *link, const char *host, int port, unsigned char ssl);
int supla_cloud_send(supla_link_t link, void *buf, int count);
int supla_cloud_recv(supla_link_t link, void *buf, int count);
int supla_cloud_disconnect(supla_link_t *link);

#endif /* SRC_PORT_NET_H_ */
