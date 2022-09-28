/*
 * Copyright (c) 2022 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <libsupla/device.h>

#include "port/util.h"
#include "port/net.h"

#include "channel-priv.h"

/* device private data */
struct supla_dev {
	char name[SUPLA_DEVICE_NAME_MAXSIZE];
	char soft_ver[SUPLA_SOFTVER_MAXSIZE];

	void *ssd;
	void *srpc;

	supla_dev_state_t state;
	struct supla_config supla_config;
	int flags; //SUPLA_DEVICE_FLAG_*

	_supla_int16_t manufacturer_id;
	_supla_int16_t product_id;

	on_change_state_callback_t on_state_change;
	supla_device_get_state_handler_t on_get_channel_state;
	on_server_time_sync_callback_t on_server_time_sync;

	struct timeval init_time;
	struct timeval reg_time;
	struct timeval last_call;

	time_t uptime;
	time_t connection_uptime;
	unsigned char connection_reset_cause;

	STAILQ_HEAD(qhead,supla_channel) channels;
};

static int supla_dev_initialized(supla_dev_t *dev)
{
	return (dev->ssd != NULL && dev->srpc != NULL);
}

static int supla_dev_read(void *buf, int count, void *dcd)
{
	supla_dev_t *dev = dcd;
	return supla_link_read(dev->ssd, buf, count);
}

static int supla_dev_write(void *buf, int count, void *dcd)
{
	supla_dev_t *dev = dcd;
	return supla_link_write(dev->ssd, buf, count);
}

static void supla_dev_set_state(supla_dev_t *dev, supla_dev_state_t new_state)
{
	if(dev->state == new_state)
		return;

	dev->state = new_state;
	if(dev->on_state_change)
		dev->on_state_change(dev,dev->state);
}

static supla_channel_t *supla_dev_get_channel_by_num(supla_dev_t *dev, int num)
{
	supla_channel_t *ch;
	struct supla_channel_priv *priv;

	STAILQ_FOREACH(ch,&dev->channels,channels){
		priv = ch->priv;
		if(priv->number == num)
			return ch;
	}
	return NULL;
}

static void supla_dev_set_connection_reset_cause(supla_dev_t *dev, unsigned char cause)
{
	dev->connection_reset_cause = cause;
}

static void supla_connection_on_version_error(TSDC_SuplaVersionError *version_error)
{
	supla_log(LOG_ERR,"Supla protocol version error: srv[%d-%d] dev:%d",
	version_error->server_version_min,
	version_error->server_version, SUPLA_PROTO_VERSION);
}

