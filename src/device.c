/*
 * Copyright (c) 2022 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "port/util.h"
#include "port/net.h"

#include "device-priv.h"
#include "channel-priv.h"

static int supla_dev_read(void *buf, int count, void *dcd)
{
    supla_dev_t *dev = dcd;
    return supla_cloud_recv(dev->cloud_link, buf, count);
}

static int supla_dev_write(void *buf, int count, void *dcd)
{
    supla_dev_t *dev = dcd;
    return supla_cloud_send(dev->cloud_link, buf, count);
}

static void supla_dev_set_state(supla_dev_t *dev, supla_dev_state_t new_state)
{
    if (dev->state == new_state)
        return;

    dev->state = new_state;
    if (dev->on_state_change)
        dev->on_state_change(dev, dev->state);
}

static void supla_dev_set_connection_reset_cause(supla_dev_t *dev, unsigned char cause)
{
    dev->connection_reset_cause = cause;
}

static void supla_connection_on_version_error(TSDC_SuplaVersionError *version_error)
{
    supla_log(LOG_ERR, "Supla protocol version error: srv[%d-%d] dev:%d", version_error->server_version_min,
              version_error->server_version, SUPLA_PROTO_VERSION);
}

static void supla_connection_on_register_result(supla_dev_t *dev, TSD_SuplaRegisterDeviceResult *regdev_res)
{
    switch (regdev_res->result_code) {
    case SUPLA_RESULTCODE_BAD_CREDENTIALS:
        supla_log(LOG_ERR, "Bad credentials!");
        break;

    case SUPLA_RESULTCODE_TEMPORARILY_UNAVAILABLE:
        supla_log(LOG_ERR, "Temporarily unavailable!");
        break;

    case SUPLA_RESULTCODE_LOCATION_CONFLICT:
        supla_log(LOG_ERR, "Location conflict!");
        break;

    case SUPLA_RESULTCODE_CHANNEL_CONFLICT:
        supla_log(LOG_ERR, "Channel conflict!");
        break;

    case SUPLA_RESULTCODE_DEVICE_DISABLED:
        supla_log(LOG_ERR, "Device is disabled!");
        break;

    case SUPLA_RESULTCODE_LOCATION_DISABLED:
        supla_log(LOG_ERR, "Location is disabled!");
        break;

    case SUPLA_RESULTCODE_DEVICE_LIMITEXCEEDED:
        supla_log(LOG_ERR, "Device limit exceeded!");
        break;

    case SUPLA_RESULTCODE_GUID_ERROR:
        supla_log(LOG_ERR, "Incorrect device GUID!");
        break;

    case SUPLA_RESULTCODE_REGISTRATION_DISABLED:
        supla_log(LOG_ERR, "Registration disabled!");
        break;

    case SUPLA_RESULTCODE_AUTHKEY_ERROR:
        supla_log(LOG_ERR, "Incorrect AuthKey!");
        break;

    case SUPLA_RESULTCODE_NO_LOCATION_AVAILABLE:
        supla_log(LOG_ERR, "No location available!");
        break;

    case SUPLA_RESULTCODE_USER_CONFLICT:
        supla_log(LOG_ERR, "User conflict!");
        break;

    case SUPLA_RESULTCODE_COUNTRY_REJECTED:
        supla_log(LOG_ERR, "Country rejected!");
        break;

    case SUPLA_RESULTCODE_TRUE: {
        TDCS_SuplaSetActivityTimeout timeout;
        supla_log(LOG_INFO, "[%s] registered: srv ver: %d(min=%d), activity timeout=%ds", dev->name,
                  regdev_res->version, regdev_res->version_min, regdev_res->activity_timeout);

        if (dev->supla_config.activity_timeout != regdev_res->activity_timeout) {
            supla_log(LOG_DEBUG, "Setting activity timeout to %ds", dev->supla_config.activity_timeout);
            timeout.activity_timeout = dev->supla_config.activity_timeout;
            srpc_dcs_async_set_activity_timeout(dev->srpc, &timeout);
        }
        gettimeofday(&dev->reg_time, NULL);
        supla_dev_set_state(dev, SUPLA_DEV_STATE_REGISTERED);
    } break;

    default:
        break;
    }
}

static void supla_dev_on_set_channel_value(supla_dev_t *dev, TSD_SuplaChannelNewValue *new_value)
{
    char success = 0;
    supla_log(LOG_DEBUG, "ch[%d] set value request", new_value->ChannelNumber);

    supla_channel_t *ch = supla_dev_get_channel_by_num(dev, new_value->ChannelNumber);
    if (!ch) {
        supla_log(LOG_ERR, "ch[%d] not found", new_value->ChannelNumber);
        success = 0;
    } else if (ch->config.on_set_value) {
        success = ch->config.on_set_value(ch, new_value);
    }
    srpc_ds_async_set_channel_result(dev->srpc, new_value->ChannelNumber, new_value->SenderID, success);
}

static void supla_dev_on_set_channel_group_value(supla_dev_t *dev, TSD_SuplaChannelGroupNewValue *gr_value)
{
    TSD_SuplaChannelNewValue new_value;
    char success = 0;
    supla_log(LOG_DEBUG, "channel group[%d] set value request for ch[%d]", gr_value->GroupID, gr_value->ChannelNumber);

    new_value.SenderID = 0;
    new_value.ChannelNumber = gr_value->ChannelNumber;
    new_value.DurationMS = gr_value->DurationMS;
    memcpy(new_value.value, gr_value->value, SUPLA_CHANNELVALUE_SIZE);

    supla_channel_t *ch = supla_dev_get_channel_by_num(dev, new_value.ChannelNumber);
    if (!ch) {
        supla_log(LOG_ERR, "ch[%d] not found", new_value.ChannelNumber);
        success = 0;
    } else if (ch->config.on_set_value) {
        success = ch->config.on_set_value(ch, &new_value);
    }
    srpc_ds_async_set_channel_result(dev->srpc, new_value.ChannelNumber, new_value.SenderID, success);
}

static void supla_dev_get_channel_state(supla_dev_t *dev, TCSD_ChannelStateRequest *channel_state_request)
{
    TDSC_ChannelState state = {};
    supla_log(LOG_DEBUG, "get ch[%d] state", channel_state_request->ChannelNumber);

    state.ReceiverID = channel_state_request->SenderID;
    state.ChannelNumber = channel_state_request->ChannelNumber;

    /* fill internal supla device state data */
    state.Fields = 0x00;
    state.Fields |= SUPLA_CHANNELSTATE_FIELD_UPTIME;
    state.Uptime = dev->uptime;

    state.Fields |= SUPLA_CHANNELSTATE_FIELD_CONNECTIONUPTIME;
    state.ConnectionUptime = dev->connection_uptime;

    state.Fields |= SUPLA_CHANNELSTATE_FIELD_LASTCONNECTIONRESETCAUSE;
    state.LastConnectionResetCause = dev->connection_reset_cause;

    if (dev->on_get_channel_state)
        dev->on_get_channel_state(dev, &state);

    supla_channel_t *ch = supla_dev_get_channel_by_num(dev, channel_state_request->ChannelNumber);
    if (ch) {
        if (ch->config.on_get_state)
            ch->config.on_get_state(ch, &state);
    } else {
        supla_log(LOG_ERR, "ch[%d] not found", channel_state_request->ChannelNumber);
    }
    srpc_csd_async_channel_state_result(dev->srpc, &state);
}

