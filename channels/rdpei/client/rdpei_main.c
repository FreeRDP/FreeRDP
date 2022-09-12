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

#include <freerdp/config.h>

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
#include <freerdp/client/channels.h>

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

#define MAX_CONTACTS 64
#define MAX_PEN_CONTACTS 4

typedef struct
{
	GENERIC_DYNVC_PLUGIN base;

	RdpeiClientContext* context;

	UINT32 version;
	UINT32 features; /* SC_READY_MULTIPEN_INJECTION_SUPPORTED */
	UINT16 maxTouchContacts;
	UINT64 currentFrameTime;
	UINT64 previousFrameTime;
	RDPINPUT_CONTACT_POINT contactPoints[MAX_CONTACTS];

	UINT64 currentPenFrameTime;
	UINT64 previousPenFrameTime;
	UINT16 maxPenContacts;
	RDPINPUT_PEN_CONTACT_POINT penContactPoints[MAX_PEN_CONTACTS];

	CRITICAL_SECTION lock;
	rdpContext* rdpcontext;
	HANDLE thread;
	HANDLE event;
	BOOL running;
} RDPEI_PLUGIN;

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpei_send_frame(RdpeiClientContext* context, RDPINPUT_TOUCH_FRAME* frame);

#ifdef WITH_DEBUG_RDPEI
static const char* rdpei_eventid_string(UINT16 event)
{
	switch (event)
	{
		case EVENTID_SC_READY:
			return "EVENTID_SC_READY";
		case EVENTID_CS_READY:
			return "EVENTID_CS_READY";
		case EVENTID_TOUCH:
			return "EVENTID_TOUCH";
		case EVENTID_SUSPEND_TOUCH:
			return "EVENTID_SUSPEND_TOUCH";
		case EVENTID_RESUME_TOUCH:
			return "EVENTID_RESUME_TOUCH";
		case EVENTID_DISMISS_HOVERING_CONTACT:
			return "EVENTID_DISMISS_HOVERING_CONTACT";
		case EVENTID_PEN:
			return "EVENTID_PEN";
		default:
			return "EVENTID_UNKNOWN";
	}
}
#endif

