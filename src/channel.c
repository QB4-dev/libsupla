/*
 * Copyright (c) 2022 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "channel-priv.h"

int supla_channel_init(supla_channel_t* ch)
{
	struct supla_channel_priv *priv;
	if(!ch)
		return EINVAL;

	//TODO verify channel type and functions

	if(ch->priv)
		return EPERM; //already initialized

	ch->priv = calloc(1, sizeof(struct supla_channel_priv));
	if(ch->priv == NULL)
		return ENOMEM;

	priv = ch->priv;
	priv->lck = lck_init();

	switch(ch->type){
	case SUPLA_CHANNELTYPE_SENSORNO:
	case SUPLA_CHANNELTYPE_SENSORNC:
	case SUPLA_CHANNELTYPE_DISTANCESENSOR:
	case SUPLA_CHANNELTYPE_CALLBUTTON:
	case SUPLA_CHANNELTYPE_RELAYHFD4:
	case SUPLA_CHANNELTYPE_RELAYG5LA1A:
	case SUPLA_CHANNELTYPE_2XRELAYG5LA1A:
	case SUPLA_CHANNELTYPE_RELAY:
	case SUPLA_CHANNELTYPE_THERMOMETERDS18B20:
	case SUPLA_CHANNELTYPE_DHT11:
	case SUPLA_CHANNELTYPE_DHT22:
	case SUPLA_CHANNELTYPE_DHT21:
	case SUPLA_CHANNELTYPE_AM2302:
	case SUPLA_CHANNELTYPE_AM2301:
	case SUPLA_CHANNELTYPE_THERMOMETER:
	case SUPLA_CHANNELTYPE_HUMIDITYSENSOR:
	case SUPLA_CHANNELTYPE_HUMIDITYANDTEMPSENSOR:
	case SUPLA_CHANNELTYPE_WINDSENSOR:
	case SUPLA_CHANNELTYPE_PRESSURESENSOR:
	case SUPLA_CHANNELTYPE_RAINSENSOR:
	case SUPLA_CHANNELTYPE_WEIGHTSENSOR:
	case SUPLA_CHANNELTYPE_WEATHER_STATION:
	case SUPLA_CHANNELTYPE_DIMMER:
	case SUPLA_CHANNELTYPE_RGBLEDCONTROLLER:
	case SUPLA_CHANNELTYPE_DIMMERANDRGBLED:
	case SUPLA_CHANNELTYPE_VALVE_OPENCLOSE:
	case SUPLA_CHANNELTYPE_VALVE_PERCENTAGE:
	case SUPLA_CHANNELTYPE_GENERAL_PURPOSE_MEASUREMENT:
	case SUPLA_CHANNELTYPE_ENGINE:
		priv->supla_val = malloc(sizeof(supla_value_t));
		if(priv->supla_val == NULL)
			goto malloc_failed;
		supla_val_init(priv->supla_val,ch->sync_values_onchange);
		break;
	case SUPLA_CHANNELTYPE_ELECTRICITY_METER:
	case SUPLA_CHANNELTYPE_IMPULSE_COUNTER:
	case SUPLA_CHANNELTYPE_THERMOSTAT:
		priv->supla_val = malloc(sizeof(supla_value_t));
		if(priv->supla_val == NULL)
			goto malloc_failed;
		supla_val_init(priv->supla_val,ch->sync_values_onchange);

		priv->supla_extval = malloc(sizeof(supla_extended_value_t));
		if(priv->supla_extval == NULL)
			goto malloc_failed;

		supla_extval_init(priv->supla_extval,ch->sync_values_onchange);
		break;
	case SUPLA_CHANNELTYPE_ACTIONTRIGGER:
		priv->action_trigger = malloc(sizeof(supla_action_trigger_t));
		if(priv->action_trigger == NULL)
			goto malloc_failed;

		supla_action_trigger_init(priv->action_trigger);
		priv->action_trigger->properties.disablesLocalOperation = ch->action_trigger_conflicts;
		priv->action_trigger->properties.relatedChannelNumber = priv->number;
		break;
	default:
		break;
	}

	/* set SUPLA_CHANNEL_FLAG_CHANNELSTATE if on_get_state callback function is set*/
	if(ch->on_get_state)
		ch->flags |= SUPLA_CHANNEL_FLAG_CHANNELSTATE;

	return 0;

malloc_failed:
	lck_free(priv->lck);
	supla_val_deinit(priv->supla_val);
	free(priv->supla_val);

	supla_extval_deinit(priv->supla_extval);
	free(priv->supla_extval);

	supla_action_trigger_deinit(priv->action_trigger);
	free(priv->action_trigger);

	free(priv);
	return ENOMEM;
}