static void supla_dev_on_set_activity_timeout_result(supla_dev_t *dev, TSDC_SuplaSetActivityTimeoutResult *res)
{
    dev->supla_config.activity_timeout = res->activity_timeout;
    supla_log(LOG_INFO, "Received activity timeout=%ds allowed[%d-%d]", res->activity_timeout, res->min, res->max);
}

static void supla_dev_on_get_user_localtime_result(supla_dev_t *dev, TSDC_UserLocalTimeResult *lt)
{
    time_t local = 0;

    supla_log(LOG_DEBUG, "Received user localtime result: %4d-%02d-%02d %02d:%02d:%02d %s", lt->year, lt->month,
              lt->day, lt->hour, lt->min, lt->sec, lt->timezone);
    if (dev->on_server_time_sync) {
        if (dev->on_server_time_sync(dev, lt) == 0) {
            time(&local);
            supla_log(LOG_INFO, "device time sync success: %s", strtok(ctime(&local), "\n"));
            /* update timers */
            dev->init_time.tv_sec = local - dev->uptime;
            dev->reg_time.tv_sec = local - dev->connection_uptime;
            dev->last_ping.tv_sec = local;
            dev->last_resp.tv_sec = local;
        } else {
            supla_log(LOG_ERR, "device time sync ERR");
        }
    }
}

static void supla_dev_on_calcfg_request(supla_dev_t *dev, TSD_DeviceCalCfgRequest *calcfg)
{
    TDS_DeviceCalCfgResult result;

    result.ReceiverID = calcfg->SenderID;
    result.ChannelNumber = calcfg->ChannelNumber;
    result.Command = calcfg->Command;

    supla_log(LOG_DEBUG, "Received calcfg from server: ch=%d cmd=%d auth=%d datatype=%d datasize=%d",
              calcfg->ChannelNumber, calcfg->Command, calcfg->SuperUserAuthorized, calcfg->DataType, calcfg->DataSize);

    if (calcfg->SuperUserAuthorized != 1) {
        result.Result = SUPLA_CALCFG_RESULT_UNAUTHORIZED;
    } else if (calcfg->ChannelNumber == -1) {
        /* no channel specified - config device */
        switch (calcfg->Command) {
        case SUPLA_CALCFG_CMD_ENTER_CFG_MODE:
            supla_log(LOG_INFO, "CALCFG ENTER CONFIG MODE received");
            supla_dev_set_state(dev, SUPLA_DEV_STATE_CONFIG);
            break;
        default:
            result.Result = SUPLA_CALCFG_RESULT_NOT_SUPPORTED;
            break;
        }
    } else {
        supla_channel_t *ch = supla_dev_get_channel_by_num(dev, calcfg->ChannelNumber);
        if (ch) {
            if (ch->config.on_calcfg_req)
                result.Result = ch->config.on_calcfg_req(ch, calcfg);
            else
                result.Result = SUPLA_CALCFG_RESULT_NOT_SUPPORTED;
        } else {
            supla_log(LOG_ERR, "ch[%d] not found", calcfg->ChannelNumber);
            result.Result = SUPLA_CALCFG_RESULT_ID_NOT_EXISTS;
        }
    }
    srpc_ds_async_device_calcfg_result(dev->srpc, &result);
}

