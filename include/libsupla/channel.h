/*
 * Copyright (c) 2022 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef SUPLA_CHANNEL_H_
#define SUPLA_CHANNEL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/queue.h>
#include "supla.h"

/**
 * @brief SUPLA channel instance
 *
 * Channel config examples:
 * @code{c}
 *
 *
 * supla_channel_t *dist_channel;
 *
 * supla_channel_config_t distance_channel_config = {
 * 	.type = SUPLA_CHANNELTYPE_DISTANCESENSOR ,
 * 	.supported_functions = SUPLA_CHANNELFNC_DISTANCESENSOR ,
 * 	.default_function = SUPLA_CHANNELFNC_DISTANCESENSOR,
 * };
 *
 *
 * supla_channel_t *light_channel;
 *
 * int power_set_value(supla_channel_t *ch, TSD_SuplaChannelNewValue *new_value)
 * {
 * 	TRelayChannel_Value *relay_val = (TRelayChannel_Value*)new_value->value;
 * 	supla_log(LOG_DEBUG,"power set value %d",relay_val->hi);
 * 	return supla_channel_set_relay_value(ch,relay_val);
 * }
 *
 * supla_channel_config_t light_channel_config = {
 * 	.type = SUPLA_CHANNELTYPE_RELAY,
 * 	.supported_functions = SUPLA_CHANNELFNC_LIGHTSWITCH|SUPLA_CHANNELFNC_POWERSWITCH,
 * 	.default_function = SUPLA_CHANNELFNC_LIGHTSWITCH,
 * 	.flags=SUPLA_CHANNEL_FLAG_COUNTDOWN_TIMER_SUPPORTED,
 * 	.on_set_value = power_set_value
 * };
 *
 * supla_channel_t *at_channel;
 *
 * supla_channel_config_t at_channel_config = {
 * 	.type = SUPLA_CHANNELTYPE_ACTIONTRIGGER,
 * 	.supported_functions = SUPLA_CHANNELFNC_ACTIONTRIGGER,
 * 	.default_function = SUPLA_CHANNELFNC_ACTIONTRIGGER,
 * 	.action_trigger_caps = SUPLA_ACTION_CAP_SHORT_PRESS_x1|SUPLA_ACTION_CAP_SHORT_PRESS_x2,
 * 	.action_trigger_conflicts = SUPLA_ACTION_CAP_SHORT_PRESS_x2,
 * 	.action_trigger_related_channel = light_channel
 * };
 * @endcode
 *
 */
typedef struct supla_channel supla_channel_t;

/**
 * @brief Function called on receive new value from server
 * Example function:
 * @code{c}
 * int set_power_value(supla_channel_t *ch, TSD_SuplaChannelNewValue *new_value)
 * {
 * 	TRelayChannel_Value *relay_val = (TRelayChannel_Value*)new_value->value;
 * 	lightswitch_t *light = ch->ctx;                          //get lightswitch instance from context
 *
 * 	supla_log(LOG_DEBUG,"power set value %d",relay_val->hi); //log to user
 * 	light_set(light,relay_val->hi);                          //perform operation on hardware
 * 	return supla_channel_set_relay_value(ch,relay_val);      //inform server about data change
 * }
 * @endcode
 *
 * @param[in] ch called channel
 * @param[in] new_value new value from server
 * @return SUPLA_RESULT_TRUE on success or error number if failed
 */
typedef int (*supla_channel_set_value_handler_t)(supla_channel_t *ch, TSD_SuplaChannelNewValue *new_value);

/**
 * @brief Function called on get state request from server
 * Example function:
 * @code{c}
 * static int get_dist_state(supla_channel_t *ch, TDSC_ChannelState *state)
 * {
 * 	state->Fields = SUPLA_CHANNELSTATE_FIELD_LIGHTSOURCEOPERATINGTIME;
 * 	state->LightSourceOperatingTime=1234;
 * 	return 0;
 * }
 * @endcode
 *
 * @param[in] ch called channel
 * @param[out] state channel state to be filled
 * @return 0 on success or error number if failed
 */
typedef int (*supla_channel_get_state_handler_t)(supla_channel_t *ch, TDSC_ChannelState *state);

/**
 * @brief Function called on channel calcfg request from server
 * Example function:
 * @code{c}
 * static int get_light_state(supla_channel_t *ch, TSD_DeviceCalCfgRequest *calcfg)
 * {
 * 	if(calcfg->Command == SUPLA_CALCFG_CMD_SET_LIGHTSOURCE_LIFESPAN){
 * 		TCalCfg_LightSourceLifespan *cfg = calcfg->Data;
 * 		if(cfg->SetTime)
 * 			set_light_lifespan(ch->ctx, cfg->LightSourceLifespan);
 * 		if(cfg->ResetCounter)
 * 			reset_light_counter(ch->ctx);
 * 	}
 * 	return SUPLA_CALCFG_RESULT_DONE;
 * }
 * @endcode
 *
 * @param[in] ch called channel
 * @param[in] calcfg channel configuration from server
 * @return SUPLA_CALCFG_RESULT_DONE on success or SUPLA_CALCFG_RESULT_* error if failed
 */
