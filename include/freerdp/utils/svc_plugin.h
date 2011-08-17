/**
 * FreeRDP: A Remote Desktop Protocol client.
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

#ifndef __SVC_PLUGIN_UTILS_H
#define __SVC_PLUGIN_UTILS_H

/* static channel plugin base implementation */

#include <freerdp/api.h>
#include <freerdp/svc.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/event.h>
#include <freerdp/utils/debug.h>

typedef struct rdp_svc_plugin_private rdpSvcPluginPrivate;
typedef struct rdp_svc_plugin rdpSvcPlugin;

struct rdp_svc_plugin
{
	CHANNEL_ENTRY_POINTS_EX channel_entry_points;
	CHANNEL_DEF channel_def;

	int interval_ms;

	void (*connect_callback)(rdpSvcPlugin* plugin);
	void (*receive_callback)(rdpSvcPlugin* plugin, STREAM* data_in);
	void (*event_callback)(rdpSvcPlugin* plugin, RDP_EVENT* event);
	void (*interval_callback)(rdpSvcPlugin* plugin);
	void (*terminate_callback)(rdpSvcPlugin* plugin);

	rdpSvcPluginPrivate* priv;
};

FREERDP_API void svc_plugin_init(rdpSvcPlugin* plugin, CHANNEL_ENTRY_POINTS* pEntryPoints);
FREERDP_API int svc_plugin_send(rdpSvcPlugin* plugin, STREAM* data_out);
FREERDP_API int svc_plugin_send_event(rdpSvcPlugin* plugin, RDP_EVENT* event);

#define svc_plugin_get_data(_p) (RDP_PLUGIN_DATA*)(((rdpSvcPlugin*)_p)->channel_entry_points.pExtendedData)

#ifdef WITH_DEBUG_SVC
#define DEBUG_SVC(fmt, ...) DEBUG_CLASS(SVC, fmt, ## __VA_ARGS__)
#else
#define DEBUG_SVC(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

#define DEFINE_SVC_PLUGIN(_prefix, _name, _options) \
\
int VirtualChannelEntry(PCHANNEL_ENTRY_POINTS pEntryPoints) \
{ \
	_prefix##Plugin* _p; \
\
	_p = xnew(_prefix##Plugin); \
\
	_p->plugin.channel_def.options = _options; \
	strcpy(_p->plugin.channel_def.name, _name); \
\
	_p->plugin.connect_callback = _prefix##_process_connect; \
	_p->plugin.receive_callback = _prefix##_process_receive; \
	_p->plugin.event_callback = _prefix##_process_event; \
	_p->plugin.terminate_callback = _prefix##_process_terminate; \
\
	svc_plugin_init((rdpSvcPlugin*)_p, pEntryPoints); \
\
	return 1; \
}

#endif /* __SVC_PLUGIN_UTILS_H */