static void supla_dev_on_get_channel_functions_result(supla_dev_t *dev, TSD_ChannelFunctions *channel_functions)
{
    supla_channel_t *ch;
    supla_log(LOG_DEBUG, "Received channel functions from server");

    for (int i = 0; i < channel_functions->ChannelCount; i++) {
        ch = supla_dev_get_channel_by_num(dev, i);
        if (ch) {
            supla_log(LOG_DEBUG, "ch[%d] active function = %d", i, channel_functions->Functions[i]);
            supla_channel_set_active_function(ch, channel_functions->Functions[i]);
        }
    }
}

static void supla_dev_on_set_channel_config(supla_dev_t *dev, TSD_ChannelConfig *ch_config)
{
    supla_log(LOG_DEBUG, "Received set channel config from server");
    supla_log(LOG_DEBUG, "ch_cfg->num=%d %d", ch_config->ChannelNumber, ch_config->ConfigType);
}

static void supla_dev_on_get_channel_config_result(supla_dev_t *dev, TSD_ChannelConfig *ch_config)
{
    supla_log(LOG_DEBUG, "Received channel config result from server: ch[%d] type=%d func=%d", ch_config->ChannelNumber,
              ch_config->ConfigType, ch_config->Func);

    supla_channel_t *ch = supla_dev_get_channel_by_num(dev, ch_config->ChannelNumber);
    if (ch) {
        supla_channel_set_active_function(ch, ch_config->Func);
        if (ch->config.on_config_recv)
            ch->config.on_config_recv(ch, ch_config);
    } else {
        supla_log(LOG_ERR, "channel[%d] not found", ch_config->ChannelNumber);
    }
}

static void supla_dev_on_channel_config_finished(supla_dev_t *dev, TSD_ChannelConfigFinished *channel_config)
{
    supla_log(LOG_DEBUG, "Received channel %d config finished from server", channel_config->ChannelNumber);
}

static void supla_dev_on_set_device_config(supla_dev_t *dev, TSDS_SetDeviceConfig *device_config)
{
    TSDS_SetDeviceConfigResult result;

    supla_log(LOG_DEBUG, "Received set device config from server");
    //TODO
    srpc_ds_async_set_device_config_result(dev->srpc, &result);
}

static void supla_dev_on_set_device_config_result(supla_dev_t *dev, TSDS_SetDeviceConfigResult *result)
{
    supla_log(LOG_DEBUG, "Received set device config result from server");
}

static void supla_dev_on_set_channel_caption_result(supla_dev_t *dev, TSCD_SetCaptionResult *result)
{
    supla_log(LOG_DEBUG, "Received set ch[%d] caption result from server: %s", result->ChannelNumber, result->Caption);
}