static RDPINPUT_CONTACT_POINT* rdpei_contact(RDPEI_PLUGIN* rdpei, INT32 externalId, BOOL active)
{
	UINT16 i;

	for (i = 0; i < rdpei->maxTouchContacts; i++)
	{
		RDPINPUT_CONTACT_POINT* contactPoint = &rdpei->contactPoints[i];

		if (!contactPoint->active && active)
			continue;
		else if (!contactPoint->active && !active)
		{
			contactPoint->contactId = i;
			contactPoint->externalId = externalId;
			contactPoint->active = TRUE;
			return contactPoint;
		}
		else if (contactPoint->externalId == externalId)
		{
			return contactPoint;
		}
	}
	return NULL;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpei_add_frame(RdpeiClientContext* context)
{
	UINT16 i;
	RDPEI_PLUGIN* rdpei;
	RDPINPUT_TOUCH_FRAME frame = { 0 };
	RDPINPUT_CONTACT_DATA contacts[MAX_CONTACTS] = { 0 };

	if (!context || !context->handle)
		return ERROR_INTERNAL_ERROR;

	rdpei = (RDPEI_PLUGIN*)context->handle;
	frame.contacts = contacts;

	for (i = 0; i < rdpei->maxTouchContacts; i++)
	{
		RDPINPUT_CONTACT_POINT* contactPoint = &rdpei->contactPoints[i];
		RDPINPUT_CONTACT_DATA* contact = &contactPoint->data;

		if (contactPoint->dirty)
		{
			contacts[frame.contactCount] = *contact;
			rdpei->contactPoints[i].dirty = FALSE;
			frame.contactCount++;
		}
		else if (contactPoint->active)
		{
			if (contact->contactFlags & RDPINPUT_CONTACT_FLAG_DOWN)
			{
				contact->contactFlags = RDPINPUT_CONTACT_FLAG_UPDATE;
				contact->contactFlags |= RDPINPUT_CONTACT_FLAG_INRANGE;
				contact->contactFlags |= RDPINPUT_CONTACT_FLAG_INCONTACT;
			}

			contacts[frame.contactCount] = *contact;
			frame.contactCount++;
		}
		if (contact->contactFlags & RDPINPUT_CONTACT_FLAG_UP)
		{
			contactPoint->active = FALSE;
			contactPoint->externalId = 0;
			contactPoint->contactId = 0;
		}
	}

	if (frame.contactCount > 0)
	{
		UINT error = rdpei_send_frame(context, &frame);
		if (error != CHANNEL_RC_OK)
		{
			WLog_ERR(TAG, "rdpei_send_frame failed with error %" PRIu32 "!", error);
			return error;
		}
	}
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpei_send_pdu(GENERIC_CHANNEL_CALLBACK* callback, wStream* s, UINT16 eventId,
                           UINT32 pduLength)
{
	UINT status;

	if (!callback || !s || !callback->channel || !callback->channel->Write || !callback->plugin)
		return ERROR_INTERNAL_ERROR;

	Stream_SetPosition(s, 0);
	Stream_Write_UINT16(s, eventId);   /* eventId (2 bytes) */
	Stream_Write_UINT32(s, pduLength); /* pduLength (4 bytes) */
	Stream_SetPosition(s, Stream_Length(s));
	status = callback->channel->Write(callback->channel, (UINT32)Stream_Length(s), Stream_Buffer(s),
	                                  NULL);
#ifdef WITH_DEBUG_RDPEI
	WLog_DBG(TAG,
	         "rdpei_send_pdu: eventId: %" PRIu16 " (%s) length: %" PRIu32 " status: %" PRIu32 "",
	         eventId, rdpei_eventid_string(eventId), pduLength, status);
#endif
	return status;
}

static UINT rdpei_write_pen_frame(wStream* s, const RDPINPUT_PEN_FRAME* frame)
{
	UINT16 x;
	if (!s || !frame)
		return ERROR_INTERNAL_ERROR;

	if (!rdpei_write_2byte_unsigned(s, frame->contactCount))
		return ERROR_OUTOFMEMORY;
	if (!rdpei_write_8byte_unsigned(s, frame->frameOffset))
		return ERROR_OUTOFMEMORY;
	for (x = 0; x < frame->contactCount; x++)
	{
		const RDPINPUT_PEN_CONTACT* contact = &frame->contacts[x];

		if (!Stream_EnsureRemainingCapacity(s, 1))
			return ERROR_OUTOFMEMORY;
		Stream_Write_UINT8(s, contact->deviceId);
		if (!rdpei_write_2byte_unsigned(s, contact->fieldsPresent))
			return ERROR_OUTOFMEMORY;
		if (!rdpei_write_4byte_signed(s, contact->x))
			return ERROR_OUTOFMEMORY;
		if (!rdpei_write_4byte_signed(s, contact->y))
			return ERROR_OUTOFMEMORY;
		if (!rdpei_write_4byte_unsigned(s, contact->contactFlags))
			return ERROR_OUTOFMEMORY;
		if (contact->fieldsPresent & RDPINPUT_PEN_CONTACT_PENFLAGS_PRESENT)
		{
			if (!rdpei_write_4byte_unsigned(s, contact->penFlags))
				return ERROR_OUTOFMEMORY;
		}
		if (contact->fieldsPresent & RDPINPUT_PEN_CONTACT_PRESSURE_PRESENT)
		{
			if (!rdpei_write_4byte_unsigned(s, contact->pressure))
				return ERROR_OUTOFMEMORY;
		}
		if (contact->fieldsPresent & RDPINPUT_PEN_CONTACT_ROTATION_PRESENT)
		{
			if (!rdpei_write_2byte_unsigned(s, contact->rotation))
				return ERROR_OUTOFMEMORY;
		}
		if (contact->fieldsPresent & RDPINPUT_PEN_CONTACT_TILTX_PRESENT)
		{
			if (!rdpei_write_2byte_signed(s, contact->tiltX))
				return ERROR_OUTOFMEMORY;
		}
		if (contact->fieldsPresent & RDPINPUT_PEN_CONTACT_TILTY_PRESENT)
		{
			if (!rdpei_write_2byte_signed(s, contact->tiltY))
				return ERROR_OUTOFMEMORY;
		}
	}
	return CHANNEL_RC_OK;
}

static UINT rdpei_send_pen_event_pdu(GENERIC_CHANNEL_CALLBACK* callback, UINT32 frameOffset,
                                     const RDPINPUT_PEN_FRAME* frames, UINT16 count)
{
	UINT status;
	wStream* s;
	UINT16 x;

	if (!frames || (count == 0))
		return ERROR_INTERNAL_ERROR;

	s = Stream_New(NULL, 64);

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
	rdpei_write_4byte_unsigned(s, frameOffset); /* encodeTime (FOUR_BYTE_UNSIGNED_INTEGER) */
	rdpei_write_2byte_unsigned(s, count);       /* (frameCount) TWO_BYTE_UNSIGNED_INTEGER */

	for (x = 0; x < count; x++)
	{
		if ((status = rdpei_write_pen_frame(s, &frames[x])))
		{
			WLog_ERR(TAG, "rdpei_write_touch_frame failed with error %" PRIu32 "!", status);
			Stream_Free(s, TRUE);
			return status;
		}
	}
	Stream_SealLength(s);

	status = rdpei_send_pdu(callback, s, EVENTID_PEN, Stream_Length(s));
	Stream_Free(s, TRUE);
	return status;
}

static UINT rdpei_send_pen_frame(RdpeiClientContext* context, RDPINPUT_PEN_FRAME* frame)
{
	const UINT64 currentTime = GetTickCount64();
	RDPEI_PLUGIN* rdpei;
	GENERIC_CHANNEL_CALLBACK* callback;
	UINT error;

	if (!context)
		return ERROR_INTERNAL_ERROR;
	rdpei = (RDPEI_PLUGIN*)context->handle;
	if (!rdpei || !rdpei->base.listener_callback)
		return ERROR_INTERNAL_ERROR;
	if (!rdpei || !rdpei->rdpcontext)
		return ERROR_INTERNAL_ERROR;
	if (freerdp_settings_get_bool(rdpei->rdpcontext->settings, FreeRDP_SuspendInput))
		return CHANNEL_RC_OK;

	callback = rdpei->base.listener_callback->channel_callback;
	/* Just ignore the event if the channel is not connected */
	if (!callback)
		return CHANNEL_RC_OK;

	if (!rdpei->previousPenFrameTime && !rdpei->currentPenFrameTime)
	{
		rdpei->currentPenFrameTime = currentTime;
		frame->frameOffset = 0;
	}
	else
	{
		rdpei->currentPenFrameTime = currentTime;
		frame->frameOffset = rdpei->currentPenFrameTime - rdpei->previousPenFrameTime;
	}

	if ((error = rdpei_send_pen_event_pdu(callback, frame->frameOffset, frame, 1)))
		return error;

	rdpei->previousPenFrameTime = rdpei->currentPenFrameTime;
	return error;
}

static UINT rdpei_add_pen_frame(RdpeiClientContext* context)
{
	UINT16 i;
	RDPEI_PLUGIN* rdpei;
	RDPINPUT_PEN_FRAME penFrame = { 0 };
	RDPINPUT_PEN_CONTACT penContacts[MAX_PEN_CONTACTS] = { 0 };

	if (!context || !context->handle)
		return ERROR_INTERNAL_ERROR;

	rdpei = (RDPEI_PLUGIN*)context->handle;

	penFrame.contacts = penContacts;

	for (i = 0; i < rdpei->maxPenContacts; i++)
	{
		RDPINPUT_PEN_CONTACT_POINT* contact = &(rdpei->penContactPoints[i]);

		if (contact->dirty)
		{
			penContacts[penFrame.contactCount++] = contact->data;
			contact->dirty = FALSE;
		}
		else if (contact->active)
		{
			if (contact->data.contactFlags & RDPINPUT_CONTACT_FLAG_DOWN)
			{
				contact->data.contactFlags = RDPINPUT_CONTACT_FLAG_UPDATE;
				contact->data.contactFlags |= RDPINPUT_CONTACT_FLAG_INRANGE;
				contact->data.contactFlags |= RDPINPUT_CONTACT_FLAG_INCONTACT;
			}

			penContacts[penFrame.contactCount++] = contact->data;
		}
		if (contact->data.contactFlags & RDPINPUT_CONTACT_FLAG_UP)
		{
			contact->externalId = 0;
			contact->active = FALSE;
		}
	}

	if (penFrame.contactCount > 0)
		return rdpei_send_pen_frame(context, &penFrame);
	return CHANNEL_RC_OK;
}

static UINT rdpei_update(RdpeiClientContext* context)
{
	UINT error = rdpei_add_frame(context);
	if (error != CHANNEL_RC_OK)
	{
		WLog_ERR(TAG, "rdpei_add_frame failed with error %" PRIu32 "!", error);
		return error;
	}

	return rdpei_add_pen_frame(context);
}

static DWORD WINAPI rdpei_periodic_update(LPVOID arg)
{
	DWORD status;
	RDPEI_PLUGIN* rdpei = (RDPEI_PLUGIN*)arg;
	UINT error = CHANNEL_RC_OK;
	RdpeiClientContext* context;

	if (!rdpei)
	{
		error = ERROR_INVALID_PARAMETER;
		goto out;
	}

	context = rdpei->context;

	if (!context)
	{
		error = ERROR_INVALID_PARAMETER;
		goto out;
	}

	while (rdpei->running)
	{
		status = WaitForSingleObject(rdpei->event, 20);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForMultipleObjects failed with error %" PRIu32 "!", error);
			break;
		}

		EnterCriticalSection(&rdpei->lock);

		error = rdpei_update(context);
		if (error != CHANNEL_RC_OK)
		{
			WLog_ERR(TAG, "rdpei_add_frame failed with error %" PRIu32 "!", error);
			break;
		}

		if (status == WAIT_OBJECT_0)
			ResetEvent(rdpei->event);

		LeaveCriticalSection(&rdpei->lock);
	}

out:

	if (error && rdpei && rdpei->rdpcontext)
		setChannelError(rdpei->rdpcontext, error, "rdpei_schedule_thread reported an error");

	if (rdpei)
		rdpei->running = FALSE;

	ExitThread(error);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpei_send_cs_ready_pdu(GENERIC_CHANNEL_CALLBACK* callback)
{
	UINT status;
	wStream* s;
	UINT32 flags = 0;
	UINT32 pduLength;
	RDPEI_PLUGIN* rdpei;

	if (!callback || !callback->plugin)
		return ERROR_INTERNAL_ERROR;
	rdpei = (RDPEI_PLUGIN*)callback->plugin;

	flags |= CS_READY_FLAGS_SHOW_TOUCH_VISUALS & rdpei->context->clientFeaturesMask;
	if (rdpei->version > RDPINPUT_PROTOCOL_V10)
		flags |= CS_READY_FLAGS_DISABLE_TIMESTAMP_INJECTION & rdpei->context->clientFeaturesMask;
	if (rdpei->features & SC_READY_MULTIPEN_INJECTION_SUPPORTED)
		flags |= CS_READY_FLAGS_ENABLE_MULTIPEN_INJECTION & rdpei->context->clientFeaturesMask;

	pduLength = RDPINPUT_HEADER_LENGTH + 10;
	s = Stream_New(NULL, pduLength);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Seek(s, RDPINPUT_HEADER_LENGTH);
	Stream_Write_UINT32(s, flags);                   /* flags (4 bytes) */
	Stream_Write_UINT32(s, rdpei->version);          /* protocolVersion (4 bytes) */
	Stream_Write_UINT16(s, rdpei->maxTouchContacts); /* maxTouchContacts (2 bytes) */
	Stream_SealLength(s);
	status = rdpei_send_pdu(callback, s, EVENTID_CS_READY, pduLength);
	Stream_Free(s, TRUE);
	return status;
}

static void rdpei_print_contact_flags(UINT32 contactFlags)
{
	if (contactFlags & RDPINPUT_CONTACT_FLAG_DOWN)
		WLog_DBG(TAG, " RDPINPUT_CONTACT_FLAG_DOWN");

	if (contactFlags & RDPINPUT_CONTACT_FLAG_UPDATE)
		WLog_DBG(TAG, " RDPINPUT_CONTACT_FLAG_UPDATE");

	if (contactFlags & RDPINPUT_CONTACT_FLAG_UP)
		WLog_DBG(TAG, " RDPINPUT_CONTACT_FLAG_UP");

	if (contactFlags & RDPINPUT_CONTACT_FLAG_INRANGE)
		WLog_DBG(TAG, " RDPINPUT_CONTACT_FLAG_INRANGE");

	if (contactFlags & RDPINPUT_CONTACT_FLAG_INCONTACT)
		WLog_DBG(TAG, " RDPINPUT_CONTACT_FLAG_INCONTACT");

	if (contactFlags & RDPINPUT_CONTACT_FLAG_CANCELED)
		WLog_DBG(TAG, " RDPINPUT_CONTACT_FLAG_CANCELED");
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpei_write_touch_frame(wStream* s, RDPINPUT_TOUCH_FRAME* frame)
{
	UINT32 index;
	int rectSize = 2;
	RDPINPUT_CONTACT_DATA* contact;
	if (!s || !frame)
		return ERROR_INTERNAL_ERROR;
#ifdef WITH_DEBUG_RDPEI
	WLog_DBG(TAG, "contactCount: %" PRIu32 "", frame->contactCount);
	WLog_DBG(TAG, "frameOffset: 0x%016" PRIX64 "", frame->frameOffset);
#endif
	rdpei_write_2byte_unsigned(s,
	                           frame->contactCount); /* contactCount (TWO_BYTE_UNSIGNED_INTEGER) */
	/**
	 * the time offset from the previous frame (in microseconds).
	 * If this is the first frame being transmitted then this field MUST be set to zero.
	 */
	rdpei_write_8byte_unsigned(s, frame->frameOffset *
	                                  1000); /* frameOffset (EIGHT_BYTE_UNSIGNED_INTEGER) */

	if (!Stream_EnsureRemainingCapacity(s, (size_t)frame->contactCount * 64))
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
		WLog_DBG(TAG, "contact[%" PRIu32 "].contactId: %" PRIu32 "", index, contact->contactId);
		WLog_DBG(TAG, "contact[%" PRIu32 "].fieldsPresent: %" PRIu32 "", index,
		         contact->fieldsPresent);
		WLog_DBG(TAG, "contact[%" PRIu32 "].x: %" PRId32 "", index, contact->x);
		WLog_DBG(TAG, "contact[%" PRIu32 "].y: %" PRId32 "", index, contact->y);
		WLog_DBG(TAG, "contact[%" PRIu32 "].contactFlags: 0x%08" PRIX32 "", index,
		         contact->contactFlags);
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
static UINT rdpei_send_touch_event_pdu(GENERIC_CHANNEL_CALLBACK* callback,
                                       RDPINPUT_TOUCH_FRAME* frame)
{
	UINT status;
	wStream* s;
	UINT32 pduLength;
	RDPEI_PLUGIN* rdpei;

	WINPR_ASSERT(callback);

	rdpei = (RDPEI_PLUGIN*)callback->plugin;
	if (!rdpei || !rdpei->rdpcontext)
		return ERROR_INTERNAL_ERROR;
	if (freerdp_settings_get_bool(rdpei->rdpcontext->settings, FreeRDP_SuspendInput))
		return CHANNEL_RC_OK;

	if (!frame)
		return ERROR_INTERNAL_ERROR;

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
	rdpei_write_4byte_unsigned(
	    s, (UINT32)frame->frameOffset); /* encodeTime (FOUR_BYTE_UNSIGNED_INTEGER) */
	rdpei_write_2byte_unsigned(s, 1);   /* (frameCount) TWO_BYTE_UNSIGNED_INTEGER */

	if ((status = rdpei_write_touch_frame(s, frame)))
	{
		WLog_ERR(TAG, "rdpei_write_touch_frame failed with error %" PRIu32 "!", status);
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
static UINT rdpei_recv_sc_ready_pdu(GENERIC_CHANNEL_CALLBACK* callback, wStream* s)
{
	UINT32 features = 0;
	UINT32 protocolVersion;
	RDPEI_PLUGIN* rdpei;

	if (!callback || !callback->plugin)
		return ERROR_INTERNAL_ERROR;

	rdpei = (RDPEI_PLUGIN*)callback->plugin;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return ERROR_INVALID_DATA;
	Stream_Read_UINT32(s, protocolVersion); /* protocolVersion (4 bytes) */

	if (protocolVersion >= RDPINPUT_PROTOCOL_V300)
	{
		if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
			return ERROR_INVALID_DATA;
	}

	if (Stream_GetRemainingLength(s) >= 4)
		Stream_Read_UINT32(s, features);

	if (rdpei->version > protocolVersion)
		rdpei->version = protocolVersion;
	rdpei->features = features;
#if 0

	if (protocolVersion != RDPINPUT_PROTOCOL_V10)
	{
		WLog_ERR(TAG,  "Unknown [MS-RDPEI] protocolVersion: 0x%08"PRIX32"", protocolVersion);
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
static UINT rdpei_recv_suspend_touch_pdu(GENERIC_CHANNEL_CALLBACK* callback, wStream* s)
{
	UINT error = CHANNEL_RC_OK;
	RdpeiClientContext* rdpei;

	WINPR_UNUSED(s);

	if (!callback || !callback->plugin)
		return ERROR_INTERNAL_ERROR;
	rdpei = (RdpeiClientContext*)callback->plugin->pInterface;
	if (!rdpei)
		return ERROR_INTERNAL_ERROR;

	IFCALLRET(rdpei->SuspendTouch, error, rdpei);

	if (error)
		WLog_ERR(TAG, "rdpei->SuspendTouch failed with error %" PRIu32 "!", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpei_recv_resume_touch_pdu(GENERIC_CHANNEL_CALLBACK* callback, wStream* s)
{
	RdpeiClientContext* rdpei;
	UINT error = CHANNEL_RC_OK;
	if (!s || !callback || !callback->plugin)
		return ERROR_INTERNAL_ERROR;
	rdpei = (RdpeiClientContext*)callback->plugin->pInterface;
	if (!rdpei)
		return ERROR_INTERNAL_ERROR;

	IFCALLRET(rdpei->ResumeTouch, error, rdpei);

	if (error)
		WLog_ERR(TAG, "rdpei->ResumeTouch failed with error %" PRIu32 "!", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpei_recv_pdu(GENERIC_CHANNEL_CALLBACK* callback, wStream* s)
{
	UINT16 eventId;
	UINT32 pduLength;
	UINT error;

	if (!s)
		return ERROR_INTERNAL_ERROR;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 6))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT16(s, eventId);   /* eventId (2 bytes) */
	Stream_Read_UINT32(s, pduLength); /* pduLength (4 bytes) */
#ifdef WITH_DEBUG_RDPEI
	WLog_DBG(TAG, "rdpei_recv_pdu: eventId: %" PRIu16 " (%s) length: %" PRIu32 "", eventId,
	         rdpei_eventid_string(eventId), pduLength);
#endif

	switch (eventId)
	{
		case EVENTID_SC_READY:
			if ((error = rdpei_recv_sc_ready_pdu(callback, s)))
			{
				WLog_ERR(TAG, "rdpei_recv_sc_ready_pdu failed with error %" PRIu32 "!", error);
				return error;
			}

			if ((error = rdpei_send_cs_ready_pdu(callback)))
			{
				WLog_ERR(TAG, "rdpei_send_cs_ready_pdu failed with error %" PRIu32 "!", error);
				return error;
			}

			break;

		case EVENTID_SUSPEND_TOUCH:
			if ((error = rdpei_recv_suspend_touch_pdu(callback, s)))
			{
				WLog_ERR(TAG, "rdpei_recv_suspend_touch_pdu failed with error %" PRIu32 "!", error);
				return error;
			}

			break;

		case EVENTID_RESUME_TOUCH:
			if ((error = rdpei_recv_resume_touch_pdu(callback, s)))
			{
				WLog_ERR(TAG, "rdpei_recv_resume_touch_pdu failed with error %" PRIu32 "!", error);
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
static UINT rdpei_on_data_received(IWTSVirtualChannelCallback* pChannelCallback, wStream* data)
{
	GENERIC_CHANNEL_CALLBACK* callback = (GENERIC_CHANNEL_CALLBACK*)pChannelCallback;
	return rdpei_recv_pdu(callback, data);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpei_on_close(IWTSVirtualChannelCallback* pChannelCallback)
{
	GENERIC_CHANNEL_CALLBACK* callback = (GENERIC_CHANNEL_CALLBACK*)pChannelCallback;
	if (callback)
	{
		RDPEI_PLUGIN* rdpei = (RDPEI_PLUGIN*)callback->plugin;
		if (rdpei && rdpei->base.listener_callback)
		{
			if (rdpei->base.listener_callback->channel_callback == callback)
				rdpei->base.listener_callback->channel_callback = NULL;
		}
	}
	free(callback);
	return CHANNEL_RC_OK;
}

/**
 * Channel Client Interface
 */

static UINT32 rdpei_get_version(RdpeiClientContext* context)
{
	RDPEI_PLUGIN* rdpei;
	if (!context || !context->handle)
		return -1;
	rdpei = (RDPEI_PLUGIN*)context->handle;
	return rdpei->version;
}

static UINT32 rdpei_get_features(RdpeiClientContext* context)
{
	RDPEI_PLUGIN* rdpei;
	if (!context || !context->handle)
		return -1;
	rdpei = (RDPEI_PLUGIN*)context->handle;
	return rdpei->features;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpei_send_frame(RdpeiClientContext* context, RDPINPUT_TOUCH_FRAME* frame)
{
	UINT64 currentTime = GetTickCount64();
	RDPEI_PLUGIN* rdpei = (RDPEI_PLUGIN*)context->handle;
	GENERIC_CHANNEL_CALLBACK* callback;
	UINT error;

	callback = rdpei->base.listener_callback->channel_callback;

	/* Just ignore the event if the channel is not connected */
	if (!callback)
		return CHANNEL_RC_OK;

	if (!rdpei->previousFrameTime && !rdpei->currentFrameTime)
	{
		rdpei->currentFrameTime = currentTime;
		frame->frameOffset = 0;
	}
	else
	{
		rdpei->currentFrameTime = currentTime;
		frame->frameOffset = rdpei->currentFrameTime - rdpei->previousFrameTime;
	}

	if ((error = rdpei_send_touch_event_pdu(callback, frame)))
	{
		WLog_ERR(TAG, "rdpei_send_touch_event_pdu failed with error %" PRIu32 "!", error);
		return error;
	}

	rdpei->previousFrameTime = rdpei->currentFrameTime;
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpei_add_contact(RdpeiClientContext* context, const RDPINPUT_CONTACT_DATA* contact)
{
	RDPINPUT_CONTACT_POINT* contactPoint;
	RDPEI_PLUGIN* rdpei;
	if (!context || !contact || !context->handle)
		return ERROR_INTERNAL_ERROR;

	rdpei = (RDPEI_PLUGIN*)context->handle;

	EnterCriticalSection(&rdpei->lock);
	contactPoint = &rdpei->contactPoints[contact->contactId];
	contactPoint->data = *contact;
	contactPoint->dirty = TRUE;
	SetEvent(rdpei->event);
	LeaveCriticalSection(&rdpei->lock);

	return CHANNEL_RC_OK;
}

static UINT rdpei_touch_process(RdpeiClientContext* context, INT32 externalId, UINT32 contactFlags,
                                INT32 x, INT32 y, INT32* contactId, UINT32 fieldFlags, va_list ap)
{
	INT64 contactIdlocal = -1;
	RDPINPUT_CONTACT_POINT* contactPoint;
	RDPEI_PLUGIN* rdpei;
	BOOL begin;
	UINT error = CHANNEL_RC_OK;

	if (!context || !contactId || !context->handle)
		return ERROR_INTERNAL_ERROR;

	rdpei = (RDPEI_PLUGIN*)context->handle;
	/* Create a new contact point in an empty slot */
	EnterCriticalSection(&rdpei->lock);
	begin = contactFlags & RDPINPUT_CONTACT_FLAG_DOWN;
	contactPoint = rdpei_contact(rdpei, externalId, !begin);
	if (contactPoint)
		contactIdlocal = contactPoint->contactId;
	LeaveCriticalSection(&rdpei->lock);

	if (contactIdlocal >= 0)
	{
		RDPINPUT_CONTACT_DATA contact = { 0 };
		contact.x = x;
		contact.y = y;
		contact.contactId = contactIdlocal;
		contact.contactFlags = contactFlags;
		contact.fieldsPresent = fieldFlags;

		if (fieldFlags & CONTACT_DATA_CONTACTRECT_PRESENT)
		{
			contact.contactRectLeft = va_arg(ap, INT32);
			contact.contactRectTop = va_arg(ap, INT32);
			contact.contactRectRight = va_arg(ap, INT32);
			contact.contactRectBottom = va_arg(ap, INT32);
		}
		if (fieldFlags & CONTACT_DATA_ORIENTATION_PRESENT)
		{
			UINT32 p = va_arg(ap, UINT32);
			if (p >= 360)
			{
				WLog_WARN(TAG,
				          "TouchContact %" PRIu32 ": Invalid orientation value %" PRIu32
				          "degree, clamping to 359 degree",
				          contactIdlocal, p);
				p = 359;
			}
			contact.orientation = p;
		}
		if (fieldFlags & CONTACT_DATA_PRESSURE_PRESENT)
		{
			UINT32 p = va_arg(ap, UINT32);
			if (p > 1024)
			{
				WLog_WARN(TAG,
				          "TouchContact %" PRIu32 ": Invalid pressure value %" PRIu32
				          ", clamping to 1024",
				          contactIdlocal, p);
				p = 1024;
			}
			contact.pressure = p;
		}

		error = context->AddContact(context, &contact);
	}

	if (contactId)
		*contactId = contactIdlocal;
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpei_touch_begin(RdpeiClientContext* context, INT32 externalId, INT32 x, INT32 y,
                              INT32* contactId)
{
	UINT rc;
	va_list ap = { 0 };
	rc = rdpei_touch_process(context, externalId,
	                         RDPINPUT_CONTACT_FLAG_DOWN | RDPINPUT_CONTACT_FLAG_INRANGE |
	                             RDPINPUT_CONTACT_FLAG_INCONTACT,
	                         x, y, contactId, 0, ap);
	return rc;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpei_touch_update(RdpeiClientContext* context, INT32 externalId, INT32 x, INT32 y,
                               INT32* contactId)
{
	UINT rc;
	va_list ap = { 0 };
	rc = rdpei_touch_process(context, externalId,
	                         RDPINPUT_CONTACT_FLAG_UPDATE | RDPINPUT_CONTACT_FLAG_INRANGE |
	                             RDPINPUT_CONTACT_FLAG_INCONTACT,
	                         x, y, contactId, 0, ap);
	return rc;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpei_touch_end(RdpeiClientContext* context, INT32 externalId, INT32 x, INT32 y,
                            INT32* contactId)
{
	UINT error;
	va_list ap = { 0 };
	error = rdpei_touch_process(context, externalId,
	                            RDPINPUT_CONTACT_FLAG_UPDATE | RDPINPUT_CONTACT_FLAG_INRANGE |
	                                RDPINPUT_CONTACT_FLAG_INCONTACT,
	                            x, y, contactId, 0, ap);
	if (error != CHANNEL_RC_OK)
		return error;
	error =
	    rdpei_touch_process(context, externalId, RDPINPUT_CONTACT_FLAG_UP, x, y, contactId, 0, ap);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpei_touch_cancel(RdpeiClientContext* context, INT32 externalId, INT32 x, INT32 y,
                               INT32* contactId)
{
	UINT rc;
	va_list ap = { 0 };
	rc = rdpei_touch_process(context, externalId,
	                         RDPINPUT_CONTACT_FLAG_UP | RDPINPUT_CONTACT_FLAG_CANCELED, x, y,
	                         contactId, 0, ap);
	return rc;
}

static UINT rdpei_touch_raw_event(RdpeiClientContext* context, INT32 externalId, INT32 x, INT32 y,
                                  INT32* contactId, UINT32 flags, UINT32 fieldFlags, ...)
{
	UINT rc;
	va_list ap;
	va_start(ap, fieldFlags);
	rc = rdpei_touch_process(context, externalId, flags, x, y, contactId, fieldFlags, ap);
	va_end(ap);
	return rc;
}

static RDPINPUT_PEN_CONTACT_POINT* rdpei_pen_contact(RDPEI_PLUGIN* rdpei, INT32 externalId,
                                                     BOOL active)
{
	UINT32 x;
	if (!rdpei)
		return NULL;

	for (x = 0; x < rdpei->maxPenContacts; x++)
	{
		RDPINPUT_PEN_CONTACT_POINT* contact = &rdpei->penContactPoints[x];
		if (active)
		{
			if (contact->active)
			{
				if (contact->externalId == externalId)
					return contact;
			}
		}
		else
		{
			if (!contact->active)
			{
				contact->externalId = externalId;
				contact->active = TRUE;
				return contact;
			}
		}
	}
	return NULL;
}

static UINT rdpei_add_pen(RdpeiClientContext* context, INT32 externalId,
                          const RDPINPUT_PEN_CONTACT* contact)
{
	RDPEI_PLUGIN* rdpei;
	RDPINPUT_PEN_CONTACT_POINT* contactPoint;

	if (!context || !contact || !context->handle)
		return ERROR_INTERNAL_ERROR;

	rdpei = (RDPEI_PLUGIN*)context->handle;

	EnterCriticalSection(&rdpei->lock);
	contactPoint = rdpei_pen_contact(rdpei, externalId, TRUE);
	if (contactPoint)
	{
		contactPoint->data = *contact;
		contactPoint->dirty = TRUE;
		SetEvent(rdpei->event);
	}
	LeaveCriticalSection(&rdpei->lock);

	return CHANNEL_RC_OK;
}

static UINT rdpei_pen_process(RdpeiClientContext* context, INT32 externalId, UINT32 contactFlags,
                              UINT32 fieldFlags, INT32 x, INT32 y, va_list ap)
{
	RDPINPUT_PEN_CONTACT_POINT* contactPoint;
	RDPEI_PLUGIN* rdpei;
	BOOL begin;
	UINT error = CHANNEL_RC_OK;

	if (!context || !context->handle)
		return ERROR_INTERNAL_ERROR;

	rdpei = (RDPEI_PLUGIN*)context->handle;
	begin = contactFlags & RDPINPUT_CONTACT_FLAG_DOWN;

	EnterCriticalSection(&rdpei->lock);
	contactPoint = rdpei_pen_contact(rdpei, externalId, !begin);
	LeaveCriticalSection(&rdpei->lock);
	if (contactPoint != NULL)
	{
		RDPINPUT_PEN_CONTACT contact = { 0 };

		contact.x = x;
		contact.y = y;
		contact.fieldsPresent = fieldFlags;

		contact.contactFlags = contactFlags;
		if (fieldFlags & RDPINPUT_PEN_CONTACT_PENFLAGS_PRESENT)
			contact.penFlags = va_arg(ap, UINT32);
		if (fieldFlags & RDPINPUT_PEN_CONTACT_PRESSURE_PRESENT)
			contact.pressure = va_arg(ap, UINT32);
		if (fieldFlags & RDPINPUT_PEN_CONTACT_ROTATION_PRESENT)
			contact.rotation = va_arg(ap, UINT32);
		if (fieldFlags & RDPINPUT_PEN_CONTACT_TILTX_PRESENT)
			contact.tiltX = va_arg(ap, INT32);
		if (fieldFlags & RDPINPUT_PEN_CONTACT_TILTY_PRESENT)
			contact.tiltY = va_arg(ap, INT32);

		error = context->AddPen(context, externalId, &contact);
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpei_pen_begin(RdpeiClientContext* context, INT32 externalId, UINT32 fieldFlags,
                            INT32 x, INT32 y, ...)
{
	UINT error;
	va_list ap;

	va_start(ap, y);
	error = rdpei_pen_process(context, externalId,
	                          RDPINPUT_CONTACT_FLAG_DOWN | RDPINPUT_CONTACT_FLAG_INRANGE |
	                              RDPINPUT_CONTACT_FLAG_INCONTACT,
	                          fieldFlags, x, y, ap);
	va_end(ap);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpei_pen_update(RdpeiClientContext* context, INT32 externalId, UINT32 fieldFlags,
                             INT32 x, INT32 y, ...)
{
	UINT error;
	va_list ap;

	va_start(ap, y);
	error = rdpei_pen_process(context, externalId,
	                          RDPINPUT_CONTACT_FLAG_UPDATE | RDPINPUT_CONTACT_FLAG_INRANGE |
	                              RDPINPUT_CONTACT_FLAG_INCONTACT,
	                          fieldFlags, x, y, ap);
	va_end(ap);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpei_pen_end(RdpeiClientContext* context, INT32 externalId, UINT32 fieldFlags, INT32 x,
                          INT32 y, ...)
{
	UINT error;
	va_list ap;

	va_start(ap, y);
	error = rdpei_pen_process(context, externalId,
	                          RDPINPUT_CONTACT_FLAG_UPDATE | RDPINPUT_CONTACT_FLAG_INRANGE |
	                              RDPINPUT_CONTACT_FLAG_INCONTACT,
	                          fieldFlags, x, y, ap);
	va_end(ap);
	if (error == CHANNEL_RC_OK)
	{
		va_start(ap, y);
		error =
		    rdpei_pen_process(context, externalId, RDPINPUT_CONTACT_FLAG_UP, fieldFlags, x, y, ap);
		va_end(ap);
	}
	return error;
}

static UINT rdpei_pen_cancel(RdpeiClientContext* context, INT32 externalId, UINT32 fieldFlags,
                             INT32 x, INT32 y, ...)
{
	UINT error;
	va_list ap;

	va_start(ap, y);
	error = rdpei_pen_process(context, externalId,
	                          RDPINPUT_CONTACT_FLAG_UP | RDPINPUT_CONTACT_FLAG_CANCELED, fieldFlags,
	                          x, y, ap);
	va_end(ap);
	return error;
}

static UINT rdpei_pen_raw_event(RdpeiClientContext* context, INT32 externalId, UINT32 contactFlags,
                                UINT32 fieldFlags, INT32 x, INT32 y, ...)
{
	UINT error;
	va_list ap;

	va_start(ap, y);
	error = rdpei_pen_process(context, externalId, contactFlags, fieldFlags, x, y, ap);
	va_end(ap);
	return error;
}

static UINT init_plugin_cb(GENERIC_DYNVC_PLUGIN* base, rdpContext* rcontext, rdpSettings* settings)
{
	RdpeiClientContext* context;
	RDPEI_PLUGIN* rdpei = (RDPEI_PLUGIN*)base;

	WINPR_ASSERT(base);
	WINPR_UNUSED(settings);

	rdpei->version = RDPINPUT_PROTOCOL_V300;
	rdpei->currentFrameTime = 0;
	rdpei->previousFrameTime = 0;
	rdpei->maxTouchContacts = MAX_CONTACTS;
	rdpei->maxPenContacts = MAX_PEN_CONTACTS;
	rdpei->rdpcontext = rcontext;

	InitializeCriticalSection(&rdpei->lock);
	rdpei->event = CreateEventA(NULL, TRUE, FALSE, NULL);
	if (!rdpei->event)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	context = (RdpeiClientContext*)calloc(1, sizeof(*context));
	if (!context)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	context->clientFeaturesMask = UINT32_MAX;
	context->handle = (void*)rdpei;
	context->GetVersion = rdpei_get_version;
	context->GetFeatures = rdpei_get_features;
	context->AddContact = rdpei_add_contact;
	context->TouchBegin = rdpei_touch_begin;
	context->TouchUpdate = rdpei_touch_update;
	context->TouchEnd = rdpei_touch_end;
	context->TouchCancel = rdpei_touch_cancel;
	context->TouchRawEvent = rdpei_touch_raw_event;
	context->AddPen = rdpei_add_pen;
	context->PenBegin = rdpei_pen_begin;
	context->PenUpdate = rdpei_pen_update;
	context->PenEnd = rdpei_pen_end;
	context->PenCancel = rdpei_pen_cancel;
	context->PenRawEvent = rdpei_pen_raw_event;

	rdpei->context = context;
	rdpei->base.iface.pInterface = (void*)context;

	rdpei->running = TRUE;
	rdpei->thread = CreateThread(NULL, 0, rdpei_periodic_update, rdpei, 0, NULL);
	if (!rdpei->thread)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	return CHANNEL_RC_OK;
}

static void terminate_plugin_cb(GENERIC_DYNVC_PLUGIN* base)
{
	RDPEI_PLUGIN* rdpei = (RDPEI_PLUGIN*)base;
	WINPR_ASSERT(rdpei);

	rdpei->running = FALSE;
	if (rdpei->event)
		SetEvent(rdpei->event);

	if (rdpei->thread)
	{
		WaitForSingleObject(rdpei->thread, INFINITE);
		CloseHandle(rdpei->thread);
	}

	if (rdpei->event)
		CloseHandle(rdpei->event);

	DeleteCriticalSection(&rdpei->lock);
	free(rdpei->context);
}

static const IWTSVirtualChannelCallback geometry_callbacks = { rdpei_on_data_received,
	                                                           NULL, /* Open */
	                                                           rdpei_on_close };

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpei_DVCPluginEntry(IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	return freerdp_generic_DVCPluginEntry(pEntryPoints, TAG, RDPEI_DVC_CHANNEL_NAME,
	                                      sizeof(RDPEI_PLUGIN), sizeof(GENERIC_CHANNEL_CALLBACK),
	                                      &geometry_callbacks, init_plugin_cb, terminate_plugin_cb);
}