typedef int (*supla_channel_set_calcfg_handler_t)(supla_channel_t *ch, TSD_DeviceCalCfgRequest *calcfg);

typedef int (*supla_channel_get_config_handler_t)(supla_channel_t *ch, TSD_ChannelConfig *chcfg);


/**
 * SUPLA channel config structure
 */
typedef struct supla_channel_config {
	int type;                                                     //SUPLA_CHANNELTYPE_*
	union {
		struct {
			unsigned int supported_functions;                     //SUPLA_BIT_FUNC_*
			char sync_values_onchange;                            //sync values with server only when changed
			int validity_time_sec;                                //measurement validity time for offline sensors
		};
		struct {
			unsigned int action_trigger_caps;                     //SUPLA_ACTION_CAP_*
			unsigned int action_trigger_conflicts;                //SUPLA_ACTION_CAP_*
			supla_channel_t **action_trigger_related_channel;     //related channel for action trigger
		};
	};
	int default_function;                                         //SUPLA_CHANNELFNC_*
	int flags;                                                    //SUPLA_CHANNEL_FLAG_*

	supla_channel_set_value_handler_t on_set_value;               //on set value request callback function
	supla_channel_get_state_handler_t on_get_state;               //on get state request callback function
	supla_channel_set_calcfg_handler_t on_calcfg_req;             //on calcfg request callback function
	supla_channel_get_config_handler_t on_config_recv;            //on config received from server
	void *data;                                                   //channel context data defined by user
} supla_channel_config_t;

/**
 * @brief  Initialize SUPLA channel - allocate channel private data
 *
 * @param[in] cfg channel configuration
 * @return SUPLA_RESULT_TRUE on success
 */
supla_channel_t* supla_channel_create(const supla_channel_config_t* config);

/**
 * @brief  Deinitialize SUPLA channel - free allocated memory
 *
 * @param[in] ch given channel
 * @return SUPLA_RESULT_TRUE on success
 */
int supla_channel_free(supla_channel_t *ch);

/**
 * @brief  Get SUPLA channel config
 *
 * @param[in] ch given channel
 * @param[out] cfg channel config
 * @return SUPLA_RESULT_TRUE on success
 */
int supla_channel_get_config(supla_channel_t *ch, supla_channel_config_t* config);


/**
 * @brief  Get SUPLA channel data
 *
 * @param[in] ch given channel
 * @return data pointer on success or NULL
 */
void *supla_channel_get_data(supla_channel_t *ch);

/**
 * @brief  Set SUPLA channel data
 *
 * @param[in] ch given channel
 * @param[in] data channel context data
 * @return data pointer
 */
void *supla_channel_set_data(supla_channel_t *ch, void *data);

/**
 * @brief  Get assigned channel number when added to device
 *
 * @param[in] ch given channel
 * @return assigned channel number or -1 on error
 */
int supla_channel_get_assigned_number(supla_channel_t *ch);

/**
 * @brief  Get channel active function
 *
 * @param[in] ch given channel
 * @param[in] function active channel function //SUPLA_CHANNELFNC_*
 * @return SUPLA_RESULT_TRUE on success
 */
int supla_channel_get_active_function(supla_channel_t *ch, int *function);


int supla_channel_set_value(supla_channel_t *ch, void *value, size_t len);
int supla_channel_set_binary_value(supla_channel_t *ch, uint8_t value);
int supla_channel_set_double_value(supla_channel_t *ch, double value);
int supla_channel_set_humidtemp_value(supla_channel_t *ch, double humid, double temp);
int supla_channel_set_relay_value(supla_channel_t *ch, TRelayChannel_Value *relay);
int supla_channel_set_rgbw_value(supla_channel_t *ch, TRGBW_Value *rgbw);
int supla_channel_set_impulse_counter_value(supla_channel_t *ch, TDS_ImpulseCounter_Value *ic);
int supla_channel_set_roller_shutter_value(supla_channel_t *ch, TDSC_RollerShutterValue *rs);
int supla_channel_set_faceblind_value(supla_channel_t *ch, TDSC_FacadeBlindValue *fb);
int supla_channel_set_electricity_meter_value(supla_channel_t *ch, TElectricityMeter_Value *em);
int supla_channel_set_thermostat_value(supla_channel_t *ch, TThermostat_Value *th);

int supla_channel_set_extval(supla_channel_t *ch, TSuplaChannelExtendedValue *extval);
int supla_channel_set_timer_state_extvalue(supla_channel_t *ch, TTimerState_ExtendedValue *tsev);
int supla_channel_set_electricity_meter_extvalue(supla_channel_t *ch, TElectricityMeter_ExtendedValue_V2 *emx);
int supla_channel_set_thermostat_extvalue(supla_channel_t *ch, TThermostat_ExtendedValue *thex);

/**
 * @brief Emit an action from SUPLA action trigger channel
 *
 * @param[in] ch given channel - must be type SUPLA_CHANNELTYPE_ACTIONTRIGGER
 * @param[in] action selected action - must be set action_trigger_caps
 * @return SUPLA_RESULT_TRUE on success or error number
 */
int supla_channel_emit_action(supla_channel_t *ch, const _supla_int_t action);

#ifdef __cplusplus
}
#endif

#endif /* SUPLA_CHANNEL_H_ */