static void supla_dev_on_remote_call_received(void *_srpc, unsigned int rr_id, unsigned int call_type, void *_dcd,
                                              unsigned char proto_version)
{
    (void)rr_id;         //suppress compiler warnings
    (void)call_type;     //suppress compiler warnings
    (void)proto_version; //suppress compiler warnings
    TsrpcReceivedData rd;
    supla_dev_t *dev = _dcd;
    char rc;

    rc = srpc_getdata(_srpc, &rd, 0);
    if (rc != SUPLA_RESULT_TRUE) {
        supla_log(LOG_ERR, "srpc_getdata error!");
        return;
    }
    gettimeofday(&dev->last_resp, NULL);

    switch (rd.call_id) {
    case SUPLA_SDC_CALL_GETVERSION_RESULT:
        supla_log(LOG_DEBUG, "Received get version from server: %d", proto_version);
        break;
    case SUPLA_SDC_CALL_VERSIONERROR:
        supla_connection_on_version_error(rd.data.sdc_version_error);
        break;
    case SUPLA_SDC_CALL_PING_SERVER_RESULT:
        break;
    case SUPLA_SD_CALL_REGISTER_DEVICE_RESULT:
        supla_connection_on_register_result(dev, rd.data.sd_register_device_result);
        break;
    case SUPLA_SD_CALL_CHANNEL_SET_VALUE:
        supla_dev_on_set_channel_value(dev, rd.data.sd_channel_new_value);
        break;
    case SUPLA_SD_CALL_CHANNELGROUP_SET_VALUE:
        supla_dev_on_set_channel_group_value(dev, rd.data.sd_channelgroup_new_value);
        break;
    case SUPLA_SDC_CALL_SET_ACTIVITY_TIMEOUT_RESULT:
        supla_dev_on_set_activity_timeout_result(dev, rd.data.sdc_set_activity_timeout_result);
        break;
    case SUPLA_SD_CALL_GET_FIRMWARE_UPDATE_URL_RESULT:
        supla_log(LOG_DEBUG, "Received update url result from server!");
        break;
    case SUPLA_DCS_CALL_GET_USER_LOCALTIME_RESULT:
        supla_dev_on_get_user_localtime_result(dev, rd.data.sdc_user_localtime_result);
        break;
    case SUPLA_SDC_CALL_GET_REGISTRATION_ENABLED_RESULT:
        supla_log(LOG_DEBUG, "Received registration enabled result from server!");
        break;
    case SUPLA_SD_CALL_DEVICE_CALCFG_REQUEST:
        supla_dev_on_calcfg_request(dev, rd.data.sd_device_calcfg_request);
        break;
    case SUPLA_CSD_CALL_GET_CHANNEL_STATE:
        supla_dev_get_channel_state(dev, rd.data.csd_channel_state_request);
        break;
    case SUPLA_SD_CALL_GET_CHANNEL_FUNCTIONS_RESULT:
        supla_dev_on_get_channel_functions_result(dev, rd.data.sd_channel_functions);
        break;
    case SUPLA_SD_CALL_SET_CHANNEL_CONFIG:
        supla_dev_on_set_channel_config(dev, rd.data.sd_channel_config);
        break;
    case SUPLA_SD_CALL_GET_CHANNEL_CONFIG_RESULT:
        supla_dev_on_get_channel_config_result(dev, rd.data.sd_channel_config);
        break;
    case SUPLA_SD_CALL_CHANNEL_CONFIG_FINISHED:
        supla_dev_on_channel_config_finished(dev, rd.data.sd_channel_config_finished);
        break;
    case SUPLA_SD_CALL_SET_DEVICE_CONFIG:
        supla_dev_on_set_device_config(dev, rd.data.sds_set_device_config_request);
        break;
    case SUPLA_SD_CALL_SET_DEVICE_CONFIG_RESULT:
        supla_dev_on_set_device_config_result(dev, rd.data.sds_set_device_config_result);
        break;
    case SUPLA_SCD_CALL_SET_CHANNEL_CAPTION_RESULT:
        supla_dev_on_set_channel_caption_result(dev, rd.data.scd_set_caption_result);
        break;
    default:
        supla_log(LOG_DEBUG, "Received unknown message from server!");
        break;
    }
    srpc_rd_free(&rd);
}

static int supla_connection_ping(supla_dev_t *dev)
{
    struct timeval now;
    gettimeofday(&now, NULL);

    if (dev->supla_config.activity_timeout == 0)
        return SUPLA_RESULT_TRUE;

    if ((now.tv_sec - dev->last_ping.tv_sec) >= (dev->supla_config.activity_timeout - 5)) {
        srpc_dcs_async_ping_server(dev->srpc);
        gettimeofday(&dev->last_ping, NULL);
    }

    if ((now.tv_sec - dev->last_resp.tv_sec) >= (dev->supla_config.activity_timeout + 10)) {
        supla_log(LOG_ERR, "ping timeout");
        return SUPLA_RESULT_FALSE;
    }
    return SUPLA_RESULT_TRUE;
}

supla_dev_t *supla_dev_create(const char *dev_name, const char *soft_ver)
{
    supla_dev_t *dev = calloc(1, sizeof(supla_dev_t));
    if (!dev)
        return NULL;

    if (dev_name)
        strncpy(dev->name, dev_name, SUPLA_DEVICE_NAME_MAXSIZE - 1);
    else
        strncpy(dev->name, "SUPLA device", SUPLA_DEVICE_NAME_MAXSIZE - 1);

    if (soft_ver)
        strncpy(dev->soft_ver, soft_ver, SUPLA_SOFTVER_MAXSIZE - 1);
    else
        strncpy(dev->soft_ver, "libsupla " LIBSUPLA_VER, SUPLA_SOFTVER_MAXSIZE - 1);

    dev->state = SUPLA_DEV_STATE_IDLE;

    TsrpcParams srpc_params;
    srpc_params_init(&srpc_params);

    srpc_params.data_read = supla_dev_read;
    srpc_params.data_write = supla_dev_write;
    srpc_params.on_remote_call_received = supla_dev_on_remote_call_received;
    srpc_params.user_params = dev;

    dev->srpc = srpc_init(&srpc_params);
    dev->lck = lck_init();

    gettimeofday(&dev->init_time, NULL);
    STAILQ_INIT(&dev->channels);
    return dev;
}

int supla_dev_free(supla_dev_t *dev)
{
    assert(NULL != dev);

    supla_channel_t *ch;

    supla_cloud_disconnect(&dev->cloud_link);
    srpc_free(dev->srpc);
    lck_free(dev->lck);

    while (!STAILQ_EMPTY(&dev->channels)) {
        ch = STAILQ_FIRST(&dev->channels);
        STAILQ_REMOVE_HEAD(&dev->channels, channels);
        supla_channel_free(ch);
    }
    free(dev);
    return SUPLA_RESULT_TRUE;
}

int supla_dev_set_name(supla_dev_t *dev, const char *name)
{
    assert(NULL != dev);
    assert(NULL != name);

    lck_lock(dev->lck);
    strncpy(dev->name, name, SUPLA_DEVICE_NAME_MAXSIZE - 1);
    lck_unlock(dev->lck);

    return SUPLA_RESULT_TRUE;
}

