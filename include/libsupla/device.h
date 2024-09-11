/*
 * Copyright (c) 2022 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef SUPLA_DEVICE_H_
#define SUPLA_DEVICE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "supla.h"
#include "channel.h"
#include "push-notification.h"

/**
 * @brief SUPLA device instance.
 *
 * Typical program flow
 * @code{c}
 *
 * static struct supla_config supla_config = {
 *    .email = "user@example.com",
 *    .auth_key = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
 *    .guid = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
 *    .server = "svr.supla.org",
 *    .ssl = 1
 * };
 *
 * supla_dev_t *dev = supla_dev_create("Test Device",NULL);
 *
 * supla_dev_set_config(dev,&supla_config);
 * supla_dev_add_channel(dev,&temp_channel);
 * supla_dev_add_channel(dev,&wind_channel);
 *
 * while(!app_quit){
 *    supla_dev_iterate(dev);
 *    usleep(10000);
 * }
 * @endcode
 */
typedef struct supla_dev supla_dev_t;

typedef enum {
    SUPLA_DEV_STATE_CONFIG,
    SUPLA_DEV_STATE_IDLE,
    SUPLA_DEV_STATE_INIT,
    SUPLA_DEV_STATE_CONNECTED,
    SUPLA_DEV_STATE_REGISTERED,
    SUPLA_DEV_STATE_ONLINE,
} supla_dev_state_t;

struct manufacturer_data {
    _supla_int16_t manufacturer_id;
    _supla_int16_t product_id;
};

/**
 * @brief Function called on device state changed
 * Example callback function:
 * @code{c}
 * void state_change_callback(supla_dev_t *dev, supla_dev_state_t state)
 * {
 * 	const char *states_str[] = {
 * 		[SUPLA_DEV_STATE_INIT] = "INIT",
 * 		[SUPLA_DEV_STATE_CONNECTED] = "CONNECTED",
 * 		[SUPLA_DEV_STATE_REGISTERED] = "REGISTERED",
 * 		[SUPLA_DEV_STATE_ONLINE] = "ONLINE",
 * 		[SUPLA_DEV_STATE_CONFIG] = "CONFIG",
 * 	};
 * 	supla_log(LOG_INFO,"state -> %s", states_str[state]);
 * }
 * @endcode
 *
 * @param[in] dev SUPLA device instance
 * @param[in] state current SUPLA device state
 */
typedef void (*on_change_state_callback_t)(supla_dev_t *dev, supla_dev_state_t state);

/**
 * @brief Function called on every channel state request. It is possible to set here
 * common state data for all channels like IP address, battery level etc.
 * Example callback function:
 * @code{c}
 * int common_channel_state_callback(supla_dev_t *dev, TDSC_ChannelState *state)
 * {
 * 	state->Fields |= SUPLA_CHANNELSTATE_FIELD_BATTERYLEVEL;
 * 	state->BatteryLevel = 75;
 * 	return 0;
 * }
 * @endcode
 *
 * @param[in] dev SUPLA device instance
 * @param[out] state common SUPLA channel state - shared by all channels
 * @return 0 on success
 */
typedef int (*supla_device_get_state_handler_t)(supla_dev_t *dev, TDSC_ChannelState *state);

/**
 * @brief Function called on user localtime received from server
 * Example callback function:
 * @code{c}
 * int time_sync_callback(supla_dev_t *dev, TSDC_UserLocalTimeResult *lt)
 * {
 * 	supla_log(LOG_DEBUG, "Received user localtime result: %4d-%02d-%02d %02d:%02d:%02d %s",
 * 		lt->year, lt->month,lt->day,lt->hour,lt->min,lt->sec, lt->timezone);
 * 	set_rtc(lt);
 * 	return 0;
 * }
 * @endcode
 *
 * @param[in] dev SUPLA device instance
 * @param[in] lt  SUPLA server user localtime
 * @return 0 on success
 */
typedef int (*on_server_time_sync_callback_t)(supla_dev_t *dev, TSDC_UserLocalTimeResult *lt);

/**
 * @brief Function called on device restart request received from server
 *
 * @param[in] dev SUPLA device instance
 * @return 0 on success
 */
typedef int (*on_restart_from_server_callback_t)(supla_dev_t *dev);

/**
 * @brief Create SUPLA device instance
 *
 * @param[in] dev_name SUPLA device name. Name "SUPLA device" will be set if dev_name is NULL
 * @param[in] soft_ver Software version. Version "libsupla #version" will be set if soft_ver is NULL
 * @return SUPLA device instance or NULL if failed
 */
