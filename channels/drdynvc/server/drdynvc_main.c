/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Dynamic Virtual Channel Extension
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

#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/stream.h>

#include "drdynvc_main.h"

static void* drdynvc_server_thread(void* arg)
{
	wStream* s;
	DWORD status;
	DWORD nCount;
	void* buffer;
	HANDLE events[8];
	HANDLE ChannelEvent;
	DWORD BytesReturned;
	DrdynvcServerContext* context;

	context = (DrdynvcServerContext*) arg;

	buffer = NULL;
	BytesReturned = 0;
	ChannelEvent = NULL;

	s = Stream_New(NULL, 4096);

	if (WTSVirtualChannelQuery(context->priv->ChannelHandle, WTSVirtualEventHandle, &buffer, &BytesReturned) == TRUE)
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
			break;
		}

		if (WTSVirtualChannelRead(context->priv->ChannelHandle, 0,
				(PCHAR) Stream_Buffer(s), Stream_Capacity(s), &BytesReturned))
		{
			if (BytesReturned)
				Stream_Seek(s, BytesReturned);
		}
		else
		{
			Stream_EnsureRemainingCapacity(s, BytesReturned);
		}
	}

	Stream_Free(s, TRUE);

	return NULL;
}

static int drdynvc_server_start(DrdynvcServerContext* context)
{
	context->priv->ChannelHandle = WTSVirtualChannelManagerOpenEx(context->vcm, "rdpdr", 0);

	if (!context->priv->ChannelHandle)
		return -1;

	context->priv->StopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	context->priv->Thread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE) drdynvc_server_thread, (void*) context, 0, NULL);

	return 0;
}

static int drdynvc_server_stop(DrdynvcServerContext* context)
{
	SetEvent(context->priv->StopEvent);

	WaitForSingleObject(context->priv->Thread, INFINITE);
	CloseHandle(context->priv->Thread);

	return 0;
}

DrdynvcServerContext* drdynvc_server_context_new(WTSVirtualChannelManager* vcm)
{
	DrdynvcServerContext* context;

	context = (DrdynvcServerContext*) malloc(sizeof(DrdynvcServerContext));

	if (context)
	{
		ZeroMemory(context, sizeof(DrdynvcServerContext));

		context->vcm = vcm;

		context->Start = drdynvc_server_start;
		context->Stop = drdynvc_server_stop;

		context->priv = (DrdynvcServerPrivate*) malloc(sizeof(DrdynvcServerPrivate));

		if (context->priv)
		{
			ZeroMemory(context->priv, sizeof(DrdynvcServerPrivate));
		}
	}

	return context;
}

void drdynvc_server_context_free(DrdynvcServerContext* context)
{
	if (context)
	{
		if (context->priv)
		{
			free(context->priv);
		}

		free(context);
	}
}
