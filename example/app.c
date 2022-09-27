/*
 * Copyright (c) 2022 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#include <libsupla/device.h>

/* Fill using your data, and don't forget to enable device registration in SUPLA Cloud.
 *  To generate AUTH key and GUID use sites below:
 * https://www.supla.org/arduino/get-authkey
 * https://www.supla.org/arduino/get-guid
 */
static struct supla_config supla_config = {
	.email = "user@example.com",
	.auth_key = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
	.guid = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
	.server = "svr.supla.org",
	.ssl = 1
};

//TEMPERATURE SENSOR
supla_channel_t temp_channel = {
	.type = SUPLA_CHANNELTYPE_THERMOMETER ,
	.supported_functions = SUPLA_CHANNELFNC_THERMOMETER,
	.default_function = SUPLA_CHANNELFNC_THERMOMETER,
};

//RELAY CONTROL
static int relay_set_value(supla_channel_t *ch, TSD_SuplaChannelNewValue *new_value)
{
	TRelayChannel_Value *relay_val = (TRelayChannel_Value*)new_value->value;
	supla_log(LOG_DEBUG,"relay set value %d",relay_val->hi);
	return supla_channel_set_relay_value(ch,relay_val);
}

supla_channel_t relay_channel = {
	.type = SUPLA_CHANNELTYPE_RELAY,
	.supported_functions = SUPLA_CHANNELFNC_LIGHTSWITCH|SUPLA_CHANNELFNC_POWERSWITCH,
	.default_function = SUPLA_CHANNELFNC_POWERSWITCH,
	.on_set_value = relay_set_value
};

//LIGHT CONTROL
static int get_light_state(supla_channel_t *ch, TDSC_ChannelState *state)
{
	state->Fields |= SUPLA_CHANNELSTATE_FIELD_LIGHTSOURCEOPERATINGTIME;
	state->LightSourceOperatingTime=123;
	return 0;
}

supla_channel_t light_channel = {
	.type = SUPLA_CHANNELTYPE_RELAY,
	.supported_functions = SUPLA_CHANNELFNC_LIGHTSWITCH,
	.default_function = SUPLA_CHANNELFNC_LIGHTSWITCH,
	.flags = SUPLA_CHANNEL_FLAG_LIGHTSOURCELIFESPAN_SETTABLE,
	.on_set_value = relay_set_value,
	.on_get_state = get_light_state
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
	while(1){
		puts("data update");
		temperature = rand() %100;
		supla_log(LOG_INFO,"temperature value=%g",temperature);
		supla_channel_set_double_value(&temp_channel,temperature);
		sleep(30);
	}
	pthread_exit(NULL);
}

int main(void) {
	debug_mode = 1;
	supla_log(LOG_DEBUG,"Debug log enabled");
    /* Create SUPLA device */
	supla_dev_t *dev = supla_dev_create("Example Device",NULL);
    if(!dev){
        supla_log(LOG_ERR,"cannt create SUPLA dev");
        exit(EXIT_FAILURE);
    }
     /* Setup SUPLA device using config parameters */ 
    if(supla_dev_setup(dev,&supla_config) != 0){
        supla_log(LOG_ERR,"SUPLA dev setup failed");
        exit(EXIT_FAILURE);
    }
    
    /* Prepare channels */ 
	supla_log(LOG_DEBUG,"Channels init");
	supla_channel_init(&temp_channel);
	supla_channel_init(&light_channel);
	supla_channel_init(&relay_channel);
	supla_log(LOG_DEBUG,"Channels initialized");
	
    /* Add channels to device*/ 
    supla_dev_add_channel(dev,&temp_channel);
	supla_dev_add_channel(dev,&light_channel);
	supla_dev_add_channel(dev,&relay_channel);

    signal(SIGINT,sig_handler); // Register signal handler
    pthread_create(&io_thread, NULL,io_thread_function,NULL);
    /* Infinite loop */
	while(!app_quit){
		supla_dev_iterate(dev);
		usleep(10000);
	}
	supla_log(LOG_DEBUG,"app_quit");
	pthread_cancel(io_thread);
	pthread_join(io_thread,NULL);
	supla_dev_free(dev);
	return EXIT_SUCCESS;
}
