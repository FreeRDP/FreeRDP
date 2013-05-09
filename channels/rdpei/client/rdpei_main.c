/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Input Virtual Channel Extension
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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
#include <winpr/stream.h>
#include <winpr/cmdline.h>

#include <freerdp/addin.h>

#include "rdpei_common.h"

#include "rdpei_main.h"

typedef struct _RDPEI_LISTENER_CALLBACK RDPEI_LISTENER_CALLBACK;
struct _RDPEI_LISTENER_CALLBACK
{
	IWTSListenerCallback iface;

	IWTSPlugin* plugin;
	IWTSVirtualChannelManager* channel_mgr;
};

typedef struct _RDPEI_CHANNEL_CALLBACK RDPEI_CHANNEL_CALLBACK;
struct _RDPEI_CHANNEL_CALLBACK
{
	IWTSVirtualChannelCallback iface;

	IWTSPlugin* plugin;
	IWTSVirtualChannelManager* channel_mgr;
	IWTSVirtualChannel* channel;
};

typedef struct _RDPEI_PLUGIN RDPEI_PLUGIN;
struct _RDPEI_PLUGIN
{
	IWTSPlugin iface;

	RDPEI_LISTENER_CALLBACK* listener_callback;
};

const char* RDPEI_EVENTID_STRINGS[] =
{
	"",
	"EVENTID_SC_READY",
	"EVENTID_CS_READY",
	"EVENTID_TOUCH",
	"EVENTID_SUSPEND_TOUCH",
	"EVENTID_RESUME_TOUCH",
	"EVENTID_DISMISS_HOVERING_CONTACT"
};

int rdpei_send_pdu(RDPEI_CHANNEL_CALLBACK* callback, wStream* s, UINT16 eventId, UINT32 pduLength)
{
	int status;

	Stream_SetPosition(s, 0);
	Stream_Write_UINT16(s, eventId); /* eventId (2 bytes) */
	Stream_Write_UINT32(s, pduLength); /* pduLength (4 bytes) */
	Stream_SetPosition(s, Stream_Length(s));

	printf("rdpei_send_pdu: eventId: %d (%s) length: %d\n",
			eventId, RDPEI_EVENTID_STRINGS[eventId], pduLength);

	status = callback->channel->Write(callback->channel,
			Stream_Length(s), Stream_Buffer(s), NULL);

	return status;
}

int rdpei_send_cs_ready_pdu(RDPEI_CHANNEL_CALLBACK* callback)
{
	int status;
	wStream* s;
	UINT32 flags;
	UINT32 pduLength;
	UINT16 maxTouchContacts;

	flags = 0;
	flags |= READY_FLAGS_SHOW_TOUCH_VISUALS;
	//flags |= READY_FLAGS_DISABLE_TIMESTAMP_INJECTION;

	maxTouchContacts = 10;

	pduLength = RDPINPUT_HEADER_LENGTH + 10;
	s = Stream_New(NULL, pduLength);
	Stream_Seek(s, RDPINPUT_HEADER_LENGTH);

	Stream_Write_UINT32(s, flags); /* flags (4 bytes) */
	Stream_Write_UINT32(s, RDPINPUT_PROTOCOL_V1); /* protocolVersion (4 bytes) */
	Stream_Write_UINT16(s, maxTouchContacts); /* maxTouchContacts (2 bytes) */

	Stream_SealLength(s);

	status = rdpei_send_pdu(callback, s, EVENTID_CS_READY, pduLength);
	Stream_Free(s, TRUE);

	return status;
}

