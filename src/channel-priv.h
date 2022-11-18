/*
 * Copyright (c) 2022 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef SUPLA_DEVICE_PRIV_H_
#define SUPLA_DEVICE_PRIV_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <libsupla/channel.h>

#include "supla-value.h"
#include "supla-extvalue.h"
#include "supla-action-trigger.h"

/* SUPLA channel private data */
struct supla_channel_priv {
	void *lck;
	int number; //filled by device
	int active_function; //may be changed by server
	supla_value_t *supla_val;
	supla_extended_value_t *supla_extval;
	supla_action_trigger_t *action_trigger;
};

/**
 * @brief  convert channel to Supla proto channel definition
 */
TDS_SuplaDeviceChannel_C supla_channel_to_register_struct(supla_channel_t *ch);

/**
 * @brief  Set active channel function(may be changed by server)
 *
 * @param[in] ch given channel
 * @param[in] function supported functions //SUPLA_CHANNELFNC_*
 * @return SUPLA_RESULT_TRUE on success
 */
int supla_channel_set_active_function(supla_channel_t *ch, int function);

/**
 * @brief  sync channel data with server
 *
 * @param[in] srpc srpc object
 * @param[in] ch given channel
 */
void supla_channel_sync(void *srpc, supla_channel_t *ch);

#ifdef __cplusplus
}
#endif

#endif /* SUPLA_DEVICE_PRIV_H_ */
