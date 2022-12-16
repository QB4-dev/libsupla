/*
 * Copyright (c) 2022 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "supla-extvalue.h"
#include "port/arch.h"

void supla_extval_init(supla_extended_value_t *supla_extval, char sync_onchange)
{
	memset(supla_extval,0,sizeof(supla_extended_value_t));
	supla_extval->sync = 1;
	supla_extval->sync_onchange = sync_onchange;
}

void supla_extval_free(supla_extended_value_t *supla_extval)
{
}

int supla_extval_set(supla_extended_value_t *supla_extval, TSuplaChannelExtendedValue *extval)
{
	if(!supla_extval || !extval || extval->size > SUPLA_CHANNELEXTENDEDVALUE_SIZE)
		return SUPLA_RESULT_FALSE;

	if(!supla_extval->sync_onchange)
		supla_extval->sync = 0;

	if(memcmp(&supla_extval->extval,extval,SUPLA_CHANNELEXTENDEDVALUE_SIZE) != 0){
		supla_extval->extval = *extval;
		supla_extval->sync = 0;
	}
	return SUPLA_RESULT_TRUE;
}

