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
#include <winpr/sysinfo.h>
#include <winpr/cmdline.h>

#include <freerdp/addin.h>

#include "rdpei_common.h"

#include "rdpei_main.h"

/**
 * Touch Input
 * http://msdn.microsoft.com/en-us/library/windows/desktop/dd562197/
 *
 * Windows Touch Input
 * http://msdn.microsoft.com/en-us/library/windows/desktop/dd317321/
 *
 * Input: Touch injection sample
 * http://code.msdn.microsoft.com/windowsdesktop/Touch-Injection-Sample-444d9bf7
 *
 * Pointer Input Message Reference
 * http://msdn.microsoft.com/en-us/library/hh454916/
 *
 * POINTER_INFO Structure
 * http://msdn.microsoft.com/en-us/library/hh454907/
 *
 * POINTER_TOUCH_INFO Structure
 * http://msdn.microsoft.com/en-us/library/hh454910/
 */

#define MAX_CONTACTS	512

struct _RDPEI_CHANNEL_CALLBACK
{
	IWTSVirtualChannelCallback iface;

	IWTSPlugin* plugin;
	IWTSVirtualChannelManager* channel_mgr;
	IWTSVirtualChannel* channel;
};
typedef struct _RDPEI_CHANNEL_CALLBACK RDPEI_CHANNEL_CALLBACK;

struct _RDPEI_LISTENER_CALLBACK
{
	IWTSListenerCallback iface;

	IWTSPlugin* plugin;
	IWTSVirtualChannelManager* channel_mgr;
	RDPEI_CHANNEL_CALLBACK* channel_callback;
};
typedef struct _RDPEI_LISTENER_CALLBACK RDPEI_LISTENER_CALLBACK;

struct _RDPEI_PLUGIN
{
	IWTSPlugin iface;

	IWTSListener* listener;
	RDPEI_LISTENER_CALLBACK* listener_callback;

	int version;
	UINT16 maxTouchContacts;
	UINT64 currentFrameTime;
	UINT64 previousFrameTime;
	RDPINPUT_TOUCH_FRAME frame;
	RDPINPUT_CONTACT_DATA contacts[MAX_CONTACTS];
	RDPINPUT_CONTACT_POINT* contactPoints;
};
typedef struct _RDPEI_PLUGIN RDPEI_PLUGIN;

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

	status = callback->channel->Write(callback->channel, Stream_Length(s), Stream_Buffer(s), NULL);

#ifdef WITH_DEBUG_RDPEI
	printf("rdpei_send_pdu: eventId: %d (%s) length: %d status: %d\n",
			eventId, RDPEI_EVENTID_STRINGS[eventId], pduLength, status);
#endif

	return status;
}

int rdpei_send_cs_ready_pdu(RDPEI_CHANNEL_CALLBACK* callback)
{
	int status;
	wStream* s;
	UINT32 flags;
	UINT32 pduLength;
	RDPEI_PLUGIN* rdpei = (RDPEI_PLUGIN*) callback->plugin;

	flags = 0;
	flags |= READY_FLAGS_SHOW_TOUCH_VISUALS;
	//flags |= READY_FLAGS_DISABLE_TIMESTAMP_INJECTION;

	pduLength = RDPINPUT_HEADER_LENGTH + 10;
	s = Stream_New(NULL, pduLength);
	Stream_Seek(s, RDPINPUT_HEADER_LENGTH);

	Stream_Write_UINT32(s, flags); /* flags (4 bytes) */
	Stream_Write_UINT32(s, RDPINPUT_PROTOCOL_V1); /* protocolVersion (4 bytes) */
	Stream_Write_UINT16(s, rdpei->maxTouchContacts); /* maxTouchContacts (2 bytes) */

	Stream_SealLength(s);

	status = rdpei_send_pdu(callback, s, EVENTID_CS_READY, pduLength);
	Stream_Free(s, TRUE);

	return status;
}

