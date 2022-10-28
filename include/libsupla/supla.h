/*
 * Copyright (c) 2022 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef LIBSUPLA_SUPLA_H_
#define LIBSUPLA_SUPLA_H_

#include <stdint.h>

#include "version.h"
#include "config.h"

#include "supla-common/proto.h"
#include "supla-common/cfg.h"
#include "supla-common/log.h"
#include "supla-common/lck.h"
#include "supla-common/srpc.h"

struct supla_config {
	char email[SUPLA_EMAIL_MAXSIZE];
	char auth_key[SUPLA_AUTHKEY_SIZE];
	char guid[SUPLA_GUID_SIZE];
	char server[SUPLA_SERVER_NAME_MAXSIZE];
	char ssl;
	int port;
	int activity_timeout;
};

typedef struct {
	const char *id;
	void *(*init)(const char host[], int port, unsigned char secure);
	int (*connect)(void *link);
	int (*read)(void *link, void *buf, int count);
	int (*write)(void *link, void *buf, int count);
	void (*close)(void *link);
	void (*free)(void *link);
} supla_cloud_backend_t;

#endif /* LIBSUPLA_SUPLA_H_ */
