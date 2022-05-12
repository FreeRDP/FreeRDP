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

#ifndef FREERDP_LIB_CORE_TIMEZONE_H
#define FREERDP_LIB_CORE_TIMEZONE_H

#include "rdp.h"

#include <freerdp/config.h>

#include <freerdp/log.h>
#include <freerdp/freerdp.h>
#include <freerdp/api.h>

#include <winpr/stream.h>

FREERDP_LOCAL BOOL rdp_read_client_time_zone(wStream* s, rdpSettings* settings);
FREERDP_LOCAL BOOL rdp_write_client_time_zone(wStream* s, rdpSettings* settings);

#define TIMEZONE_TAG FREERDP_TAG("core.timezone")
#ifdef WITH_DEBUG_TIMEZONE
#define DEBUG_TIMEZONE(...) WLog_DBG(TIMEZONE_TAG, __VA_ARGS__)
#else
#define DEBUG_TIMEZONE(...) \
	do                      \
	{                       \
	} while (0)
#endif

#endif /* FREERDP_LIB_CORE_TIMEZONE_H */
