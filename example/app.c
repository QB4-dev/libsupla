/*
 * Copyright (c) 2022 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>

#include <libsupla/device.h>
#include <libsupla/supla-common/tools.h>

/* Fill using your data, and don't forget to enable device registration in SUPLA Cloud.
 *  To generate AUTH key and GUID use sites below:
 * https://www.supla.org/arduino/get-authkey
 * https://www.supla.org/arduino/get-guid
 */
static struct supla_config supla_config = {
    .email = "user@example.com",
    .auth_key = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    .guid = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    .server = "svr.supla.org",
#ifdef NOSSL
    .ssl = 0
#else
    .ssl = 1
#endif /*ifndef NOSSL*/
};

/* Define channels here */

//TEMPERATURE SENSOR
static supla_channel_t *temp_channel;

static supla_channel_config_t temp_channel_config = {
    .type = SUPLA_CHANNELTYPE_THERMOMETER,
    .supported_functions = SUPLA_CHANNELFNC_THERMOMETER,
    .default_function = SUPLA_CHANNELFNC_THERMOMETER,
};

//LIGHT CONTROL
static supla_channel_t *light_channel;

static int relay_on_set_value(supla_channel_t *ch, TSD_SuplaChannelNewValue *new_value)
{
    TRelayChannel_Value *relay_val = (TRelayChannel_Value *)new_value->value;
    supla_log(LOG_DEBUG, "light set value %s", relay_val->hi ? "ON" : "OFF");
    return supla_channel_set_relay_value(ch, relay_val);
}

static int get_light_state(supla_channel_t *ch, TDSC_ChannelState *state)
{
    state->Fields |= SUPLA_CHANNELSTATE_FIELD_LIGHTSOURCEOPERATINGTIME;
    state->LightSourceOperatingTime = 123;
    return 0;
}

static int set_light_config(supla_channel_t *ch, TSDS_SetChannelConfig *chcfg)
{
    supla_log(LOG_DEBUG, "set_light_config: ch:%d func=%d typ=%d", chcfg->ChannelNumber, chcfg->Func,
              chcfg->ConfigType);
    return 0;
}

static int light_config_recv(supla_channel_t *ch, TSDS_SetChannelConfig *chcfg)
{
    supla_log(LOG_DEBUG, "set_light_config: ch:%d func=%d typ=%d", chcfg->ChannelNumber, chcfg->Func,
              chcfg->ConfigType);
    return 0;
}

static supla_channel_config_t light_channel_config = {
    .type = SUPLA_CHANNELTYPE_RELAY,
    .supported_functions = 0xff, //SUPLA_CHANNELFNC_LIGHTSWITCH,
    .default_function = SUPLA_CHANNELFNC_LIGHTSWITCH,
    .flags = SUPLA_CHANNEL_FLAG_LIGHTSOURCELIFESPAN_SETTABLE,
    .on_set_value = relay_on_set_value,
    .on_get_state = get_light_state,
    .on_config_set = set_light_config,
    .on_config_recv = light_config_recv //
};

//ACTION TRIGGER
static supla_channel_t *at_channel;

static supla_channel_config_t at_channel_config = {
    .type = SUPLA_CHANNELTYPE_ACTIONTRIGGER,
    .supported_functions = SUPLA_CHANNELFNC_ACTIONTRIGGER,
    .default_function = SUPLA_CHANNELFNC_ACTIONTRIGGER,
    .action_trigger_caps = SUPLA_ACTION_CAP_SHORT_PRESS_x1 | SUPLA_ACTION_CAP_SHORT_PRESS_x2,
    //.action_trigger_related_channel = &light_channel
};

static pthread_t io_thread;
static sig_atomic_t app_quit;

static void sig_handler(int signum)
{
    (void)signum;
    app_quit = 1;
}

/* this is example thread where channel data is updated*/
static void *io_thread_function(void *data)
{
    double temperature = 15;

    srand(time(NULL));
    sleep(30);
    while (1) {
        puts("data update");
        temperature = rand() % 100;
        supla_log(LOG_INFO, "temperature value=%g", temperature);
        supla_channel_set_double_value(temp_channel, temperature);
        sleep(10);
    }
    pthread_exit(NULL);
}

int main(int argc, char **argv)
{
    int opt;
    const char *options = "available options:\n"
                          "    -s SUPLA server address\n"
                          "    -e SUPLA account email\n"
                          "    -h print this help\n";

    while ((opt = getopt(argc, argv, "e:s:h")) != -1) {
        switch (opt) {
        case 'e':
            strncpy(supla_config.email, optarg, SUPLA_EMAIL_MAXSIZE - 1);
            break;
        case 's':
            strncpy(supla_config.server, optarg, SUPLA_SERVER_NAME_MAXSIZE - 1);
            break;
        case 'h':
        default: /* '?' */
            printf("Usage: %s -s server -e email\n", argv[0]);
            puts(options);
            exit(EXIT_FAILURE);
            break;
        }
    }

    debug_mode = 1;
    supla_log(LOG_DEBUG, "Debug log enabled");

    /* Create SUPLA device */
    supla_dev_t *dev = supla_dev_create("Example Device", NULL);
    if (!dev) {
        supla_log(LOG_ERR, "Cannot create SUPLA dev");
        exit(EXIT_FAILURE);
    }

    /* generate GUID and AUTH key */
    st_read_guid_from_file("supla_guid", supla_config.guid, 1);
    st_read_authkey_from_file("supla_auth", supla_config.auth_key, 1);

    /* Setup SUPLA device using config parameters */
    if (supla_dev_set_config(dev, &supla_config) != SUPLA_RESULT_TRUE) {
        supla_log(LOG_ERR, "SUPLA dev setup failed");
        supla_dev_free(dev);
        exit(EXIT_FAILURE);
    }

    /* Prepare channels */
    supla_log(LOG_DEBUG, "Channels init");
    temp_channel = supla_channel_create(&temp_channel_config);
    light_channel = supla_channel_create(&light_channel_config);
    at_channel = supla_channel_create(&at_channel_config);
    supla_log(LOG_DEBUG, "Channels initialized");

    /* Add channels to device*/
    supla_dev_add_channel(dev, temp_channel);
    supla_dev_add_channel(dev, light_channel);
    supla_dev_add_channel(dev, at_channel);
    supla_dev_start(dev);

    signal(SIGINT, sig_handler); // Register signal handler
    pthread_create(&io_thread, NULL, io_thread_function, NULL);
    /* Infinite loop */
    while (!app_quit) {
        supla_dev_iterate(dev);
        usleep(10000);
    }
    supla_log(LOG_DEBUG, "app_quit");
    pthread_cancel(io_thread);
    pthread_join(io_thread, NULL);
    supla_dev_free(dev);
    return EXIT_SUCCESS;
}