supla_dev_t *supla_dev_create(const char *dev_name, const char *soft_ver);

/**
 * @brief Free SUPLA device instance and all internal allocated resources
 *
 * @param[in] dev SUPLA device instance
 * @return SUPLA_RESULT_TRUE on success
 */
int supla_dev_free(supla_dev_t *dev);

/**
 * @brief Set SUPLA device name
 *
 * @param[in] dev SUPLA device instance
 * @param[in] name name buffer
 * @return SUPLA_RESULT_TRUE on success
 */
int supla_dev_set_name(supla_dev_t *dev, const char *name);

/**
 * @brief Get SUPLA device name
 *
 * @param[in] dev SUPLA device instance
 * @param[out] name name buffer
 * @param[in] len name buffer length
 * @return SUPLA_RESULT_TRUE on success
 */
int supla_dev_get_name(const supla_dev_t *dev, char *name, size_t len);

/**
 * @brief Get SUPLA device software version
 *
 * @param[in] dev SUPLA device instance
 * @param[out] version name buffer
 * @param[in] len version buffer length
 * @return SUPLA_RESULT_TRUE on success
 */
int supla_dev_get_software_version(const supla_dev_t *dev, char *version, size_t len);

/**
 * @brief Get current SUPLA device state
 *
 * @param[in] dev SUPLA device instance
 * @param[out] state SUPLA device state
 * @return SUPLA_RESULT_TRUE on success or SUPLA_RESULT_FALSE if dev or state is NULL
 */
int supla_dev_get_state(const supla_dev_t *dev, supla_dev_state_t *state);

/**
 * @brief Get SUPLA device state string
 *
 * @param[in] state SUPLA device state
 * @return state string
 */
const char *supla_dev_state_str(supla_dev_state_t state);

/**
 * @brief Set SUPLA device flags
 *
 * @param[in] dev SUPLA device instance
 * @param[in] flags SUPLA device flags from proto.h use defines: SUPLA_DEVICE_FLAG_*
 * @return SUPLA_RESULT_TRUE on success
 */
int supla_dev_set_flags(supla_dev_t *dev, int flags);

/**
 * @brief Get SUPLA device flags
 *
 * @param[in] dev SUPLA device instance
 * @param[out] flags SUPLA device flags from proto.h use defines: SUPLA_DEVICE_FLAG_*
 * @return SUPLA_RESULT_TRUE on success
 */
int supla_dev_get_flags(const supla_dev_t *dev, int *flags);

/**
 * @brief Set SUPLA device manufacturer data
 *
 * @param[in] dev SUPLA device instance
 * @param[in] mfr_data manufacturer data. See struct manufacturer_data for details
 * @return SUPLA_RESULT_TRUE on success
 */
int supla_dev_set_manufacturer_data(supla_dev_t *dev, const struct manufacturer_data *mfr_data);

/**
 * @brief Get SUPLA device manufacturer data
 *
 * @param[in] dev SUPLA device instance
 * @param[out] mfr_data manufacturer data. See struct manufacturer_data for details
 * @return SUPLA_RESULT_TRUE on success
 */
int supla_dev_get_manufacturer_data(const supla_dev_t *dev, struct manufacturer_data *mfr_data);

/**
 * @brief Get SUPLA device uptime
 *
 * @param[in] dev SUPLA device instance
 * @param[out] uptime SUPLA device uptime
 * @return SUPLA_RESULT_TRUE on success
 */
int supla_dev_get_uptime(const supla_dev_t *dev, time_t *uptime);

/**
 * @brief Get SUPLA device cloud connection uptime
 *
 * @param[in] dev SUPLA device instance
 * @param[out] connection_uptime SUPLA device loud connection uptime
 * @return SUPLA_RESULT_TRUE on success
 */
int supla_dev_get_connection_uptime(const supla_dev_t *dev, time_t *connection_uptime);

/**
 * @brief Set on device state change callback function
 *
 * @param[in] dev SUPLA device instance
 * @param[in] callback function called on device state change
 * @return SUPLA_RESULT_TRUE on success
 */
int supla_dev_set_state_changed_callback(supla_dev_t *dev, on_change_state_callback_t callback);

/**
 * @brief Set common channel state callback function
 *
 * @param[in] dev SUPLA device instance
 * @param[in] callback function called on every channel state request from server
 * @return SUPLA_RESULT_TRUE on success
 */
