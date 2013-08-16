/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Clipboard Virtual Channel Extension
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
#include <winpr/stream.h>

#include "cliprdr_main.h"

/**
 *                                    Initialization Sequence\n
 *     Client                                                                    Server\n
 *        |                                                                         |\n
 *        |<----------------------Server Clipboard Capabilities PDU-----------------|\n
 *        |<-----------------------------Monitor Ready PDU--------------------------|\n
 *        |-----------------------Client Clipboard Capabilities PDU---------------->|\n
 *        |---------------------------Temporary Directory PDU---------------------->|\n
 *        |-------------------------------Format List PDU-------------------------->|\n
 *        |<--------------------------Format List Response PDU----------------------|\n
 *
 */

static int cliprdr_server_send_capabilities(CliprdrServerContext* context)
{
	return 0;
}

static int cliprdr_server_send_monitor_ready(CliprdrServerContext* context)
{
	return 0;
}

static int cliprdr_server_send_format_list_response(CliprdrServerContext* context)
{
	return 0;
}

static void* cliprdr_server_thread(void* arg)
{
	wStream* s;
	DWORD status;
	DWORD nCount;
	void* buffer;
	HANDLE events[8];
	HANDLE ChannelEvent;
	UINT32 BytesReturned;
	CliprdrServerContext* context;

	context = (CliprdrServerContext*) arg;

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
				Stream_Buffer(s), Stream_Capacity(s), &BytesReturned) == FALSE)
		{
			if (BytesReturned == 0)
				break;

			Stream_EnsureRemainingCapacity(s, (int) BytesReturned);

			if (WTSVirtualChannelRead(context->priv->ChannelHandle, 0,
					Stream_Buffer(s), Stream_Capacity(s), &BytesReturned) == FALSE)
			{
				break;
			}
		}
	}

	Stream_Free(s, TRUE);

	return NULL;
}

static int cliprdr_server_start(CliprdrServerContext* context)
{
	context->priv->ChannelHandle = WTSVirtualChannelOpenEx(context->vcm, "cliprdr", 0);

	if (!context->priv->ChannelHandle)
		return -1;

	context->priv->StopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	context->priv->Thread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE) cliprdr_server_thread, (void*) context, 0, NULL);

	return 0;
}

static int cliprdr_server_stop(CliprdrServerContext* context)
{
	SetEvent(context->priv->StopEvent);

	WaitForSingleObject(context->priv->Thread, INFINITE);
	CloseHandle(context->priv->Thread);

	return 0;
}

CliprdrServerContext* cliprdr_server_context_new(WTSVirtualChannelManager* vcm)
{
	CliprdrServerContext* context;

	context = (CliprdrServerContext*) malloc(sizeof(CliprdrServerContext));

	if (context)
	{
		ZeroMemory(context, sizeof(CliprdrServerContext));

		context->vcm = vcm;

		context->Start = cliprdr_server_start;
		context->Stop = cliprdr_server_stop;

		context->SendCapabilities = cliprdr_server_send_capabilities;
		context->SendMonitorReady = cliprdr_server_send_monitor_ready;
		context->SendFormatListResponse = cliprdr_server_send_format_list_response;

		context->priv = (CliprdrServerPrivate*) malloc(sizeof(CliprdrServerPrivate));

		if (context->priv)
		{
			ZeroMemory(context->priv, sizeof(CliprdrServerPrivate));
		}
	}

	return context;
}

void cliprdr_server_context_free(CliprdrServerContext* context)
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
