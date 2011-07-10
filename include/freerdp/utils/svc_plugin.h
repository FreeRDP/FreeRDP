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

#include <freerdp/svc.h>
#include <freerdp/utils/stream.h>

typedef struct rdp_svc_plugin_private rdpSvcPluginPrivate;
typedef struct rdp_svc_plugin rdpSvcPlugin;
struct rdp_svc_plugin
{
	CHANNEL_ENTRY_POINTS channel_entry_points;
	CHANNEL_DEF channel_def;

	void (*connect_callback)(rdpSvcPlugin* plugin);
	void (*receive_callback)(rdpSvcPlugin* plugin, STREAM* data_in);
	void (*terminate_callback)(rdpSvcPlugin* plugin);

	rdpSvcPluginPrivate* priv;
};

void svc_plugin_init(rdpSvcPlugin* plugin);
int svc_plugin_send(rdpSvcPlugin* plugin, uint8* data, int size);

#endif /* __SVC_PLUGIN_UTILS_H */