int supla_channel_deinit(supla_channel_t *ch)
{
	struct supla_channel_priv *priv = ch->priv;
	if(priv == NULL)
		return 0;

	lck_free(priv->lck);
	free(priv->supla_val);
	free(priv->supla_extval);
	free(priv->action_trigger);
	free(priv);
	ch->priv = NULL;
	return 0;
}

int supla_channel_get_assigned_number(supla_channel_t *ch)
{
	struct supla_channel_priv *priv = ch->priv;
	if(!priv)
		return -1;
	else
		return priv->number;
}


int supla_channel_set_value(supla_channel_t *ch, void *value, size_t len)
{
	int rc;
	struct supla_channel_priv *priv = ch->priv;

	if(!ch->priv || !value || !len)
		return SUPLA_RESULT_FALSE;

	lck_lock(priv->lck);
	rc = supla_val_set(priv->supla_val,value,len);
	//TODO allow to set sync policy: everytime/on change
	lck_unlock(priv->lck);
	return rc;
}

int supla_channel_set_binary_value(supla_channel_t *ch, uint8_t value)
{
	//TODO check channel type
	return supla_channel_set_value(ch, &value,sizeof(uint8_t));
}

int supla_channel_set_double_value(supla_channel_t *ch, double value)
{
	//TODO check channel type
	return supla_channel_set_value(ch,&value,sizeof(double));
}

int supla_channel_humidtemp_value(supla_channel_t *ch, double humid, double temp)
{
	//TODO check channel type
	char out[SUPLA_CHANNELVALUE_SIZE];
	_supla_int_t t = temp * 1000.0;
	_supla_int_t h = humid * 1000.0;
	memcpy(&out[0], &t, 4);
	memcpy(&out[4], &h, 4);
	return supla_channel_set_value(ch,out,sizeof(out));
}

int supla_channel_set_relay_value(supla_channel_t *ch, TRelayChannel_Value *relay)
{
	//TODO check channel type
	return supla_channel_set_value(ch,relay,sizeof(TRelayChannel_Value));
}

int supla_channel_set_rgbw_value(supla_channel_t *ch, TRGBW_Value *rgbw)
{
	//TODO check channel type
	return supla_channel_set_value(ch,rgbw,sizeof(TRGBW_Value));
}

int supla_channel_set_impulse_counter_value(supla_channel_t *ch, TDS_ImpulseCounter_Value *ic)
{
	struct supla_channel_priv *priv = ch->priv;
	if(ch->type != SUPLA_CHANNELTYPE_IMPULSE_COUNTER){
		supla_log(LOG_ERR,"ch[%d] cannot set impulse counter value: bad channel type",priv->number);
		return SUPLA_RESULTCODE_CHANNEL_CONFLICT;
	}
	return supla_channel_set_value(ch,ic,sizeof(TDS_ImpulseCounter_Value));
}

int supla_channel_set_roller_shutter_value(supla_channel_t *ch, TDSC_RollerShutterValue *rs)
{
	struct supla_channel_priv *priv = ch->priv;
	if(ch->type != SUPLA_CHANNELTYPE_RELAY){
		supla_log(LOG_ERR,"ch[%d] cannot set roller shuter value: bad channel type",priv->number);
		return SUPLA_RESULTCODE_CHANNEL_CONFLICT;
	}
	return supla_channel_set_value(ch,rs,sizeof(TDSC_RollerShutterValue));
}

int supla_channel_set_faceblind_value(supla_channel_t *ch, TDSC_FacadeBlindValue *fb)
{
	struct supla_channel_priv *priv = ch->priv;
	if(ch->type != SUPLA_CHANNELTYPE_RELAY){
		supla_log(LOG_ERR,"ch[%d] cannot set faceblind value: bad channel type",priv->number);
		return SUPLA_RESULTCODE_CHANNEL_CONFLICT;
	}
	return supla_channel_set_value(ch,fb,sizeof(TDSC_FacadeBlindValue));
}

int supla_channel_set_electricity_meter_value(supla_channel_t *ch, TElectricityMeter_Value *em)
{
	struct supla_channel_priv *priv = ch->priv;
	if(ch->type != SUPLA_CHANNELTYPE_ELECTRICITY_METER){
		supla_log(LOG_ERR,"ch[%d] cannot set electricity meter value: bad channel type",priv->number);
		return SUPLA_RESULTCODE_CHANNEL_CONFLICT;
	}
	return supla_channel_set_value(ch,em,sizeof(TElectricityMeter_Value));
}

