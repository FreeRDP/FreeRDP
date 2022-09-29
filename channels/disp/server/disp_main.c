/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDPEDISP Virtual Channel Extension
 *
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
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

#include "disp_main.h"
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

#include <freerdp/server/disp.h>
#include "../disp_common.h"

#define TAG CHANNELS_TAG("rdpedisp.server")

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */

static wStream* disp_server_single_packet_new(UINT32 type, UINT32 length)
{
	UINT error;
	DISPLAY_CONTROL_HEADER header;
	wStream* s = Stream_New(NULL, DISPLAY_CONTROL_HEADER_LENGTH + length);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		goto error;
	}

	header.type = type;
	header.length = DISPLAY_CONTROL_HEADER_LENGTH + length;

	if ((error = disp_write_header(s, &header)))
	{
		WLog_ERR(TAG, "Failed to write header with error %" PRIu32 "!", error);
		goto error;
	}

	return s;
error:
	Stream_Free(s, TRUE);
	return NULL;
}

static void disp_server_sanitize_monitor_layout(DISPLAY_CONTROL_MONITOR_LAYOUT* monitor)
{
	if (monitor->PhysicalWidth < DISPLAY_CONTROL_MIN_PHYSICAL_MONITOR_WIDTH ||
	    monitor->PhysicalWidth > DISPLAY_CONTROL_MAX_PHYSICAL_MONITOR_WIDTH ||
	    monitor->PhysicalHeight < DISPLAY_CONTROL_MIN_PHYSICAL_MONITOR_HEIGHT ||
	    monitor->PhysicalHeight > DISPLAY_CONTROL_MAX_PHYSICAL_MONITOR_HEIGHT)
	{
		if (monitor->PhysicalWidth != 0 || monitor->PhysicalHeight != 0)
			WLog_DBG(
			    TAG,
			    "Sanitizing invalid physical monitor size. Old physical monitor size: [%" PRIu32
			    ", %" PRIu32 "]",
			    monitor->PhysicalWidth, monitor->PhysicalHeight);

		monitor->PhysicalWidth = monitor->PhysicalHeight = 0;
	}
}

static BOOL disp_server_is_monitor_layout_valid(const DISPLAY_CONTROL_MONITOR_LAYOUT* monitor)
{
	WINPR_ASSERT(monitor);

	if (monitor->Width < DISPLAY_CONTROL_MIN_MONITOR_WIDTH ||
	    monitor->Width > DISPLAY_CONTROL_MAX_MONITOR_WIDTH)
	{
		WLog_WARN(TAG, "Received invalid value for monitor->Width: %" PRIu32 "", monitor->Width);
		return FALSE;
	}

	if (monitor->Height < DISPLAY_CONTROL_MIN_MONITOR_HEIGHT ||
	    monitor->Height > DISPLAY_CONTROL_MAX_MONITOR_HEIGHT)
	{
		WLog_WARN(TAG, "Received invalid value for monitor->Height: %" PRIu32 "", monitor->Width);
		return FALSE;
	}

	switch (monitor->Orientation)
	{
		case ORIENTATION_LANDSCAPE:
		case ORIENTATION_PORTRAIT:
		case ORIENTATION_LANDSCAPE_FLIPPED:
		case ORIENTATION_PORTRAIT_FLIPPED:
			break;

		default:
			WLog_WARN(TAG, "Received incorrect value for monitor->Orientation: %" PRIu32 "",
			          monitor->Orientation);
			return FALSE;
	}

	return TRUE;
}