int supla_dev_get_name(const supla_dev_t *dev, char *name, size_t len)
{
    assert(NULL != dev);
    assert(NULL != name);
    assert(0 != len);

    lck_lock(dev->lck);
    strncpy(name, dev->name, len);
    lck_unlock(dev->lck);

    return SUPLA_RESULT_TRUE;
}

int supla_dev_get_software_version(const supla_dev_t *dev, char *version, size_t len)
{
    assert(NULL != dev);
    assert(NULL != version);
    assert(0 != len);

    lck_lock(dev->lck);
    strncpy(version, dev->soft_ver, len);
    lck_unlock(dev->lck);

    return SUPLA_RESULT_TRUE;
}

int supla_dev_get_state(const supla_dev_t *dev, supla_dev_state_t *state)
{
    assert(NULL != dev);
    assert(NULL != state);

    lck_lock(dev->lck);
    *state = dev->state;
    lck_unlock(dev->lck);

    return SUPLA_RESULT_TRUE;
}

const char *supla_dev_state_str(supla_dev_state_t state)
{
    switch (state) {
    case SUPLA_DEV_STATE_CONFIG:
        return "CONFIG";
    case SUPLA_DEV_STATE_IDLE:
        return "IDLE";
    case SUPLA_DEV_STATE_INIT:
        return "INIT";
    case SUPLA_DEV_STATE_CONNECTED:
        return "CONNECTED";
    case SUPLA_DEV_STATE_REGISTERED:
        return "REGISTERED";
    case SUPLA_DEV_STATE_ONLINE:
        return "ONLINE";
    default:
        return "UNKNOWN";
    }
}

int supla_dev_set_flags(supla_dev_t *dev, int flags)
{
    assert(NULL != dev);

    lck_lock(dev->lck);
    dev->flags = flags;
    lck_unlock(dev->lck);

    return SUPLA_RESULT_TRUE;
}

int supla_dev_get_flags(const supla_dev_t *dev, int *flags)
{
    assert(NULL != dev);
    assert(NULL != flags);

    lck_lock(dev->lck);
    *flags = dev->flags;
    lck_unlock(dev->lck);

    return SUPLA_RESULT_TRUE;
}

int supla_dev_set_manufacturer_data(supla_dev_t *dev, const struct manufacturer_data *mfr_data)
{
    assert(NULL != dev);
    assert(NULL != mfr_data);

    lck_lock(dev->lck);
    dev->mfr_data = *mfr_data;
    lck_unlock(dev->lck);

    return SUPLA_RESULT_TRUE;
}

int supla_dev_get_manufacturer_data(const supla_dev_t *dev, struct manufacturer_data *mfr_data)
{
    assert(NULL != dev);
    assert(NULL != mfr_data);

    lck_lock(dev->lck);
    *mfr_data = dev->mfr_data;
    lck_unlock(dev->lck);

    return SUPLA_RESULT_TRUE;
}

int supla_dev_get_uptime(const supla_dev_t *dev, time_t *uptime)
{
    assert(NULL != dev);
    assert(NULL != uptime);

    lck_lock(dev->lck);
    *uptime = dev->uptime;
    lck_unlock(dev->lck);

    return SUPLA_RESULT_TRUE;
}

int supla_dev_get_connection_uptime(const supla_dev_t *dev, time_t *connection_uptime)
{
    assert(NULL != dev);
    assert(NULL != connection_uptime);

    lck_lock(dev->lck);
    *connection_uptime = dev->connection_uptime;
    lck_unlock(dev->lck);

    return SUPLA_RESULT_TRUE;
}

int supla_dev_set_state_changed_callback(supla_dev_t *dev, on_change_state_callback_t callback)
{
    assert(NULL != dev);

    lck_lock(dev->lck);
    dev->on_state_change = callback;
    lck_unlock(dev->lck);

    return SUPLA_RESULT_TRUE;
}

int supla_dev_set_common_channel_state_callback(supla_dev_t *dev, supla_device_get_state_handler_t callback)
{
    assert(NULL != dev);

    lck_lock(dev->lck);
    dev->on_get_channel_state = callback;
    lck_unlock(dev->lck);

    return SUPLA_RESULT_TRUE;
}

int supla_dev_set_server_time_sync_callback(supla_dev_t *dev, on_server_time_sync_callback_t callback)
{
    assert(NULL != dev);

    lck_lock(dev->lck);
    dev->on_server_time_sync = callback;
    lck_unlock(dev->lck);

    return SUPLA_RESULT_TRUE;
}

