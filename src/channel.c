/*
 * Copyright (c) 2022 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "channel-priv.h"

supla_channel_t *supla_channel_create(const supla_channel_config_t *config)
{
    assert(NULL != config);

    supla_channel_t *ch = calloc(1, sizeof(supla_channel_t));
    if (!ch)
        return NULL;

    //TODO verify channel config: type and functions

    ch->lck = lck_init();
    ch->config = *config;

    /* set SUPLA_CHANNEL_FLAG_CHANNELSTATE if on_get_state callback function is set*/
    if (ch->config.on_get_state)
        ch->config.flags |= SUPLA_CHANNEL_FLAG_CHANNELSTATE;

    ch->number = -1;
    ch->active_function = ch->config.default_function;

    switch (ch->config.type) {
    case SUPLA_CHANNELTYPE_SENSORNO:
    case SUPLA_CHANNELTYPE_SENSORNC:
    case SUPLA_CHANNELTYPE_DISTANCESENSOR:
    case SUPLA_CHANNELTYPE_CALLBUTTON:
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
        ch->supla_val = malloc(sizeof(supla_value_t));
        if (ch->supla_val == NULL)
            goto malloc_failed;
        supla_val_init(ch->supla_val, ch->config.sync_values_onchange);
        break;
    case SUPLA_CHANNELTYPE_RELAY:
    case SUPLA_CHANNELTYPE_ELECTRICITY_METER:
    case SUPLA_CHANNELTYPE_IMPULSE_COUNTER:
    case SUPLA_CHANNELTYPE_THERMOSTAT:
        ch->supla_val = malloc(sizeof(supla_value_t));
        if (ch->supla_val == NULL)
            goto malloc_failed;
        supla_val_init(ch->supla_val, ch->config.sync_values_onchange);

        ch->supla_extval = malloc(sizeof(supla_extended_value_t));
        if (ch->supla_extval == NULL)
            goto malloc_failed;

        supla_extval_init(ch->supla_extval, ch->config.sync_values_onchange);
        break;
    case SUPLA_CHANNELTYPE_ACTIONTRIGGER:
        ch->action_trigger = malloc(sizeof(supla_action_trigger_t));
        if (ch->action_trigger == NULL)
            goto malloc_failed;

        supla_action_trigger_init(ch->action_trigger);
        ch->action_trigger->properties.disablesLocalOperation = ch->config.action_trigger_conflicts;
        ch->action_trigger->properties.relatedChannelNumber = ch->number;
        break;
    default:
        break;
    }
    return ch;

malloc_failed:
    lck_free(ch->lck);
    supla_val_free(ch->supla_val);
    free(ch->supla_val);

    supla_extval_free(ch->supla_extval);
    free(ch->supla_extval);

    supla_action_trigger_free(ch->action_trigger);
    free(ch->action_trigger);

    free(ch);
    return NULL;
}

int supla_channel_free(supla_channel_t *ch)
{
    assert(NULL != ch);

    lck_free(ch->lck);
    free(ch->supla_val);
    free(ch->supla_extval);
    free(ch->action_trigger);
    free(ch);
    return SUPLA_RESULT_TRUE;
}

int supla_channel_get_config(supla_channel_t *ch, supla_channel_config_t *config)
{
    assert(NULL != ch);
    assert(NULL != config);

    lck_lock(ch->lck);
    *config = ch->config;
    lck_unlock(ch->lck);
    return SUPLA_RESULT_TRUE;
}

void *supla_channel_get_data(supla_channel_t *ch)
{
    assert(NULL != ch);
    void *data = NULL;

    lck_lock(ch->lck);
    data = ch->config.data;
    lck_unlock(ch->lck);
    return data;
}

void *supla_channel_set_data(supla_channel_t *ch, void *data)
{
    assert(NULL != ch);

    lck_lock(ch->lck);
    ch->config.data = data;
    lck_unlock(ch->lck);
    return data;
}

int supla_channel_get_assigned_number(supla_channel_t *ch)
{
    int num;
    if (ch == NULL)
        return -1;

    lck_lock(ch->lck);
    num = ch->number;
    lck_unlock(ch->lck);
    return num;
}

int supla_channel_get_active_function(supla_channel_t *ch, int *function)
{
    assert(NULL != ch);
    assert(NULL != function);

    lck_lock(ch->lck);
    *function = ch->active_function;
    lck_unlock(ch->lck);

    return SUPLA_RESULT_TRUE;
}

int supla_channel_set_value(supla_channel_t *ch, void *value, size_t len)
{
    assert(NULL != ch);
    int rc;

    lck_lock(ch->lck);
    rc = supla_val_set(ch->supla_val, value, len);
    lck_unlock(ch->lck);
    return rc;
}

int supla_channel_set_binary_value(supla_channel_t *ch, uint8_t value)
{
    //TODO check channel type
    return supla_channel_set_value(ch, &value, sizeof(uint8_t));
}