int rdpei_write_touch_frame(wStream* s, RDPINPUT_TOUCH_FRAME* frame)
{
	int index;
	RDPINPUT_CONTACT_DATA* contact;

	rdpei_write_2byte_unsigned(s, frame->contactCount);
	rdpei_write_8byte_unsigned(s, frame->frameOffset);

	Stream_EnsureRemainingCapacity(s, frame->contactCount * 32);

	for (index = 0; index < frame->contactCount; index++)
	{
		contact = &frame->contacts[index];

		Stream_Write_UINT8(s, contact->contactId);
		rdpei_write_2byte_unsigned(s, contact->fieldsPresent);
		rdpei_write_4byte_signed(s, contact->x);
		rdpei_write_4byte_signed(s, contact->y);
		rdpei_write_4byte_unsigned(s, contact->contactFlags);

		if (contact->fieldsPresent & CONTACT_DATA_CONTACTRECT_PRESENT)
		{
			rdpei_write_2byte_signed(s, contact->contactRectLeft);
			rdpei_write_2byte_signed(s, contact->contactRectTop);
			rdpei_write_2byte_signed(s, contact->contactRectRight);
			rdpei_write_2byte_signed(s, contact->contactRectBottom);
		}

		if (contact->fieldsPresent & CONTACT_DATA_ORIENTATION_PRESENT)
		{
			rdpei_write_4byte_unsigned(s, contact->orientation);
		}

		if (contact->fieldsPresent & CONTACT_DATA_PRESSURE_PRESENT)
		{
			rdpei_write_4byte_unsigned(s, contact->pressure);
		}
	}

	return 0;
}

int rdpei_send_touch_event_pdu(RDPEI_CHANNEL_CALLBACK* callback)
{
	int status;
	wStream* s;
	UINT32 pduLength;
	UINT32 encodeTime;
	UINT16 frameCount;
	RDPINPUT_TOUCH_FRAME frame;
	RDPINPUT_CONTACT_DATA contact;

	encodeTime = 123;
	frameCount = 1;

	frame.contactCount = 1;
	frame.contacts = &contact;

	s = Stream_New(NULL, 512);
	Stream_Seek(s, RDPINPUT_HEADER_LENGTH);

	rdpei_write_4byte_unsigned(s, encodeTime);
	rdpei_write_2byte_unsigned(s, frameCount);

	rdpei_write_touch_frame(s, &frame);

	Stream_SealLength(s);
	pduLength = Stream_Length(s);

	status = rdpei_send_pdu(callback, s, EVENTID_TOUCH, pduLength);
	Stream_Free(s, TRUE);

	return status;
}

int rdpei_recv_sc_ready_pdu(RDPEI_CHANNEL_CALLBACK* callback, wStream* s)
{
	UINT32 protocolVersion;

	Stream_Read_UINT32(s, protocolVersion); /* protocolVersion (4 bytes) */

	if (protocolVersion != RDPINPUT_PROTOCOL_V1)
	{
		printf("Unknown [MS-RDPEI] protocolVersion: 0x%08X\n", protocolVersion);
		return -1;
	}

	return 0;
}

int rdpei_recv_suspend_touch_pdu(RDPEI_CHANNEL_CALLBACK* callback, wStream* s)
{
	return 0;
}

int rdpei_recv_resume_touch_pdu(RDPEI_CHANNEL_CALLBACK* callback, wStream* s)
{
	return 0;
}

int rdpei_recv_pdu(RDPEI_CHANNEL_CALLBACK* callback, wStream* s)
{
	UINT16 eventId;
	UINT32 pduLength;

	Stream_Read_UINT16(s, eventId); /* eventId (2 bytes) */
	Stream_Read_UINT32(s, pduLength); /* pduLength (4 bytes) */

	printf("rdpei_recv_pdu: eventId: %d (%s) length: %d\n",
			eventId, RDPEI_EVENTID_STRINGS[eventId], pduLength);

	switch (eventId)
	{
		case EVENTID_SC_READY:
			rdpei_recv_sc_ready_pdu(callback, s);
			rdpei_send_cs_ready_pdu(callback);
			break;

		case EVENTID_SUSPEND_TOUCH:
			rdpei_recv_suspend_touch_pdu(callback, s);
			break;

		case EVENTID_RESUME_TOUCH:
			rdpei_recv_resume_touch_pdu(callback, s);
			break;

		default:
			break;
	}

	return 0;
}

