/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Server Audio Input Virtual Channel
 *
 * Copyright 2012 Vic Lee
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
 * Copyright 2022 Pascal Nowack <Pascal.Nowack@gmx.de>
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

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/stream.h>

#include <freerdp/freerdp.h>
#include <freerdp/server/audin.h>
#include <freerdp/channels/log.h>

#define TAG CHANNELS_TAG("audin.server")

#define SNDIN_HEADER_SIZE 1

typedef enum
{
	MSG_SNDIN_VERSION = 0x01,
	MSG_SNDIN_FORMATS = 0x02,
	MSG_SNDIN_OPEN = 0x03,
	MSG_SNDIN_OPEN_REPLY = 0x04,
	MSG_SNDIN_DATA_INCOMING = 0x05,
	MSG_SNDIN_DATA = 0x06,
	MSG_SNDIN_FORMATCHANGE = 0x07,
} MSG_SNDIN;

typedef struct
{
	audin_server_context context;

	HANDLE stopEvent;

	HANDLE thread;
	void* audin_channel;

	DWORD SessionId;
} audin_server;

static UINT audin_server_recv_version(audin_server_context* context, wStream* s,
                                      const SNDIN_PDU* header)
{
	SNDIN_VERSION pdu = { 0 };
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(context);
	WINPR_ASSERT(header);

	pdu.Header = *header;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return ERROR_NO_DATA;

	Stream_Read_UINT32(s, pdu.Version);

	IFCALLRET(context->ReceiveVersion, error, context, &pdu);
	if (error)
		WLog_ERR(TAG, "context->ReceiveVersion failed with error %" PRIu32 "", error);

	return error;
}

static UINT audin_server_recv_formats(audin_server_context* context, wStream* s,
                                      const SNDIN_PDU* header)
{
	SNDIN_FORMATS pdu = { 0 };
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(context);
	WINPR_ASSERT(header);

	pdu.Header = *header;

	/* Implementations MUST, at a minimum, support WAVE_FORMAT_PCM (0x0001) */
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4 + 4 + 18))
		return ERROR_NO_DATA;

	Stream_Read_UINT32(s, pdu.NumFormats);
	Stream_Read_UINT32(s, pdu.cbSizeFormatsPacket);

	if (pdu.NumFormats == 0)
	{
		WLog_ERR(TAG, "Sound Formats PDU contains no formats");
		return ERROR_INVALID_DATA;
	}

	pdu.SoundFormats = audio_formats_new(pdu.NumFormats);
	if (!pdu.SoundFormats)
	{
		WLog_ERR(TAG, "Failed to allocate %u SoundFormats", pdu.NumFormats);
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	for (UINT32 i = 0; i < pdu.NumFormats; ++i)
	{
		AUDIO_FORMAT* format = &pdu.SoundFormats[i];

		if (!audio_format_read(s, format))
		{
			WLog_ERR(TAG, "Failed to read audio format");
			audio_formats_free(pdu.SoundFormats, i + i);
			return ERROR_INVALID_DATA;
		}

		audio_format_print(WLog_Get(TAG), WLOG_DEBUG, format);
	}

	if (pdu.cbSizeFormatsPacket != Stream_GetPosition(s))
	{
		WLog_WARN(TAG, "cbSizeFormatsPacket is invalid! Expected: %u Got: %zu. Fixing size",
		          pdu.cbSizeFormatsPacket, Stream_GetPosition(s));
		pdu.cbSizeFormatsPacket = Stream_GetPosition(s);
	}

	pdu.ExtraDataSize = Stream_GetRemainingLength(s);

	IFCALLRET(context->ReceiveFormats, error, context, &pdu);
	if (error)
		WLog_ERR(TAG, "context->ReceiveFormats failed with error %" PRIu32 "", error);

	audio_formats_free(pdu.SoundFormats, pdu.NumFormats);

	return error;
}

static UINT audin_server_recv_open_reply(audin_server_context* context, wStream* s,
                                         const SNDIN_PDU* header)
{
	SNDIN_OPEN_REPLY pdu = { 0 };
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(context);
	WINPR_ASSERT(header);

	pdu.Header = *header;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return ERROR_NO_DATA;

	Stream_Read_UINT32(s, pdu.Result);

	IFCALLRET(context->OpenReply, error, context, &pdu);
	if (error)
		WLog_ERR(TAG, "context->OpenReply failed with error %" PRIu32 "", error);

	return error;
}

static UINT audin_server_recv_data_incoming(audin_server_context* context, wStream* s,
                                            const SNDIN_PDU* header)
{
	SNDIN_DATA_INCOMING pdu = { 0 };
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(context);
	WINPR_ASSERT(header);

	pdu.Header = *header;

	IFCALLRET(context->IncomingData, error, context, &pdu);
	if (error)
		WLog_ERR(TAG, "context->IncomingData failed with error %" PRIu32 "", error);

	return error;
}

