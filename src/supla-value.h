/*
 * Copyright (c) 2022 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef SUPLA_VALUE_H_
#define SUPLA_VALUE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <libsupla/supla.h>

typedef struct {
	char sync;
	char sync_onchange;
	TSuplaChannelValue data;
} supla_value_t;

void supla_val_init(supla_value_t *supla_val, char sync_onchange);
void supla_val_deinit(supla_value_t *supla_val);

int supla_val_set(supla_value_t *supla_val, void *value, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* SUPLA_VALUE_H_ */