int supla_channel_set_double_value(supla_channel_t *ch, double value)
{
    //TODO check channel type
    return supla_channel_set_value(ch, &value, sizeof(double));
}

int supla_channel_set_humidtemp_value(supla_channel_t *ch, double humid, double temp)
{
    //TODO check channel type
    char    out[SUPLA_CHANNELVALUE_SIZE];
    int32_t t = temp * 1000.0;
    int32_t h = humid * 1000.0;
    memcpy(&out[0], &t, 4);
    memcpy(&out[4], &h, 4);
    return supla_channel_set_value(ch, out, sizeof(out));
}

int supla_channel_set_relay_value(supla_channel_t *ch, TRelayChannel_Value *relay)
{
    //TODO check channel type
    return supla_channel_set_value(ch, relay, sizeof(TRelayChannel_Value));
}

int supla_channel_set_rgbw_value(supla_channel_t *ch, TRGBW_Value *rgbw)
{
    //TODO check channel type
    return supla_channel_set_value(ch, rgbw, sizeof(TRGBW_Value));
}

int supla_channel_set_impulse_counter_value(supla_channel_t *ch, TDS_ImpulseCounter_Value *ic)
{
    assert(NULL != ch);

    if (ch->config.type != SUPLA_CHANNELTYPE_IMPULSE_COUNTER) {
        supla_log(LOG_ERR, "ch[%d] cannot set impulse counter value: bad channel type", ch->number);
        return SUPLA_RESULTCODE_CHANNEL_CONFLICT;
    }
    return supla_channel_set_value(ch, ic, sizeof(TDS_ImpulseCounter_Value));
}

int supla_channel_set_roller_shutter_value(supla_channel_t *ch, TDSC_RollerShutterValue *rs)
{
    assert(NULL != ch);

    if (ch->config.type != SUPLA_CHANNELTYPE_RELAY) {
        supla_log(LOG_ERR, "ch[%d] cannot set roller shuter value: bad channel type", ch->number);
        return SUPLA_RESULTCODE_CHANNEL_CONFLICT;
    }
    return supla_channel_set_value(ch, rs, sizeof(TDSC_RollerShutterValue));
}

int supla_channel_set_faceblind_value(supla_channel_t *ch, TDSC_FacadeBlindValue *fb)
{
    assert(NULL != ch);

    if (ch->config.type != SUPLA_CHANNELTYPE_RELAY) {
        supla_log(LOG_ERR, "ch[%d] cannot set faceblind value: bad channel type", ch->number);
        return SUPLA_RESULTCODE_CHANNEL_CONFLICT;
    }
    return supla_channel_set_value(ch, fb, sizeof(TDSC_FacadeBlindValue));
}

int supla_channel_set_electricity_meter_value(supla_channel_t *ch, TElectricityMeter_Value *em)
{
    assert(NULL != ch);

    if (ch->config.type != SUPLA_CHANNELTYPE_ELECTRICITY_METER) {
        supla_log(LOG_ERR, "ch[%d] cannot set electricity meter value: bad channel type",
                  ch->number);
        return SUPLA_RESULTCODE_CHANNEL_CONFLICT;
    }
    return supla_channel_set_value(ch, em, sizeof(TElectricityMeter_Value));
}

int supla_channel_set_thermostat_value(supla_channel_t *ch, TThermostat_Value *th)
{
    assert(NULL != ch);

    if (ch->config.type != SUPLA_CHANNELTYPE_THERMOSTAT) {
        supla_log(LOG_ERR, "ch[%d] cannot set thermostat value: bad channel type", ch->number);
        return SUPLA_RESULTCODE_CHANNEL_CONFLICT;
    }
    return supla_channel_set_value(ch, th, sizeof(TThermostat_Value));
}

int supla_channel_set_extval(supla_channel_t *ch, TSuplaChannelExtendedValue *extval)
{
    assert(NULL != ch);
    int rc;

    lck_lock(ch->lck);
    rc = supla_extval_set(ch->supla_extval, extval);
    lck_unlock(ch->lck);
    return rc;
}

int supla_channel_set_timer_state_extvalue(supla_channel_t *ch, TTimerState_ExtendedValue *tsev)
{
    assert(NULL != ch);

    if (ch->config.type != SUPLA_CHANNELTYPE_RELAY) {
        supla_log(LOG_ERR, "ch[%d] cannot set timer extvalue: bad channel type", ch->number);
        return SUPLA_RESULTCODE_CHANNEL_CONFLICT;
    }

    TSuplaChannelExtendedValue extval;

    extval.type = EV_TYPE_TIMER_STATE_V1;
    extval.size = sizeof(TTimerState_ExtendedValue);
    memcpy(extval.value, tsev, extval.size);
    return supla_channel_set_extval(ch, &extval);
}