static UINT audin_server_recv_data(audin_server_context* context, wStream* s,
                                   const SNDIN_PDU* header)
{
	SNDIN_DATA pdu = { 0 };
	wStream dataBuffer = { 0 };
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(context);
	WINPR_ASSERT(header);

	pdu.Header = *header;

	pdu.Data = Stream_StaticInit(&dataBuffer, Stream_Pointer(s), Stream_GetRemainingLength(s));

	IFCALLRET(context->Data, error, context, &pdu);
	if (error)
		WLog_ERR(TAG, "context->Data failed with error %" PRIu32 "", error);

	return error;
}

static UINT audin_server_recv_format_change(audin_server_context* context, wStream* s,
                                            const SNDIN_PDU* header)
{
	SNDIN_FORMATCHANGE pdu = { 0 };
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(context);
	WINPR_ASSERT(header);

	pdu.Header = *header;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return ERROR_NO_DATA;

	Stream_Read_UINT32(s, pdu.NewFormat);

	IFCALLRET(context->ReceiveFormatChange, error, context, &pdu);
	if (error)
		WLog_ERR(TAG, "context->ReceiveFormatChange failed with error %" PRIu32 "", error);

	return error;
}

static DWORD WINAPI audin_server_thread_func(LPVOID arg)
{
	wStream* s;
	void* buffer;
	DWORD nCount;
	HANDLE events[8];
	BOOL ready = FALSE;
	HANDLE ChannelEvent;
	DWORD BytesReturned = 0;
	audin_server* audin = (audin_server*)arg;
	UINT error = CHANNEL_RC_OK;
	DWORD status;
	buffer = NULL;
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
		if (status == WAIT_OBJECT_0)
			goto out;

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
		SNDIN_VERSION version = { 0 };

		version.Version = audin->context.serverVersion;

		if ((error = audin->context.SendVersion(&audin->context, &version)))
		{
			WLog_ERR(TAG, "SendVersion failed with error %" PRIu32 "!", error);
			goto out_capacity;
		}
	}

	while (ready)
	{
		SNDIN_PDU header = { 0 };

		if ((status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE)) == WAIT_OBJECT_0)
			break;

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForMultipleObjects failed with error %" PRIu32 "", error);
			break;
		}
		if (status == WAIT_OBJECT_0)
			break;

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

		Stream_SetLength(s, BytesReturned);
		if (!Stream_CheckAndLogRequiredLength(TAG, s, SNDIN_HEADER_SIZE))
		{
			error = ERROR_INTERNAL_ERROR;
			break;
		}

		Stream_Read_UINT8(s, header.MessageId);

		switch (header.MessageId)
		{
			case MSG_SNDIN_VERSION:
				error = audin_server_recv_version(&audin->context, s, &header);
				break;
			case MSG_SNDIN_FORMATS:
				error = audin_server_recv_formats(&audin->context, s, &header);
				break;
			case MSG_SNDIN_OPEN_REPLY:
				error = audin_server_recv_open_reply(&audin->context, s, &header);
				break;
			case MSG_SNDIN_DATA_INCOMING:
				error = audin_server_recv_data_incoming(&audin->context, s, &header);
				break;
			case MSG_SNDIN_DATA:
				error = audin_server_recv_data(&audin->context, s, &header);
				break;
			case MSG_SNDIN_FORMATCHANGE:
				error = audin_server_recv_format_change(&audin->context, s, &header);
				break;
			default:
				WLog_ERR(TAG, "audin_server_thread_func: unknown or invalid MessageId %" PRIu8 "",
				         header.MessageId);
				error = ERROR_INVALID_DATA;
				break;
		}
		if (error)
			break;
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

	return TRUE;
}

static wStream* audin_server_packet_new(size_t size, BYTE MessageId)
{
	wStream* s;

	/* Allocate what we need plus header bytes */
	s = Stream_New(NULL, size + SNDIN_HEADER_SIZE);
	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return NULL;
	}

	Stream_Write_UINT8(s, MessageId);

	return s;
}

static UINT audin_server_packet_send(audin_server_context* context, wStream* s)
{
	audin_server* audin = (audin_server*)context;
	UINT error = CHANNEL_RC_OK;
	ULONG written;

	WINPR_ASSERT(context);
	WINPR_ASSERT(s);

	if (!WTSVirtualChannelWrite(audin->audin_channel, (PCHAR)Stream_Buffer(s),
	                            Stream_GetPosition(s), &written))
	{
		WLog_ERR(TAG, "WTSVirtualChannelWrite failed!");
		error = ERROR_INTERNAL_ERROR;
		goto out;
	}

	if (written < Stream_GetPosition(s))
	{
		WLog_WARN(TAG, "Unexpected bytes written: %" PRIu32 "/%" PRIuz "", written,
		          Stream_GetPosition(s));
	}

out:
	Stream_Free(s, TRUE);
	return error;
}

static UINT audin_server_send_version(audin_server_context* context, const SNDIN_VERSION* version)
{
	wStream* s;

	WINPR_ASSERT(context);
	WINPR_ASSERT(version);

	s = audin_server_packet_new(4, MSG_SNDIN_VERSION);
	if (!s)
		return ERROR_NOT_ENOUGH_MEMORY;

	Stream_Write_UINT32(s, version->Version);

	return audin_server_packet_send(context, s);
}

