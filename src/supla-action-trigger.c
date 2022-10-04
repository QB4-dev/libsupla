/*
 * Copyright (c) 2022 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "supla-action-trigger.h"
#include "port/arch.h"

void supla_action_trigger_init(supla_action_trigger_t *supla_action_trigger)
{
	memset(&supla_action_trigger->at,0,sizeof(TDS_ActionTrigger));
	supla_action_trigger->sync = 1;
}

void supla_action_trigger_deinit(supla_action_trigger_t *supla_action_trigger)
{
}

int supla_action_trigger_emit(supla_action_trigger_t *supla_action_trigger, const int ch_num, const _supla_int_t action)
{
	if(!supla_action_trigger || !action)
		return SUPLA_RESULT_FALSE;

	supla_action_trigger->at.ChannelNumber = ch_num;
	supla_action_trigger->at.ActionTrigger = action;
	supla_action_trigger->sync = 0;
	return SUPLA_RESULT_TRUE;
}