void rdpei_print_contact_flags(UINT32 contactFlags)
{
	if (contactFlags & CONTACT_FLAG_DOWN)
		printf(" CONTACT_FLAG_DOWN");
	if (contactFlags & CONTACT_FLAG_UPDATE)
		printf(" CONTACT_FLAG_UPDATE");
	if (contactFlags & CONTACT_FLAG_UP)
		printf(" CONTACT_FLAG_UP");
	if (contactFlags & CONTACT_FLAG_INRANGE)
		printf(" CONTACT_FLAG_INRANGE");
	if (contactFlags & CONTACT_FLAG_INCONTACT)
		printf(" CONTACT_FLAG_INCONTACT");
	if (contactFlags & CONTACT_FLAG_CANCELED)
		printf(" CONTACT_FLAG_CANCELED");
}

int rdpei_write_touch_frame(wStream* s, RDPINPUT_TOUCH_FRAME* frame)
{
	int index;
	RDPINPUT_CONTACT_DATA* contact;

#ifdef WITH_DEBUG_RDPEI
	printf("contactCount: %d\n", frame->contactCount);
	printf("frameOffset: 0x%08X\n", (UINT32) frame->frameOffset);
#endif

	rdpei_write_2byte_unsigned(s, frame->contactCount); /* contactCount (TWO_BYTE_UNSIGNED_INTEGER) */
	rdpei_write_8byte_unsigned(s, frame->frameOffset); /* frameOffset (EIGHT_BYTE_UNSIGNED_INTEGER) */

	Stream_EnsureRemainingCapacity(s, frame->contactCount * 32);

	for (index = 0; index < frame->contactCount; index++)
	{
		contact = &frame->contacts[index];

#ifdef WITH_DEBUG_RDPEI
		printf("contact[%d].contactId: %d\n", index, contact->contactId);
		printf("contact[%d].fieldsPresent: %d\n", index, contact->fieldsPresent);
		printf("contact[%d].x: %d\n", index, contact->x);
		printf("contact[%d].y: %d\n", index, contact->y);
		printf("contact[%d].contactFlags: 0x%04X", index, contact->contactFlags);
		rdpei_print_contact_flags(contact->contactFlags);
		printf("\n");
#endif

		Stream_Write_UINT8(s, contact->contactId); /* contactId (1 byte) */

		/* fieldsPresent (TWO_BYTE_UNSIGNED_INTEGER) */
		rdpei_write_2byte_unsigned(s, contact->fieldsPresent);

		rdpei_write_4byte_signed(s, contact->x); /* x (FOUR_BYTE_SIGNED_INTEGER) */
		rdpei_write_4byte_signed(s, contact->y); /* y (FOUR_BYTE_SIGNED_INTEGER) */

		/* contactFlags (FOUR_BYTE_UNSIGNED_INTEGER) */
		rdpei_write_4byte_unsigned(s, contact->contactFlags);

		if (contact->fieldsPresent & CONTACT_DATA_CONTACTRECT_PRESENT)
		{
			/* contactRectLeft (TWO_BYTE_SIGNED_INTEGER) */
			rdpei_write_2byte_signed(s, contact->contactRectLeft);
			/* contactRectTop (TWO_BYTE_SIGNED_INTEGER) */
			rdpei_write_2byte_signed(s, contact->contactRectTop);
			/* contactRectRight (TWO_BYTE_SIGNED_INTEGER) */
			rdpei_write_2byte_signed(s, contact->contactRectRight);
			/* contactRectBottom (TWO_BYTE_SIGNED_INTEGER) */
			rdpei_write_2byte_signed(s, contact->contactRectBottom);
		}

		if (contact->fieldsPresent & CONTACT_DATA_ORIENTATION_PRESENT)
		{
			/* orientation (FOUR_BYTE_UNSIGNED_INTEGER) */
			rdpei_write_4byte_unsigned(s, contact->orientation);
		}

		if (contact->fieldsPresent & CONTACT_DATA_PRESSURE_PRESENT)
		{
			/* pressure (FOUR_BYTE_UNSIGNED_INTEGER) */
			rdpei_write_4byte_unsigned(s, contact->pressure);
		}
	}

	return 0;
}

