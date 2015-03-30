/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Extended Input channel server-side implementation
 *
 * Copyright 2014 Thincast Technologies Gmbh.
 * Copyright 2014 David FORT <contact@hardening-consulting.com>
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

#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/stream.h>

#include "rdpei_main.h"
#include "../rdpei_common.h"
#include <freerdp/channels/rdpei.h>
#include <freerdp/server/rdpei.h>

/** @brief */
enum RdpEiState {
	STATE_INITIAL,
	STATE_WAITING_CLIENT_READY,
	STATE_WAITING_FRAME,
	STATE_SUSPENDED,
};

struct _rdpei_server_private
{
	HANDLE channelHandle;
	HANDLE eventHandle;

	UINT32 expectedBytes;
	BOOL waitingHeaders;
	wStream *inputStream;
	wStream *outputStream;

	UINT16 currentMsgType;

	RDPINPUT_TOUCH_EVENT touchEvent;

	enum RdpEiState automataState;
};


RdpeiServerContext* rdpei_server_context_new(HANDLE vcm)
{
	RdpeiServerContext *ret = calloc(1, sizeof(*ret));
	RdpeiServerPrivate *priv;

	if (!ret)
		return NULL;

	ret->priv = priv = calloc(1, sizeof(*ret->priv));
	if (!priv)
		goto out_free;

	priv->inputStream = Stream_New(NULL, 256);
	if (!priv->inputStream)
		goto out_free_priv;

	priv->outputStream = Stream_New(NULL, 200);
	if (!priv->inputStream)
		goto out_free_input_stream;

	ret->vcm = vcm;
	rdpei_server_context_reset(ret);
	return ret;

out_free_input_stream:
	Stream_Free(priv->inputStream, TRUE);
out_free_priv:
	free(ret->priv);
out_free:
	free(ret);
	return NULL;
}

int rdpei_server_init(RdpeiServerContext *context)
{
	void *buffer = NULL;
	DWORD bytesReturned;
	RdpeiServerPrivate *priv = context->priv;

	priv->channelHandle = WTSVirtualChannelOpenEx(WTS_CURRENT_SESSION, RDPEI_DVC_CHANNEL_NAME, WTS_CHANNEL_OPTION_DYNAMIC);
	if (!priv->channelHandle)
	{
		fprintf(stderr, "%s: unable to open channel\n", __FUNCTION__);
		return -1;
	}

	if (!WTSVirtualChannelQuery(priv->channelHandle, WTSVirtualEventHandle, &buffer, &bytesReturned) || (bytesReturned != sizeof(HANDLE)))
	{
		fprintf(stderr, "%s: error during WTSVirtualChannelQuery(WTSVirtualEventHandle) or invalid returned size(%d)\n",
				__FUNCTION__, bytesReturned);
		if (buffer)
			WTSFreeMemory(buffer);
		goto out_close;
	}
	CopyMemory(&priv->eventHandle, buffer, sizeof(HANDLE));
	WTSFreeMemory(buffer);

	return 0;

out_close:
	WTSVirtualChannelClose(priv->channelHandle);
	return -1;
}


void rdpei_server_context_reset(RdpeiServerContext *context)
{
	RdpeiServerPrivate *priv = context->priv;

	priv->channelHandle = INVALID_HANDLE_VALUE;
	priv->expectedBytes = RDPINPUT_HEADER_LENGTH;
	priv->waitingHeaders = TRUE;
	priv->automataState = STATE_INITIAL;
	Stream_SetPosition(priv->inputStream, 0);
}

void rdpei_server_context_free(RdpeiServerContext* context)
{
	RdpeiServerPrivate *priv = context->priv;
	if (priv->channelHandle != INVALID_HANDLE_VALUE)
		WTSVirtualChannelClose(priv->channelHandle);
	Stream_Free(priv->inputStream, TRUE);
	free(priv);
	free(context);
}

HANDLE rdpei_server_get_event_handle(RdpeiServerContext *context)
{
	return context->priv->eventHandle;
}


static int read_cs_ready_message(RdpeiServerContext *context, wStream *s)
{
	if (Stream_GetRemainingLength(s) < 10)
		return -1;

	Stream_Read_UINT32(s, context->protocolFlags);
	Stream_Read_UINT32(s, context->clientVersion);
	Stream_Read_UINT16(s, context->maxTouchPoints);

	switch (context->clientVersion)
	{
	case RDPINPUT_PROTOCOL_V10:
	case RDPINPUT_PROTOCOL_V101:
		break;
	default:
		fprintf(stderr, "%s: unhandled RPDEI protocol version 0x%x\n", __FUNCTION__, context->clientVersion);
		break;
	}

	if (context->onClientReady)
		context->onClientReady(context);
	return 0;
}