static void supla_connection_on_register_result(supla_dev_t *dev, TSD_SuplaRegisterDeviceResult *register_device_result)
{
	switch (register_device_result->result_code) {
	case SUPLA_RESULTCODE_TRUE:{
		TDCS_SuplaSetActivityTimeout timeout;
		supla_log(LOG_INFO,"[%s] registered: srv ver: %d(min=%d), activity timeout=%ds",
				dev->name,
				register_device_result->version,
				register_device_result->version_min,
				register_device_result->activity_timeout);

		if(dev->supla_config.activity_timeout != register_device_result->activity_timeout){
			supla_log(LOG_DEBUG, "Setting activity timeout to %ds", dev->supla_config.activity_timeout);
			timeout.activity_timeout =  dev->supla_config.activity_timeout;
			srpc_dcs_async_set_activity_timeout(dev->srpc,&timeout);
		}
		gettimeofday(&dev->reg_time,NULL);
		supla_dev_set_state(dev,SUPLA_DEV_STATE_REGISTERED);
		}break;

	case SUPLA_RESULTCODE_BAD_CREDENTIALS:
		supla_log(LOG_ERR,"Bad credentials!");
		break;

	case SUPLA_RESULTCODE_TEMPORARILY_UNAVAILABLE:
		supla_log(LOG_NOTICE,"Temporarily unavailable!");
		break;

	case SUPLA_RESULTCODE_LOCATION_CONFLICT:
		supla_log(LOG_ERR,"Location conflict!");
		break;

	case SUPLA_RESULTCODE_CHANNEL_CONFLICT:
		supla_log(LOG_ERR,"Channel conflict!");
		break;

	case SUPLA_RESULTCODE_DEVICE_DISABLED:
		supla_log(LOG_NOTICE,"Device is disabled!");
		break;

	case SUPLA_RESULTCODE_LOCATION_DISABLED:
		supla_log(LOG_NOTICE,"Location is disabled!");
		break;

	case SUPLA_RESULTCODE_DEVICE_LIMITEXCEEDED:
		supla_log(LOG_NOTICE,"Device limit exceeded!");
		break;

	case SUPLA_RESULTCODE_GUID_ERROR:
		supla_log(LOG_NOTICE,"Incorrect device GUID!");
		break;

	case SUPLA_RESULTCODE_REGISTRATION_DISABLED:
		supla_log(LOG_NOTICE,"Registration disabled!");
		break;

	case SUPLA_RESULTCODE_AUTHKEY_ERROR:
		supla_log(LOG_NOTICE,"Incorrect AuthKey!");
		break;

	case SUPLA_RESULTCODE_NO_LOCATION_AVAILABLE:
		supla_log(LOG_NOTICE, "No location available!");
		break;

	case SUPLA_RESULTCODE_USER_CONFLICT:
		supla_log(LOG_NOTICE,"User conflict!");
		break;
  }
}

static void supla_dev_on_set_channel_value(supla_dev_t *dev, TSD_SuplaChannelNewValue *new_value)
{
	char success = 0;
	supla_log(LOG_DEBUG,"channel[%d] set value request",new_value->ChannelNumber);

	supla_channel_t *ch = supla_dev_get_channel_by_num(dev,new_value->ChannelNumber);
	if(!ch){
		supla_log(LOG_ERR,"channel[%d] not found",new_value->ChannelNumber);
		success = 0;
	}else if(ch->on_set_value){
		success = ch->on_set_value(ch,new_value);
	}
	srpc_ds_async_set_channel_result(dev->srpc, new_value->ChannelNumber, new_value->SenderID, success);
}

static void supla_dev_on_set_channel_group_value(supla_dev_t *dev, TSD_SuplaChannelGroupNewValue *gr_value)
{
	TSD_SuplaChannelNewValue new_value;
	char success = 0;
	supla_log(LOG_DEBUG,"channel group[%d] set value request for ch[%d]",gr_value->GroupID,gr_value->ChannelNumber);

	new_value.SenderID = 0;
	new_value.ChannelNumber = gr_value->ChannelNumber;
	new_value.DurationMS = gr_value->DurationMS;
	memcpy(new_value.value,gr_value->value,SUPLA_CHANNELVALUE_SIZE);

	supla_channel_t *ch = supla_dev_get_channel_by_num(dev,new_value.ChannelNumber);
	if(!ch){
		supla_log(LOG_ERR,"channel[%d] not found",new_value.ChannelNumber);
		success = 0;
	}else if(ch->on_set_value){
		success = ch->on_set_value(ch,&new_value);
	}
	srpc_ds_async_set_channel_result(dev->srpc, new_value.ChannelNumber, new_value.SenderID, success);
}

