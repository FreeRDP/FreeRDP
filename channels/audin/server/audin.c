/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Server Audio Input Virtual Channel
 *
 * Copyright 2012 Vic Lee
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/stream.h>

#include <freerdp/codec/dsp.h>
#include <freerdp/codec/audio.h>
#include <freerdp/channels/wtsvc.h>
#include <freerdp/channels/audin.h>
#include <freerdp/server/audin.h>
#include <freerdp/channels/log.h>

#define TAG CHANNELS_TAG("audin.server")
#define MSG_SNDIN_VERSION 0x01
#define MSG_SNDIN_FORMATS 0x02
#define MSG_SNDIN_OPEN 0x03
#define MSG_SNDIN_OPEN_REPLY 0x04
#define MSG_SNDIN_DATA_INCOMING 0x05
#define MSG_SNDIN_DATA 0x06
#define MSG_SNDIN_FORMATCHANGE 0x07

typedef struct
{
	audin_server_context context;

	BOOL opened;

	HANDLE stopEvent;

	HANDLE thread;
	void* audin_channel;

	DWORD SessionId;

	FREERDP_DSP_CONTEXT* dsp_context;

} audin_server;

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_server_select_format(audin_server_context* context, size_t client_format_index)
{
	audin_server* audin = (audin_server*)context;

	WINPR_ASSERT(audin);

	if (client_format_index >= context->num_client_formats)
	{
		WLog_ERR(TAG, "error in protocol: client_format_index >= context->num_client_formats!");
		return ERROR_INVALID_DATA;
	}

	context->selected_client_format = (SSIZE_T)client_format_index;

	if (!freerdp_dsp_context_reset(audin->dsp_context,
	                               &audin->context.client_formats[client_format_index], 0u))
	{
		WLog_ERR(TAG, "Failed to reset dsp context format!");
		return ERROR_INTERNAL_ERROR;
	}

	if (audin->opened)
	{
		/* TODO: send MSG_SNDIN_FORMATCHANGE */
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_server_send_version(audin_server* audin, wStream* s)
{
	ULONG written;
	WINPR_ASSERT(audin);

	Stream_Write_UINT8(s, MSG_SNDIN_VERSION);
	Stream_Write_UINT32(s, 1); /* Version (4 bytes) */

	WINPR_ASSERT(Stream_GetPosition(s) <= ULONG_MAX);
	if (!WTSVirtualChannelWrite(audin->audin_channel, (PCHAR)Stream_Buffer(s),
	                            (ULONG)Stream_GetPosition(s), &written))
	{
		WLog_ERR(TAG, "WTSVirtualChannelWrite failed!");
		return ERROR_INTERNAL_ERROR;
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_server_recv_version(audin_server* audin, wStream* s, UINT32 length)
{
	UINT32 Version;

	WINPR_ASSERT(audin);
	if (length < 4)
	{
		WLog_ERR(TAG, "error parsing version info: expected at least 4 bytes, got %" PRIu32 "",
		         length);
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, Version);

	if (Version < 1)
	{
		WLog_ERR(TAG, "expected Version > 0 but got %" PRIu32 "", Version);
		return ERROR_INVALID_DATA;
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_server_send_formats(audin_server* audin, wStream* s)
{
	size_t i;
	ULONG written;
	WINPR_ASSERT(audin);

	Stream_SetPosition(s, 0);
	Stream_Write_UINT8(s, MSG_SNDIN_FORMATS);
	WINPR_ASSERT(audin->context.num_server_formats <= UINT32_MAX);
	Stream_Write_UINT32(s, audin->context.num_server_formats); /* NumFormats (4 bytes) */
	Stream_Write_UINT32(s, 0); /* cbSizeFormatsPacket (4 bytes), client-to-server only */

	for (i = 0; i < audin->context.num_server_formats; i++)
	{
		AUDIO_FORMAT format = audin->context.server_formats[i];

		if (!audio_format_write(s, &format))
		{
			WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
			return CHANNEL_RC_NO_MEMORY;
		}
	}

	WINPR_ASSERT(Stream_GetPosition(s) <= ULONG_MAX);
	return WTSVirtualChannelWrite(audin->audin_channel, (PCHAR)Stream_Buffer(s),
	                              (ULONG)Stream_GetPosition(s), &written)
	           ? CHANNEL_RC_OK
	           : ERROR_INTERNAL_ERROR;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_server_recv_formats(audin_server* audin, wStream* s, UINT32 length)
{
	size_t i;
	UINT success = CHANNEL_RC_OK;

	WINPR_ASSERT(audin);
	if (length < 8)
	{
		WLog_ERR(TAG, "error parsing rec formats: expected at least 8 bytes, got %" PRIu32 "",
		         length);
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, audin->context.num_client_formats); /* NumFormats (4 bytes) */
	Stream_Seek_UINT32(s);                                    /* cbSizeFormatsPacket (4 bytes) */
	length -= 8;

	if (audin->context.num_client_formats <= 0)
	{
		WLog_ERR(TAG, "num_client_formats expected > 0 but got %d",
		         audin->context.num_client_formats);
		return ERROR_INVALID_DATA;
	}

	audin->context.client_formats = audio_formats_new(audin->context.num_client_formats);

	if (!audin->context.client_formats)
		return ERROR_NOT_ENOUGH_MEMORY;

	for (i = 0; i < audin->context.num_client_formats; i++)
	{
		AUDIO_FORMAT* format = &audin->context.client_formats[i];

		if (!audio_format_read(s, format))
		{
			audio_formats_free(audin->context.client_formats, i);
			audin->context.client_formats = NULL;
			WLog_ERR(TAG, "expected length at least 18, but got %" PRIu32 "", length);
			return ERROR_INVALID_DATA;
		}

		audio_format_print(WLog_Get(TAG), WLOG_DEBUG, format);
	}

	IFCALLRET(audin->context.Opening, success, &audin->context);

	if (success)
		WLog_ERR(TAG, "context.Opening failed with error %" PRIu32 "", success);

	return success;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_server_send_open(audin_server* audin, wStream* s)
{
	ULONG written;

	WINPR_ASSERT(audin);
	if (audin->context.selected_client_format < 0)
	{
		WLog_ERR(TAG, "audin->context.selected_client_format = %d",
		         audin->context.selected_client_format);
		return ERROR_INVALID_DATA;
	}

	audin->opened = TRUE;
	Stream_SetPosition(s, 0);
	Stream_Write_UINT8(s, MSG_SNDIN_OPEN);
	Stream_Write_UINT32(s, audin->context.frames_per_packet); /* FramesPerPacket (4 bytes) */
	WINPR_ASSERT(audin->context.selected_client_format >= 0);
	WINPR_ASSERT(audin->context.selected_client_format <= UINT32_MAX);
	Stream_Write_UINT32(
	    s, (UINT32)audin->context.selected_client_format); /* initialFormat (4 bytes) */
	/*
	 * [MS-RDPEAI] 3.2.5.1.6
	 * The second format specify the format that SHOULD be used to capture data from
	 * the actual audio input device.
	 */
	Stream_Write_UINT16(s, 1);             /* wFormatTag = PCM */
	Stream_Write_UINT16(s, 2);             /* nChannels */
	Stream_Write_UINT32(s, 44100);         /* nSamplesPerSec */
	Stream_Write_UINT32(s, 44100 * 2 * 2); /* nAvgBytesPerSec */
	Stream_Write_UINT16(s, 4);             /* nBlockAlign */
	Stream_Write_UINT16(s, 16);            /* wBitsPerSample */
	Stream_Write_UINT16(s, 0);             /* cbSize */
	WINPR_ASSERT(Stream_GetPosition(s) <= ULONG_MAX);
	return WTSVirtualChannelWrite(audin->audin_channel, (PCHAR)Stream_Buffer(s),
	                              (ULONG)Stream_GetPosition(s), &written)
	           ? CHANNEL_RC_OK
	           : ERROR_INTERNAL_ERROR;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_server_recv_open_reply(audin_server* audin, wStream* s, UINT32 length)
{
	UINT32 Result;
	UINT success = CHANNEL_RC_OK;

	WINPR_ASSERT(audin);
	if (length < 4)
	{
		WLog_ERR(TAG, "error parsing version info: expected at least 4 bytes, got %" PRIu32 "",
		         length);
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, Result);
	IFCALLRET(audin->context.OpenResult, success, &audin->context, Result);

	if (success)
		WLog_ERR(TAG, "context.OpenResult failed with error %" PRIu32 "", success);

	return success;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_server_recv_data(audin_server* audin, wStream* s, UINT32 length)
{
	AUDIO_FORMAT* format;
	size_t sbytes_per_sample;
	size_t sbytes_per_frame;
	size_t frames;
	wStream* out;
	UINT success = ERROR_INTERNAL_ERROR;

	WINPR_ASSERT(audin);
	if (audin->context.selected_client_format < 0)
	{
		WLog_ERR(TAG, "audin->context.selected_client_format = %d",
		         audin->context.selected_client_format);
		return ERROR_INVALID_DATA;
	}

	out = Stream_New(NULL, 4096);

	if (!out)
		return ERROR_OUTOFMEMORY;

	format = &audin->context.client_formats[audin->context.selected_client_format];

	if (freerdp_dsp_decode(audin->dsp_context, format, Stream_Pointer(s), length, out))
	{
		AUDIO_FORMAT dformat = *format;
		dformat.wFormatTag = WAVE_FORMAT_PCM;
		dformat.wBitsPerSample = 16;
		Stream_SealLength(out);
		Stream_SetPosition(out, 0);
		sbytes_per_sample = format->wBitsPerSample / 8UL;
		sbytes_per_frame = format->nChannels * sbytes_per_sample;
		frames = Stream_Length(out) / sbytes_per_frame;
		IFCALLRET(audin->context.ReceiveSamples, success, &audin->context, &dformat, out, frames);

		if (success)
			WLog_ERR(TAG, "context.ReceiveSamples failed with error %" PRIu32 "", success);
	}
	else
		WLog_ERR(TAG, "freerdp_dsp_decode failed!");

	Stream_Free(out, TRUE);
	return success;
}

static DWORD WINAPI audin_server_thread_func(LPVOID arg)
{
	wStream* s;
	void* buffer;
	DWORD nCount;
	BYTE MessageId;
	HANDLE events[8];
	BOOL ready = FALSE;
	HANDLE ChannelEvent;
	DWORD BytesReturned = 0;
	audin_server* audin = (audin_server*)arg;
	UINT error = CHANNEL_RC_OK;
	DWORD status;
	buffer = NULL;
	BytesReturned = 0;
	ChannelEvent = NULL;

	WINPR_ASSERT(audin);

	if (WTSVirtualChannelQuery(audin->audin_channel, WTSVirtualEventHandle, &buffer,
	                           &BytesReturned) == TRUE)
	{
		if (BytesReturned == sizeof(HANDLE))
			CopyMemory(&ChannelEvent, buffer, sizeof(HANDLE));

		WTSFreeMemory(buffer);
	}
	else
	{
		WLog_ERR(TAG, "WTSVirtualChannelQuery failed");
		error = ERROR_INTERNAL_ERROR;
		goto out;
	}

	nCount = 0;
	events[nCount++] = audin->stopEvent;
	events[nCount++] = ChannelEvent;

	/* Wait for the client to confirm that the Audio Input dynamic channel is ready */

	while (1)
	{
		if ((status = WaitForMultipleObjects(nCount, events, FALSE, 100)) == WAIT_OBJECT_0)
			goto out;

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForMultipleObjects failed with error %" PRIu32 "", error);
			goto out;
		}

		if (WTSVirtualChannelQuery(audin->audin_channel, WTSVirtualChannelReady, &buffer,
		                           &BytesReturned) == FALSE)
		{
			WLog_ERR(TAG, "WTSVirtualChannelQuery failed");
			error = ERROR_INTERNAL_ERROR;
			goto out;
		}

		ready = *((BOOL*)buffer);
		WTSFreeMemory(buffer);

		if (ready)
			break;
	}

	s = Stream_New(NULL, 4096);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		error = CHANNEL_RC_NO_MEMORY;
		goto out;
	}

	if (ready)
	{
		if ((error = audin_server_send_version(audin, s)))
		{
			WLog_ERR(TAG, "audin_server_send_version failed with error %" PRIu32 "!", error);
			goto out_capacity;
		}
	}

	while (ready)
	{
		if ((status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE)) == WAIT_OBJECT_0)
			break;

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForMultipleObjects failed with error %" PRIu32 "", error);
			goto out;
		}

		Stream_SetPosition(s, 0);

		if (!WTSVirtualChannelRead(audin->audin_channel, 0, NULL, 0, &BytesReturned))
		{
			WLog_ERR(TAG, "WTSVirtualChannelRead failed!");
			error = ERROR_INTERNAL_ERROR;
			break;
		}

		if (BytesReturned < 1)
			continue;

		if (!Stream_EnsureRemainingCapacity(s, BytesReturned))
			break;

		WINPR_ASSERT(Stream_Capacity(s) <= ULONG_MAX);
		if (WTSVirtualChannelRead(audin->audin_channel, 0, (PCHAR)Stream_Buffer(s),
		                          (ULONG)Stream_Capacity(s), &BytesReturned) == FALSE)
		{
			WLog_ERR(TAG, "WTSVirtualChannelRead failed!");
			error = ERROR_INTERNAL_ERROR;
			break;
		}

		Stream_Read_UINT8(s, MessageId);
		BytesReturned--;

		switch (MessageId)
		{
			case MSG_SNDIN_VERSION:
				if ((error = audin_server_recv_version(audin, s, BytesReturned)))
				{
					WLog_ERR(TAG, "audin_server_recv_version failed with error %" PRIu32 "!",
					         error);
					goto out_capacity;
				}

				if ((error = audin_server_send_formats(audin, s)))
				{
					WLog_ERR(TAG, "audin_server_send_formats failed with error %" PRIu32 "!",
					         error);
					goto out_capacity;
				}

				break;

			case MSG_SNDIN_FORMATS:
				if ((error = audin_server_recv_formats(audin, s, BytesReturned)))
				{
					WLog_ERR(TAG, "audin_server_recv_formats failed with error %" PRIu32 "!",
					         error);
					goto out_capacity;
				}

				if ((error = audin_server_send_open(audin, s)))
				{
					WLog_ERR(TAG, "audin_server_send_open failed with error %" PRIu32 "!", error);
					goto out_capacity;
				}

				break;

			case MSG_SNDIN_OPEN_REPLY:
				if ((error = audin_server_recv_open_reply(audin, s, BytesReturned)))
				{
					WLog_ERR(TAG, "audin_server_recv_open_reply failed with error %" PRIu32 "!",
					         error);
					goto out_capacity;
				}

				break;

			case MSG_SNDIN_DATA_INCOMING:
				break;

			case MSG_SNDIN_DATA:
				if ((error = audin_server_recv_data(audin, s, BytesReturned)))
				{
					WLog_ERR(TAG, "audin_server_recv_data failed with error %" PRIu32 "!", error);
					goto out_capacity;
				}

				break;

			case MSG_SNDIN_FORMATCHANGE:
				break;

			default:
				WLog_ERR(TAG, "audin_server_thread_func: unknown MessageId %" PRIu8 "", MessageId);
				break;
		}
	}

out_capacity:
	Stream_Free(s, TRUE);
out:
	WTSVirtualChannelClose(audin->audin_channel);
	audin->audin_channel = NULL;

	if (error && audin->context.rdpcontext)
		setChannelError(audin->context.rdpcontext, error,
		                "audin_server_thread_func reported an error");

	ExitThread(error);
	return error;
}

static BOOL audin_server_open(audin_server_context* context)
{
	audin_server* audin = (audin_server*)context;

	WINPR_ASSERT(audin);
	if (!audin->thread)
	{
		PULONG pSessionId = NULL;
		DWORD BytesReturned = 0;
		audin->SessionId = WTS_CURRENT_SESSION;
		UINT32 channelId;
		BOOL status = TRUE;

		if (WTSQuerySessionInformationA(context->vcm, WTS_CURRENT_SESSION, WTSSessionId,
		                                (LPSTR*)&pSessionId, &BytesReturned))
		{
			audin->SessionId = (DWORD)*pSessionId;
			WTSFreeMemory(pSessionId);
		}

		audin->audin_channel = WTSVirtualChannelOpenEx(audin->SessionId, AUDIN_DVC_CHANNEL_NAME,
		                                               WTS_CHANNEL_OPTION_DYNAMIC);

		if (!audin->audin_channel)
		{
			WLog_ERR(TAG, "WTSVirtualChannelOpenEx failed!");
			return FALSE;
		}

		channelId = WTSChannelGetIdByHandle(audin->audin_channel);

		IFCALLRET(context->ChannelIdAssigned, status, context, channelId);
		if (!status)
		{
			WLog_ERR(TAG, "context->ChannelIdAssigned failed!");
			return ERROR_INTERNAL_ERROR;
		}

		if (!(audin->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
		{
			WLog_ERR(TAG, "CreateEvent failed!");
			return FALSE;
		}

		if (!(audin->thread =
		          CreateThread(NULL, 0, audin_server_thread_func, (void*)audin, 0, NULL)))
		{
			WLog_ERR(TAG, "CreateThread failed!");
			CloseHandle(audin->stopEvent);
			audin->stopEvent = NULL;
			return FALSE;
		}

		return TRUE;
	}

	WLog_ERR(TAG, "thread already running!");
	return FALSE;
}

static BOOL audin_server_is_open(audin_server_context* context)
{
	audin_server* audin = (audin_server*)context;

	WINPR_ASSERT(audin);
	return audin->thread != NULL;
}

static BOOL audin_server_close(audin_server_context* context)
{
	audin_server* audin = (audin_server*)context;
	WINPR_ASSERT(audin);

	if (audin->thread)
	{
		SetEvent(audin->stopEvent);

		if (WaitForSingleObject(audin->thread, INFINITE) == WAIT_FAILED)
		{
			WLog_ERR(TAG, "WaitForSingleObject failed with error %" PRIu32 "", GetLastError());
			return FALSE;
		}

		CloseHandle(audin->thread);
		CloseHandle(audin->stopEvent);
		audin->thread = NULL;
		audin->stopEvent = NULL;
	}

	if (audin->audin_channel)
	{
		WTSVirtualChannelClose(audin->audin_channel);
		audin->audin_channel = NULL;
	}

	audin->context.selected_client_format = -1;
	return TRUE;
}

audin_server_context* audin_server_context_new(HANDLE vcm)
{
	audin_server* audin;
	audin = (audin_server*)calloc(1, sizeof(audin_server));

	if (!audin)
	{
		WLog_ERR(TAG, "calloc failed!");
		return NULL;
	}

	audin->context.vcm = vcm;
	audin->context.selected_client_format = -1;
	audin->context.frames_per_packet = 4096;
	audin->context.SelectFormat = audin_server_select_format;
	audin->context.Open = audin_server_open;
	audin->context.IsOpen = audin_server_is_open;
	audin->context.Close = audin_server_close;
	audin->dsp_context = freerdp_dsp_context_new(FALSE);

	if (!audin->dsp_context)
	{
		WLog_ERR(TAG, "freerdp_dsp_context_new failed!");
		free(audin);
		return NULL;
	}

	return (audin_server_context*)audin;
}

void audin_server_context_free(audin_server_context* context)
{
	audin_server* audin = (audin_server*)context;

	if (!audin)
		return;

	audin_server_close(context);
	freerdp_dsp_context_free(audin->dsp_context);
	audio_formats_free(audin->context.client_formats, audin->context.num_client_formats);
	audio_formats_free(audin->context.server_formats, audin->context.num_server_formats);
	free(audin);
}
