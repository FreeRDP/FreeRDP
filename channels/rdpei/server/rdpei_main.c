/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Extended Input channel server-side implementation
 *
 * Copyright 2014 Thincast Technologies Gmbh.
 * Copyright 2014 David FORT <contact@hardening-consulting.com>
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

#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/stream.h>

#include "rdpei_main.h"
#include "../rdpei_common.h"
#include <freerdp/channels/rdpei.h>
#include <freerdp/server/rdpei.h>

enum RdpEiState
{
	STATE_INITIAL,
	STATE_WAITING_CLIENT_READY,
	STATE_WAITING_FRAME,
	STATE_SUSPENDED,
};

struct s_rdpei_server_private
{
	HANDLE channelHandle;
	HANDLE eventHandle;

	UINT32 expectedBytes;
	BOOL waitingHeaders;
	wStream* inputStream;
	wStream* outputStream;

	UINT16 currentMsgType;

	RDPINPUT_TOUCH_EVENT touchEvent;
	RDPINPUT_PEN_EVENT penEvent;

	enum RdpEiState automataState;
};

RdpeiServerContext* rdpei_server_context_new(HANDLE vcm)
{
	RdpeiServerContext* ret = calloc(1, sizeof(*ret));
	RdpeiServerPrivate* priv;

	if (!ret)
		return NULL;

	ret->priv = priv = calloc(1, sizeof(*ret->priv));
	if (!priv)
		goto fail;

	priv->inputStream = Stream_New(NULL, 256);
	if (!priv->inputStream)
		goto fail;

	priv->outputStream = Stream_New(NULL, 200);
	if (!priv->inputStream)
		goto fail;

	ret->vcm = vcm;
	rdpei_server_context_reset(ret);
	return ret;

fail:
	rdpei_server_context_free(ret);
	return NULL;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpei_server_init(RdpeiServerContext* context)
{
	void* buffer = NULL;
	DWORD bytesReturned;
	RdpeiServerPrivate* priv = context->priv;
	UINT32 channelId;
	BOOL status = TRUE;

	priv->channelHandle = WTSVirtualChannelOpenEx(WTS_CURRENT_SESSION, RDPEI_DVC_CHANNEL_NAME,
	                                              WTS_CHANNEL_OPTION_DYNAMIC);
	if (!priv->channelHandle)
	{
		WLog_ERR(TAG, "WTSVirtualChannelOpenEx failed!");
		return CHANNEL_RC_INITIALIZATION_ERROR;
	}

	channelId = WTSChannelGetIdByHandle(priv->channelHandle);

	IFCALLRET(context->onChannelIdAssigned, status, context, channelId);
	if (!status)
	{
		WLog_ERR(TAG, "context->onChannelIdAssigned failed!");
		goto out_close;
	}

	if (!WTSVirtualChannelQuery(priv->channelHandle, WTSVirtualEventHandle, &buffer,
	                            &bytesReturned) ||
	    (bytesReturned != sizeof(HANDLE)))
	{
		WLog_ERR(TAG,
		         "WTSVirtualChannelQuery failed or invalid invalid returned size(%" PRIu32 ")!",
		         bytesReturned);
		if (buffer)
			WTSFreeMemory(buffer);
		goto out_close;
	}
	CopyMemory(&priv->eventHandle, buffer, sizeof(HANDLE));
	WTSFreeMemory(buffer);

	return CHANNEL_RC_OK;

out_close:
	WTSVirtualChannelClose(priv->channelHandle);
	return CHANNEL_RC_INITIALIZATION_ERROR;
}

void rdpei_server_context_reset(RdpeiServerContext* context)
{
	RdpeiServerPrivate* priv = context->priv;

	priv->channelHandle = INVALID_HANDLE_VALUE;
	priv->expectedBytes = RDPINPUT_HEADER_LENGTH;
	priv->waitingHeaders = TRUE;
	priv->automataState = STATE_INITIAL;
	Stream_SetPosition(priv->inputStream, 0);
}

void rdpei_server_context_free(RdpeiServerContext* context)
{
	RdpeiServerPrivate* priv;

	if (!context)
		return;
	priv = context->priv;
	if (priv)
	{
		if (priv->channelHandle != INVALID_HANDLE_VALUE)
			WTSVirtualChannelClose(priv->channelHandle);
		Stream_Free(priv->inputStream, TRUE);
	}
	free(priv);
	free(context);
}

HANDLE rdpei_server_get_event_handle(RdpeiServerContext* context)
{
	return context->priv->eventHandle;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT read_cs_ready_message(RdpeiServerContext* context, wStream* s)
{
	UINT error = CHANNEL_RC_OK;
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 10))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, context->protocolFlags);
	Stream_Read_UINT32(s, context->clientVersion);
	Stream_Read_UINT16(s, context->maxTouchPoints);

	switch (context->clientVersion)
	{
		case RDPINPUT_PROTOCOL_V10:
		case RDPINPUT_PROTOCOL_V101:
		case RDPINPUT_PROTOCOL_V200:
		case RDPINPUT_PROTOCOL_V300:
			break;
		default:
			WLog_ERR(TAG, "unhandled RPDEI protocol version 0x%" PRIx32 "", context->clientVersion);
			break;
	}

	IFCALLRET(context->onClientReady, error, context);
	if (error)
		WLog_ERR(TAG, "context->onClientReady failed with error %" PRIu32 "", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT read_touch_contact_data(RdpeiServerContext* context, wStream* s,
                                    RDPINPUT_CONTACT_DATA* contactData)
{
	WINPR_UNUSED(context);
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT8(s, contactData->contactId);
	if (!rdpei_read_2byte_unsigned(s, &contactData->fieldsPresent) ||
	    !rdpei_read_4byte_signed(s, &contactData->x) ||
	    !rdpei_read_4byte_signed(s, &contactData->y) ||
	    !rdpei_read_4byte_unsigned(s, &contactData->contactFlags))
	{
		WLog_ERR(TAG, "rdpei_read_ failed!");
		return ERROR_INTERNAL_ERROR;
	}

	if (contactData->fieldsPresent & CONTACT_DATA_CONTACTRECT_PRESENT)
	{
		if (!rdpei_read_2byte_signed(s, &contactData->contactRectLeft) ||
		    !rdpei_read_2byte_signed(s, &contactData->contactRectTop) ||
		    !rdpei_read_2byte_signed(s, &contactData->contactRectRight) ||
		    !rdpei_read_2byte_signed(s, &contactData->contactRectBottom))
		{
			WLog_ERR(TAG, "rdpei_read_ failed!");
			return ERROR_INTERNAL_ERROR;
		}
	}

	if ((contactData->fieldsPresent & CONTACT_DATA_ORIENTATION_PRESENT) &&
	    !rdpei_read_4byte_unsigned(s, &contactData->orientation))
	{
		WLog_ERR(TAG, "rdpei_read_ failed!");
		return ERROR_INTERNAL_ERROR;
	}

	if ((contactData->fieldsPresent & CONTACT_DATA_PRESSURE_PRESENT) &&
	    !rdpei_read_4byte_unsigned(s, &contactData->pressure))
	{
		WLog_ERR(TAG, "rdpei_read_ failed!");
		return ERROR_INTERNAL_ERROR;
	}

	return CHANNEL_RC_OK;
}

static UINT read_pen_contact(RdpeiServerContext* context, wStream* s,
                             RDPINPUT_PEN_CONTACT* contactData)
{
	WINPR_UNUSED(context);
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT8(s, contactData->deviceId);
	if (!rdpei_read_2byte_unsigned(s, &contactData->fieldsPresent) ||
	    !rdpei_read_4byte_signed(s, &contactData->x) ||
	    !rdpei_read_4byte_signed(s, &contactData->y) ||
	    !rdpei_read_4byte_unsigned(s, &contactData->contactFlags))
	{
		WLog_ERR(TAG, "rdpei_read_ failed!");
		return ERROR_INTERNAL_ERROR;
	}

	if (contactData->fieldsPresent & RDPINPUT_PEN_CONTACT_PENFLAGS_PRESENT)
	{
		if (!rdpei_read_4byte_unsigned(s, &contactData->penFlags))
			return ERROR_INVALID_DATA;
	}
	if (contactData->fieldsPresent & RDPINPUT_PEN_CONTACT_PRESSURE_PRESENT)
	{
		if (!rdpei_read_4byte_unsigned(s, &contactData->pressure))
			return ERROR_INVALID_DATA;
	}
	if (contactData->fieldsPresent & RDPINPUT_PEN_CONTACT_ROTATION_PRESENT)
	{
		if (!rdpei_read_2byte_unsigned(s, &contactData->rotation))
			return ERROR_INVALID_DATA;
	}
	if (contactData->fieldsPresent & RDPINPUT_PEN_CONTACT_TILTX_PRESENT)
	{
		if (!rdpei_read_2byte_signed(s, &contactData->tiltX))
			return ERROR_INVALID_DATA;
	}
	if (contactData->fieldsPresent & RDPINPUT_PEN_CONTACT_TILTY_PRESENT)
	{
		if (!rdpei_read_2byte_signed(s, &contactData->tiltY))
			return ERROR_INVALID_DATA;
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT read_touch_frame(RdpeiServerContext* context, wStream* s, RDPINPUT_TOUCH_FRAME* frame)
{
	UINT32 i;
	RDPINPUT_CONTACT_DATA* contact;
	UINT error;

	if (!rdpei_read_2byte_unsigned(s, &frame->contactCount) ||
	    !rdpei_read_8byte_unsigned(s, &frame->frameOffset))
	{
		WLog_ERR(TAG, "rdpei_read_ failed!");
		return ERROR_INTERNAL_ERROR;
	}

	frame->contacts = contact = calloc(frame->contactCount, sizeof(RDPINPUT_CONTACT_DATA));
	if (!frame->contacts)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	for (i = 0; i < frame->contactCount; i++, contact++)
	{
		if ((error = read_touch_contact_data(context, s, contact)))
		{
			WLog_ERR(TAG, "read_touch_contact_data failed with error %" PRIu32 "!", error);
			frame->contactCount = i;
			touch_frame_reset(frame);
			return error;
		}
	}
	return CHANNEL_RC_OK;
}

static UINT read_pen_frame(RdpeiServerContext* context, wStream* s, RDPINPUT_PEN_FRAME* frame)
{
	UINT32 i;
	RDPINPUT_PEN_CONTACT* contact;
	UINT error;

	if (!rdpei_read_2byte_unsigned(s, &frame->contactCount) ||
	    !rdpei_read_8byte_unsigned(s, &frame->frameOffset))
	{
		WLog_ERR(TAG, "rdpei_read_ failed!");
		return ERROR_INTERNAL_ERROR;
	}

	frame->contacts = contact = calloc(frame->contactCount, sizeof(RDPINPUT_PEN_CONTACT));
	if (!frame->contacts)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	for (i = 0; i < frame->contactCount; i++, contact++)
	{
		if ((error = read_pen_contact(context, s, contact)))
		{
			WLog_ERR(TAG, "read_touch_contact_data failed with error %" PRIu32 "!", error);
			frame->contactCount = i;
			pen_frame_reset(frame);
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
static UINT read_touch_event(RdpeiServerContext* context, wStream* s)
{
	UINT16 frameCount;
	UINT32 i;
	RDPINPUT_TOUCH_EVENT* event = &context->priv->touchEvent;
	RDPINPUT_TOUCH_FRAME* frame;
	UINT error = CHANNEL_RC_OK;

	if (!rdpei_read_4byte_unsigned(s, &event->encodeTime) ||
	    !rdpei_read_2byte_unsigned(s, &frameCount))
	{
		WLog_ERR(TAG, "rdpei_read_ failed!");
		return ERROR_INTERNAL_ERROR;
	}

	event->frameCount = frameCount;
	event->frames = frame = calloc(event->frameCount, sizeof(RDPINPUT_TOUCH_FRAME));
	if (!event->frames)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	for (i = 0; i < frameCount; i++, frame++)
	{
		if ((error = read_touch_frame(context, s, frame)))
		{
			WLog_ERR(TAG, "read_touch_contact_data failed with error %" PRIu32 "!", error);
			event->frameCount = i;
			goto out_cleanup;
		}
	}

	IFCALLRET(context->onTouchEvent, error, context, event);
	if (error)
		WLog_ERR(TAG, "context->onTouchEvent failed with error %" PRIu32 "", error);

out_cleanup:
	touch_event_reset(event);
	return error;
}

static UINT read_pen_event(RdpeiServerContext* context, wStream* s)
{
	UINT16 frameCount;
	UINT32 i;
	RDPINPUT_PEN_EVENT* event = &context->priv->penEvent;
	RDPINPUT_PEN_FRAME* frame;
	UINT error = CHANNEL_RC_OK;

	if (!rdpei_read_4byte_unsigned(s, &event->encodeTime) ||
	    !rdpei_read_2byte_unsigned(s, &frameCount))
	{
		WLog_ERR(TAG, "rdpei_read_ failed!");
		return ERROR_INTERNAL_ERROR;
	}

	event->frameCount = frameCount;
	event->frames = frame = calloc(event->frameCount, sizeof(RDPINPUT_PEN_FRAME));
	if (!event->frames)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	for (i = 0; i < frameCount; i++, frame++)
	{
		if ((error = read_pen_frame(context, s, frame)))
		{
			WLog_ERR(TAG, "read_pen_frame failed with error %" PRIu32 "!", error);
			event->frameCount = i;
			goto out_cleanup;
		}
	}

	IFCALLRET(context->onPenEvent, error, context, event);
	if (error)
		WLog_ERR(TAG, "context->onPenEvent failed with error %" PRIu32 "", error);

out_cleanup:
	pen_event_reset(event);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT read_dismiss_hovering_contact(RdpeiServerContext* context, wStream* s)
{
	BYTE contactId;
	UINT error = CHANNEL_RC_OK;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT8(s, contactId);

	IFCALLRET(context->onTouchReleased, error, context, contactId);
	if (error)
		WLog_ERR(TAG, "context->onTouchReleased failed with error %" PRIu32 "", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpei_server_handle_messages(RdpeiServerContext* context)
{
	DWORD bytesReturned;
	RdpeiServerPrivate* priv = context->priv;
	wStream* s = priv->inputStream;
	UINT error = CHANNEL_RC_OK;

	if (!WTSVirtualChannelRead(priv->channelHandle, 0, (PCHAR)Stream_Pointer(s),
	                           priv->expectedBytes, &bytesReturned))
	{
		if (GetLastError() == ERROR_NO_DATA)
			return ERROR_READ_FAULT;

		WLog_DBG(TAG, "channel connection closed");
		return CHANNEL_RC_OK;
	}
	priv->expectedBytes -= bytesReturned;
	Stream_Seek(s, bytesReturned);

	if (priv->expectedBytes)
		return CHANNEL_RC_OK;

	Stream_SealLength(s);
	Stream_SetPosition(s, 0);

	if (priv->waitingHeaders)
	{
		UINT32 pduLen;

		/* header case */
		Stream_Read_UINT16(s, priv->currentMsgType);
		Stream_Read_UINT16(s, pduLen);

		if (pduLen < RDPINPUT_HEADER_LENGTH)
		{
			WLog_ERR(TAG, "invalid pduLength %" PRIu32 "", pduLen);
			return ERROR_INVALID_DATA;
		}
		priv->expectedBytes = pduLen - RDPINPUT_HEADER_LENGTH;
		priv->waitingHeaders = FALSE;
		Stream_SetPosition(s, 0);
		if (priv->expectedBytes)
		{
			if (!Stream_EnsureCapacity(s, priv->expectedBytes))
			{
				WLog_ERR(TAG, "Stream_EnsureCapacity failed!");
				return CHANNEL_RC_NO_MEMORY;
			}
			return CHANNEL_RC_OK;
		}
	}

	/* when here we have the header + the body */
	switch (priv->currentMsgType)
	{
		case EVENTID_CS_READY:
			if (priv->automataState != STATE_WAITING_CLIENT_READY)
			{
				WLog_ERR(TAG, "not expecting a CS_READY packet in this state(%d)",
				         priv->automataState);
				return ERROR_INVALID_STATE;
			}

			if ((error = read_cs_ready_message(context, s)))
			{
				WLog_ERR(TAG, "read_cs_ready_message failed with error %" PRIu32 "", error);
				return error;
			}
			break;

		case EVENTID_TOUCH:
			if ((error = read_touch_event(context, s)))
			{
				WLog_ERR(TAG, "read_touch_event failed with error %" PRIu32 "", error);
				return error;
			}
			break;
		case EVENTID_DISMISS_HOVERING_CONTACT:
			if ((error = read_dismiss_hovering_contact(context, s)))
			{
				WLog_ERR(TAG, "read_dismiss_hovering_contact failed with error %" PRIu32 "", error);
				return error;
			}
			break;
		case EVENTID_PEN:
			if ((error = read_pen_event(context, s)))
			{
				WLog_ERR(TAG, "read_pen_event failed with error %" PRIu32 "", error);
				return error;
			}
			break;
		default:
			WLog_ERR(TAG, "unexpected message type 0x%" PRIx16 "", priv->currentMsgType);
	}

	Stream_SetPosition(s, 0);
	priv->waitingHeaders = TRUE;
	priv->expectedBytes = RDPINPUT_HEADER_LENGTH;
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpei_server_send_sc_ready(RdpeiServerContext* context, UINT32 version, UINT32 features)
{
	ULONG written;
	RdpeiServerPrivate* priv = context->priv;
	UINT32 pduLen = 4;

	if (priv->automataState != STATE_INITIAL)
	{
		WLog_ERR(TAG, "called from unexpected state %d", priv->automataState);
		return ERROR_INVALID_STATE;
	}

	Stream_SetPosition(priv->outputStream, 0);

	if (version >= RDPINPUT_PROTOCOL_V300)
		pduLen += 4;

	if (!Stream_EnsureCapacity(priv->outputStream, RDPINPUT_HEADER_LENGTH + pduLen))
	{
		WLog_ERR(TAG, "Stream_EnsureCapacity failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT16(priv->outputStream, EVENTID_SC_READY);
	Stream_Write_UINT32(priv->outputStream, RDPINPUT_HEADER_LENGTH + pduLen);
	Stream_Write_UINT32(priv->outputStream, version);
	if (version >= RDPINPUT_PROTOCOL_V300)
		Stream_Write_UINT32(priv->outputStream, features);

	if (!WTSVirtualChannelWrite(priv->channelHandle, (PCHAR)Stream_Buffer(priv->outputStream),
	                            Stream_GetPosition(priv->outputStream), &written))
	{
		WLog_ERR(TAG, "WTSVirtualChannelWrite failed!");
		return ERROR_INTERNAL_ERROR;
	}

	priv->automataState = STATE_WAITING_CLIENT_READY;
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpei_server_suspend(RdpeiServerContext* context)
{
	ULONG written;
	RdpeiServerPrivate* priv = context->priv;

	switch (priv->automataState)
	{
		case STATE_SUSPENDED:
			WLog_ERR(TAG, "already suspended");
			return CHANNEL_RC_OK;
		case STATE_WAITING_FRAME:
			break;
		default:
			WLog_ERR(TAG, "called from unexpected state %d", priv->automataState);
			return ERROR_INVALID_STATE;
	}

	Stream_SetPosition(priv->outputStream, 0);
	if (!Stream_EnsureCapacity(priv->outputStream, RDPINPUT_HEADER_LENGTH))
	{
		WLog_ERR(TAG, "Stream_EnsureCapacity failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT16(priv->outputStream, EVENTID_SUSPEND_TOUCH);
	Stream_Write_UINT32(priv->outputStream, RDPINPUT_HEADER_LENGTH);

	if (!WTSVirtualChannelWrite(priv->channelHandle, (PCHAR)Stream_Buffer(priv->outputStream),
	                            Stream_GetPosition(priv->outputStream), &written))
	{
		WLog_ERR(TAG, "WTSVirtualChannelWrite failed!");
		return ERROR_INTERNAL_ERROR;
	}

	priv->automataState = STATE_SUSPENDED;
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpei_server_resume(RdpeiServerContext* context)
{
	ULONG written;
	RdpeiServerPrivate* priv = context->priv;

	switch (priv->automataState)
	{
		case STATE_WAITING_FRAME:
			WLog_ERR(TAG, "not suspended");
			return CHANNEL_RC_OK;
		case STATE_SUSPENDED:
			break;
		default:
			WLog_ERR(TAG, "called from unexpected state %d", priv->automataState);
			return ERROR_INVALID_STATE;
	}

	Stream_SetPosition(priv->outputStream, 0);
	if (!Stream_EnsureCapacity(priv->outputStream, RDPINPUT_HEADER_LENGTH))
	{
		WLog_ERR(TAG, "Stream_EnsureCapacity failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT16(priv->outputStream, EVENTID_RESUME_TOUCH);
	Stream_Write_UINT32(priv->outputStream, RDPINPUT_HEADER_LENGTH);

	if (!WTSVirtualChannelWrite(priv->channelHandle, (PCHAR)Stream_Buffer(priv->outputStream),
	                            Stream_GetPosition(priv->outputStream), &written))
	{
		WLog_ERR(TAG, "WTSVirtualChannelWrite failed!");
		return ERROR_INTERNAL_ERROR;
	}

	priv->automataState = STATE_WAITING_FRAME;
	return CHANNEL_RC_OK;
}