static UINT audin_server_send_formats(audin_server_context* context, const SNDIN_FORMATS* formats)
{
	wStream* s;

	WINPR_ASSERT(context);
	WINPR_ASSERT(formats);

	s = audin_server_packet_new(4 + 4 + 18, MSG_SNDIN_FORMATS);
	if (!s)
		return ERROR_NOT_ENOUGH_MEMORY;

	Stream_Write_UINT32(s, formats->NumFormats);
	Stream_Write_UINT32(s, formats->cbSizeFormatsPacket);

	for (UINT32 i = 0; i < formats->NumFormats; ++i)
	{
		AUDIO_FORMAT* format = &formats->SoundFormats[i];

		if (!audio_format_write(s, format))
		{
			WLog_ERR(TAG, "Failed to write audio format");
			Stream_Free(s, TRUE);
			return CHANNEL_RC_NO_MEMORY;
		}
	}

	return audin_server_packet_send(context, s);
}

static UINT audin_server_send_open(audin_server_context* context, const SNDIN_OPEN* open)
{
	wStream* s;

	WINPR_ASSERT(context);
	WINPR_ASSERT(open);

	s = audin_server_packet_new(4 + 4 + 18 + 22, MSG_SNDIN_OPEN);
	if (!s)
		return ERROR_NOT_ENOUGH_MEMORY;

	Stream_Write_UINT32(s, open->FramesPerPacket);
	Stream_Write_UINT32(s, open->initialFormat);

	Stream_Write_UINT16(s, open->captureFormat.wFormatTag);
	Stream_Write_UINT16(s, open->captureFormat.nChannels);
	Stream_Write_UINT32(s, open->captureFormat.nSamplesPerSec);
	Stream_Write_UINT32(s, open->captureFormat.nAvgBytesPerSec);
	Stream_Write_UINT16(s, open->captureFormat.nBlockAlign);
	Stream_Write_UINT16(s, open->captureFormat.wBitsPerSample);

	if (open->ExtraFormatData)
	{
		Stream_Write_UINT16(s, 22); /* cbSize */

		Stream_Write_UINT16(s, open->ExtraFormatData->Samples.wReserved);
		Stream_Write_UINT32(s, open->ExtraFormatData->dwChannelMask);

		Stream_Write_UINT32(s, open->ExtraFormatData->SubFormat.Data1);
		Stream_Write_UINT16(s, open->ExtraFormatData->SubFormat.Data2);
		Stream_Write_UINT16(s, open->ExtraFormatData->SubFormat.Data3);
		Stream_Write_UINT8(s, open->ExtraFormatData->SubFormat.Data4[0]);
		Stream_Write_UINT8(s, open->ExtraFormatData->SubFormat.Data4[1]);
		Stream_Write_UINT8(s, open->ExtraFormatData->SubFormat.Data4[2]);
		Stream_Write_UINT8(s, open->ExtraFormatData->SubFormat.Data4[3]);
		Stream_Write_UINT8(s, open->ExtraFormatData->SubFormat.Data4[4]);
		Stream_Write_UINT8(s, open->ExtraFormatData->SubFormat.Data4[5]);
		Stream_Write_UINT8(s, open->ExtraFormatData->SubFormat.Data4[6]);
		Stream_Write_UINT8(s, open->ExtraFormatData->SubFormat.Data4[7]);
	}
	else
	{
		WINPR_ASSERT(open->captureFormat.wFormatTag != WAVE_FORMAT_EXTENSIBLE);

		Stream_Write_UINT16(s, 0); /* cbSize */
	}

	return audin_server_packet_send(context, s);
}

static UINT audin_server_send_format_change(audin_server_context* context,
                                            const SNDIN_FORMATCHANGE* format_change)
{
	wStream* s;

	WINPR_ASSERT(context);
	WINPR_ASSERT(format_change);

	s = audin_server_packet_new(4, MSG_SNDIN_FORMATCHANGE);
	if (!s)
		return ERROR_NOT_ENOUGH_MEMORY;

	Stream_Write_UINT32(s, format_change->NewFormat);

	return audin_server_packet_send(context, s);
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
	audin->context.Open = audin_server_open;
	audin->context.IsOpen = audin_server_is_open;
	audin->context.Close = audin_server_close;

	audin->context.SendVersion = audin_server_send_version;
	audin->context.SendFormats = audin_server_send_formats;
	audin->context.SendOpen = audin_server_send_open;
	audin->context.SendFormatChange = audin_server_send_format_change;

	/* Default values */
	audin->context.serverVersion = SNDIN_VERSION_Version_2;

	return (audin_server_context*)audin;
}

void audin_server_context_free(audin_server_context* context)
{
	audin_server* audin = (audin_server*)context;

	if (!audin)
		return;

	audin_server_close(context);
	free(audin);
}
