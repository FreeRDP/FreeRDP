/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Device Redirection Virtual Channel
 *
 * Copyright 2010-2011 Vic Lee
 * Copyright 2010-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>

#include <freerdp/types.h>
#include <freerdp/constants.h>
#include <winpr/stream.h>
#include <freerdp/channels/rdpdr.h>
#include <freerdp/utils/svc_plugin.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "rdpdr_capabilities.h"

#include "devman.h"
#include "irp.h"

#include "rdpdr_main.h"

static void rdpdr_process_connect(rdpSvcPlugin* plugin)
{
	int index;
	RDPDR_DEVICE* device;
	rdpSettings* settings;
	rdpdrPlugin* rdpdr = (rdpdrPlugin*) plugin;

	rdpdr->devman = devman_new(plugin);
	settings = (rdpSettings*) plugin->channel_entry_points.pExtendedData;

	strncpy(rdpdr->computerName, settings->ComputerName, sizeof(rdpdr->computerName) - 1);

	for (index = 0; index < settings->DeviceCount; index++)
	{
		device = settings->DeviceArray[index];
		devman_load_device_service(rdpdr->devman, device);
	}
}

static void rdpdr_process_server_announce_request(rdpdrPlugin* rdpdr, wStream* data_in)
{
	stream_read_UINT16(data_in, rdpdr->versionMajor);
	stream_read_UINT16(data_in, rdpdr->versionMinor);
	stream_read_UINT32(data_in, rdpdr->clientID);

	DEBUG_SVC("version %d.%d clientID %d", rdpdr->versionMajor, rdpdr->versionMinor, rdpdr->clientID);
}

static void rdpdr_send_client_announce_reply(rdpdrPlugin* rdpdr)
{
	wStream* data_out;

	data_out = stream_new(12);

	stream_write_UINT16(data_out, RDPDR_CTYP_CORE);
	stream_write_UINT16(data_out, PAKID_CORE_CLIENTID_CONFIRM);

	stream_write_UINT16(data_out, rdpdr->versionMajor);
	stream_write_UINT16(data_out, rdpdr->versionMinor);
	stream_write_UINT32(data_out, (UINT32) rdpdr->clientID);

	svc_plugin_send((rdpSvcPlugin*) rdpdr, data_out);
}

static void rdpdr_send_client_name_request(rdpdrPlugin* rdpdr)
{
	wStream* data_out;
	WCHAR* computerNameW = NULL;
	size_t computerNameLenW;

	if (!rdpdr->computerName[0])
		gethostname(rdpdr->computerName, sizeof(rdpdr->computerName) - 1);

	computerNameLenW = ConvertToUnicode(CP_UTF8, 0, rdpdr->computerName, -1, &computerNameW, 0) * 2;

	data_out = stream_new(16 + computerNameLenW + 2);

	stream_write_UINT16(data_out, RDPDR_CTYP_CORE);
	stream_write_UINT16(data_out, PAKID_CORE_CLIENT_NAME);

	stream_write_UINT32(data_out, 1); /* unicodeFlag, 0 for ASCII and 1 for Unicode */
	stream_write_UINT32(data_out, 0); /* codePage, must be set to zero */
	stream_write_UINT32(data_out, computerNameLenW + 2); /* computerNameLen, including null terminator */
	stream_write(data_out, computerNameW, computerNameLenW);
	stream_write_UINT16(data_out, 0); /* null terminator */

	free(computerNameW);

	svc_plugin_send((rdpSvcPlugin*) rdpdr, data_out);
}

static void rdpdr_process_server_clientid_confirm(rdpdrPlugin* rdpdr, wStream* data_in)
{
	UINT16 versionMajor;
	UINT16 versionMinor;
	UINT32 clientID;

	stream_read_UINT16(data_in, versionMajor);
	stream_read_UINT16(data_in, versionMinor);
	stream_read_UINT32(data_in, clientID);

	if (versionMajor != rdpdr->versionMajor || versionMinor != rdpdr->versionMinor)
	{
		DEBUG_WARN("unmatched version %d.%d", versionMajor, versionMinor);
		rdpdr->versionMajor = versionMajor;
		rdpdr->versionMinor = versionMinor;
	}

	if (clientID != rdpdr->clientID)
	{
		DEBUG_WARN("unmatched clientID %d", clientID);
		rdpdr->clientID = clientID;
	}
}