int supla_channel_set_thermostat_value(supla_channel_t *ch, TThermostat_Value *th)
{
	struct supla_channel_priv *priv = ch->priv;
	if(ch->type != SUPLA_CHANNELTYPE_THERMOSTAT){
		supla_log(LOG_ERR,"ch[%d] cannot set thermostat value: bad channel type",priv->number);
		return SUPLA_RESULTCODE_CHANNEL_CONFLICT;
	}
	return supla_channel_set_value(ch,th,sizeof(TThermostat_Value));
}


int supla_channel_set_extval(supla_channel_t *ch, TSuplaChannelExtendedValue *extval)
{
	int rc;
	struct supla_channel_priv *priv = ch->priv;

	if(!ch->priv || !extval)
		return SUPLA_RESULT_FALSE;

	lck_lock(priv->lck);
	rc = supla_extval_set(priv->supla_extval,extval);
	//TODO allow to set sync policy: everytime/on change
	lck_unlock(priv->lck);
	return rc;
}

int supla_channel_set_electricity_meter_extvalue(supla_channel_t *ch, TElectricityMeter_ExtendedValue_V2 *emx)
{
	struct supla_channel_priv *priv = ch->priv;
	if(ch->type != SUPLA_CHANNELTYPE_ELECTRICITY_METER){
		supla_log(LOG_ERR,"ch[%d] cannot set electricity meter extvalue: bad channel type",priv->number);
		return SUPLA_RESULTCODE_CHANNEL_CONFLICT;
	}

	TSuplaChannelExtendedValue extval;
	srpc_evtool_v2_emextended2extended(emx,&extval);
	return supla_channel_set_extval(ch,&extval);
}

int supla_channel_set_thermostat_extvalue(supla_channel_t *ch, TThermostat_ExtendedValue *thex)
{
	struct supla_channel_priv *priv = ch->priv;
	if(ch->type != SUPLA_CHANNELTYPE_THERMOSTAT){
		supla_log(LOG_ERR,"ch[%d] cannot set thermostat extvalue: bad channel type",priv->number);
		return SUPLA_RESULTCODE_CHANNEL_CONFLICT;
	}

	TSuplaChannelExtendedValue extval;
	srpc_evtool_v1_thermostatextended2extended(thex,&extval);
	return supla_channel_set_extval(ch,&extval);
}


int supla_channel_emit_action(supla_channel_t *ch, const _supla_int_t action)
{
	int rc;
	struct supla_channel_priv *priv = ch->priv;

	if(!ch->priv)
		return EINVAL;

	lck_lock(priv->lck);
	rc = supla_action_trigger_emit(priv->action_trigger,priv->number,action);
	lck_unlock(priv->lck);
	return rc;
}

//private functions
TDS_SuplaDeviceChannel_C supla_channel_to_register_struct(supla_channel_t *ch)
{
	TDS_SuplaDeviceChannel_C reg_channel;
	struct supla_channel_priv *priv = ch->priv;

	reg_channel.Number = priv->number;
	reg_channel.Type = ch->type;
	reg_channel.Default = ch->default_function;
	reg_channel.Flags = ch->flags;

	if(ch->type == SUPLA_CHANNELTYPE_ACTIONTRIGGER){
		reg_channel.ActionTriggerCaps = ch->action_trigger_caps;
		if(priv->action_trigger)
			reg_channel.actionTriggerProperties = priv->action_trigger->properties;
	}else{
		reg_channel.FuncList = ch->supported_functions;
		if(priv->supla_val)
			memcpy(reg_channel.value, priv->supla_val->data.value, SUPLA_CHANNELVALUE_SIZE);
	}
	return reg_channel;
}

void supla_channel_sync(void *srpc, supla_channel_t *ch)
{
	struct supla_channel_priv *priv = ch->priv;

	lck_lock(priv->lck);
	if(priv->supla_val && !priv->supla_val->sync){
		supla_log(LOG_DEBUG,"sync channel[%d] val ",priv->number);
		priv->supla_val->sync =
				srpc_ds_async_channel_value_changed_c(srpc,priv->number,priv->supla_val->data.value,0,0);
		//TODO online flag and validity time
	}

	if(priv->supla_extval && !priv->supla_extval->sync){
		supla_log(LOG_DEBUG,"sync channel[%d] extval",priv->number);
		priv->supla_extval->sync =
				srpc_ds_async_channel_extendedvalue_changed(srpc,priv->number,&priv->supla_extval->extval);
	}

	if(priv->action_trigger && !priv->action_trigger->sync){
		supla_log(LOG_DEBUG,"sync channel[%d] action ch[%d]->%d",
				priv->number,priv->action_trigger->at.ChannelNumber,priv->action_trigger->at.ActionTrigger);

		priv->action_trigger->sync =
				srpc_ds_async_action_trigger(srpc,&priv->action_trigger->at);
	}
	lck_unlock(priv->lck);
}

