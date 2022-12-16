/*
 * Copyright (c) 2022 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef SRC_SUPLA_ACTION_TRIGGER_H_
#define SRC_SUPLA_ACTION_TRIGGER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <libsupla/supla.h>

typedef struct {
	char sync;
	TActionTriggerProperties properties;
	TDS_ActionTrigger at;
} supla_action_trigger_t;

void supla_action_trigger_init(supla_action_trigger_t *supla_action_trigger);
void supla_action_trigger_free(supla_action_trigger_t *supla_action_trigger);

int supla_action_trigger_emit(supla_action_trigger_t *supla_action_trigger, const int ch_num, const _supla_int_t action);


#ifdef __cplusplus
}
#endif

#endif /* SRC_SUPLA_ACTION_TRIGGER_H_ */