int supla_dev_add_channel(supla_dev_t *dev, supla_channel_t *ch)
{
    assert(NULL != dev);
    assert(NULL != ch);
    int channel_count;

    channel_count = supla_dev_get_channel_count(dev);
    if (channel_count + 1 >= SUPLA_CHANNELMAXCOUNT) {
        supla_log(LOG_ERR, "[%s] cannot add channel: channel max count reached", dev->name);
        return SUPLA_RESULT_FALSE;
    }

    lck_lock(dev->lck);
    lck_lock(ch->lck);
    if (ch->config.type == SUPLA_CHANNELTYPE_ACTIONTRIGGER)
        supla_log(LOG_DEBUG, "[%s] [%d]add new action trigger", dev->name, channel_count);
    else
        supla_log(LOG_DEBUG, "[%s] [%d]add new channel", dev->name, channel_count);

    ch->number = channel_count;
    STAILQ_INSERT_TAIL(&dev->channels, ch, channels);

    lck_unlock(ch->lck);
    lck_unlock(dev->lck);
    return SUPLA_RESULT_TRUE;
}

int supla_dev_get_channel_count(const supla_dev_t *dev)
{
    assert(NULL != dev);

    supla_channel_t *ch;
    int n = 0;

    lck_lock(dev->lck);
    STAILQ_FOREACH(ch, &dev->channels, channels)
    {
        n++;
    }
    lck_unlock(dev->lck);
    return n;
}

supla_channel_t *supla_dev_get_channel_by_num(const supla_dev_t *dev, int num)
{
    assert(NULL != dev);

    supla_channel_t *ch, *out = NULL;

    lck_lock(dev->lck);
    STAILQ_FOREACH(ch, &dev->channels, channels)
    {
        if (supla_channel_get_assigned_number(ch) == num)
            out = ch;
    }
    lck_unlock(dev->lck);
    return out;
}

int supla_dev_set_config(supla_dev_t *dev, const struct supla_config *config)
{
    assert(NULL != dev);
    assert(NULL != config);

    const char empty_auth[SUPLA_AUTHKEY_SIZE] = { 0 };
    const char empty_guid[SUPLA_GUID_SIZE] = { 0 };

    if (memcmp(config->auth_key, empty_auth, SUPLA_AUTHKEY_SIZE) == 0) {
        supla_log(LOG_ERR, "auth key not set");
        return SUPLA_RESULTCODE_AUTHKEY_ERROR;
    }

    if (memcmp(config->guid, empty_guid, SUPLA_GUID_SIZE) == 0) {
        supla_log(LOG_ERR, "guid not set");
        return SUPLA_RESULTCODE_GUID_ERROR;
    }

    lck_lock(dev->lck);
    dev->supla_config = *config;
    /* set defaults if port or activity timeout not provided */
    dev->supla_config.port = dev->supla_config.port ? dev->supla_config.port : dev->supla_config.ssl ? 2016 : 2015;
    dev->supla_config.activity_timeout = dev->supla_config.activity_timeout ? dev->supla_config.activity_timeout : 120;
    lck_unlock(dev->lck);

    return SUPLA_RESULT_TRUE;
}

int supla_dev_get_config(supla_dev_t *dev, struct supla_config *config)
{
    assert(NULL != dev);
    assert(NULL != config);

    lck_lock(dev->lck);
    *config = dev->supla_config;
    lck_unlock(dev->lck);

    return SUPLA_RESULT_TRUE;
}

int supla_dev_enable_notifications(supla_dev_t *dev, const unsigned char server_managed_fields)
{
    assert(NULL != dev);

    lck_lock(dev->lck);
    dev->push_notification.enabled = true;
    dev->push_notification.srv_managed_fields = server_managed_fields;
    lck_unlock(dev->lck);

    return SUPLA_RESULT_TRUE;
}

int supla_dev_disable_notifications(supla_dev_t *dev)
{
    assert(NULL != dev);

    lck_lock(dev->lck);
    dev->push_notification.enabled = false;
    lck_unlock(dev->lck);

    return SUPLA_RESULT_TRUE;
}

int supla_dev_send_notification(supla_dev_t *dev, int ctx, const char *title, const char *message, int sound_id)
{
    assert(NULL != dev);

    supla_channel_t *ch;
    TDS_PushNotification notification = {};
    bool enabled = false;

    lck_lock(dev->lck);
    if (ctx == -1) {
        enabled = dev->push_notification.enabled;
    } else {
        STAILQ_FOREACH(ch, &dev->channels, channels)
        {
            if (ctx == supla_channel_get_assigned_number(ch) && ch->config.push_notification.enabled)
                enabled = true;
        }
    }

    if (!enabled) {
        supla_log(LOG_ERR, "PUSH notifications for context=%d disabled", ctx);
        lck_unlock(dev->lck);
        return SUPLA_RESULT_FALSE;
    }

    notification.Context = ctx;
    if (dev->push_notification.srv_managed_fields & PN_SERVER_MANAGED_TITLE) {
        notification.TitleSize = 0;
    } else {
        notification.TitleSize = strlen(title) + 1;
        strncpy((char *)notification.TitleAndBody, title, SUPLA_PN_TITLE_MAXSIZE);
    }

    if (dev->push_notification.srv_managed_fields & PN_SERVER_MANAGED_BODY) {
        notification.BodySize = 0;
    } else {
        notification.BodySize = strlen(message) + 1;
        strncpy((char *)notification.TitleAndBody + notification.TitleSize, message, SUPLA_PN_BODY_MAXSIZE);
    }

    if (dev->push_notification.srv_managed_fields & PN_SERVER_MANAGED_SOUND)
        notification.SoundId = -1;
    else
        notification.SoundId = sound_id;
    lck_unlock(dev->lck);

    supla_log(LOG_DEBUG, "dev notify: %s: %s", title, message);

    return srpc_ds_async_send_push_notification(dev->srpc, &notification);
}

