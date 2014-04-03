/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Static Virtual Channel Interface
 *
 * Copyright 2009-2011 Jay Sorg
 * Copyright 2010-2011 Vic Lee
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

#ifndef FREERDP_UTILS_SVC_PLUGIN_H
#define FREERDP_UTILS_SVC_PLUGIN_H

/* static channel plugin base implementation */

#include <freerdp/api.h>
#include <freerdp/svc.h>
#include <freerdp/addin.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/stream.h>
#include <winpr/collections.h>

#include <freerdp/svc.h>
#include <freerdp/utils/debug.h>
#include <freerdp/utils/event.h>

typedef struct rdp_svc_plugin rdpSvcPlugin;

struct rdp_svc_plugin
{
	CHANNEL_ENTRY_POINTS_FREERDP channel_entry_points;
	CHANNEL_DEF channel_def;

	void (*connect_callback)(rdpSvcPlugin* plugin);
	void (*receive_callback)(rdpSvcPlugin* plugin, wStream* data_in);
	void (*event_callback)(rdpSvcPlugin* plugin, wMessage* event);
	void (*terminate_callback)(rdpSvcPlugin* plugin);

	HANDLE thread;
	HANDLE started;
	wStream* data_in;
	void* InitHandle;
	DWORD OpenHandle;
	wMessagePipe* MsgPipe;
};

#ifdef __cplusplus
extern "C" {
#endif

FREERDP_API void svc_plugin_init(rdpSvcPlugin* plugin, CHANNEL_ENTRY_POINTS* pEntryPoints);
FREERDP_API int svc_plugin_send(rdpSvcPlugin* plugin, wStream* data_out);
FREERDP_API int svc_plugin_send_event(rdpSvcPlugin* plugin, wMessage* event);

#ifdef __cplusplus
}
#endif

#ifdef WITH_DEBUG_SVC
#define DEBUG_SVC(fmt, ...) DEBUG_CLASS(SVC, fmt, ## __VA_ARGS__)
#else
#define DEBUG_SVC(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

#endif /* FREERDP_UTILS_SVC_PLUGIN_H */
