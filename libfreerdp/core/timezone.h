/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Time Zone Redirection
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __TIMEZONE_H
#define __TIMEZONE_H

#include "rdp.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <freerdp/freerdp.h>
#include <freerdp/locale/timezone.h>

#include <winpr/stream.h>

void rdp_read_system_time(wStream* s, SYSTEM_TIME* system_time);
void rdp_write_system_time(wStream* s, SYSTEM_TIME* system_time);
void rdp_get_client_time_zone(wStream* s, rdpSettings* settings);
BOOL rdp_read_client_time_zone(wStream* s, rdpSettings* settings);
void rdp_write_client_time_zone(wStream* s, rdpSettings* settings);

#ifdef WITH_DEBUG_TIMEZONE
#define DEBUG_TIMEZONE(fmt, ...) DEBUG_CLASS(TIMEZONE, fmt, ## __VA_ARGS__)
#else
#define DEBUG_TIMEZONE(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

#endif /* __TIMEZONE_H */