static void supla_dev_get_channel_state(supla_dev_t *dev, TCSD_ChannelStateRequest *csd_channel_state_request)
{
	TDSC_ChannelState state = {};
	supla_log(LOG_DEBUG,"get channel[%d] state",csd_channel_state_request->ChannelNumber);

	state.ReceiverID = csd_channel_state_request->SenderID;
	state.ChannelNumber = csd_channel_state_request->ChannelNumber;

	/* fill internal supla device state data */
	state.Fields = 0x00;
	state.Fields |= SUPLA_CHANNELSTATE_FIELD_UPTIME;
	state.Uptime = dev->uptime;
	
	state.Fields |= SUPLA_CHANNELSTATE_FIELD_CONNECTIONUPTIME;
	state.ConnectionUptime = dev->connection_uptime;
	
	state.Fields |= SUPLA_CHANNELSTATE_FIELD_LASTCONNECTIONRESETCAUSE;
	state.LastConnectionResetCause = dev->connection_reset_cause;

	if(dev->on_get_channel_state)
		dev->on_get_channel_state(dev,&state);

	supla_channel_t *ch = supla_dev_get_channel_by_num(dev,csd_channel_state_request->ChannelNumber);
	if(ch){
		if(ch->on_get_state)
			ch->on_get_state(ch, &state);
	} else {
		supla_log(LOG_ERR,"channel[%d] not found", csd_channel_state_request->ChannelNumber);
	}
	srpc_csd_async_channel_state_result(dev->srpc, &state);
}

static void supla_dev_on_set_activity_timeout_result(supla_dev_t *dev, TSDC_SuplaSetActivityTimeoutResult *res)
{
	dev->supla_config.activity_timeout = res->activity_timeout;
	supla_log(LOG_INFO, "Received activity timeout=%ds allowed[%d-%d]",res->activity_timeout,res->min,res->max);
}

static void supla_dev_on_get_user_localtime_result(supla_dev_t *dev, TSDC_UserLocalTimeResult *lt)
{
	time_t local = 0;

	supla_log(LOG_DEBUG, "Received user localtime result: %4d-%02d-%02d %02d:%02d:%02d %s",
		lt->year, lt->month,lt->day,lt->hour,lt->min,lt->sec, lt->timezone);
	if(dev->on_server_time_sync){
		if(dev->on_server_time_sync(dev,lt) == 0){
			time(&local);
			supla_log(LOG_INFO, "device time sync success: %s",strtok(ctime(&local),"\n"));
			/* update timers */
			dev->init_time.tv_sec = local - dev->uptime;
			dev->reg_time.tv_sec = local - dev->connection_uptime;
		}else{
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
			calcfg->ChannelNumber,calcfg->Command,calcfg->SuperUserAuthorized,calcfg->DataType,calcfg->DataSize);

	if(calcfg->SuperUserAuthorized != 1) {
		result.Result = SUPLA_CALCFG_RESULT_UNAUTHORIZED;
	}else if(calcfg->ChannelNumber == -1){
		/* no channel specified - config device */
		switch(calcfg->Command){
		case SUPLA_CALCFG_CMD_ENTER_CFG_MODE:
			supla_log(LOG_INFO,"CALCFG ENTER CONFIG MODE received");
			supla_dev_set_state(dev, SUPLA_DEV_STATE_CONFIG);
			break;
		default:
			result.Result = SUPLA_CALCFG_RESULT_NOT_SUPPORTED;
			break;
		}
	}else{
		supla_channel_t *ch = supla_dev_get_channel_by_num(dev,calcfg->ChannelNumber);
		if(ch){
			if(ch->on_calcfg_req)
				result.Result = ch->on_calcfg_req(ch, calcfg);
			else
				result.Result = SUPLA_CALCFG_RESULT_NOT_SUPPORTED;
		} else {
			supla_log(LOG_ERR,"channel[%d] not found", calcfg->ChannelNumber);
			result.Result = SUPLA_CALCFG_RESULT_ID_NOT_EXISTS;
		}
	}
	srpc_ds_async_device_calcfg_result(dev->srpc, &result);
}

static void supla_dev_on_remote_call_received(void *_srpc,
		unsigned int rr_id, unsigned int call_type, void *_dcd, unsigned char proto_version)
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

	switch (rd.call_type) {
	case SUPLA_SDC_CALL_GETVERSION_RESULT:
		supla_log(LOG_DEBUG, "Received get version from server: %d",proto_version);
		break;
	case SUPLA_SDC_CALL_VERSIONERROR:
		supla_connection_on_version_error(rd.data.sdc_version_error);
		break;
	case SUPLA_SDC_CALL_PING_SERVER_RESULT:
		break;
	case SUPLA_SD_CALL_REGISTER_DEVICE_RESULT:
		supla_connection_on_register_result(dev,rd.data.sd_register_device_result);
		break;
	case SUPLA_SD_CALL_CHANNEL_SET_VALUE:
		supla_dev_on_set_channel_value(dev,rd.data.sd_channel_new_value);
		break;
	case SUPLA_SD_CALL_CHANNELGROUP_SET_VALUE:
		supla_dev_on_set_channel_group_value(dev,rd.data.sd_channelgroup_new_value);
		break;
	case SUPLA_SDC_CALL_SET_ACTIVITY_TIMEOUT_RESULT:
		supla_dev_on_set_activity_timeout_result(dev,rd.data.sdc_set_activity_timeout_result);
		break;
	case SUPLA_SD_CALL_GET_FIRMWARE_UPDATE_URL_RESULT:
		supla_log(LOG_DEBUG, "Received update url result from server!");
		break;
	case SUPLA_DCS_CALL_GET_USER_LOCALTIME_RESULT:
		supla_dev_on_get_user_localtime_result(dev,rd.data.sdc_user_localtime_result);
		break;
	case SUPLA_SDC_CALL_GET_REGISTRATION_ENABLED_RESULT:
		supla_log(LOG_DEBUG, "Received registration enabled result from server!");
		break;
	case SUPLA_SD_CALL_DEVICE_CALCFG_REQUEST:
		supla_dev_on_calcfg_request(dev,rd.data.sd_device_calcfg_request);
		break;
	case SUPLA_CSD_CALL_GET_CHANNEL_STATE:
		supla_dev_get_channel_state(dev, rd.data.csd_channel_state_request);
		break;
	case SUPLA_SD_CALL_GET_CHANNEL_FUNCTIONS_RESULT:
		supla_log(LOG_DEBUG, "Received channel functions result from server!");
		break;
	case SUPLA_SD_CALL_GET_CHANNEL_CONFIG_RESULT:
		supla_log(LOG_DEBUG, "Received channel config result from server!");
		break;
	default:
		supla_log(LOG_DEBUG, "Received unknown message from server!");
		break;
	}
	srpc_rd_free(&rd);
	gettimeofday(&dev->last_call,NULL);
}

