/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Multiparty Virtual Channel
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include "encomsp_main.h"

static int encomsp_server_receive_pdu(EncomspServerContext* context, wStream* s)
{
	return 0;
}

static void* encomsp_server_thread(void* arg)
{
	wStream* s;
	DWORD status;
	DWORD nCount;
	void* buffer;
	HANDLE events[8];
	HANDLE ChannelEvent;
	DWORD BytesReturned;
	EncomspServerContext* context;

	context = (EncomspServerContext*) arg;

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

		WTSVirtualChannelRead(context->priv->ChannelHandle, 0, NULL, 0, &BytesReturned);
		if (BytesReturned < 1)
			continue;
		Stream_EnsureRemainingCapacity(s, BytesReturned);
		if (!WTSVirtualChannelRead(context->priv->ChannelHandle, 0,
			(PCHAR) Stream_Buffer(s), Stream_Capacity(s), &BytesReturned))
		{
			break;
		}

		if (0)
		{
			encomsp_server_receive_pdu(context, s);
		}
	}

	Stream_Free(s, TRUE);

	return NULL;
}

static int encomsp_server_start(EncomspServerContext* context)
{
	context->priv->ChannelHandle = WTSVirtualChannelOpen(context->vcm, WTS_CURRENT_SESSION, "encomsp");

	if (!context->priv->ChannelHandle)
		return -1;

	context->priv->StopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	context->priv->Thread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE) encomsp_server_thread, (void*) context, 0, NULL);

	return 0;
}

static int encomsp_server_stop(EncomspServerContext* context)
{
	SetEvent(context->priv->StopEvent);

	WaitForSingleObject(context->priv->Thread, INFINITE);
	CloseHandle(context->priv->Thread);

	return 0;
}

EncomspServerContext* encomsp_server_context_new(HANDLE vcm)
{
	EncomspServerContext* context;

	context = (EncomspServerContext*) calloc(1, sizeof(EncomspServerContext));

	if (context)
	{
		context->vcm = vcm;

		context->Start = encomsp_server_start;
		context->Stop = encomsp_server_stop;

		context->priv = (EncomspServerPrivate*) calloc(1, sizeof(EncomspServerPrivate));

		if (context->priv)
		{

		}
	}

	return context;
}

void encomsp_server_context_free(EncomspServerContext* context)
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