static int read_touch_contact_data(RdpeiServerContext *context, wStream *s, RDPINPUT_CONTACT_DATA *contactData)
{
	if (Stream_GetRemainingLength(s) < 1)
		return -1;

	Stream_Read_UINT8(s, contactData->contactId);
	if (!rdpei_read_2byte_unsigned(s, &contactData->fieldsPresent) ||
		!rdpei_read_4byte_signed(s, &contactData->x) ||
		!rdpei_read_4byte_signed(s, &contactData->y) ||
		!rdpei_read_4byte_unsigned(s, &contactData->contactFlags))
		return -1;

	if (contactData->fieldsPresent & CONTACT_DATA_CONTACTRECT_PRESENT)
	{
		if (!rdpei_read_2byte_signed(s, &contactData->contactRectLeft) ||
			!rdpei_read_2byte_signed(s, &contactData->contactRectTop) ||
			!rdpei_read_2byte_signed(s, &contactData->contactRectRight) ||
			!rdpei_read_2byte_signed(s, &contactData->contactRectBottom))
			return -1;
	}

	if ((contactData->fieldsPresent & CONTACT_DATA_ORIENTATION_PRESENT) &&
			!rdpei_read_4byte_unsigned(s, &contactData->orientation))
		return -1;

	if ((contactData->fieldsPresent & CONTACT_DATA_PRESSURE_PRESENT) &&
			!rdpei_read_4byte_unsigned(s, &contactData->pressure))
		return -1;

	return 0;
}

static int read_touch_frame(RdpeiServerContext *context, wStream *s, RDPINPUT_TOUCH_FRAME *frame)
{
	int i;
	RDPINPUT_CONTACT_DATA *contact;

	if (!rdpei_read_2byte_unsigned(s, &frame->contactCount) || !rdpei_read_8byte_unsigned(s, &frame->frameOffset))
		return -1;

	frame->contacts = contact = calloc(frame->contactCount, sizeof(RDPINPUT_CONTACT_DATA));
	if (!frame->contacts)
		return -1;

	for (i = 0; i < frame->contactCount; i++, contact++)
	{
		if (read_touch_contact_data(context, s, contact) < 0)
		{
			frame->contactCount = i;
			goto out_cleanup;
		}
	}
	return 0;

out_cleanup:
	touch_frame_reset(frame);
	return -1;
}

static int read_touch_event(RdpeiServerContext *context, wStream *s)
{
	UINT32 frameCount;
	int i, ret;
	RDPINPUT_TOUCH_EVENT *event = &context->priv->touchEvent;
	RDPINPUT_TOUCH_FRAME *frame;

	ret = -1;
	if (!rdpei_read_4byte_unsigned(s, &event->encodeTime) || !rdpei_read_2byte_unsigned(s, &frameCount))
		return -1;

	event->frameCount = frameCount;
	event->frames = frame = calloc(event->frameCount, sizeof(RDPINPUT_TOUCH_FRAME));
	if (!event->frames)
		return -1;

	for (i = 0; i < frameCount; i++, frame++)
	{
		if (read_touch_frame(context, s, frame) < 0)
		{
			event->frameCount = i;
			goto out_cleanup;
		}
	}

	if (context->onTouchEvent)
		context->onTouchEvent(context, event);

	ret = 0;

out_cleanup:
	touch_event_reset(event);
	return ret;
}


static int read_dismiss_hovering_contact(RdpeiServerContext *context, wStream *s) {
	BYTE contactId;
	if (Stream_GetRemainingLength(s) < 1)
		return -1;

	Stream_Read_UINT8(s, contactId);
	if (context->onTouchReleased)
		context->onTouchReleased(context, contactId);
	return 0;
}


int rdpei_server_handle_messages(RdpeiServerContext *context) {
	DWORD bytesReturned;
	RdpeiServerPrivate *priv = context->priv;
	wStream *s = priv->inputStream;

	if (!WTSVirtualChannelRead(priv->channelHandle, 0, (PCHAR)Stream_Pointer(s), priv->expectedBytes, &bytesReturned))
	{
		if (GetLastError() == ERROR_NO_DATA)
			return -1;

		fprintf(stderr, "%s: channel connection closed\n", __FUNCTION__);
		return 0;
	}
	priv->expectedBytes -= bytesReturned;
	Stream_Seek(s, bytesReturned);

	if (priv->expectedBytes)
		return 1;

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
			fprintf(stderr, "%s: invalid pduLength %d\n", __FUNCTION__, pduLen);
			return -1;
		}
		priv->expectedBytes = pduLen - RDPINPUT_HEADER_LENGTH;
		priv->waitingHeaders = FALSE;
		Stream_SetPosition(s, 0);
		if (priv->expectedBytes)
		{
			if (!Stream_EnsureCapacity(s, priv->expectedBytes))
			{
				fprintf(stderr, "%s: couldn't allocate memory for stream\n", __FUNCTION__);
				return -1;
			}
			return 1;
		}
	}

	/* when here we have the header + the body */
	switch (priv->currentMsgType)
	{
	case EVENTID_CS_READY:
		if (priv->automataState != STATE_WAITING_CLIENT_READY)
		{
			fprintf(stderr, "%s: not expecting a CS_READY packet in this state(%d)\n", __FUNCTION__, (int)priv->automataState);
			return 0;
		}

		if (read_cs_ready_message(context, s) < 0)
			return 0;
		break;

	case EVENTID_TOUCH:
		if (read_touch_event(context, s) < 0)
		{
			fprintf(stderr, "%s: error in touch event packet\n", __FUNCTION__);
			return 0;
		}
		break;
	case EVENTID_DISMISS_HOVERING_CONTACT:
		if (read_dismiss_hovering_contact(context, s) < 0)
		{
			fprintf(stderr, "%s: error reading read_dismiss_hovering_contact\n", __FUNCTION__);
			return 0;
		}
		break;
	default:
		fprintf(stderr, "%s: unexpected message type 0x%x\n", __FUNCTION__, priv->currentMsgType);
	}

	Stream_SetPosition(s, 0);
	priv->waitingHeaders = TRUE;
	priv->expectedBytes = RDPINPUT_HEADER_LENGTH;
	return 1;
}