static int rdpei_on_data_received(IWTSVirtualChannelCallback* pChannelCallback, UINT32 cbSize, BYTE* pBuffer)
{
	wStream* s;
	int status = 0;
	RDPEI_CHANNEL_CALLBACK* callback = (RDPEI_CHANNEL_CALLBACK*) pChannelCallback;

	s = Stream_New(pBuffer, cbSize);

	status = rdpei_recv_pdu(callback, s);

	Stream_Free(s, FALSE);

	return status;
}

static int rdpei_on_close(IWTSVirtualChannelCallback* pChannelCallback)
{
	RDPEI_CHANNEL_CALLBACK* callback = (RDPEI_CHANNEL_CALLBACK*) pChannelCallback;

	DEBUG_DVC("");

	free(callback);

	return 0;
}

static int rdpei_on_new_channel_connection(IWTSListenerCallback* pListenerCallback,
	IWTSVirtualChannel* pChannel, BYTE* Data, int* pbAccept,
	IWTSVirtualChannelCallback** ppCallback)
{
	RDPEI_CHANNEL_CALLBACK* callback;
	RDPEI_LISTENER_CALLBACK* listener_callback = (RDPEI_LISTENER_CALLBACK*) pListenerCallback;

	DEBUG_DVC("");

	callback = (RDPEI_CHANNEL_CALLBACK*) malloc(sizeof(RDPEI_CHANNEL_CALLBACK));
	ZeroMemory(callback, sizeof(RDPEI_CHANNEL_CALLBACK));

	callback->iface.OnDataReceived = rdpei_on_data_received;
	callback->iface.OnClose = rdpei_on_close;
	callback->plugin = listener_callback->plugin;
	callback->channel_mgr = listener_callback->channel_mgr;
	callback->channel = pChannel;

	*ppCallback = (IWTSVirtualChannelCallback*) callback;

	return 0;
}

static int rdpei_plugin_initialize(IWTSPlugin* pPlugin, IWTSVirtualChannelManager* pChannelMgr)
{
	RDPEI_PLUGIN* echo = (RDPEI_PLUGIN*) pPlugin;

	DEBUG_DVC("");

	echo->listener_callback = (RDPEI_LISTENER_CALLBACK*) malloc(sizeof(RDPEI_LISTENER_CALLBACK));
	ZeroMemory(echo->listener_callback, sizeof(RDPEI_LISTENER_CALLBACK));

	echo->listener_callback->iface.OnNewChannelConnection = rdpei_on_new_channel_connection;
	echo->listener_callback->plugin = pPlugin;
	echo->listener_callback->channel_mgr = pChannelMgr;

	return pChannelMgr->CreateListener(pChannelMgr, "Microsoft::Windows::RDS::Input", 0,
		(IWTSListenerCallback*) echo->listener_callback, NULL);
}

static int rdpei_plugin_terminated(IWTSPlugin* pPlugin)
{
	RDPEI_PLUGIN* echo = (RDPEI_PLUGIN*) pPlugin;

	DEBUG_DVC("");

	free(echo);

	return 0;
}

#ifdef STATIC_CHANNELS
#define DVCPluginEntry		rdpei_DVCPluginEntry
#endif

int DVCPluginEntry(IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	int error = 0;
	RDPEI_PLUGIN* rdpei;

	rdpei = (RDPEI_PLUGIN*) pEntryPoints->GetPlugin(pEntryPoints, "rdpei");

	if (rdpei == NULL)
	{
		rdpei = (RDPEI_PLUGIN*) malloc(sizeof(RDPEI_PLUGIN));
		ZeroMemory(rdpei, sizeof(RDPEI_PLUGIN));

		rdpei->iface.Initialize = rdpei_plugin_initialize;
		rdpei->iface.Connected = NULL;
		rdpei->iface.Disconnected = NULL;
		rdpei->iface.Terminated = rdpei_plugin_terminated;

		error = pEntryPoints->RegisterPlugin(pEntryPoints, "rdpei", (IWTSPlugin*) rdpei);
	}

	return error;
}
