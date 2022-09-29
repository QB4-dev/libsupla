/*
 * Copyright (c) 2022 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "util.h"

int supla_delay_ms(const unsigned int ms)
{
#if LIBSUPLA_ARCH == LIBSUPLA_ARCH_WIN32

#elif LIBSUPLA_ARCH == LIBSUPLA_ARCH_ESP8266
	vTaskDelay(ms / portTICK_RATE_MS);
	return 0;
#elif LIBSUPLA_ARCH == LIBSUPLA_ARCH_ESP32
	vTaskDelay(ms / portTICK_RATE_MS);
	return 0;
#elif LIBSUPLA_ARCH == LIBSUPLA_ARCH_UNIX
	return usleep(ms * 1000);
#endif
}
