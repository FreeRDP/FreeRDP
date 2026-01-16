/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDPXXXX Remote App Graphics Redirection Virtual Channel Extension
 *
 * Copyright 2020 Microsoft
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <freerdp/config.h>

#include "gfxredir_main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/stream.h>
#include <winpr/sysinfo.h>
#include <freerdp/channels/wtsvc.h>
#include <freerdp/channels/log.h>

#include <freerdp/server/gfxredir.h>
#include <gfxredir_common.h>

#define TAG CHANNELS_TAG("gfxredir.server")

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gfxredir_recv_legacy_caps_pdu(wStream* s, GfxRedirServerContext* context)
{
	UINT32 error = CHANNEL_RC_OK;
	GFXREDIR_LEGACY_CAPS_PDU pdu = { 0 };

	WINPR_ASSERT(context);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 2))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT16(s, pdu.version); /* version (2 bytes) */

	if (context)
		IFCALLRET(context->GraphicsRedirectionLegacyCaps, error, context, &pdu);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gfxredir_recv_caps_advertise_pdu(wStream* s, UINT32 length,
                                             GfxRedirServerContext* context)
{
	UINT32 error = CHANNEL_RC_OK;
	GFXREDIR_CAPS_ADVERTISE_PDU pdu = { 0 };

	WINPR_ASSERT(context);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, length))
		return ERROR_INVALID_DATA;

	pdu.length = length;
	Stream_GetPointer(s, pdu.caps);
	Stream_Seek(s, length);

	if (!context->GraphicsRedirectionCapsAdvertise)
	{
		WLog_ERR(TAG, "server does not support CapsAdvertise PDU!");
		return ERROR_NOT_SUPPORTED;
	}
	else if (context)
	{
		IFCALLRET(context->GraphicsRedirectionCapsAdvertise, error, context, &pdu);
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gfxredir_recv_present_buffer_ack_pdu(wStream* s, GfxRedirServerContext* context)
{
	UINT32 error = CHANNEL_RC_OK;
	GFXREDIR_PRESENT_BUFFER_ACK_PDU pdu = { 0 };

	WINPR_ASSERT(context);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 16))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT64(s, pdu.windowId);  /* windowId (8 bytes) */
	Stream_Read_UINT64(s, pdu.presentId); /* presentId (8 bytes) */

	if (context)
	{
		IFCALLRET(context->PresentBufferAck, error, context, &pdu);
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gfxredir_server_receive_pdu(GfxRedirServerContext* context, wStream* s)
{
	UINT error = CHANNEL_RC_OK;
	GFXREDIR_HEADER header = { 0 };

	WINPR_ASSERT(context);

	const size_t beg = Stream_GetPosition(s);

	if ((error = gfxredir_read_header(s, &header)))
	{
		WLog_ERR(TAG, "gfxredir_read_header failed with error %" PRIu32 "!", error);
		return error;
	}

	switch (header.cmdId)
	{
		case GFXREDIR_CMDID_LEGACY_CAPS:
			if ((error = gfxredir_recv_legacy_caps_pdu(s, context)))
				WLog_ERR(TAG,
				         "gfxredir_recv_legacy_caps_pdu "
				         "failed with error %" PRIu32 "!",
				         error);

			break;

		case GFXREDIR_CMDID_CAPS_ADVERTISE:
			if ((error = gfxredir_recv_caps_advertise_pdu(s, header.length - 8, context)))
				WLog_ERR(TAG,
				         "gfxredir_recv_caps_advertise "
				         "failed with error %" PRIu32 "!",
				         error);
			break;

		case GFXREDIR_CMDID_PRESENT_BUFFER_ACK:
			if ((error = gfxredir_recv_present_buffer_ack_pdu(s, context)))
				WLog_ERR(TAG,
				         "gfxredir_recv_present_buffer_ask_pdu "
				         "failed with error %" PRIu32 "!",
				         error);
			break;

		default:
			error = CHANNEL_RC_BAD_PROC;
			WLog_WARN(TAG, "Received unknown PDU type: %" PRIu32 "", header.cmdId);
			break;
	}

	const size_t end = Stream_GetPosition(s);

	if (end != (beg + header.length))
	{
		WLog_ERR(TAG, "Unexpected GFXREDIR pdu end: Actual: %" PRIuz ", Expected: %" PRIuz "", end,
		         (beg + header.length));
		Stream_SetPosition(s, (beg + header.length));
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gfxredir_server_handle_messages(GfxRedirServerContext* context)
{
	UINT ret = CHANNEL_RC_OK;

	WINPR_ASSERT(context);

	GfxRedirServerPrivate* priv = context->priv;
	WINPR_ASSERT(priv);

	wStream* s = priv->input_stream;

	/* Check whether the dynamic channel is ready */
	if (!priv->isReady)
	{
		void* buffer = NULL;
		DWORD BytesReturned = 0;
		if (WTSVirtualChannelQuery(priv->gfxredir_channel, WTSVirtualChannelReady, &buffer,
		                           &BytesReturned) == FALSE)
		{
			if (GetLastError() == ERROR_NO_DATA)
				return ERROR_NO_DATA;

			WLog_ERR(TAG, "WTSVirtualChannelQuery failed");
			return ERROR_INTERNAL_ERROR;
		}

		priv->isReady = *((BOOL*)buffer);
		WTSFreeMemory(buffer);
	}

	/* Consume channel event only after the dynamic channel is ready */
	if (priv->isReady)
	{
		Stream_SetPosition(s, 0);

		DWORD BytesReturned = 0;
		if (!WTSVirtualChannelRead(priv->gfxredir_channel, 0, NULL, 0, &BytesReturned))
		{
			if (GetLastError() == ERROR_NO_DATA)
				return ERROR_NO_DATA;

			WLog_ERR(TAG, "WTSVirtualChannelRead failed!");
			return ERROR_INTERNAL_ERROR;
		}

		if (BytesReturned < 1)
			return CHANNEL_RC_OK;

		if (!Stream_EnsureRemainingCapacity(s, BytesReturned))
		{
			WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
			return CHANNEL_RC_NO_MEMORY;
		}

		const ULONG cap = WINPR_ASSERTING_INT_CAST(ULONG, Stream_Capacity(s));
		if (WTSVirtualChannelRead(priv->gfxredir_channel, 0, (PCHAR)Stream_Buffer(s), cap,
		                          &BytesReturned) == FALSE)
		{
			WLog_ERR(TAG, "WTSVirtualChannelRead failed!");
			return ERROR_INTERNAL_ERROR;
		}

		Stream_SetLength(s, BytesReturned);
		Stream_SetPosition(s, 0);

		while (Stream_GetPosition(s) < Stream_Length(s))
		{
			if ((ret = gfxredir_server_receive_pdu(context, s)))
			{
				WLog_ERR(TAG,
				         "gfxredir_server_receive_pdu "
				         "failed with error %" PRIu32 "!",
				         ret);
				return ret;
			}
		}
	}

	return ret;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static DWORD WINAPI gfxredir_server_thread_func(LPVOID arg)
{
	GfxRedirServerContext* context = (GfxRedirServerContext*)arg;
	WINPR_ASSERT(context);

	GfxRedirServerPrivate* priv = context->priv;
	WINPR_ASSERT(priv);

	HANDLE events[8] = { 0 };
	UINT error = CHANNEL_RC_OK;
	DWORD nCount = 0;
	events[nCount++] = priv->stopEvent;
	events[nCount++] = priv->channelEvent;

	while (TRUE)
	{
		const DWORD status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForMultipleObjects failed with error %" PRIu32 "", error);
			break;
		}

		/* Stop Event */
		if (status == WAIT_OBJECT_0)
			break;

		if ((error = gfxredir_server_handle_messages(context)))
		{
			WLog_ERR(TAG, "gfxredir_server_handle_messages failed with error %" PRIu32 "", error);
			break;
		}
	}

	ExitThread(error);
	return error;
}

/**
 * Function description
 * Create new stream for single gfxredir packet. The new stream length
 * would be required data length + header. The header will be written
 * to the stream before return.
 *
 * @param cmdId the command identifier
 * @param length data length without header
 *
 * @return new stream
 */
static wStream* gfxredir_server_single_packet_new(UINT32 cmdId, size_t length)
{
	GFXREDIR_HEADER header = { 0 };
	wStream* s = Stream_New(NULL, GFXREDIR_HEADER_SIZE + length);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		goto error;
	}

	header.cmdId = cmdId;
	header.length = WINPR_ASSERTING_INT_CAST(UINT32, GFXREDIR_HEADER_SIZE + length);

	const UINT error = gfxredir_write_header(s, &header);
	if (error)
	{
		WLog_ERR(TAG, "Failed to write header with error %" PRIu32 "!", error);
		goto error;
	}

	return s;
error:
	Stream_Free(s, TRUE);
	return NULL;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gfxredir_server_packet_send(GfxRedirServerContext* context, wStream* s)
{
	UINT ret = ERROR_INTERNAL_ERROR;
	ULONG written = 0;

	WINPR_ASSERT(context);

	const ULONG cap = WINPR_ASSERTING_INT_CAST(ULONG, Stream_GetPosition(s));
	if (!WTSVirtualChannelWrite(context->priv->gfxredir_channel, (PCHAR)Stream_Buffer(s), cap,
	                            &written))
	{
		WLog_ERR(TAG, "WTSVirtualChannelWrite failed!");
		ret = ERROR_INTERNAL_ERROR;
		goto out;
	}

	if (written < Stream_GetPosition(s))
	{
		WLog_WARN(TAG, "Unexpected bytes written: %" PRIu32 "/%" PRIuz "", written,
		          Stream_GetPosition(s));
	}

	ret = CHANNEL_RC_OK;
out:
	Stream_Free(s, TRUE);
	return ret;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gfxredir_send_error(GfxRedirServerContext* context, const GFXREDIR_ERROR_PDU* error)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(error);

	wStream* s = gfxredir_server_single_packet_new(GFXREDIR_CMDID_ERROR, 4);

	if (!s)
	{
		WLog_ERR(TAG, "gfxredir_server_single_packet_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT32(s, error->errorCode);
	return gfxredir_server_packet_send(context, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gfxredir_send_caps_confirm(GfxRedirServerContext* context,
                                       const GFXREDIR_CAPS_CONFIRM_PDU* graphicsCapsConfirm)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(graphicsCapsConfirm);

	if (graphicsCapsConfirm->length < GFXREDIR_CAPS_HEADER_SIZE)
	{
		WLog_ERR(TAG, "length must be greater than header size, failed!");
		return ERROR_INVALID_DATA;
	}

	wStream* s =
	    gfxredir_server_single_packet_new(GFXREDIR_CMDID_CAPS_CONFIRM, graphicsCapsConfirm->length);

	if (!s)
	{
		WLog_ERR(TAG, "gfxredir_server_single_packet_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT32(s, GFXREDIR_CAPS_SIGNATURE);
	Stream_Write_UINT32(s, graphicsCapsConfirm->version);
	Stream_Write_UINT32(s, graphicsCapsConfirm->length);
	if (graphicsCapsConfirm->length > GFXREDIR_CAPS_HEADER_SIZE)
		Stream_Write(s, graphicsCapsConfirm->capsData,
		             graphicsCapsConfirm->length - GFXREDIR_CAPS_HEADER_SIZE);
	const UINT ret = gfxredir_server_packet_send(context, s);
	if (ret == CHANNEL_RC_OK)
		context->confirmedCapsVersion = graphicsCapsConfirm->version;
	return ret;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gfxredir_send_open_pool(GfxRedirServerContext* context,
                                    const GFXREDIR_OPEN_POOL_PDU* openPool)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(openPool);

	if (context->confirmedCapsVersion != GFXREDIR_CAPS_VERSION2_0)
	{
		WLog_ERR(TAG, "open_pool is called with invalid version!");
		return ERROR_INTERNAL_ERROR;
	}

	if (openPool->sectionNameLength == 0 || openPool->sectionName == NULL)
	{
		WLog_ERR(TAG, "section name must be provided!");
		return ERROR_INVALID_DATA;
	}

	/* make sure null-terminate */
	if (openPool->sectionName[openPool->sectionNameLength - 1] != 0)
	{
		WLog_ERR(TAG, "section name must be terminated with NULL!");
		return ERROR_INVALID_DATA;
	}

	wStream* s = gfxredir_server_single_packet_new(GFXREDIR_CMDID_OPEN_POOL,
	                                               20 + (openPool->sectionNameLength * 2));

	if (!s)
	{
		WLog_ERR(TAG, "gfxredir_server_single_packet_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT64(s, openPool->poolId);
	Stream_Write_UINT64(s, openPool->poolSize);
	Stream_Write_UINT32(s, openPool->sectionNameLength);
	Stream_Write(s, openPool->sectionName, (2ULL * openPool->sectionNameLength));
	return gfxredir_server_packet_send(context, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gfxredir_send_close_pool(GfxRedirServerContext* context,
                                     const GFXREDIR_CLOSE_POOL_PDU* closePool)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(closePool);

	if (context->confirmedCapsVersion != GFXREDIR_CAPS_VERSION2_0)
	{
		WLog_ERR(TAG, "close_pool is called with invalid version!");
		return ERROR_INTERNAL_ERROR;
	}

	wStream* s = gfxredir_server_single_packet_new(GFXREDIR_CMDID_CLOSE_POOL, 8);

	if (!s)
	{
		WLog_ERR(TAG, "gfxredir_server_single_packet_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT64(s, closePool->poolId);
	return gfxredir_server_packet_send(context, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gfxredir_send_create_buffer(GfxRedirServerContext* context,
                                        const GFXREDIR_CREATE_BUFFER_PDU* createBuffer)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(createBuffer);

	if (context->confirmedCapsVersion != GFXREDIR_CAPS_VERSION2_0)
	{
		WLog_ERR(TAG, "create_buffer is called with invalid version!");
		return ERROR_INTERNAL_ERROR;
	}

	wStream* s = gfxredir_server_single_packet_new(GFXREDIR_CMDID_CREATE_BUFFER, 40);

	if (!s)
	{
		WLog_ERR(TAG, "gfxredir_server_single_packet_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT64(s, createBuffer->poolId);
	Stream_Write_UINT64(s, createBuffer->bufferId);
	Stream_Write_UINT64(s, createBuffer->offset);
	Stream_Write_UINT32(s, createBuffer->stride);
	Stream_Write_UINT32(s, createBuffer->width);
	Stream_Write_UINT32(s, createBuffer->height);
	Stream_Write_UINT32(s, createBuffer->format);
	return gfxredir_server_packet_send(context, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gfxredir_send_destroy_buffer(GfxRedirServerContext* context,
                                         const GFXREDIR_DESTROY_BUFFER_PDU* destroyBuffer)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(destroyBuffer);

	if (context->confirmedCapsVersion != GFXREDIR_CAPS_VERSION2_0)
	{
		WLog_ERR(TAG, "destroy_buffer is called with invalid version!");
		return ERROR_INTERNAL_ERROR;
	}

	wStream* s = gfxredir_server_single_packet_new(GFXREDIR_CMDID_DESTROY_BUFFER, 8);

	if (!s)
	{
		WLog_ERR(TAG, "gfxredir_server_single_packet_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT64(s, destroyBuffer->bufferId);
	return gfxredir_server_packet_send(context, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gfxredir_send_present_buffer(GfxRedirServerContext* context,
                                         const GFXREDIR_PRESENT_BUFFER_PDU* presentBuffer)
{
	RECTANGLE_32 dummyRect = { 0, 0, 0, 0 };

	WINPR_ASSERT(context);
	WINPR_ASSERT(presentBuffer);

	if (context->confirmedCapsVersion != GFXREDIR_CAPS_VERSION2_0)
	{
		WLog_ERR(TAG, "present_buffer is called with invalid version!");
		return ERROR_INTERNAL_ERROR;
	}

	if (presentBuffer->numOpaqueRects > GFXREDIR_MAX_OPAQUE_RECTS)
	{
		WLog_ERR(TAG, "numOpaqueRects is larger than its limit!");
		return ERROR_INVALID_DATA;
	}

	const size_t length =
	    64ULL + ((presentBuffer->numOpaqueRects ? presentBuffer->numOpaqueRects : 1) *
	             sizeof(RECTANGLE_32));

	wStream* s = gfxredir_server_single_packet_new(GFXREDIR_CMDID_PRESENT_BUFFER, length);

	if (!s)
	{
		WLog_ERR(TAG, "gfxredir_server_single_packet_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT64(s, presentBuffer->timestamp);
	Stream_Write_UINT64(s, presentBuffer->presentId);
	Stream_Write_UINT64(s, presentBuffer->windowId);
	Stream_Write_UINT64(s, presentBuffer->bufferId);
	Stream_Write_UINT32(s, presentBuffer->orientation);
	Stream_Write_UINT32(s, presentBuffer->targetWidth);
	Stream_Write_UINT32(s, presentBuffer->targetHeight);
	Stream_Write_UINT32(s, presentBuffer->dirtyRect.left);
	Stream_Write_UINT32(s, presentBuffer->dirtyRect.top);
	Stream_Write_UINT32(s, presentBuffer->dirtyRect.width);
	Stream_Write_UINT32(s, presentBuffer->dirtyRect.height);
	Stream_Write_UINT32(s, presentBuffer->numOpaqueRects);
	if (presentBuffer->numOpaqueRects)
		Stream_Write(s, presentBuffer->opaqueRects,
		             (presentBuffer->numOpaqueRects * sizeof(RECTANGLE_32)));
	else
		Stream_Write(s, &dummyRect, sizeof(RECTANGLE_32));
	return gfxredir_server_packet_send(context, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gfxredir_server_open(GfxRedirServerContext* context)
{
	UINT rc = ERROR_INTERNAL_ERROR;
	WINPR_ASSERT(context);

	GfxRedirServerPrivate* priv = context->priv;
	WINPR_ASSERT(priv);

	DWORD BytesReturned = 0;
	PULONG pSessionId = NULL;
	void* buffer = NULL;
	priv->SessionId = WTS_CURRENT_SESSION;

	if (WTSQuerySessionInformationA(context->vcm, WTS_CURRENT_SESSION, WTSSessionId,
	                                (LPSTR*)&pSessionId, &BytesReturned) == FALSE)
	{
		WLog_ERR(TAG, "WTSQuerySessionInformationA failed!");
		rc = ERROR_INTERNAL_ERROR;
		goto out_close;
	}

	priv->SessionId = (DWORD)*pSessionId;
	WTSFreeMemory(pSessionId);
	priv->gfxredir_channel = WTSVirtualChannelOpenEx(priv->SessionId, GFXREDIR_DVC_CHANNEL_NAME,
	                                                 WTS_CHANNEL_OPTION_DYNAMIC);

	if (!priv->gfxredir_channel)
	{
		WLog_ERR(TAG, "WTSVirtualChannelOpenEx failed!");
		rc = GetLastError();
		goto out_close;
	}

	/* Query for channel event handle */
	if (!WTSVirtualChannelQuery(priv->gfxredir_channel, WTSVirtualEventHandle, &buffer,
	                            &BytesReturned) ||
	    (BytesReturned != sizeof(HANDLE)))
	{
		WLog_ERR(TAG,
		         "WTSVirtualChannelQuery failed "
		         "or invalid returned size(%" PRIu32 ")",
		         BytesReturned);

		if (buffer)
			WTSFreeMemory(buffer);

		rc = ERROR_INTERNAL_ERROR;
		goto out_close;
	}

	priv->channelEvent = *(HANDLE*)buffer;
	WTSFreeMemory(buffer);

	if (priv->thread == NULL)
	{
		if (!(priv->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
		{
			WLog_ERR(TAG, "CreateEvent failed!");
			goto out_close;
		}

		if (!(priv->thread =
		          CreateThread(NULL, 0, gfxredir_server_thread_func, (void*)context, 0, NULL)))
		{
			WLog_ERR(TAG, "CreateEvent failed!");
			(void)CloseHandle(priv->stopEvent);
			priv->stopEvent = NULL;
			goto out_close;
		}
	}

	return CHANNEL_RC_OK;
out_close:
	WTSVirtualChannelClose(priv->gfxredir_channel);
	priv->gfxredir_channel = NULL;
	priv->channelEvent = NULL;
	return rc;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gfxredir_server_close(GfxRedirServerContext* context)
{
	UINT error = CHANNEL_RC_OK;
	WINPR_ASSERT(context);

	GfxRedirServerPrivate* priv = context->priv;
	WINPR_ASSERT(priv);

	if (priv->thread)
	{
		(void)SetEvent(priv->stopEvent);

		if (WaitForSingleObject(priv->thread, INFINITE) == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %" PRIu32 "", error);
			return error;
		}

		(void)CloseHandle(priv->thread);
		(void)CloseHandle(priv->stopEvent);
		priv->thread = NULL;
		priv->stopEvent = NULL;
	}

	if (priv->gfxredir_channel)
	{
		WTSVirtualChannelClose(priv->gfxredir_channel);
		priv->gfxredir_channel = NULL;
	}

	return error;
}

GfxRedirServerContext* gfxredir_server_context_new(HANDLE vcm)
{
	GfxRedirServerContext* context =
	    (GfxRedirServerContext*)calloc(1, sizeof(GfxRedirServerContext));

	if (!context)
	{
		WLog_ERR(TAG, "gfxredir_server_context_new(): calloc GfxRedirServerContext failed!");
		return NULL;
	}

	GfxRedirServerPrivate* priv = context->priv =
	    (GfxRedirServerPrivate*)calloc(1, sizeof(GfxRedirServerPrivate));

	if (!context->priv)
	{
		WLog_ERR(TAG, "gfxredir_server_context_new(): calloc GfxRedirServerPrivate failed!");
		goto fail;
	}

	priv->input_stream = Stream_New(NULL, 4);

	if (!priv->input_stream)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		goto fail;
	}

	context->vcm = vcm;
	context->Open = gfxredir_server_open;
	context->Close = gfxredir_server_close;
	context->Error = gfxredir_send_error;
	context->GraphicsRedirectionCapsConfirm = gfxredir_send_caps_confirm;
	context->OpenPool = gfxredir_send_open_pool;
	context->ClosePool = gfxredir_send_close_pool;
	context->CreateBuffer = gfxredir_send_create_buffer;
	context->DestroyBuffer = gfxredir_send_destroy_buffer;
	context->PresentBuffer = gfxredir_send_present_buffer;
	context->confirmedCapsVersion = GFXREDIR_CAPS_VERSION1;
	priv->isReady = FALSE;
	return context;
fail:
	gfxredir_server_context_free(context);
	return NULL;
}

void gfxredir_server_context_free(GfxRedirServerContext* context)
{
	if (!context)
		return;

	gfxredir_server_close(context);

	if (context->priv)
	{
		Stream_Free(context->priv->input_stream, TRUE);
		free(context->priv);
	}

	free(context);
}