int supla_channel_set_electricity_meter_extvalue(supla_channel_t                    *ch,
                                                 TElectricityMeter_ExtendedValue_V2 *emev)
{
    assert(NULL != ch);

    if (ch->config.type != SUPLA_CHANNELTYPE_ELECTRICITY_METER) {
        supla_log(LOG_ERR, "ch[%d] cannot set electricity meter extvalue: bad channel type",
                  ch->number);
        return SUPLA_RESULTCODE_CHANNEL_CONFLICT;
    }

    TSuplaChannelExtendedValue extval;
    srpc_evtool_v2_emextended2extended(emev, &extval);
    return supla_channel_set_extval(ch, &extval);
}

int supla_channel_set_thermostat_extvalue(supla_channel_t *ch, TThermostat_ExtendedValue *thev)
{
    assert(NULL != ch);

    if (ch->config.type != SUPLA_CHANNELTYPE_THERMOSTAT) {
        supla_log(LOG_ERR, "ch[%d] cannot set thermostat extvalue: bad channel type", ch->number);
        return SUPLA_RESULTCODE_CHANNEL_CONFLICT;
    }

    TSuplaChannelExtendedValue extval;
    srpc_evtool_v1_thermostatextended2extended(thev, &extval);
    return supla_channel_set_extval(ch, &extval);
}

int supla_channel_emit_action(supla_channel_t *ch, const _supla_int_t action)
{
    assert(NULL != ch);
    int rc;

    if (ch->config.type != SUPLA_CHANNELTYPE_ACTIONTRIGGER) {
        supla_log(LOG_ERR, "ch[%d] cannot emit action: bad channel type", ch->number);
        return SUPLA_RESULTCODE_CHANNEL_CONFLICT;
    }

    lck_lock(ch->lck);
    rc = supla_action_trigger_emit(ch->action_trigger, ch->number, action);
    lck_unlock(ch->lck);
    return rc;
}

//private functions
TDS_SuplaDeviceChannel_C supla_channel_to_register_struct(supla_channel_t *ch)
{
    assert(NULL != ch);
    TDS_SuplaDeviceChannel_C reg_channel;
    int                      rel_num = 0;

    lck_lock(ch->lck);
    reg_channel.Number = ch->number;
    reg_channel.Type = ch->config.type;
    reg_channel.Default = ch->config.default_function;
    reg_channel.Flags = ch->config.flags;

    if (ch->config.type == SUPLA_CHANNELTYPE_ACTIONTRIGGER) {
        if (ch->config.action_trigger_related_channel) {
            rel_num =
                supla_channel_get_assigned_number(*ch->config.action_trigger_related_channel) + 1;
            ch->action_trigger->properties.relatedChannelNumber = rel_num;
        }

        reg_channel.ActionTriggerCaps = ch->config.action_trigger_caps;
        reg_channel.actionTriggerProperties = ch->action_trigger->properties;
    } else {
        reg_channel.FuncList = ch->config.supported_functions;
        memcpy(reg_channel.value, ch->supla_val->data.value, SUPLA_CHANNELVALUE_SIZE);
    }
    lck_unlock(ch->lck);

    return reg_channel;
}

int supla_channel_set_active_function(supla_channel_t *ch, int function)
{
    assert(NULL != ch);
    int rc;

    lck_lock(ch->lck);
    if (ch->config.type == SUPLA_CHANNELTYPE_ACTIONTRIGGER) {
        rc = SUPLA_RESULT_FALSE;
    } else {
        ch->active_function = function;
        rc = SUPLA_RESULT_TRUE;
    }
    lck_unlock(ch->lck);

    return rc;
}

void supla_channel_sync(void *srpc, supla_channel_t *ch)
{
    assert(NULL != ch);

    lck_lock(ch->lck);
    if (ch->supla_val && !ch->supla_val->sync) {
        supla_log(LOG_DEBUG, "sync channel[%d] val ", ch->number);
        ch->supla_val->sync = srpc_ds_async_channel_value_changed_c(
            srpc, ch->number, ch->supla_val->data.value, 0, ch->config.validity_time_sec);
        //TODO support offline flag
    }

    if (ch->supla_extval && !ch->supla_extval->sync) {
        supla_log(LOG_DEBUG, "sync channel[%d] extval", ch->number);

        ch->supla_extval->sync = srpc_ds_async_channel_extendedvalue_changed(
            srpc, ch->number, &ch->supla_extval->extval);
    }

    if (ch->action_trigger && !ch->action_trigger->sync) {
        supla_log(LOG_DEBUG, "sync channel[%d] action ch[%d]->%d", ch->number,
                  ch->action_trigger->at.ChannelNumber, ch->action_trigger->at.ActionTrigger);

        ch->action_trigger->sync = srpc_ds_async_action_trigger(srpc, &ch->action_trigger->at);
    }
    lck_unlock(ch->lck);
}