static int supla_connection_ping(supla_dev_t *dev)
{
	struct timeval now;
	gettimeofday(&now, NULL);

	if (dev->supla_config.activity_timeout == 0)
		return SUPLA_RESULT_TRUE;

	if ((now.tv_sec - dev->last_call.tv_sec) >= (dev->supla_config.activity_timeout - 5))
		return srpc_dcs_async_ping_server(dev->srpc);

	if ((now.tv_sec - dev->last_call.tv_sec) >= (dev->supla_config.activity_timeout + 10)){
		supla_log(LOG_ERR,"ping timeout");
		return SUPLA_RESULT_FALSE;
	}
	return SUPLA_RESULT_TRUE;
}

supla_dev_t* supla_dev_create(const char *dev_name, const char *soft_ver)
{
	supla_dev_t *dev = calloc(1,sizeof(supla_dev_t));
	if(!dev)
		return NULL;

	if(dev_name)
		strncpy(dev->name,dev_name,SUPLA_DEVICE_NAME_MAXSIZE-1);
	else
		strncpy(dev->name,"SUPLA device",SUPLA_DEVICE_NAME_MAXSIZE-1);

	if(soft_ver)
		strncpy(dev->soft_ver,soft_ver,SUPLA_SOFTVER_MAXSIZE-1);
	else
		strncpy(dev->soft_ver,"libsupla "LIBSUPLA_VER,SUPLA_SOFTVER_MAXSIZE-1);

	gettimeofday(&dev->init_time,NULL);
	STAILQ_INIT(&dev->channels);
	return dev;
}

