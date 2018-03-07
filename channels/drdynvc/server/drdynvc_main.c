/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Dynamic Virtual Channel Extension
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

#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/stream.h>
#include <freerdp/channels/log.h>

#include "drdynvc_main.h"

#define TAG CHANNELS_TAG("drdynvc.server")


static DWORD WINAPI drdynvc_server_thread(LPVOID arg)
{
#if 0
	wStream* s;
	DWORD status;
	DWORD nCount;
	void* buffer;
	HANDLE events[8];
	HANDLE ChannelEvent;
	DWORD BytesReturned;
	DrdynvcServerContext* context;
	UINT error = ERROR_INTERNAL_ERROR;
	context = (DrdynvcServerContext*) arg;
	buffer = NULL;
	BytesReturned = 0;
	ChannelEvent = NULL;

	s = Stream_New(NULL, 4096);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		ExitThread((DWORD) CHANNEL_RC_NO_MEMORY);
		return CHANNEL_RC_NO_MEMORY;
	}

	if (WTSVirtualChannelQuery(context->priv->ChannelHandle, WTSVirtualEventHandle,
	                           &buffer, &BytesReturned) == TRUE)
	{
		if (BytesReturned == sizeof(HANDLE))
			CopyMemory(&ChannelEvent, buffer, sizeof(HANDLE));

		WTSFreeMemory(buffer);
	}

	nCount = 0;
	events[nCount++] = ChannelEvent;
	events[nCount++] = context->priv->StopEvent;

	while (1)
	{
		status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);

		if (WaitForSingleObject(context->priv->StopEvent, 0) == WAIT_OBJECT_0)
		{
			error = CHANNEL_RC_OK;
			break;
		}

		if (!WTSVirtualChannelRead(context->priv->ChannelHandle, 0, NULL, 0,
		                           &BytesReturned))
		{
			WLog_ERR(TAG, "WTSVirtualChannelRead failed!");
			break;
		}

		if (BytesReturned < 1)
			continue;

		if (!Stream_EnsureRemainingCapacity(s, BytesReturned))
		{
			WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
			break;
		}

		if (!WTSVirtualChannelRead(context->priv->ChannelHandle, 0,
		                           (PCHAR) Stream_Buffer(s), Stream_Capacity(s), &BytesReturned))
		{
			WLog_ERR(TAG, "WTSVirtualChannelRead failed!");
			break;
		}
	}

	Stream_Free(s, TRUE);
	ExitThread((DWORD) error);
#endif
	// WTF ... this code only reads data into the stream until there is no more memory
	ExitThread(0);
	return 0;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT drdynvc_server_start(DrdynvcServerContext* context)
{
	context->priv->ChannelHandle = WTSVirtualChannelOpen(context->vcm,
	                               WTS_CURRENT_SESSION, "drdynvc");

	if (!context->priv->ChannelHandle)
	{
		WLog_ERR(TAG, "WTSVirtualChannelOpen failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	if (!(context->priv->StopEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
	{
		WLog_ERR(TAG, "CreateEvent failed!");
		return ERROR_INTERNAL_ERROR;
	}

	if (!(context->priv->Thread = CreateThread(NULL, 0, drdynvc_server_thread, (void*) context, 0, NULL)))
	{
		WLog_ERR(TAG, "CreateThread failed!");
		CloseHandle(context->priv->StopEvent);
		context->priv->StopEvent = NULL;
		return ERROR_INTERNAL_ERROR;
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT drdynvc_server_stop(DrdynvcServerContext* context)
{
	UINT error;
	SetEvent(context->priv->StopEvent);

	if (WaitForSingleObject(context->priv->Thread, INFINITE) == WAIT_FAILED)
	{
		error = GetLastError();
		WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"!", error);
		return error;
	}

	CloseHandle(context->priv->Thread);
	return CHANNEL_RC_OK;
}

DrdynvcServerContext* drdynvc_server_context_new(HANDLE vcm)
{
	DrdynvcServerContext* context;
	context = (DrdynvcServerContext*) calloc(1, sizeof(DrdynvcServerContext));

	if (context)
	{
		context->vcm = vcm;
		context->Start = drdynvc_server_start;
		context->Stop = drdynvc_server_stop;
		context->priv = (DrdynvcServerPrivate*) calloc(1, sizeof(DrdynvcServerPrivate));

		if (!context->priv)
		{
			WLog_ERR(TAG, "calloc failed!");
			free(context);
			return NULL;
		}
	}
	else
	{
		WLog_ERR(TAG, "calloc failed!");
	}

	return context;
}

void drdynvc_server_context_free(DrdynvcServerContext* context)
{
	if (context)
	{
		free(context->priv);
		free(context);
	}
}
