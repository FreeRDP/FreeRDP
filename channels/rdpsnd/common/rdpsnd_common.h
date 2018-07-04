/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Server Audio Virtual Channel
 *
 * Copyright 2018 Armin Novak <armin.novak@thincast.com>
 * Copyright 2018 Thincast Technologies GmbH
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FREERDP_CHANNEL_RDPSND_COMMON_MAIN_H
#define FREERDP_CHANNEL_RDPSND_COMMON_MAIN_H

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>

#include <freerdp/codec/dsp.h>
#include <freerdp/channels/wtsvc.h>
#include <freerdp/channels/log.h>
#include <freerdp/server/rdpsnd.h>

typedef enum
{
	CHANNEL_VERSION_WIN_XP = 0x02,
	CHANNEL_VERSION_WIN_XP_SP1 = 0x05,
	CHANNEL_VERSION_WIN_VISTA = 0x05,
	CHANNEL_VERSION_WIN_7 = 0x06,
	CHANNEL_VERSION_WIN_8 = 0x08,
	CHANNEL_VERSION_WIN_MAX = CHANNEL_VERSION_WIN_8
} RdpSndChannelVersion;

#endif /* FREERDP_CHANNEL_RDPSND_COMMON_MAIN_H */
