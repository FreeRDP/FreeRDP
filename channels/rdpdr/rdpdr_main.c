/**
 * FreeRDP: A Remote Desktop Protocol client.
 * File System Virtual Channel
 *
 * Copyright 2010-2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freerdp/constants.h>
#include <freerdp/types.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/svc_plugin.h>

#include "rdpdr_types.h"
#include "rdpdr_constants.h"
#include "devman.h"

typedef struct rdpdr_plugin rdpdrPlugin;
struct rdpdr_plugin
{
	rdpSvcPlugin plugin;

	DEVMAN* devman;

	uint16 versionMinor;
	uint16 clientID;
	const char* computerName;
};

static void rdpdr_process_connect(rdpSvcPlugin* plugin)
{
	rdpdrPlugin* rdpdr = (rdpdrPlugin*)plugin;
	FRDP_PLUGIN_DATA* data;

	rdpdr->devman = devman_new(plugin);
	data = (FRDP_PLUGIN_DATA*)plugin->channel_entry_points.pExtendedData;
	while (data && data->size > 0)
	{
		if (strcmp((char*)data->data[0], "clientname") == 0)
		{
			rdpdr->computerName = (const char*)data->data[1];
			DEBUG_SVC("computerName %s", rdpdr->computerName);
		}
		else
		{
			devman_load_device_service(rdpdr->devman, data);
		}
		data = (FRDP_PLUGIN_DATA*)(((void*)data) + data->size);
	}
}

static void rdpdr_process_receive(rdpSvcPlugin* plugin, STREAM* data_in)
{
	uint16 component;
	uint16 packetID;
	uint32 deviceID;
	uint32 status;

	stream_read_uint16(data_in, component);
	stream_read_uint16(data_in, packetID);

	if (component == RDPDR_CTYP_CORE)
	{
		switch (packetID)
		{
			case PAKID_CORE_SERVER_ANNOUNCE:
				DEBUG_SVC("RDPDR_CTYP_CORE / PAKID_CORE_SERVER_ANNOUNCE");
				break;

			case PAKID_CORE_SERVER_CAPABILITY:
				DEBUG_SVC("RDPDR_CTYP_CORE / PAKID_CORE_SERVER_CAPABILITY");
				break;

			case PAKID_CORE_CLIENTID_CONFIRM:
				DEBUG_SVC("RDPDR_CTYP_CORE / PAKID_CORE_CLIENTID_CONFIRM");
				break;

			case PAKID_CORE_USER_LOGGEDON:
				DEBUG_SVC("RDPDR_CTYP_CORE / PAKID_CORE_USER_LOGGEDON");
				break;

			case PAKID_CORE_DEVICE_REPLY:
				/* connect to a specific resource */
				stream_get_uint32(data_in, deviceID);
				stream_get_uint32(data_in, status);
				DEBUG_SVC("RDPDR_CTYP_CORE / PAKID_CORE_DEVICE_REPLY (deviceID=%d status=%d)", deviceID, status);
				break;

			case PAKID_CORE_DEVICE_IOREQUEST:
				DEBUG_SVC("RDPDR_CTYP_CORE / PAKID_CORE_DEVICE_IOREQUEST");
				break;

			default:
				DEBUG_WARN("RDPDR_CTYP_CORE / unknown packetID: 0x%02X", packetID);
				break;

		}
	}
	else if (component == RDPDR_CTYP_PRN)
	{
		DEBUG_SVC("RDPDR_CTYP_PRN");
	}
	else
	{
		DEBUG_WARN("RDPDR component: 0x%02X packetID: 0x%02X\n", component, packetID);
	}

	stream_free(data_in);
}

static void rdpdr_process_event(rdpSvcPlugin* plugin, FRDP_EVENT* event)
{
	freerdp_event_free(event);
}

static void rdpdr_process_terminate(rdpSvcPlugin* plugin)
{
	rdpdrPlugin* rdpdr = (rdpdrPlugin*)plugin;

	devman_free(rdpdr->devman);
	xfree(plugin);
}

DEFINE_SVC_PLUGIN(rdpdr, "rdpdr",
	CHANNEL_OPTION_INITIALIZED | CHANNEL_OPTION_ENCRYPT_RDP |
	CHANNEL_OPTION_COMPRESS_RDP)
