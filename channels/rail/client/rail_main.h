/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RAIL Virtual Channel Plugin
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2011 Roman Barabanov <romanbarabanov@gmail.com>
 * Copyright 2011 Vic Lee
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

#ifndef FREERDP_CHANNEL_CLIENT_RAIL_MAIN_H
#define FREERDP_CHANNEL_CLIENT_RAIL_MAIN_H

#include <freerdp/rail.h>
#include <freerdp/svc.h>
#include <freerdp/addin.h>
#include <freerdp/settings.h>
#include <freerdp/client/rail.h>

#include <winpr/crt.h>
#include <winpr/wlog.h>
#include <winpr/stream.h>

#include "../rail_common.h"

struct rail_plugin
{
	CHANNEL_DEF channelDef;
	CHANNEL_ENTRY_POINTS_FREERDP channelEntryPoints;

	RailClientContext* context;

	wLog* log;
	HANDLE thread;
	wStream* data_in;
	void* InitHandle;
	DWORD OpenHandle;
	wMessageQueue* queue;
};
typedef struct rail_plugin railPlugin;

RailClientContext* rail_get_client_interface(railPlugin* rail);
void rail_send_channel_data(railPlugin* rail, void* data, size_t length);

#endif /* FREERDP_CHANNEL_CLIENT_RAIL_MAIN_H */
