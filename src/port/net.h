/*
 * Copyright (c) 2022 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef SRC_PORT_NET_H_
#define SRC_PORT_NET_H_

#include "arch.h"

void *supla_link_init(const char host[], int port, unsigned char secure);
int supla_link_connect(void *link);
int supla_link_read(void *link, void *buf, int count);
int supla_link_write(void *link, void *buf, int count);
void supla_link_close(void *link);
void supla_link_free(void *link);

#endif /* SRC_PORT_NET_H_ */