int supla_dev_set_common_channel_state_callback(supla_dev_t *dev, supla_device_get_state_handler_t callback);

/**
 * @brief Set on receive user localtime from server callback function
 *
 * @param[in] dev SUPLA device instance
 * @param[in] callback function called on get user localtime response from server
 * @return SUPLA_RESULT_TRUE on success
 */
int supla_dev_set_server_time_sync_callback(supla_dev_t *dev, on_server_time_sync_callback_t callback);

/**
 * @brief Set on restart from server callback function
 *
 * @param[in] dev SUPLA device instance
 * @param[in] callback function called on restart request from server
 * @return SUPLA_RESULT_TRUE on success
 */
int supla_dev_set_server_req_restart_callback(supla_dev_t *dev, on_restart_from_server_callback_t callback);

/**
 * @brief Add new channel to SUPLA device
 *
 * @note  Channel must be initialized before
 *
 * @param[in] dev SUPLA device instance
 * @param[in] ch channel to add
 * @return SUPLA_RESULT_TRUE on success or error number
 */
int supla_dev_add_channel(supla_dev_t *dev, supla_channel_t *ch);

/**
 * @brief Get SUPLA device channels count
 *
 * @param[in] dev SUPLA device instance
 * @return assigned channels count
 */
int supla_dev_get_channel_count(const supla_dev_t *dev);

/**
 * @brief Get SUPLA device channel by assigned number
 *
 * @param[in] dev SUPLA device instance
 * @param[in] num requested channel number
 * @return SUPLA channel or NULL if channel not available
 */
supla_channel_t *supla_dev_get_channel_by_num(const supla_dev_t *dev, int num);

/**
 * @brief Set SUPLA device connection config
 *
 * @param[in] dev SUPLA device instance
 * @param[in] config SUPLA connection config
 * @return SUPLA_RESULT_TRUE on success
 */
int supla_dev_set_config(supla_dev_t *dev, const struct supla_config *config);

/**
 * @brief Get SUPLA device connection config
 *
 * @param[in] dev SUPLA device instance
 * @param[out] config SUPLA connection config
 * @return SUPLA_RESULT_TRUE on success
 */
int supla_dev_get_config(supla_dev_t *dev, struct supla_config *config);

/**
 * @brief enable SUPLA device device-level notification
 *
 * @param[in] dev SUPLA device instance
 * @param[in] server_managed_fields fields managed by server. See PN_SERVER_MANAGED_* in proto.h
 * @return SUPLA_RESULT_TRUE on success
 */
int supla_dev_enable_notifications(supla_dev_t *dev, const unsigned char server_managed_fields);

/**
 * @brief disable SUPLA device device-level notification
 *
 * @param[in] dev SUPLA device instance
 * @return SUPLA_RESULT_TRUE on success
 */
int supla_dev_disable_notifications(supla_dev_t *dev);

/**
 * @brief SUPLA device send notification
 *
 * @param[in] dev SUPLA device instance
 * @param[in] title notification title
 * @param[in] message notification message
 * @param[in] sound_id sound id
 * @return SUPLA_RESULT_TRUE on success
 */
int supla_dev_send_notification(supla_dev_t *dev, int ctx, const char *title, const char *message, int sound_id);

/**
 * @brief Start SUPLA device activity
 * or in config mode
 *
 * @param[in] dev SUPLA device instance
 * @return SUPLA_RESULT_TRUE on success
 */
int supla_dev_start(supla_dev_t *dev);

/**
 * @brief Stop SUPLA device activity
 *
 * @param[in] dev SUPLA device instance
 * @return SUPLA_RESULT_TRUE on success
 */
int supla_dev_stop(supla_dev_t *dev);

/**
 * @brief SUPLA device iterate - handle connection with server/sync data,
 * put this function in endless loop
 *
 * @param[in] dev SUPLA device instance
 * @return 0 on success
 */
int supla_dev_iterate(supla_dev_t *dev);

/**
 * @brief Enter config mode
 *
 * @param[in] dev SUPLA device instance
 * @return SUPLA_RESULT_TRUE on success
 */
int supla_dev_enter_config_mode(supla_dev_t *dev);

/**
 * @brief Exit config mode
 *
 * @param[in] dev SUPLA device instance
 * @return SUPLA_RESULT_TRUE on success
 */
int supla_dev_exit_config_mode(supla_dev_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* SUPLA_DEVICE_H_ */