int rdpei_server_send_sc_ready(RdpeiServerContext *context, UINT32 version)
{
	ULONG written;
	RdpeiServerPrivate *priv = context->priv;

	if (priv->automataState != STATE_INITIAL)
	{
		fprintf(stderr, "%s: called from unexpected state %d\n", __FUNCTION__, priv->automataState);
		return -1;
	}

	Stream_SetPosition(priv->outputStream, 0);

	if (!Stream_EnsureCapacity(priv->outputStream, RDPINPUT_HEADER_LENGTH + 4))
	{
		fprintf(stderr, "%s: could not re-allocate memory for stream\n", __FUNCTION__);
		return -1;
	}

	Stream_Write_UINT16(priv->outputStream, EVENTID_SC_READY);
	Stream_Write_UINT32(priv->outputStream, RDPINPUT_HEADER_LENGTH + 4);
	Stream_Write_UINT32(priv->outputStream, version);

	if (!WTSVirtualChannelWrite(priv->channelHandle, (PCHAR)Stream_Buffer(priv->outputStream),
			Stream_GetPosition(priv->outputStream), &written))
	{
		fprintf(stderr, "%s: error writing ready message\n", __FUNCTION__);
		return -1;
	}

	priv->automataState = STATE_WAITING_CLIENT_READY;
	return 0;
}

int rdpei_server_suspend(RdpeiServerContext *context)
{
	ULONG written;
	RdpeiServerPrivate *priv = context->priv;

	switch (priv->automataState)
	{
	case STATE_SUSPENDED:
		fprintf(stderr, "%s: already suspended\n", __FUNCTION__);
		return 0;
	case STATE_WAITING_FRAME:
		break;
	default:
		fprintf(stderr, "%s: called from unexpected state %d\n", __FUNCTION__, priv->automataState);
		return -1;
	}

	Stream_SetPosition(priv->outputStream, 0);
	if (!Stream_EnsureCapacity(priv->outputStream, RDPINPUT_HEADER_LENGTH))
	{
		fprintf(stderr, "%s: could not re-allocate memory for stream\n", __FUNCTION__);
		return -1;
	}

	Stream_Write_UINT16(priv->outputStream, EVENTID_SUSPEND_TOUCH);
	Stream_Write_UINT32(priv->outputStream, RDPINPUT_HEADER_LENGTH);

	if (!WTSVirtualChannelWrite(priv->channelHandle, (PCHAR)Stream_Buffer(priv->outputStream),
			Stream_GetPosition(priv->outputStream), &written))
	{
		fprintf(stderr, "%s: error writing suspendTouch message\n", __FUNCTION__);
		return -1;
	}

	priv->automataState = STATE_SUSPENDED;
	return 0;
}


int rdpei_server_resume(RdpeiServerContext *context)
{
	ULONG written;
	RdpeiServerPrivate *priv = context->priv;

	switch (priv->automataState)
	{
	case STATE_WAITING_FRAME:
		fprintf(stderr, "%s: not suspended\n", __FUNCTION__);
		return 0;
	case STATE_SUSPENDED:
		break;
	default:
		fprintf(stderr, "%s: called from unexpected state %d\n", __FUNCTION__, priv->automataState);
		return -1;
	}

	Stream_SetPosition(priv->outputStream, 0);
	if (!Stream_EnsureCapacity(priv->outputStream, RDPINPUT_HEADER_LENGTH))
	{
		fprintf(stderr, "%s: couldn't allocate memory for stream\n", __FUNCTION__);
		return -1;
	}

	Stream_Write_UINT16(priv->outputStream, EVENTID_RESUME_TOUCH);
	Stream_Write_UINT32(priv->outputStream, RDPINPUT_HEADER_LENGTH);

	if (!WTSVirtualChannelWrite(priv->channelHandle, (PCHAR)Stream_Buffer(priv->outputStream),
			Stream_GetPosition(priv->outputStream), &written))
	{
		fprintf(stderr, "%s: error writing resumeTouch message\n", __FUNCTION__);
		return -1;
	}

	priv->automataState = STATE_WAITING_FRAME;
	return 0;
}

