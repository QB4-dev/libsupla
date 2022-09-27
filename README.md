# libsupla

[SUPLA](https://www.supla.org) is an open source project for home automation.

# libsupla library

This library can be used to create devices and services connected with SUPLA Cloud.
If compared with more featured [supla-device](https://github.com/SUPLA/supla-device) which is like 
firmware/SDK based on Arduino lib ported to various platforms this library concept is more classical, 
and has less dependencies.

## Supported platforms

This library has been tested on following platforms:
- Linux
- ESP8266 currently without SSL/TLS support 

## Getting started

To compile this library on Linux please install first:

`sudo apt-get install libssl-dev`

or use `NOSSL=1` flag to compile without SSL support

Then run

```
make all
```

and to install library:

```
sudo make install
```

From now you can start to write your own software connected with [SUPLA](https://www.supla.org)!
Just add to your C code:

```
#include <libsupla/device.h>
```

and also add linker flag `-lsupla`  and `-lssl` if compiled without `NOSSL=1` flag

## The basics

### Device

We will start from creation of SUPLA device object:

```
supla_dev_t *dev = supla_dev_create("Test Device",NULL);

```

Then we need supla connection config like:

```
static struct supla_config supla_config = {
	.email = "user@example.com",
	.auth_key = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
	.guid = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
	.server = "svr.supla.org",
	.ssl = 1
};

```

You must have supla account registered with email address and to quick start generate your device data use links below:
- [AUTHKEY generator](https://www.supla.org/arduino/get-authkey)
- [GUID generator](https://www.supla.org/arduino/get-guid)


SUPLA device must be configured before first connection:

```
supla_dev_setup(dev,&supla_config);
```

### Channels

SUPLA device needs channels. We can start from the most basic thermometer channel:

```
supla_channel_t temp_channel = {
	.type = SUPLA_CHANNELTYPE_THERMOMETER ,
	.supported_functions = SUPLA_CHANNELFNC_THERMOMETER ,
	.default_function = SUPLA_CHANNELFNC_THERMOMETER,
};
```

Channel must be initialized before added to device

```
supla_channel_init(&temp_channel);
supla_dev_add_channel(dev,&temp_channel);
```
In seperate thread/task we can set temperature

```
supla_channel_set_double_value(&temp_channel,temp);
```

And in main SUPLA thread put `supla_dev_iterate` inside infinite loop

```
while(!app_quit){
	supla_dev_iterate(dev);
	usleep(10000);
}
```

Device would automatically synchronize channel data with server

