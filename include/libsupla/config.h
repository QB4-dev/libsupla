/*
 * Copyright (c) 2022 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef SUPLA_CONFIG_H_
#define SUPLA_CONFIG_H_

#define LIBSUPLA_ARCH_CUSTOM 0
#define LIBSUPLA_ARCH_UNIX 1
#define LIBSUPLA_ARCH_WIN32 2
#define LIBSUPLA_ARCH_ESP8266 3
#define LIBSUPLA_ARCH_ESP32 4

/* auto detect arch */
#if !defined(LIBSUPLA_ARCH)
#if defined(__unix__) || defined(__APPLE__)
#define LIBSUPLA_ARCH LIBSUPLA_ARCH_UNIX
#elif defined(_WIN32)
#define LIBSUPLA_ARCH LIBSUPLA_ARCH_WIN32
#elif defined(ICACHE_FLASH) || defined(ICACHE_RAM_ATTR)
#define LIBSUPLA_ARCH LIBSUPLA_ARCH_ESP8266
#elif defined(ESP_PLATFORM)
#define LIBSUPLA_ARCH LIBSUPLA_ARCH_ESP32
#endif
#if !defined(LIBSUPLA_ARCH)
#error "LIBSUPLA_ARCH is not specified. Set -D LIBSUPLA_ARCH=..."
#endif
#endif  // !defined(LIBSUPLA_ARCH)


#if LIBSUPLA_ARCH == LIBSUPLA_ARCH_ESP8266

#ifndef SUPLA_DEVICE
#define SUPLA_DEVICE
#endif

#elif LIBSUPLA_ARCH == LIBSUPLA_ARCH_ESP32

#ifndef SUPLA_DEVICE
#define SUPLA_DEVICE
#endif

#endif


#endif /* LIBSUPLA_CONFIG_H_ */