static UINT disp_recv_display_control_monitor_layout_pdu(wStream* s, DispServerContext* context)
{
	UINT32 error = CHANNEL_RC_OK;
	UINT32 index;
	DISPLAY_CONTROL_MONITOR_LAYOUT_PDU pdu = { 0 };

	WINPR_ASSERT(s);
	WINPR_ASSERT(context);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, pdu.MonitorLayoutSize); /* MonitorLayoutSize (4 bytes) */

	if (pdu.MonitorLayoutSize != DISPLAY_CONTROL_MONITOR_LAYOUT_SIZE)
	{
		WLog_ERR(TAG, "MonitorLayoutSize is set to %" PRIu32 ". expected %" PRIu32 "",
		         pdu.MonitorLayoutSize, DISPLAY_CONTROL_MONITOR_LAYOUT_SIZE);
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, pdu.NumMonitors); /* NumMonitors (4 bytes) */

	if (pdu.NumMonitors > context->MaxNumMonitors)
	{
		WLog_ERR(TAG, "NumMonitors (%" PRIu32 ")> server MaxNumMonitors (%" PRIu32 ")",
		         pdu.NumMonitors, context->MaxNumMonitors);
		return ERROR_INVALID_DATA;
	}

	if (!Stream_CheckAndLogRequiredLength(
	        TAG, s, pdu.NumMonitors * 1ull * DISPLAY_CONTROL_MONITOR_LAYOUT_SIZE))
		return ERROR_INVALID_DATA;

	pdu.Monitors = (DISPLAY_CONTROL_MONITOR_LAYOUT*)calloc(pdu.NumMonitors,
	                                                       sizeof(DISPLAY_CONTROL_MONITOR_LAYOUT));

	if (!pdu.Monitors)
	{
		WLog_ERR(TAG, "disp_recv_display_control_monitor_layout_pdu(): calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	WLog_DBG(TAG, "disp_recv_display_control_monitor_layout_pdu: NumMonitors=%" PRIu32 "",
	         pdu.NumMonitors);

	for (index = 0; index < pdu.NumMonitors; index++)
	{
		DISPLAY_CONTROL_MONITOR_LAYOUT* monitor = &(pdu.Monitors[index]);

		Stream_Read_UINT32(s, monitor->Flags);              /* Flags (4 bytes) */
		Stream_Read_UINT32(s, monitor->Left);               /* Left (4 bytes) */
		Stream_Read_UINT32(s, monitor->Top);                /* Top (4 bytes) */
		Stream_Read_UINT32(s, monitor->Width);              /* Width (4 bytes) */
		Stream_Read_UINT32(s, monitor->Height);             /* Height (4 bytes) */
		Stream_Read_UINT32(s, monitor->PhysicalWidth);      /* PhysicalWidth (4 bytes) */
		Stream_Read_UINT32(s, monitor->PhysicalHeight);     /* PhysicalHeight (4 bytes) */
		Stream_Read_UINT32(s, monitor->Orientation);        /* Orientation (4 bytes) */
		Stream_Read_UINT32(s, monitor->DesktopScaleFactor); /* DesktopScaleFactor (4 bytes) */
		Stream_Read_UINT32(s, monitor->DeviceScaleFactor);  /* DeviceScaleFactor (4 bytes) */

		disp_server_sanitize_monitor_layout(monitor);
		WLog_DBG(TAG,
		         "\t%" PRIu32 " : Flags: 0x%08" PRIX32 " Left/Top: (%" PRId32 ",%" PRId32
		         ") W/H=%" PRIu32 "x%" PRIu32 ")",
		         index, monitor->Flags, monitor->Left, monitor->Top, monitor->Width,
		         monitor->Height);
		WLog_DBG(TAG,
		         "\t   PhysicalWidth: %" PRIu32 " PhysicalHeight: %" PRIu32 " Orientation: %" PRIu32
		         "",
		         monitor->PhysicalWidth, monitor->PhysicalHeight, monitor->Orientation);

		if (!disp_server_is_monitor_layout_valid(monitor))
		{
			error = ERROR_INVALID_DATA;
			goto out;
		}
	}

	if (context)
		IFCALLRET(context->DispMonitorLayout, error, context, &pdu);

out:
	free(pdu.Monitors);
	return error;
}

static UINT disp_server_receive_pdu(DispServerContext* context, wStream* s)
{
	UINT error = CHANNEL_RC_OK;
	size_t beg, end;
	DISPLAY_CONTROL_HEADER header = { 0 };

	WINPR_ASSERT(s);
	WINPR_ASSERT(context);

	beg = Stream_GetPosition(s);

	if ((error = disp_read_header(s, &header)))
	{
		WLog_ERR(TAG, "disp_read_header failed with error %" PRIu32 "!", error);
		return error;
	}

	switch (header.type)
	{
		case DISPLAY_CONTROL_PDU_TYPE_MONITOR_LAYOUT:
			if ((error = disp_recv_display_control_monitor_layout_pdu(s, context)))
				WLog_ERR(TAG,
				         "disp_recv_display_control_monitor_layout_pdu "
				         "failed with error %" PRIu32 "!",
				         error);

			break;

		default:
			error = CHANNEL_RC_BAD_PROC;
			WLog_WARN(TAG, "Received unknown PDU type: %" PRIu32 "", header.type);
			break;
	}

	end = Stream_GetPosition(s);

	if (end != (beg + header.length))
	{
		WLog_ERR(TAG, "Unexpected DISP pdu end: Actual: %" PRIuz ", Expected: %" PRIuz "", end,
		         (beg + header.length));
		Stream_SetPosition(s, (beg + header.length));
	}

	return error;
}

