/*
 * Copyright (c) 2022 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef SRC_SUPLA_EXTVALUE_H_
#define SRC_SUPLA_EXTVALUE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <libsupla/supla.h>

typedef struct {
	char sync;
	char sync_onchange;
	TSuplaChannelExtendedValue extval;
} supla_extended_value_t;

void supla_extval_init(supla_extended_value_t *supla_extval, char sync_onchange);
void supla_extval_deinit(supla_extended_value_t *supla_extval);

int supla_extval_set(supla_extended_value_t *supla_extval, TSuplaChannelExtendedValue *extval);

#ifdef __cplusplus
}
#endif

#endif /* SRC_SUPLA_EXTVALUE_H_ */
