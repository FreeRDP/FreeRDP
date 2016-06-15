/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Input Virtual Channel Extension
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/stream.h>
#include <winpr/sysinfo.h>
#include <winpr/cmdline.h>
#include <winpr/collections.h>

#include <freerdp/addin.h>
#include <freerdp/freerdp.h>

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

	RdpeiClientContext* context;

	int version;
	UINT16 maxTouchContacts;
	UINT64 currentFrameTime;
	UINT64 previousFrameTime;
	RDPINPUT_TOUCH_FRAME frame;
	RDPINPUT_CONTACT_DATA contacts[MAX_CONTACTS];
	RDPINPUT_CONTACT_POINT* contactPoints;

	HANDLE event;
	HANDLE stopEvent;
	HANDLE thread;

	CRITICAL_SECTION lock;
	rdpContext* rdpcontext;
};
typedef struct _RDPEI_PLUGIN RDPEI_PLUGIN;

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpei_send_frame(RdpeiClientContext* context);

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

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpei_add_frame(RdpeiClientContext* context)
{
	int i;
	RDPINPUT_CONTACT_DATA* contact;
	RDPEI_PLUGIN* rdpei = (RDPEI_PLUGIN*) context->handle;

	rdpei->frame.contactCount = 0;

	for (i = 0; i < rdpei->maxTouchContacts; i++)
	{
		contact = (RDPINPUT_CONTACT_DATA*) &(rdpei->contactPoints[i].data);

		if (rdpei->contactPoints[i].dirty)
		{
			CopyMemory(&(rdpei->contacts[rdpei->frame.contactCount]), contact, sizeof(RDPINPUT_CONTACT_DATA));
			rdpei->contactPoints[i].dirty = FALSE;
			rdpei->frame.contactCount++;
		}
		else if (rdpei->contactPoints[i].active)
		{
			if (contact->contactFlags & CONTACT_FLAG_DOWN)
			{
				contact->contactFlags = CONTACT_FLAG_UPDATE;
				contact->contactFlags |= CONTACT_FLAG_INRANGE;
				contact->contactFlags |= CONTACT_FLAG_INCONTACT;
			}

			CopyMemory(&(rdpei->contacts[rdpei->frame.contactCount]), contact, sizeof(RDPINPUT_CONTACT_DATA));
			rdpei->frame.contactCount++;
		}
	}

	return CHANNEL_RC_OK;
}