int supla_dev_free(supla_dev_t *dev)
{
	supla_channel_t *ch;

	supla_link_free(dev->ssd);
	srpc_free(dev->srpc);

	STAILQ_FOREACH(ch,&dev->channels,channels){
		supla_channel_deinit(ch);
	}
	free(dev);
	return SUPLA_RESULT_TRUE;
}

int supla_dev_get_state(supla_dev_t *dev, supla_dev_state_t *state)
{
	if(!dev || !state)
		return SUPLA_RESULT_FALSE;

	*state = dev->state;
	return SUPLA_RESULT_TRUE;
}


int supla_dev_get_channel_count(const supla_dev_t *dev)
{
	supla_channel_t *ch;
	int n = 0;
	STAILQ_FOREACH(ch,&dev->channels,channels){
		n++;
	}
	return n;
}

int supla_dev_set_flags(supla_dev_t *dev, int flags)
{
	if(!dev)
		return SUPLA_RESULT_FALSE;
	dev->flags = flags;
	return SUPLA_RESULT_TRUE;
}

int supla_dev_set_manufaturer_data(supla_dev_t *dev, const struct manufacturer_data *mfr_data)
{
	if(!dev)
		return SUPLA_RESULT_FALSE;

	dev->manufacturer_id = mfr_data->manufacturer_id;
	dev->product_id = mfr_data->product_id;
	return SUPLA_RESULT_TRUE;
}

int supla_dev_set_state_changed_callback(supla_dev_t *dev, on_change_state_callback_t callback)
{
	if(!dev)
		return SUPLA_RESULT_FALSE;

	dev->on_state_change = callback;
	return SUPLA_RESULT_TRUE;
}

int supla_dev_set_common_channel_state_callback(supla_dev_t *dev, supla_device_get_state_handler_t callback)
{
	if(!dev)
		return SUPLA_RESULT_FALSE;

	dev->on_get_channel_state = callback;
	return SUPLA_RESULT_TRUE;
}

int supla_dev_set_server_time_sync_callback(supla_dev_t *dev, on_server_time_sync_callback_t callback)
{
	if(!dev)
		return SUPLA_RESULT_FALSE;
	dev->on_server_time_sync = callback;
	return SUPLA_RESULT_TRUE;
}

int supla_dev_add_channel(supla_dev_t *dev, supla_channel_t *ch)
{
	int channel_count;

	struct supla_channel_priv *priv = ch->priv;
	if(!ch->priv){
		supla_log(LOG_ERR,"[%s] cannot add channel: channel not initialized", dev->name);
		return EPERM;
	}

	channel_count = supla_dev_get_channel_count(dev);

	if(channel_count >= SUPLA_CHANNELMAXCOUNT){
		supla_log(LOG_ERR,"[%s] cannot add channel: channel max count reached", dev->name);
		return EPERM;
	}

	if(ch->type == SUPLA_CHANNELTYPE_ACTIONTRIGGER){
		if(!priv->action_trigger){
			supla_log(LOG_ERR,"[%s] cannot add channel: action trigger channel has no action trigger", dev->name);
			return SUPLA_RESULTCODE_CHANNEL_CONFLICT;
		}
		if(ch->action_trigger_related_channel){
			priv->action_trigger->properties.relatedChannelNumber = supla_channel_get_assigned_number(ch->action_trigger_related_channel)+1;
		} else {
			priv->action_trigger->properties.relatedChannelNumber = 0;
		}
		supla_log(LOG_DEBUG,"[%s] [%d]added new action trigger", dev->name, channel_count);
	} else {
		if(!priv->supla_val){
			supla_log(LOG_ERR,"[%s] cannot add channel: channel has no value assigned", dev->name);
			return SUPLA_RESULTCODE_CHANNEL_CONFLICT;
		}
		supla_log(LOG_DEBUG,"[%s] [%d]added new channel",dev->name,channel_count);
	}
	STAILQ_INSERT_TAIL(&dev->channels,ch,channels);
	priv->number = channel_count;
	return 0;
}