int supla_dev_start(supla_dev_t *dev)
{
    assert(NULL != dev);

    supla_dev_state_t state;
    supla_dev_get_state(dev, &state);

    if (state != SUPLA_DEV_STATE_IDLE)
        return SUPLA_RESULT_FALSE;

    lck_lock(dev->lck);
    supla_dev_set_state(dev, SUPLA_DEV_STATE_INIT);
    lck_unlock(dev->lck);

    return SUPLA_RESULT_TRUE;
}

int supla_dev_stop(supla_dev_t *dev)
{
    assert(NULL != dev);

    lck_lock(dev->lck);
    supla_dev_set_state(dev, SUPLA_DEV_STATE_IDLE);
    lck_unlock(dev->lck);

    return SUPLA_RESULT_TRUE;
}

static int supla_dev_register(supla_dev_t *dev)
{
    TDS_SuplaRegisterDevice_F reg_dev = { 0 };
    supla_channel_t *ch;
    uint8_t ch_num = 0;

    strncpy(reg_dev.Email, dev->supla_config.email, SUPLA_EMAIL_MAXSIZE);
    strncpy(reg_dev.AuthKey, dev->supla_config.auth_key, SUPLA_AUTHKEY_SIZE);
    strncpy(reg_dev.GUID, dev->supla_config.guid, SUPLA_GUID_SIZE);
    strncpy(reg_dev.Name, dev->name, SUPLA_DEVICE_NAME_MAXSIZE);
    strncpy(reg_dev.SoftVer, dev->soft_ver, SUPLA_SOFTVER_MAXSIZE);
    strncpy(reg_dev.ServerName, dev->supla_config.server, SUPLA_SERVER_NAME_MAXSIZE);
    reg_dev.Flags = dev->flags;
    reg_dev.ManufacturerID = dev->mfr_data.manufacturer_id;
    reg_dev.ProductID = dev->mfr_data.product_id;

    STAILQ_FOREACH(ch, &dev->channels, channels)
    {
        reg_dev.channels[ch_num] = supla_channel_to_register_struct(ch);
        ch_num++;
    }
    reg_dev.channel_count = ch_num;

    gettimeofday(&dev->reg_time, NULL);
    supla_log(LOG_INFO, "[%s] register device...", dev->name);
    return srpc_ds_async_registerdevice_f(dev->srpc, &reg_dev);
}

static int supla_dev_time_sync(supla_dev_t *dev)
{
    if (dev->on_server_time_sync)
        return srpc_dcs_async_get_user_localtime(dev->srpc);
    else
        return SUPLA_RESULT_FALSE;
}

static int supla_dev_set_channel_captions(supla_dev_t *dev)
{
    supla_channel_t *ch;
    TDCS_SetCaption req = {};

    STAILQ_FOREACH(ch, &dev->channels, channels)
    {
        if (ch->config.default_caption) {
            req.ChannelNumber = supla_channel_get_assigned_number(ch);
            strncpy(req.Caption, ch->config.default_caption, SUPLA_CAPTION_MAXSIZE - 1);
            req.CaptionSize = strnlen(req.Caption, SUPLA_CAPTION_MAXSIZE) + 1;
            srpc_dcs_async_set_channel_caption(dev->srpc, &req);
        }
    }
    return SUPLA_RESULT_TRUE;
}

static int supla_dev_get_channel_functions(supla_dev_t *dev)
{
    return srpc_ds_async_get_channel_functions(dev->srpc);
}

static int supla_dev_get_channel_config(supla_dev_t *dev)
{
    supla_channel_t *ch;
    TDS_GetChannelConfigRequest req = {};

    STAILQ_FOREACH(ch, &dev->channels, channels)
    {
        if (ch->config.on_config_recv) {
            req.ChannelNumber = supla_channel_get_assigned_number(ch);
            req.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
            srpc_ds_async_get_channel_config_request(dev->srpc, &req);
        }
    }
    return SUPLA_RESULT_TRUE;
}

static int supla_dev_register_push_notifications(supla_dev_t *dev)
{
    supla_channel_t *ch;
    TDS_RegisterPushNotification pn_reg = {};

    if (dev->push_notification.enabled) {
        pn_reg.Context = -1; //Device context
        pn_reg.ServerManagedFields = dev->push_notification.srv_managed_fields;
        supla_log(LOG_DEBUG, "[%s] register dev PUSH notification", dev->name);
        srpc_ds_async_register_push_notification(dev->srpc, &pn_reg);
    }
    STAILQ_FOREACH(ch, &dev->channels, channels)
    {
        if (ch->config.push_notification.enabled) {
            memset(&pn_reg, 0, sizeof(TDS_RegisterPushNotification));
            pn_reg.Context = supla_channel_get_assigned_number(ch);
            pn_reg.ServerManagedFields = ch->config.push_notification.srv_managed_fields;
            supla_log(LOG_DEBUG, "[%s] register ch[%d] PUSH notification", dev->name, pn_reg.Context);
            srpc_ds_async_register_push_notification(dev->srpc, &pn_reg);
        }
    }
    return 0;
}