static void rdpdr_send_device_list_announce_request(rdpdrPlugin* rdpdr, BOOL user_loggedon)
{
	int i;
	int pos;
	BYTE c;
	UINT32 count;
	int data_len;
	int count_pos;
	wStream* data_out;
	DEVICE* device;
	LIST_ITEM* item;

	data_out = stream_new(256);

	stream_write_UINT16(data_out, RDPDR_CTYP_CORE);
	stream_write_UINT16(data_out, PAKID_CORE_DEVICELIST_ANNOUNCE);

	count_pos = stream_get_pos(data_out);
	count = 0;
	stream_seek_UINT32(data_out); /* deviceCount */

	for (item = rdpdr->devman->devices->head; item; item = item->next)
	{
		device = (DEVICE*) item->data;

		/**
		 * 1. versionMinor 0x0005 doesn't send PAKID_CORE_USER_LOGGEDON
		 *    so all devices should be sent regardless of user_loggedon
		 * 2. smartcard devices should be always sent
		 * 3. other devices are sent only after user_loggedon
		 */

		if ((rdpdr->versionMinor == 0x0005) ||
			(device->type == RDPDR_DTYP_SMARTCARD) || user_loggedon)
		{
			data_len = (device->data == NULL ? 0 : stream_get_length(device->data));
			stream_check_size(data_out, 20 + data_len);

			stream_write_UINT32(data_out, device->type); /* deviceType */
			stream_write_UINT32(data_out, device->id); /* deviceID */
			strncpy((char*) stream_get_tail(data_out), device->name, 8);

			for (i = 0; i < 8; i++)
			{
				stream_peek_BYTE(data_out, c);

				if (c > 0x7F)
					stream_write_BYTE(data_out, '_');
				else
					stream_seek_BYTE(data_out);
			}

			stream_write_UINT32(data_out, data_len);

			if (data_len > 0)
				stream_write(data_out, stream_get_data(device->data), data_len);

			count++;

			fprintf(stderr, "registered device #%d: %s (type=%d id=%d)\n",
				count, device->name, device->type, device->id);
		}
	}

	pos = stream_get_pos(data_out);
	stream_set_pos(data_out, count_pos);
	stream_write_UINT32(data_out, count);
	stream_set_pos(data_out, pos);
	stream_seal(data_out);

	svc_plugin_send((rdpSvcPlugin*) rdpdr, data_out);
}

static BOOL rdpdr_process_irp(rdpdrPlugin* rdpdr, wStream* data_in)
{
	IRP* irp;

	irp = irp_new(rdpdr->devman, data_in);

	if (irp == NULL)
		return FALSE;

	IFCALL(irp->device->IRPRequest, irp->device, irp);

	return TRUE;
}

static void rdpdr_process_receive(rdpSvcPlugin* plugin, wStream* data_in)
{
	UINT16 component;
	UINT16 packetID;
	UINT32 deviceID;
	UINT32 status;
	rdpdrPlugin* rdpdr = (rdpdrPlugin*) plugin;

	stream_read_UINT16(data_in, component);
	stream_read_UINT16(data_in, packetID);

	if (component == RDPDR_CTYP_CORE)
	{
		switch (packetID)
		{
			case PAKID_CORE_SERVER_ANNOUNCE:
				DEBUG_SVC("RDPDR_CTYP_CORE / PAKID_CORE_SERVER_ANNOUNCE");
				rdpdr_process_server_announce_request(rdpdr, data_in);
				rdpdr_send_client_announce_reply(rdpdr);
				rdpdr_send_client_name_request(rdpdr);
				break;

			case PAKID_CORE_SERVER_CAPABILITY:
				DEBUG_SVC("RDPDR_CTYP_CORE / PAKID_CORE_SERVER_CAPABILITY");
				rdpdr_process_capability_request(rdpdr, data_in);
				rdpdr_send_capability_response(rdpdr);
				break;

			case PAKID_CORE_CLIENTID_CONFIRM:
				DEBUG_SVC("RDPDR_CTYP_CORE / PAKID_CORE_CLIENTID_CONFIRM");
				rdpdr_process_server_clientid_confirm(rdpdr, data_in);
				rdpdr_send_device_list_announce_request(rdpdr, FALSE);
				break;

			case PAKID_CORE_USER_LOGGEDON:
				DEBUG_SVC("RDPDR_CTYP_CORE / PAKID_CORE_USER_LOGGEDON");
				rdpdr_send_device_list_announce_request(rdpdr, TRUE);
				break;

			case PAKID_CORE_DEVICE_REPLY:
				/* connect to a specific resource */
				stream_read_UINT32(data_in, deviceID);
				stream_read_UINT32(data_in, status);
				DEBUG_SVC("RDPDR_CTYP_CORE / PAKID_CORE_DEVICE_REPLY (deviceID=%d status=0x%08X)", deviceID, status);
				break;

			case PAKID_CORE_DEVICE_IOREQUEST:
				DEBUG_SVC("RDPDR_CTYP_CORE / PAKID_CORE_DEVICE_IOREQUEST");
				if (rdpdr_process_irp(rdpdr, data_in))
					data_in = NULL;
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
		DEBUG_WARN("RDPDR component: 0x%02X packetID: 0x%02X", component, packetID);
	}

	stream_free(data_in);
}

static void rdpdr_process_event(rdpSvcPlugin* plugin, wMessage* event)
{
	freerdp_event_free(event);
}

static void rdpdr_process_terminate(rdpSvcPlugin* plugin)
{
	rdpdrPlugin* rdpdr = (rdpdrPlugin*) plugin;

	devman_free(rdpdr->devman);
	free(plugin);
}

/* rdpdr is always built-in */
#define VirtualChannelEntry	rdpdr_VirtualChannelEntry

int VirtualChannelEntry(PCHANNEL_ENTRY_POINTS pEntryPoints)
{
	rdpdrPlugin* _p;

	_p = (rdpdrPlugin*) malloc(sizeof(rdpdrPlugin));
	ZeroMemory(_p, sizeof(rdpdrPlugin));

	_p->plugin.channel_def.options =
			CHANNEL_OPTION_INITIALIZED |
			CHANNEL_OPTION_ENCRYPT_RDP |
			CHANNEL_OPTION_COMPRESS_RDP;

	strcpy(_p->plugin.channel_def.name, "rdpdr");

	_p->plugin.connect_callback = rdpdr_process_connect;
	_p->plugin.receive_callback = rdpdr_process_receive;
	_p->plugin.event_callback = rdpdr_process_event;
	_p->plugin.terminate_callback = rdpdr_process_terminate;

	svc_plugin_init((rdpSvcPlugin*) _p, pEntryPoints);

	return 1;
}