int supla_dev_enter_config_mode(supla_dev_t *dev)
{
	supla_dev_set_state(dev,SUPLA_DEV_STATE_CONFIG);
	return 0;
}

int supla_dev_exit_config_mode(supla_dev_t *dev)
{
	supla_dev_set_state(dev,SUPLA_DEV_STATE_OFFLINE);
	return 0;
}

int supla_dev_setup(supla_dev_t *dev,  const struct supla_config *cfg)
{
	if(supla_dev_initialized(dev)){
		supla_log(LOG_ERR,"Device [%s] already initialized", dev->name);
		return SUPLA_RESULT_FALSE;
	}

	dev->state = SUPLA_DEV_STATE_OFFLINE;
	if(cfg->email[0]){
		strncpy(dev->supla_config.email, cfg->email, SUPLA_EMAIL_MAXSIZE);
	}else {
		supla_log(LOG_ERR,"email not set");
		return SUPLA_RESULT_FALSE;
	}

	if(cfg->auth_key[0]){
		memcpy(dev->supla_config.auth_key, cfg->auth_key, SUPLA_AUTHKEY_SIZE);
	}else {
		supla_log(LOG_ERR,"auth key not set");
		return SUPLA_RESULTCODE_AUTHKEY_ERROR;
	}

	if(cfg->guid[0]){
		memcpy(dev->supla_config.guid, cfg->guid, SUPLA_GUID_SIZE);
	}else {
		supla_log(LOG_ERR,"guid not set");
		return SUPLA_RESULTCODE_GUID_ERROR;
	}

	if(cfg->server[0]){
		strncpy(dev->supla_config.server, cfg->server,SUPLA_SERVER_NAME_MAXSIZE);
	}else {
		supla_log(LOG_ERR,"server not set");
		return SUPLA_RESULTCODE_CANTCONNECTTOHOST;
	}

	dev->supla_config.ssl = cfg->ssl ? 1:0;
	if(!dev->supla_config.port)
		dev->supla_config.port = dev->supla_config.ssl ? 2016 : 2015;

	if(cfg->activity_timeout)
		dev->supla_config.activity_timeout = cfg->activity_timeout;
	else
		dev->supla_config.activity_timeout = 60;

	TsrpcParams srpc_params;
	srpc_params_init(&srpc_params);

	srpc_params.data_read = supla_dev_read;
	srpc_params.data_write = supla_dev_write;
	srpc_params.on_remote_call_received = supla_dev_on_remote_call_received;
	srpc_params.user_params = dev;

	dev->srpc = srpc_init(&srpc_params);
	srpc_set_proto_version(dev->srpc, SUPLA_PROTO_VERSION);

	dev->ssd = supla_link_init(dev->supla_config.server, dev->supla_config.port, dev->supla_config.ssl);
	if(!dev->ssd){
		supla_log(LOG_ERR,"Cannot create socket");
		return SUPLA_RESULT_FALSE;
	}
	supla_log(LOG_INFO,"Device [%s] setup completed", dev->name);
	return SUPLA_RESULT_TRUE;
}

static int supla_dev_register(supla_dev_t *dev)
{
	TDS_SuplaRegisterDevice_E reg_dev = {0};
	supla_channel_t *ch;
	uint8_t ch_num = 0;

	if(!supla_dev_initialized(dev)){
		supla_log(LOG_ERR,"[%s] Cannot register - not initialized", dev->name);
		return SUPLA_RESULT_FALSE;
	}

	strncpy(reg_dev.Email, dev->supla_config.email, SUPLA_EMAIL_MAXSIZE);
	strncpy(reg_dev.AuthKey, dev->supla_config.auth_key, SUPLA_AUTHKEY_SIZE);
	strncpy(reg_dev.GUID, dev->supla_config.guid, SUPLA_GUID_SIZE);
	strncpy(reg_dev.Name, dev->name, SUPLA_DEVICE_NAME_MAXSIZE);
	strncpy(reg_dev.SoftVer, dev->soft_ver, SUPLA_SOFTVER_MAXSIZE);
	strncpy(reg_dev.ServerName, dev->supla_config.server, SUPLA_SERVER_NAME_MAXSIZE);
	reg_dev.Flags = dev->flags;
	reg_dev.ManufacturerID = dev->manufacturer_id;
	reg_dev.ProductID = dev->product_id;

	STAILQ_FOREACH(ch,&dev->channels,channels){
		reg_dev.channels[ch_num] = supla_channel_to_register_struct(ch);
		ch_num++;
	}
	reg_dev.channel_count = ch_num;

	supla_log(LOG_INFO, "[%s] register device...", dev->name);
	gettimeofday(&dev->reg_time,NULL);
	return srpc_ds_async_registerdevice_e(dev->srpc, &reg_dev);
}

