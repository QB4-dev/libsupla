/*
 * Copyright (c) 2022 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef SUPLA_PUSH_NOTIFICATION_H_
#define SUPLA_PUSH_NOTIFICATION_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <libsupla/supla.h>

typedef struct {
    char enabled;
    unsigned char srv_managed_fields;
} supla_push_notification_config_t;

#ifdef __cplusplus
}
#endif

#endif /* SUPLA_PUSH_NOTIFICATION_H_ */