int rdpei_send_touch_event_pdu(RDPEI_CHANNEL_CALLBACK* callback, RDPINPUT_TOUCH_FRAME* frame)
{
	int status;
	wStream* s;
	UINT32 pduLength;

	pduLength = 64 + (frame->contactCount * 32);

	s = Stream_New(NULL, pduLength);
	Stream_Seek(s, RDPINPUT_HEADER_LENGTH);

	rdpei_write_4byte_unsigned(s, frame->frameOffset); /* FOUR_BYTE_UNSIGNED_INTEGER */
	rdpei_write_2byte_unsigned(s, 1); /* TWO_BYTE_UNSIGNED_INTEGER */

	rdpei_write_touch_frame(s, frame);

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

#ifdef WITH_DEBUG_RDPEI
	printf("rdpei_recv_pdu: eventId: %d (%s) length: %d\n",
			eventId, RDPEI_EVENTID_STRINGS[eventId], pduLength);
#endif

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
	listener_callback->channel_callback = callback;

	*ppCallback = (IWTSVirtualChannelCallback*) callback;

	return 0;
}

static int rdpei_plugin_initialize(IWTSPlugin* pPlugin, IWTSVirtualChannelManager* pChannelMgr)
{
	int status;
	RDPEI_PLUGIN* rdpei = (RDPEI_PLUGIN*) pPlugin;

	DEBUG_DVC("");

	rdpei->listener_callback = (RDPEI_LISTENER_CALLBACK*) malloc(sizeof(RDPEI_LISTENER_CALLBACK));
	ZeroMemory(rdpei->listener_callback, sizeof(RDPEI_LISTENER_CALLBACK));

	rdpei->listener_callback->iface.OnNewChannelConnection = rdpei_on_new_channel_connection;
	rdpei->listener_callback->plugin = pPlugin;
	rdpei->listener_callback->channel_mgr = pChannelMgr;

	status = pChannelMgr->CreateListener(pChannelMgr, RDPEI_DVC_CHANNEL_NAME, 0,
		(IWTSListenerCallback*) rdpei->listener_callback, &(rdpei->listener));

	rdpei->listener->pInterface = rdpei->iface.pInterface;

	return status;
}

static int rdpei_plugin_terminated(IWTSPlugin* pPlugin)
{
	RDPEI_PLUGIN* rdpei = (RDPEI_PLUGIN*) pPlugin;

	DEBUG_DVC("");

	free(rdpei);

	return 0;
}

/**
 * Channel Client Interface
 */

int rdpei_get_version(RdpeiClientContext* context)
{
	RDPEI_PLUGIN* rdpei = (RDPEI_PLUGIN*) context->handle;
	return rdpei->version;
}

int rdpei_send_frame(RdpeiClientContext* context)
{
	UINT64 currentTime;
	RDPEI_PLUGIN* rdpei = (RDPEI_PLUGIN*) context->handle;
	RDPEI_CHANNEL_CALLBACK* callback = rdpei->listener_callback->channel_callback;

	currentTime = GetTickCount64();

	if (!rdpei->previousFrameTime && !rdpei->currentFrameTime)
	{
		rdpei->currentFrameTime = currentTime;
		rdpei->frame.frameOffset = 0;
	}
	else
	{
		rdpei->currentFrameTime = currentTime;
		rdpei->frame.frameOffset = rdpei->currentFrameTime - rdpei->previousFrameTime;
	}

	rdpei_send_touch_event_pdu(callback, &rdpei->frame);
	rdpei->previousFrameTime = rdpei->currentFrameTime;
	rdpei->frame.contactCount = 0;

	return 1;
}

int rdpei_add_contact(RdpeiClientContext* context, RDPINPUT_CONTACT_DATA* contact)
{
	RDPEI_PLUGIN* rdpei = (RDPEI_PLUGIN*) context->handle;

	if (rdpei->frame.contactCount < MAX_CONTACTS)
	{
		CopyMemory(&(rdpei->contacts[rdpei->frame.contactCount]), contact, sizeof(RDPINPUT_CONTACT_DATA));
		rdpei->frame.contactCount++;
	}

	rdpei_send_frame(context);

	return 1;
}