static void supla_dev_sync_channels_data(supla_dev_t *dev)
{
    supla_channel_t *ch;

    STAILQ_FOREACH(ch, &dev->channels, channels)
    {
        supla_channel_sync(dev->srpc, ch);
    }
}

static int supla_dev_iterate_tick(supla_dev_t *dev)
{
    struct timeval sys_time;
    gettimeofday(&sys_time, NULL);

    dev->uptime = difftime(sys_time.tv_sec, dev->init_time.tv_sec);
    switch (dev->state) {
    case SUPLA_DEV_STATE_CONFIG:
    case SUPLA_DEV_STATE_IDLE:
        return SUPLA_RESULT_TRUE;

    case SUPLA_DEV_STATE_INIT:
        memset(&dev->reg_time, 0, sizeof(dev->reg_time));
        memset(&dev->last_ping, 0, sizeof(dev->last_ping));
        memset(&dev->last_resp, 0, sizeof(dev->last_resp));

        supla_log(LOG_INFO, "[%s] Connecting to: %s:%d", dev->name, dev->supla_config.server, dev->supla_config.port);
        supla_cloud_disconnect(&dev->cloud_link);
        if (supla_cloud_connect(&dev->cloud_link, dev->supla_config.server, dev->supla_config.port,
                                dev->supla_config.ssl)) {
            supla_log(LOG_INFO, "[%s] Connected to server", dev->name);
            if (!supla_dev_register(dev)) {
                supla_log(LOG_ERR, "[%s] supla_dev_register failed!", dev->name);
                supla_dev_set_state(dev, SUPLA_DEV_STATE_INIT);
            }
            supla_dev_set_state(dev, SUPLA_DEV_STATE_CONNECTED);
        } else {
            supla_delay_ms(5000); //FIXME
            return SUPLA_RESULT_FALSE;
        }
        break;

    case SUPLA_DEV_STATE_CONNECTED:
        if (difftime(sys_time.tv_sec, dev->reg_time.tv_sec) > 10) {
            supla_log(LOG_ERR, "[%s] Register failed: server not responded!", dev->name);
            supla_dev_set_connection_reset_cause(dev, SUPLA_LASTCONNECTIONRESETCAUSE_SERVER_CONNECTION_LOST);
            supla_dev_set_state(dev, SUPLA_DEV_STATE_INIT);
        }
        break;

    case SUPLA_DEV_STATE_REGISTERED:
        supla_dev_time_sync(dev);
        supla_dev_set_channel_captions(dev);
        supla_dev_get_channel_functions(dev);
        supla_dev_get_channel_config(dev);
        supla_dev_register_push_notifications(dev);
        supla_dev_set_state(dev, SUPLA_DEV_STATE_ONLINE);
        break;

    case SUPLA_DEV_STATE_ONLINE:
        dev->connection_uptime = difftime(sys_time.tv_sec, dev->reg_time.tv_sec);
        if (supla_connection_ping(dev) == SUPLA_RESULT_FALSE) {
            supla_dev_set_connection_reset_cause(dev, SUPLA_LASTCONNECTIONRESETCAUSE_ACTIVITY_TIMEOUT);
            supla_dev_set_state(dev, SUPLA_DEV_STATE_INIT);
            return SUPLA_RESULT_FALSE;
        }
        supla_dev_sync_channels_data(dev);
        break;
    default:
        break;
    }

    if (srpc_iterate(dev->srpc) == SUPLA_RESULT_FALSE) {
        supla_log(LOG_DEBUG, "srpc_iterate failed");
        supla_dev_set_connection_reset_cause(dev, SUPLA_LASTCONNECTIONRESETCAUSE_SERVER_CONNECTION_LOST);
        supla_dev_set_state(dev, SUPLA_DEV_STATE_INIT);
        supla_delay_ms(5000); //FIXME
        return SUPLA_RESULT_FALSE;
    }
    return 0;
}

int supla_dev_iterate(supla_dev_t *dev)
{
    assert(NULL != dev);
    int result;

    lck_lock(dev->lck);
    result = supla_dev_iterate_tick(dev);
    lck_unlock(dev->lck);
    return result;
}

int supla_dev_enter_config_mode(supla_dev_t *dev)
{
    assert(NULL != dev);

    lck_lock(dev->lck);
    supla_dev_set_state(dev, SUPLA_DEV_STATE_CONFIG);
    lck_unlock(dev->lck);
    return SUPLA_RESULT_TRUE;
}

int supla_dev_exit_config_mode(supla_dev_t *dev)
{
    assert(NULL != dev);

    supla_dev_state_t state;
    supla_dev_get_state(dev, &state);

    if (state != SUPLA_DEV_STATE_CONFIG)
        return SUPLA_RESULT_FALSE;

    lck_lock(dev->lck);
    supla_dev_set_state(dev, SUPLA_DEV_STATE_IDLE);
    lck_unlock(dev->lck);
    return SUPLA_RESULT_TRUE;
}