static UINT disp_server_handle_messages(DispServerContext* context)
{
	DWORD BytesReturned;
	void* buffer;
	UINT ret = CHANNEL_RC_OK;
	DispServerPrivate* priv;
	wStream* s;

	WINPR_ASSERT(context);

	priv = context->priv;
	WINPR_ASSERT(priv);

	s = priv->input_stream;
	WINPR_ASSERT(s);

	/* Check whether the dynamic channel is ready */
	if (!priv->isReady)
	{
		if (WTSVirtualChannelQuery(priv->disp_channel, WTSVirtualChannelReady, &buffer,
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

	/* Consume channel event only after the disp dynamic channel is ready */
	Stream_SetPosition(s, 0);

	if (!WTSVirtualChannelRead(priv->disp_channel, 0, NULL, 0, &BytesReturned))
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

	if (WTSVirtualChannelRead(priv->disp_channel, 0, (PCHAR)Stream_Buffer(s), Stream_Capacity(s),
	                          &BytesReturned) == FALSE)
	{
		WLog_ERR(TAG, "WTSVirtualChannelRead failed!");
		return ERROR_INTERNAL_ERROR;
	}

	Stream_SetLength(s, BytesReturned);
	Stream_SetPosition(s, 0);

	while (Stream_GetPosition(s) < Stream_Length(s))
	{
		if ((ret = disp_server_receive_pdu(context, s)))
		{
			WLog_ERR(TAG,
			         "disp_server_receive_pdu "
			         "failed with error %" PRIu32 "!",
			         ret);
			return ret;
		}
	}

	return ret;
}

static DWORD WINAPI disp_server_thread_func(LPVOID arg)
{
	DispServerContext* context = (DispServerContext*)arg;
	DispServerPrivate* priv;
	DWORD status;
	DWORD nCount = 0;
	HANDLE events[8] = { 0 };
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(context);

	priv = context->priv;
	WINPR_ASSERT(priv);

	events[nCount++] = priv->stopEvent;
	events[nCount++] = priv->channelEvent;

	/* Main virtual channel loop. RDPEDISP do not need version negotiation */
	while (TRUE)
	{
		status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForMultipleObjects failed with error %" PRIu32 "", error);
			break;
		}

		/* Stop Event */
		if (status == WAIT_OBJECT_0)
			break;

		if ((error = disp_server_handle_messages(context)))
		{
			WLog_ERR(TAG, "disp_server_handle_messages failed with error %" PRIu32 "", error);
			break;
		}
	}

	ExitThread(error);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT disp_server_open(DispServerContext* context)
{
	UINT rc = ERROR_INTERNAL_ERROR;
	DispServerPrivate* priv;
	DWORD BytesReturned = 0;
	PULONG pSessionId = NULL;
	void* buffer = NULL;
	UINT32 channelId;
	BOOL status = TRUE;

	WINPR_ASSERT(context);

	priv = context->priv;
	WINPR_ASSERT(priv);

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
	priv->disp_channel = (HANDLE)WTSVirtualChannelOpenEx(priv->SessionId, DISP_DVC_CHANNEL_NAME,
	                                                     WTS_CHANNEL_OPTION_DYNAMIC);

	if (!priv->disp_channel)
	{
		WLog_ERR(TAG, "WTSVirtualChannelOpenEx failed!");
		rc = GetLastError();
		goto out_close;
	}

	channelId = WTSChannelGetIdByHandle(priv->disp_channel);

	IFCALLRET(context->ChannelIdAssigned, status, context, channelId);
	if (!status)
	{
		WLog_ERR(TAG, "context->ChannelIdAssigned failed!");
		rc = ERROR_INTERNAL_ERROR;
		goto out_close;
	}

	/* Query for channel event handle */
	if (!WTSVirtualChannelQuery(priv->disp_channel, WTSVirtualEventHandle, &buffer,
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

	CopyMemory(&priv->channelEvent, buffer, sizeof(HANDLE));
	WTSFreeMemory(buffer);

	if (priv->thread == NULL)
	{
		if (!(priv->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
		{
			WLog_ERR(TAG, "CreateEvent failed!");
			rc = ERROR_INTERNAL_ERROR;
			goto out_close;
		}

		if (!(priv->thread =
		          CreateThread(NULL, 0, disp_server_thread_func, (void*)context, 0, NULL)))
		{
			WLog_ERR(TAG, "CreateEvent failed!");
			CloseHandle(priv->stopEvent);
			priv->stopEvent = NULL;
			rc = ERROR_INTERNAL_ERROR;
			goto out_close;
		}
	}

	return CHANNEL_RC_OK;
out_close:
	WTSVirtualChannelClose(priv->disp_channel);
	priv->disp_channel = NULL;
	priv->channelEvent = NULL;
	return rc;
}

static UINT disp_server_packet_send(DispServerContext* context, wStream* s)
{
	UINT ret;
	ULONG written;

	WINPR_ASSERT(context);
	WINPR_ASSERT(s);

	if (!WTSVirtualChannelWrite(context->priv->disp_channel, (PCHAR)Stream_Buffer(s),
	                            Stream_GetPosition(s), &written))
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
static UINT disp_server_send_caps_pdu(DispServerContext* context)
{
	wStream* s;

	WINPR_ASSERT(context);

	s = disp_server_single_packet_new(DISPLAY_CONTROL_PDU_TYPE_CAPS, 12);

	if (!s)
	{
		WLog_ERR(TAG, "disp_server_single_packet_new failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write_UINT32(s, context->MaxNumMonitors);        /* MaxNumMonitors (4 bytes) */
	Stream_Write_UINT32(s, context->MaxMonitorAreaFactorA); /* MaxMonitorAreaFactorA (4 bytes) */
	Stream_Write_UINT32(s, context->MaxMonitorAreaFactorB); /* MaxMonitorAreaFactorB (4 bytes) */
	return disp_server_packet_send(context, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT disp_server_close(DispServerContext* context)
{
	UINT error = CHANNEL_RC_OK;
	DispServerPrivate* priv;

	WINPR_ASSERT(context);

	priv = context->priv;
	WINPR_ASSERT(priv);

	if (priv->thread)
	{
		SetEvent(priv->stopEvent);

		if (WaitForSingleObject(priv->thread, INFINITE) == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %" PRIu32 "", error);
			return error;
		}

		CloseHandle(priv->thread);
		CloseHandle(priv->stopEvent);
		priv->thread = NULL;
		priv->stopEvent = NULL;
	}

	if (priv->disp_channel)
	{
		WTSVirtualChannelClose(priv->disp_channel);
		priv->disp_channel = NULL;
	}

	return error;
}

DispServerContext* disp_server_context_new(HANDLE vcm)
{
	DispServerContext* context;
	DispServerPrivate* priv;
	context = (DispServerContext*)calloc(1, sizeof(DispServerContext));

	if (!context)
	{
		WLog_ERR(TAG, "disp_server_context_new(): calloc DispServerContext failed!");
		goto fail;
	}

	priv = context->priv = (DispServerPrivate*)calloc(1, sizeof(DispServerPrivate));

	if (!context->priv)
	{
		WLog_ERR(TAG, "disp_server_context_new(): calloc DispServerPrivate failed!");
		goto fail;
	}

	priv->input_stream = Stream_New(NULL, 4);

	if (!priv->input_stream)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		goto fail;
	}

	context->vcm = vcm;
	context->Open = disp_server_open;
	context->Close = disp_server_close;
	context->DisplayControlCaps = disp_server_send_caps_pdu;
	priv->isReady = FALSE;
	return context;
fail:
	disp_server_context_free(context);
	return NULL;
}

void disp_server_context_free(DispServerContext* context)
{
	if (!context)
		return;

	if (context->priv)
	{
		disp_server_close(context);
		Stream_Free(context->priv->input_stream, TRUE);
		free(context->priv);
	}

	free(context);
}