int rdpei_contact_begin(RdpeiClientContext* context, int externalId)
{
	int i;
	int contactId = -1;
	RDPEI_PLUGIN* rdpei = (RDPEI_PLUGIN*) context->handle;

	/* Create a new contact point in an empty slot */

	for (i = 0; i < rdpei->maxTouchContacts; i++)
	{
		if (!rdpei->contactPoints[i].flags)
		{
			rdpei->contactPoints[i].flags = 1;
			rdpei->contactPoints[i].contactId = i;

			if (!rdpei->contactPoints[i].externalId)
			{
				rdpei->contactPoints[i].externalId = externalId;
				contactId = rdpei->contactPoints[i].contactId;
				break;
			}
		}
	}

	return contactId;
}

int rdpei_contact_update(RdpeiClientContext* context, int externalId)
{
	int i;
	int contactId = -1;
	RDPEI_PLUGIN* rdpei = (RDPEI_PLUGIN*) context->handle;

	for (i = 0; i < rdpei->maxTouchContacts; i++)
	{
		if (!rdpei->contactPoints[i].flags)
			continue;

		if (rdpei->contactPoints[i].externalId == externalId)
		{
			contactId = rdpei->contactPoints[i].contactId;
			break;
		}
	}

	return contactId;
}

int rdpei_contact_end(RdpeiClientContext* context, int externalId)
{
	int i;
	int contactId = -1;
	RDPEI_PLUGIN* rdpei = (RDPEI_PLUGIN*) context->handle;

	for (i = 0; i < rdpei->maxTouchContacts; i++)
	{
		if (!rdpei->contactPoints[i].flags)
			continue;

		if (rdpei->contactPoints[i].externalId == externalId)
		{
			contactId = rdpei->contactPoints[i].contactId;
			rdpei->contactPoints[i].externalId = 0;
			rdpei->contactPoints[i].flags = 0;
			rdpei->contactPoints[i].contactId = 0;
			break;
		}
	}

	return contactId;
}

#ifdef STATIC_CHANNELS
#define DVCPluginEntry		rdpei_DVCPluginEntry
#endif

int DVCPluginEntry(IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	int error = 0;
	RDPEI_PLUGIN* rdpei;
	RdpeiClientContext* context;

	rdpei = (RDPEI_PLUGIN*) pEntryPoints->GetPlugin(pEntryPoints, "rdpei");

	if (rdpei == NULL)
	{
		size_t size;

		rdpei = (RDPEI_PLUGIN*) malloc(sizeof(RDPEI_PLUGIN));
		ZeroMemory(rdpei, sizeof(RDPEI_PLUGIN));

		rdpei->iface.Initialize = rdpei_plugin_initialize;
		rdpei->iface.Connected = NULL;
		rdpei->iface.Disconnected = NULL;
		rdpei->iface.Terminated = rdpei_plugin_terminated;

		rdpei->version = 1;
		rdpei->currentFrameTime = 0;
		rdpei->previousFrameTime = 0;
		rdpei->frame.contacts = (RDPINPUT_CONTACT_DATA*) rdpei->contacts;

		rdpei->maxTouchContacts = 10;
		size = rdpei->maxTouchContacts * sizeof(RDPINPUT_CONTACT_POINT);
		rdpei->contactPoints = (RDPINPUT_CONTACT_POINT*) malloc(size);
		ZeroMemory(rdpei->contactPoints, size);

		context = (RdpeiClientContext*) malloc(sizeof(RdpeiClientContext));

		context->handle = (void*) rdpei;
		context->GetVersion = rdpei_get_version;
		context->AddContact = rdpei_add_contact;

		context->ContactBegin = rdpei_contact_begin;
		context->ContactUpdate = rdpei_contact_update;
		context->ContactEnd = rdpei_contact_end;

		rdpei->iface.pInterface = (void*) context;

		error = pEntryPoints->RegisterPlugin(pEntryPoints, "rdpei", (IWTSPlugin*) rdpei);
	}

	return error;
}