static int supla_dev_time_sync(supla_dev_t *dev)
{
	if(dev->on_server_time_sync)
		return srpc_dcs_async_get_user_localtime(dev->srpc);
	else
		return SUPLA_RESULT_FALSE;
}

static void supla_dev_handle_data_changed(supla_dev_t *dev)
{
	supla_channel_t *ch;

	STAILQ_FOREACH(ch,&dev->channels,channels){
		supla_channel_sync(dev->srpc,ch);
	}
}

int supla_dev_iterate(supla_dev_t *dev)
{
	struct timeval sys_time;
	gettimeofday(&sys_time,NULL);

	if(!supla_dev_initialized(dev)){
		supla_log(LOG_ERR,"[%s] Cannot iterate - not initialized", dev->name);
		return SUPLA_RESULT_FALSE;
	}

	dev->uptime = difftime(sys_time.tv_sec, dev->init_time.tv_sec);
	switch (dev->state) {
		case SUPLA_DEV_STATE_OFFLINE:
			supla_log(LOG_INFO,"Connecting to: %s:%d",dev->supla_config.server,dev->supla_config.port);
			if (supla_link_connect(dev->ssd) == 0){
				supla_delay_ms(5000);
				return SUPLA_RESULT_FALSE;
			} else {
				supla_log(LOG_INFO,"Connected to server");
				supla_dev_set_state(dev,SUPLA_DEV_STATE_CONNECTED);
			}
			break;

		case SUPLA_DEV_STATE_CONNECTED:
			if(!dev->reg_time.tv_sec || difftime(sys_time.tv_sec,dev->reg_time.tv_sec) > 10){
				if(!supla_dev_register(dev)){
					supla_log(LOG_ERR, "supla_dev_register failed!");
				}
			}
			break;

		case SUPLA_DEV_STATE_REGISTERED:
			supla_dev_time_sync(dev);
			supla_dev_set_state(dev,SUPLA_DEV_STATE_ONLINE);
			break;

		case SUPLA_DEV_STATE_ONLINE:
			dev->connection_uptime = difftime(sys_time.tv_sec, dev->reg_time.tv_sec);
			if(supla_connection_ping(dev) == SUPLA_RESULT_FALSE){
				supla_dev_set_connection_reset_cause(dev,SUPLA_LASTCONNECTIONRESETCAUSE_ACTIVITY_TIMEOUT);
				supla_dev_set_state(dev,SUPLA_DEV_STATE_OFFLINE);
				return SUPLA_RESULT_FALSE;
			}
			supla_dev_handle_data_changed(dev);
			break;
		case SUPLA_DEV_STATE_CONFIG:
			break;
		default:
			break;
	}

	if(srpc_iterate(dev->srpc) == SUPLA_RESULT_FALSE) {
		supla_log(LOG_DEBUG, "srpc_iterate failed");
		supla_dev_set_connection_reset_cause(dev,SUPLA_LASTCONNECTIONRESETCAUSE_SERVER_CONNECTION_LOST);
		supla_dev_set_state(dev,SUPLA_DEV_STATE_OFFLINE);
		supla_delay_ms(5000);
		return SUPLA_RESULT_FALSE;
	}
	return 0;
}