static void* rdpei_schedule_thread(void* arg)
{
	DWORD status;
	RDPEI_PLUGIN* rdpei = (RDPEI_PLUGIN*) arg;
	RdpeiClientContext* context = (RdpeiClientContext*) rdpei->iface.pInterface;
	HANDLE hdl[] = {rdpei->event, rdpei->stopEvent};
	UINT error = CHANNEL_RC_OK;

	if (!rdpei)
	{
		error = ERROR_INVALID_PARAMETER;
		goto out;
	}


	if (!context)
	{
		error = ERROR_INVALID_PARAMETER;
		goto out;
	}

	while (1)
	{
		status = WaitForMultipleObjects(2, hdl, FALSE, 20);

        if (status == WAIT_FAILED)
        {
            error = GetLastError();
            WLog_ERR(TAG, "WaitForMultipleObjects failed with error %lu!", error);
            break;
        }

		if (status == WAIT_OBJECT_0 + 1)
			break;

		EnterCriticalSection(&rdpei->lock);

		if ((error = rdpei_add_frame(context)))
		{
			WLog_ERR(TAG, "rdpei_add_frame failed with error %lu!", error);
			break;
		}

		if (rdpei->frame.contactCount > 0)
		{
			if ((error = rdpei_send_frame(context)))
			{
				WLog_ERR(TAG, "rdpei_send_frame failed with error %lu!", error);
				break;
			}
		}

		if (status == WAIT_OBJECT_0)
			ResetEvent(rdpei->event);

		LeaveCriticalSection(&rdpei->lock);
	}

out:
	if (error && rdpei->rdpcontext)
		setChannelError(rdpei->rdpcontext, error, "rdpei_schedule_thread reported an error");

	ExitThread(0);

	return NULL;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpei_send_pdu(RDPEI_CHANNEL_CALLBACK* callback, wStream* s, UINT16 eventId, UINT32 pduLength)
{
	UINT status;

	Stream_SetPosition(s, 0);
	Stream_Write_UINT16(s, eventId); /* eventId (2 bytes) */
	Stream_Write_UINT32(s, pduLength); /* pduLength (4 bytes) */
	Stream_SetPosition(s, Stream_Length(s));

	status = callback->channel->Write(callback->channel, (UINT32) Stream_Length(s), Stream_Buffer(s), NULL);

#ifdef WITH_DEBUG_RDPEI
	WLog_DBG(TAG,  "rdpei_send_pdu: eventId: %d (%s) length: %d status: %d",
			 eventId, RDPEI_EVENTID_STRINGS[eventId], pduLength, status);
#endif

	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpei_send_cs_ready_pdu(RDPEI_CHANNEL_CALLBACK* callback)
{
	UINT status;
	wStream* s;
	UINT32 flags;
	UINT32 pduLength;
	RDPEI_PLUGIN* rdpei = (RDPEI_PLUGIN*) callback->plugin;

	flags = 0;
	flags |= READY_FLAGS_SHOW_TOUCH_VISUALS;
	//flags |= READY_FLAGS_DISABLE_TIMESTAMP_INJECTION;

	pduLength = RDPINPUT_HEADER_LENGTH + 10;
	s = Stream_New(NULL, pduLength);
	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}
	Stream_Seek(s, RDPINPUT_HEADER_LENGTH);

	Stream_Write_UINT32(s, flags); /* flags (4 bytes) */
	Stream_Write_UINT32(s, RDPINPUT_PROTOCOL_V10); /* protocolVersion (4 bytes) */
	Stream_Write_UINT16(s, rdpei->maxTouchContacts); /* maxTouchContacts (2 bytes) */

	Stream_SealLength(s);

	status = rdpei_send_pdu(callback, s, EVENTID_CS_READY, pduLength);
	Stream_Free(s, TRUE);

	return status;
}

void rdpei_print_contact_flags(UINT32 contactFlags)
{
	if (contactFlags & CONTACT_FLAG_DOWN)
		WLog_DBG(TAG, " CONTACT_FLAG_DOWN");

	if (contactFlags & CONTACT_FLAG_UPDATE)
		WLog_DBG(TAG, " CONTACT_FLAG_UPDATE");

	if (contactFlags & CONTACT_FLAG_UP)
		WLog_DBG(TAG, " CONTACT_FLAG_UP");

	if (contactFlags & CONTACT_FLAG_INRANGE)
		WLog_DBG(TAG, " CONTACT_FLAG_INRANGE");

	if (contactFlags & CONTACT_FLAG_INCONTACT)
		WLog_DBG(TAG, " CONTACT_FLAG_INCONTACT");

	if (contactFlags & CONTACT_FLAG_CANCELED)
		WLog_DBG(TAG, " CONTACT_FLAG_CANCELED");
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpei_write_touch_frame(wStream* s, RDPINPUT_TOUCH_FRAME* frame)
{
	UINT32 index;
	int rectSize = 2;
	RDPINPUT_CONTACT_DATA* contact;

#ifdef WITH_DEBUG_RDPEI
	WLog_DBG(TAG, "contactCount: %d", frame->contactCount);
	WLog_DBG(TAG, "frameOffset: 0x%08X", (UINT32) frame->frameOffset);
#endif

	rdpei_write_2byte_unsigned(s, frame->contactCount); /* contactCount (TWO_BYTE_UNSIGNED_INTEGER) */

	/**
	 * the time offset from the previous frame (in microseconds).
	 * If this is the first frame being transmitted then this field MUST be set to zero.
	 */
	rdpei_write_8byte_unsigned(s, frame->frameOffset * 1000); /* frameOffset (EIGHT_BYTE_UNSIGNED_INTEGER) */

	if (!Stream_EnsureRemainingCapacity(s, (size_t) frame->contactCount * 64))
	{
		WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	for (index = 0; index < frame->contactCount; index++)
	{
		contact = &frame->contacts[index];

		contact->fieldsPresent |= CONTACT_DATA_CONTACTRECT_PRESENT;
		contact->contactRectLeft = contact->x - rectSize;
		contact->contactRectTop = contact->y - rectSize;
		contact->contactRectRight = contact->x + rectSize;
		contact->contactRectBottom = contact->y + rectSize;

#ifdef WITH_DEBUG_RDPEI
		WLog_DBG(TAG, "contact[%d].contactId: %d", index, contact->contactId);
		WLog_DBG(TAG, "contact[%d].fieldsPresent: %d", index, contact->fieldsPresent);
		WLog_DBG(TAG, "contact[%d].x: %d", index, contact->x);
		WLog_DBG(TAG, "contact[%d].y: %d", index, contact->y);
		WLog_DBG(TAG, "contact[%d].contactFlags: 0x%04X", index, contact->contactFlags);
		rdpei_print_contact_flags(contact->contactFlags);
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

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpei_send_touch_event_pdu(RDPEI_CHANNEL_CALLBACK* callback, RDPINPUT_TOUCH_FRAME* frame)
{
	UINT status;
	wStream* s;
	UINT32 pduLength;

	pduLength = 64 + (frame->contactCount * 64);

	s = Stream_New(NULL, pduLength);
	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}
	Stream_Seek(s, RDPINPUT_HEADER_LENGTH);

	/**
	 * the time that has elapsed (in milliseconds) from when the oldest touch frame
	 * was generated to when it was encoded for transmission by the client.
	 */
	rdpei_write_4byte_unsigned(s, (UINT32) frame->frameOffset); /* encodeTime (FOUR_BYTE_UNSIGNED_INTEGER) */

	rdpei_write_2byte_unsigned(s, 1); /* (frameCount) TWO_BYTE_UNSIGNED_INTEGER */

	if ((status = rdpei_write_touch_frame(s, frame)))
	{
		WLog_ERR(TAG, "rdpei_write_touch_frame failed with error %lu!", status);
		Stream_Free(s, TRUE);
		return status;
	}

	Stream_SealLength(s);
	pduLength = Stream_Length(s);

	status = rdpei_send_pdu(callback, s, EVENTID_TOUCH, pduLength);
	Stream_Free(s, TRUE);

	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpei_recv_sc_ready_pdu(RDPEI_CHANNEL_CALLBACK* callback, wStream* s)
{
	UINT32 protocolVersion;

	Stream_Read_UINT32(s, protocolVersion); /* protocolVersion (4 bytes) */

#if 0
	if (protocolVersion != RDPINPUT_PROTOCOL_V10)
	{
		WLog_ERR(TAG,  "Unknown [MS-RDPEI] protocolVersion: 0x%08X", protocolVersion);
		return -1;
	}
#endif

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpei_recv_suspend_touch_pdu(RDPEI_CHANNEL_CALLBACK* callback, wStream* s)
{
	RdpeiClientContext* rdpei = (RdpeiClientContext*) callback->plugin->pInterface;
	UINT error = CHANNEL_RC_OK;

	IFCALLRET(rdpei->SuspendTouch, error, rdpei);
	if (error)
		WLog_ERR(TAG, "rdpei->SuspendTouch failed with error %lu!", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpei_recv_resume_touch_pdu(RDPEI_CHANNEL_CALLBACK* callback, wStream* s)
{
	RdpeiClientContext* rdpei = (RdpeiClientContext*) callback->plugin->pInterface;
	UINT error = CHANNEL_RC_OK;

	IFCALLRET(rdpei->ResumeTouch, error, rdpei);
	if (error)
		WLog_ERR(TAG, "rdpei->ResumeTouch failed with error %lu!", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpei_recv_pdu(RDPEI_CHANNEL_CALLBACK* callback, wStream* s)
{
	UINT16 eventId;
	UINT32 pduLength;
	UINT error;

	Stream_Read_UINT16(s, eventId); /* eventId (2 bytes) */
	Stream_Read_UINT32(s, pduLength); /* pduLength (4 bytes) */

#ifdef WITH_DEBUG_RDPEI
	WLog_DBG(TAG,  "rdpei_recv_pdu: eventId: %d (%s) length: %d",
			 eventId, RDPEI_EVENTID_STRINGS[eventId], pduLength);
#endif

	switch (eventId)
	{
		case EVENTID_SC_READY:
			if ((error = rdpei_recv_sc_ready_pdu(callback, s)))
			{
				WLog_ERR(TAG, "rdpei_recv_sc_ready_pdu failed with error %lu!", error);
				return error;
			}
			if ((error = rdpei_send_cs_ready_pdu(callback)))
			{
				WLog_ERR(TAG, "rdpei_send_cs_ready_pdu failed with error %lu!", error);
				return error;
			}
			break;

		case EVENTID_SUSPEND_TOUCH:
			if ((error = rdpei_recv_suspend_touch_pdu(callback, s)))
			{
				WLog_ERR(TAG, "rdpei_recv_suspend_touch_pdu failed with error %lu!", error);
				return error;
			}
			break;

		case EVENTID_RESUME_TOUCH:
			if ((error = rdpei_recv_resume_touch_pdu(callback, s)))
			{
				WLog_ERR(TAG, "rdpei_recv_resume_touch_pdu failed with error %lu!", error);
				return error;
			}
			break;

		default:
			break;
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpei_on_data_received(IWTSVirtualChannelCallback* pChannelCallback, wStream *data)
{
	RDPEI_CHANNEL_CALLBACK* callback = (RDPEI_CHANNEL_CALLBACK*) pChannelCallback;

	return  rdpei_recv_pdu(callback, data);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpei_on_close(IWTSVirtualChannelCallback* pChannelCallback)
{
	RDPEI_CHANNEL_CALLBACK* callback = (RDPEI_CHANNEL_CALLBACK*) pChannelCallback;

	free(callback);

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpei_on_new_channel_connection(IWTSListenerCallback* pListenerCallback,
	IWTSVirtualChannel* pChannel, BYTE* Data, BOOL* pbAccept,
	IWTSVirtualChannelCallback** ppCallback)
{
	RDPEI_CHANNEL_CALLBACK* callback;
	RDPEI_LISTENER_CALLBACK* listener_callback = (RDPEI_LISTENER_CALLBACK*) pListenerCallback;

	callback = (RDPEI_CHANNEL_CALLBACK*) calloc(1, sizeof(RDPEI_CHANNEL_CALLBACK));
	if (!callback)
	{
		WLog_ERR(TAG,"calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	callback->iface.OnDataReceived = rdpei_on_data_received;
	callback->iface.OnClose = rdpei_on_close;
	callback->plugin = listener_callback->plugin;
	callback->channel_mgr = listener_callback->channel_mgr;
	callback->channel = pChannel;
	listener_callback->channel_callback = callback;

	*ppCallback = (IWTSVirtualChannelCallback*) callback;

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpei_plugin_initialize(IWTSPlugin* pPlugin, IWTSVirtualChannelManager* pChannelMgr)
{
	UINT error;
	RDPEI_PLUGIN* rdpei = (RDPEI_PLUGIN*) pPlugin;

	rdpei->listener_callback = (RDPEI_LISTENER_CALLBACK*) calloc(1 ,sizeof(RDPEI_LISTENER_CALLBACK));
	if (!rdpei->listener_callback)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	rdpei->listener_callback->iface.OnNewChannelConnection = rdpei_on_new_channel_connection;
	rdpei->listener_callback->plugin = pPlugin;
	rdpei->listener_callback->channel_mgr = pChannelMgr;

	if ((error = pChannelMgr->CreateListener(pChannelMgr, RDPEI_DVC_CHANNEL_NAME, 0,
		(IWTSListenerCallback*) rdpei->listener_callback, &(rdpei->listener))))
	{
		WLog_ERR(TAG, "ChannelMgr->CreateListener failed with error %lu!", error);
		goto error_out;
	}

	rdpei->listener->pInterface = rdpei->iface.pInterface;

	InitializeCriticalSection(&rdpei->lock);
	if (!(rdpei->event = CreateEvent(NULL, TRUE, FALSE, NULL)))
	{
		WLog_ERR(TAG, "CreateEvent failed!");
		goto error_out;

	}

	if (!(rdpei->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
	{
		WLog_ERR(TAG, "CreateEvent failed!");
		goto error_out;

	}

	if (!(rdpei->thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)
			rdpei_schedule_thread, (void*) rdpei, 0, NULL)))
	{
		WLog_ERR(TAG, "CreateThread failed!");
		goto error_out;

	}

	return error;
error_out:
	CloseHandle(rdpei->stopEvent);
	CloseHandle(rdpei->event);
	free(rdpei->listener_callback);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpei_plugin_terminated(IWTSPlugin* pPlugin)
{
	RDPEI_PLUGIN* rdpei = (RDPEI_PLUGIN*) pPlugin;
    UINT error;

	if (!pPlugin)
		return ERROR_INVALID_PARAMETER;

	SetEvent(rdpei->stopEvent);
	EnterCriticalSection(&rdpei->lock);

	if (WaitForSingleObject(rdpei->thread, INFINITE) == WAIT_FAILED)
    {
        error = GetLastError();
        WLog_ERR(TAG, "WaitForSingleObject failed with error %lu!", error);
        return error;
    }

	CloseHandle(rdpei->stopEvent);
	CloseHandle(rdpei->event);
	CloseHandle(rdpei->thread);

	DeleteCriticalSection(&rdpei->lock);

	free(rdpei->listener_callback);
	free(rdpei->context);
	free(rdpei);

	return CHANNEL_RC_OK;
}

/**
 * Channel Client Interface
 */

int rdpei_get_version(RdpeiClientContext* context)
{
	RDPEI_PLUGIN* rdpei = (RDPEI_PLUGIN*) context->handle;
	return rdpei->version;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpei_send_frame(RdpeiClientContext* context)
{
	UINT64 currentTime;
	RDPEI_PLUGIN* rdpei = (RDPEI_PLUGIN*) context->handle;
	RDPEI_CHANNEL_CALLBACK* callback = rdpei->listener_callback->channel_callback;
	UINT error;

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

	if ((error = rdpei_send_touch_event_pdu(callback, &rdpei->frame)))
	{
		WLog_ERR(TAG, "rdpei_send_touch_event_pdu failed with error %lu!", error);
		return error;
	}
	rdpei->previousFrameTime = rdpei->currentFrameTime;
	rdpei->frame.contactCount = 0;

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpei_add_contact(RdpeiClientContext* context, RDPINPUT_CONTACT_DATA* contact)
{
	RDPINPUT_CONTACT_POINT* contactPoint;
	RDPEI_PLUGIN* rdpei = (RDPEI_PLUGIN*) context->handle;

	EnterCriticalSection(&rdpei->lock);

	contactPoint = (RDPINPUT_CONTACT_POINT*) &rdpei->contactPoints[contact->contactId];
	CopyMemory(&(contactPoint->data), contact, sizeof(RDPINPUT_CONTACT_DATA));
	contactPoint->dirty = TRUE;

	SetEvent(rdpei->event);

	LeaveCriticalSection(&rdpei->lock);

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpei_touch_begin(RdpeiClientContext* context, int externalId, int x, int y, int* contactId)
{
	unsigned int i;
	int contactIdlocal = -1;
	RDPINPUT_CONTACT_DATA contact;
	RDPINPUT_CONTACT_POINT* contactPoint = NULL;
	RDPEI_PLUGIN* rdpei = (RDPEI_PLUGIN*) context->handle;
	UINT  error = CHANNEL_RC_OK;

	/* Create a new contact point in an empty slot */

	for (i = 0; i < rdpei->maxTouchContacts; i++)
	{
		contactPoint = (RDPINPUT_CONTACT_POINT*) &rdpei->contactPoints[i];

		if (!contactPoint->active)
		{
			contactPoint->contactId = i;
			contactIdlocal = contactPoint->contactId;
			contactPoint->externalId = externalId;
			contactPoint->active = TRUE;
			contactPoint->state = RDPINPUT_CONTACT_STATE_ENGAGED;
			break;
		}
	}

	if (contactIdlocal >= 0)
	{
		ZeroMemory(&contact, sizeof(RDPINPUT_CONTACT_DATA));

		contactPoint->lastX = x;
		contactPoint->lastY = y;

		contact.x = x;
		contact.y = y;
		contact.contactId = (UINT32) contactIdlocal;

		contact.contactFlags |= CONTACT_FLAG_DOWN;
		contact.contactFlags |= CONTACT_FLAG_INRANGE;
		contact.contactFlags |= CONTACT_FLAG_INCONTACT;

		error = context->AddContact(context, &contact);
	}
	*contactId = contactIdlocal;
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpei_touch_update(RdpeiClientContext* context, int externalId, int x, int y, int* contactId)
{
	unsigned int i;
	int contactIdlocal = -1;
	RDPINPUT_CONTACT_DATA contact;
	RDPINPUT_CONTACT_POINT* contactPoint = NULL;
	RDPEI_PLUGIN* rdpei = (RDPEI_PLUGIN*) context->handle;
	UINT error = CHANNEL_RC_OK;

	for (i = 0; i < rdpei->maxTouchContacts; i++)
	{
		contactPoint = (RDPINPUT_CONTACT_POINT*) &rdpei->contactPoints[i];

		if (!contactPoint->active)
			continue;

		if (contactPoint->externalId == externalId)
		{
			contactIdlocal = contactPoint->contactId;
			break;
		}
	}

	if (contactIdlocal >= 0)
	{
		ZeroMemory(&contact, sizeof(RDPINPUT_CONTACT_DATA));

		contactPoint->lastX = x;
		contactPoint->lastY = y;

		contact.x = x;
		contact.y = y;
		contact.contactId = (UINT32) contactIdlocal;

		contact.contactFlags |= CONTACT_FLAG_UPDATE;
		contact.contactFlags |= CONTACT_FLAG_INRANGE;
		contact.contactFlags |= CONTACT_FLAG_INCONTACT;

		error = context->AddContact(context, &contact);
	}

	*contactId = contactIdlocal;

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpei_touch_end(RdpeiClientContext* context, int externalId, int x, int y, int* contactId)
{
	unsigned int i;
	int contactIdlocal = -1;
	int tempvalue;
	RDPINPUT_CONTACT_DATA contact;
	RDPINPUT_CONTACT_POINT* contactPoint = NULL;
	RDPEI_PLUGIN* rdpei = (RDPEI_PLUGIN*) context->handle;
	UINT error;

	for (i = 0; i < rdpei->maxTouchContacts; i++)
	{
		contactPoint = (RDPINPUT_CONTACT_POINT*) &rdpei->contactPoints[i];

		if (!contactPoint->active)
			continue;

		if (contactPoint->externalId == externalId)
		{
			contactIdlocal = contactPoint->contactId;
			break;
		}
	}

	if (contactIdlocal >= 0)
	{
		ZeroMemory(&contact, sizeof(RDPINPUT_CONTACT_DATA));

		if ((contactPoint->lastX != x) && (contactPoint->lastY != y))
		{
			if ((error = context->TouchUpdate(context, externalId, x, y, &tempvalue)))
			{
				WLog_ERR(TAG, "context->TouchUpdate failed with error %lu!", error);
				return error;
			}
		}

		contact.x = x;
		contact.y = y;
		contact.contactId = (UINT32) contactIdlocal;

		contact.contactFlags |= CONTACT_FLAG_UP;

		if ((error = context->AddContact(context, &contact)))
		{
			WLog_ERR(TAG, "context->AddContact failed with error %lu!", error);
			return error;
		}

		contactPoint->externalId = 0;
		contactPoint->active = FALSE;
		contactPoint->flags = 0;
		contactPoint->contactId = 0;
		contactPoint->state = RDPINPUT_CONTACT_STATE_OUT_OF_RANGE;
	}
	*contactId = contactIdlocal;

	return CHANNEL_RC_OK;
}

#ifdef BUILTIN_CHANNELS
#define DVCPluginEntry		rdpei_DVCPluginEntry
#else
#define DVCPluginEntry		FREERDP_API DVCPluginEntry
#endif

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT DVCPluginEntry(IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	UINT error;
	RDPEI_PLUGIN* rdpei = NULL;
	RdpeiClientContext* context = NULL;

	rdpei = (RDPEI_PLUGIN*) pEntryPoints->GetPlugin(pEntryPoints, "rdpei");

	if (!rdpei)
	{
		size_t size;

		rdpei = (RDPEI_PLUGIN*) calloc(1, sizeof(RDPEI_PLUGIN));
		if(!rdpei)
		{
			WLog_ERR(TAG, "calloc failed!");
			return CHANNEL_RC_NO_MEMORY;
		}

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
		rdpei->contactPoints = (RDPINPUT_CONTACT_POINT*) calloc(1, size);

		rdpei->rdpcontext = ((freerdp*)((rdpSettings*) pEntryPoints->GetRdpSettings(pEntryPoints))->instance)->context;


		if (!rdpei->contactPoints)
		{
			WLog_ERR(TAG, "calloc failed!");
			error = CHANNEL_RC_NO_MEMORY;
			goto error_out;
		}

		context = (RdpeiClientContext*) calloc(1, sizeof(RdpeiClientContext));
		if (!context)
		{
			WLog_ERR(TAG, "calloc failed!");
			error = CHANNEL_RC_NO_MEMORY;
			goto error_out;
		}

		context->handle = (void*) rdpei;
		context->GetVersion = rdpei_get_version;
		context->AddContact = rdpei_add_contact;

		context->TouchBegin = rdpei_touch_begin;
		context->TouchUpdate = rdpei_touch_update;
		context->TouchEnd = rdpei_touch_end;

		rdpei->iface.pInterface = (void*) context;

		if ((error = pEntryPoints->RegisterPlugin(pEntryPoints, "rdpei", (IWTSPlugin*) rdpei)))
		{
			WLog_ERR(TAG, "EntryPoints->RegisterPlugin failed with error %lu!", error);
			error = CHANNEL_RC_NO_MEMORY;
			goto error_out;
		}

		rdpei->context = context;
	}

	return CHANNEL_RC_OK;
error_out:
	free(context);
	free(rdpei->contactPoints);
	free(rdpei);
	return error;
}
