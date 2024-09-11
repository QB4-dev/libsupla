/*
 * Copyright (c) 2022 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef SUPLA_CHANNEL_PRIV_H_
#define SUPLA_CHANNEL_PRIV_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include <libsupla/device.h>

#include "../include/libsupla/push-notification.h"

/* device private data */
struct supla_dev {
    char name[SUPLA_DEVICE_NAME_MAXSIZE];
    char soft_ver[SUPLA_SOFTVER_MAXSIZE];
    struct manufacturer_data mfr_data;
    int flags; //SUPLA_DEVICE_FLAG_*

    supla_dev_state_t state;
    struct supla_config supla_config;

    supla_link_t cloud_link;
    void *srpc;
    void *lck;

    on_change_state_callback_t on_state_change;
    supla_device_get_state_handler_t on_get_channel_state;
    on_server_time_sync_callback_t on_server_time_sync;

    supla_push_notification_config_t push_notification;

    uint64_t iterate_time_msec;
    uint64_t wait_iterate_msec;

    struct timeval init_time;
    struct timeval register_time;
    struct timeval last_ping;
    struct timeval last_resp;

    time_t uptime;
    time_t connection_uptime;
    unsigned char connection_reset_cause;

    STAILQ_HEAD(qhead, supla_channel) channels;
};

#ifdef __cplusplus
}
#endif

#endif /* SUPLA_CHANNEL_PRIV_H_ */
