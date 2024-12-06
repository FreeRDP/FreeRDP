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
#include <freerdp/server/server-common.h>
#include <freerdp/server/audin.h>
#include <freerdp/channels/log.h>

#define AUDIN_TAG CHANNELS_TAG("audin.server")

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

	AUDIO_FORMAT* audin_server_formats;
	UINT32 audin_n_server_formats;
	AUDIO_FORMAT* audin_negotiated_format;
	UINT32 audin_client_format_idx;
	wLog* log;
} audin_server;

static UINT audin_server_recv_version(audin_server_context* context, wStream* s,
                                      const SNDIN_PDU* header)
{
	audin_server* audin = (audin_server*)context;
	SNDIN_VERSION pdu = { 0 };
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(context);
	WINPR_ASSERT(header);

	pdu.Header = *header;

	if (!Stream_CheckAndLogRequiredLengthWLog(audin->log, s, 4))
		return ERROR_NO_DATA;

	Stream_Read_UINT32(s, pdu.Version);

	IFCALLRET(context->ReceiveVersion, error, context, &pdu);
	if (error)
		WLog_Print(audin->log, WLOG_ERROR, "context->ReceiveVersion failed with error %" PRIu32 "",
		           error);

	return error;
}

static UINT audin_server_recv_formats(audin_server_context* context, wStream* s,
                                      const SNDIN_PDU* header)
{
	audin_server* audin = (audin_server*)context;
	SNDIN_FORMATS pdu = { 0 };
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(context);
	WINPR_ASSERT(header);

	pdu.Header = *header;

	/* Implementations MUST, at a minimum, support WAVE_FORMAT_PCM (0x0001) */
	if (!Stream_CheckAndLogRequiredLengthWLog(audin->log, s, 4 + 4 + 18))
		return ERROR_NO_DATA;

	Stream_Read_UINT32(s, pdu.NumFormats);
	Stream_Read_UINT32(s, pdu.cbSizeFormatsPacket);

	if (pdu.NumFormats == 0)
	{
		WLog_Print(audin->log, WLOG_ERROR, "Sound Formats PDU contains no formats");
		return ERROR_INVALID_DATA;
	}

	pdu.SoundFormats = audio_formats_new(pdu.NumFormats);
	if (!pdu.SoundFormats)
	{
		WLog_Print(audin->log, WLOG_ERROR, "Failed to allocate %u SoundFormats", pdu.NumFormats);
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	for (UINT32 i = 0; i < pdu.NumFormats; ++i)
	{
		AUDIO_FORMAT* format = &pdu.SoundFormats[i];

		if (!audio_format_read(s, format))
		{
			WLog_Print(audin->log, WLOG_ERROR, "Failed to read audio format");
			audio_formats_free(pdu.SoundFormats, i + i);
			return ERROR_INVALID_DATA;
		}

		audio_format_print(audin->log, WLOG_DEBUG, format);
	}

	if (pdu.cbSizeFormatsPacket != Stream_GetPosition(s))
	{
		WLog_Print(audin->log, WLOG_WARN,
		           "cbSizeFormatsPacket is invalid! Expected: %u Got: %zu. Fixing size",
		           pdu.cbSizeFormatsPacket, Stream_GetPosition(s));
		const size_t pos = Stream_GetPosition(s);
		if (pos > UINT32_MAX)
		{
			WLog_Print(audin->log, WLOG_ERROR, "Stream too long, %" PRIuz " exceeds UINT32_MAX",
			           pos);
			error = ERROR_INVALID_PARAMETER;
			goto fail;
		}
		pdu.cbSizeFormatsPacket = (UINT32)pos;
	}

	pdu.ExtraDataSize = Stream_GetRemainingLength(s);

	IFCALLRET(context->ReceiveFormats, error, context, &pdu);
	if (error)
		WLog_Print(audin->log, WLOG_ERROR, "context->ReceiveFormats failed with error %" PRIu32 "",
		           error);

fail:
	audio_formats_free(pdu.SoundFormats, pdu.NumFormats);

	return error;
}

static UINT audin_server_recv_open_reply(audin_server_context* context, wStream* s,
                                         const SNDIN_PDU* header)
{
	audin_server* audin = (audin_server*)context;
	SNDIN_OPEN_REPLY pdu = { 0 };
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(context);
	WINPR_ASSERT(header);

	pdu.Header = *header;

	if (!Stream_CheckAndLogRequiredLengthWLog(audin->log, s, 4))
		return ERROR_NO_DATA;

	Stream_Read_UINT32(s, pdu.Result);

	IFCALLRET(context->OpenReply, error, context, &pdu);
	if (error)
		WLog_Print(audin->log, WLOG_ERROR, "context->OpenReply failed with error %" PRIu32 "",
		           error);

	return error;
}

static UINT audin_server_recv_data_incoming(audin_server_context* context, wStream* s,
                                            const SNDIN_PDU* header)
{
	audin_server* audin = (audin_server*)context;
	SNDIN_DATA_INCOMING pdu = { 0 };
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(context);
	WINPR_ASSERT(header);

	pdu.Header = *header;

	IFCALLRET(context->IncomingData, error, context, &pdu);
	if (error)
		WLog_Print(audin->log, WLOG_ERROR, "context->IncomingData failed with error %" PRIu32 "",
		           error);

	return error;
}

static UINT audin_server_recv_data(audin_server_context* context, wStream* s,
                                   const SNDIN_PDU* header)
{
	audin_server* audin = (audin_server*)context;
	SNDIN_DATA pdu = { 0 };
	wStream dataBuffer = { 0 };
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(context);
	WINPR_ASSERT(header);

	pdu.Header = *header;

	pdu.Data = Stream_StaticInit(&dataBuffer, Stream_Pointer(s), Stream_GetRemainingLength(s));

	IFCALLRET(context->Data, error, context, &pdu);
	if (error)
		WLog_Print(audin->log, WLOG_ERROR, "context->Data failed with error %" PRIu32 "", error);

	return error;
}

static UINT audin_server_recv_format_change(audin_server_context* context, wStream* s,
                                            const SNDIN_PDU* header)
{
	audin_server* audin = (audin_server*)context;
	SNDIN_FORMATCHANGE pdu = { 0 };
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(context);
	WINPR_ASSERT(header);

	pdu.Header = *header;

	if (!Stream_CheckAndLogRequiredLengthWLog(audin->log, s, 4))
		return ERROR_NO_DATA;

	Stream_Read_UINT32(s, pdu.NewFormat);

	IFCALLRET(context->ReceiveFormatChange, error, context, &pdu);
	if (error)
		WLog_Print(audin->log, WLOG_ERROR,
		           "context->ReceiveFormatChange failed with error %" PRIu32 "", error);

	return error;
}

static DWORD WINAPI audin_server_thread_func(LPVOID arg)
{
	wStream* s = NULL;
	void* buffer = NULL;
	DWORD nCount = 0;
	HANDLE events[8] = { 0 };
	BOOL ready = FALSE;
	HANDLE ChannelEvent = NULL;
	DWORD BytesReturned = 0;
	audin_server* audin = (audin_server*)arg;
	UINT error = CHANNEL_RC_OK;
	DWORD status = ERROR_INTERNAL_ERROR;

	WINPR_ASSERT(audin);

	if (WTSVirtualChannelQuery(audin->audin_channel, WTSVirtualEventHandle, &buffer,
	                           &BytesReturned) == TRUE)
	{
		if (BytesReturned == sizeof(HANDLE))
			ChannelEvent = *(HANDLE*)buffer;

		WTSFreeMemory(buffer);
	}
	else
	{
		WLog_Print(audin->log, WLOG_ERROR, "WTSVirtualChannelQuery failed");
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
			WLog_Print(audin->log, WLOG_ERROR,
			           "WaitForMultipleObjects failed with error %" PRIu32 "", error);
			goto out;
		}
		if (status == WAIT_OBJECT_0)
			goto out;

		if (WTSVirtualChannelQuery(audin->audin_channel, WTSVirtualChannelReady, &buffer,
		                           &BytesReturned) == FALSE)
		{
			WLog_Print(audin->log, WLOG_ERROR, "WTSVirtualChannelQuery failed");
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
		WLog_Print(audin->log, WLOG_ERROR, "Stream_New failed!");
		error = CHANNEL_RC_NO_MEMORY;
		goto out;
	}

	if (ready)
	{
		SNDIN_VERSION version = { 0 };

		version.Version = audin->context.serverVersion;

		if ((error = audin->context.SendVersion(&audin->context, &version)))
		{
			WLog_Print(audin->log, WLOG_ERROR, "SendVersion failed with error %" PRIu32 "!", error);
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
			WLog_Print(audin->log, WLOG_ERROR,
			           "WaitForMultipleObjects failed with error %" PRIu32 "", error);
			break;
		}
		if (status == WAIT_OBJECT_0)
			break;

		Stream_SetPosition(s, 0);

		if (!WTSVirtualChannelRead(audin->audin_channel, 0, NULL, 0, &BytesReturned))
		{
			WLog_Print(audin->log, WLOG_ERROR, "WTSVirtualChannelRead failed!");
			error = ERROR_INTERNAL_ERROR;
			break;
		}

		if (BytesReturned < 1)
			continue;

		if (!Stream_EnsureRemainingCapacity(s, BytesReturned))
			break;

		WINPR_ASSERT(Stream_Capacity(s) <= UINT32_MAX);
		if (WTSVirtualChannelRead(audin->audin_channel, 0, Stream_BufferAs(s, char),
		                          (ULONG)Stream_Capacity(s), &BytesReturned) == FALSE)
		{
			WLog_Print(audin->log, WLOG_ERROR, "WTSVirtualChannelRead failed!");
			error = ERROR_INTERNAL_ERROR;
			break;
		}

		Stream_SetLength(s, BytesReturned);
		if (!Stream_CheckAndLogRequiredLengthWLog(audin->log, s, SNDIN_HEADER_SIZE))
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
				WLog_Print(audin->log, WLOG_ERROR,
				           "audin_server_thread_func: unknown or invalid MessageId %" PRIu8 "",
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
	(void)WTSVirtualChannelClose(audin->audin_channel);
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
		UINT32 channelId = 0;
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
			WLog_Print(audin->log, WLOG_ERROR, "WTSVirtualChannelOpenEx failed!");
			return FALSE;
		}

		channelId = WTSChannelGetIdByHandle(audin->audin_channel);

		IFCALLRET(context->ChannelIdAssigned, status, context, channelId);
		if (!status)
		{
			WLog_Print(audin->log, WLOG_ERROR, "context->ChannelIdAssigned failed!");
			return FALSE;
		}

		if (!(audin->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
		{
			WLog_Print(audin->log, WLOG_ERROR, "CreateEvent failed!");
			return FALSE;
		}

		if (!(audin->thread =
		          CreateThread(NULL, 0, audin_server_thread_func, (void*)audin, 0, NULL)))
		{
			WLog_Print(audin->log, WLOG_ERROR, "CreateThread failed!");
			(void)CloseHandle(audin->stopEvent);
			audin->stopEvent = NULL;
			return FALSE;
		}

		return TRUE;
	}

	WLog_Print(audin->log, WLOG_ERROR, "thread already running!");
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
		(void)SetEvent(audin->stopEvent);

		if (WaitForSingleObject(audin->thread, INFINITE) == WAIT_FAILED)
		{
			WLog_Print(audin->log, WLOG_ERROR, "WaitForSingleObject failed with error %" PRIu32 "",
			           GetLastError());
			return FALSE;
		}

		(void)CloseHandle(audin->thread);
		(void)CloseHandle(audin->stopEvent);
		audin->thread = NULL;
		audin->stopEvent = NULL;
	}

	if (audin->audin_channel)
	{
		(void)WTSVirtualChannelClose(audin->audin_channel);
		audin->audin_channel = NULL;
	}

	audin->audin_negotiated_format = NULL;

	return TRUE;
}

static wStream* audin_server_packet_new(wLog* log, size_t size, BYTE MessageId)
{
	WINPR_ASSERT(log);

	/* Allocate what we need plus header bytes */
	wStream* s = Stream_New(NULL, size + SNDIN_HEADER_SIZE);
	if (!s)
	{
		WLog_Print(log, WLOG_ERROR, "Stream_New failed!");
		return NULL;
	}

	Stream_Write_UINT8(s, MessageId);

	return s;
}

static UINT audin_server_packet_send(audin_server_context* context, wStream* s)
{
	audin_server* audin = (audin_server*)context;
	UINT error = CHANNEL_RC_OK;
	ULONG written = 0;

	WINPR_ASSERT(context);
	WINPR_ASSERT(s);

	const size_t pos = Stream_GetPosition(s);
	WINPR_ASSERT(pos <= UINT32_MAX);
	if (!WTSVirtualChannelWrite(audin->audin_channel, Stream_BufferAs(s, char), (UINT32)pos,
	                            &written))
	{
		WLog_Print(audin->log, WLOG_ERROR, "WTSVirtualChannelWrite failed!");
		error = ERROR_INTERNAL_ERROR;
		goto out;
	}

	if (written < Stream_GetPosition(s))
	{
		WLog_Print(audin->log, WLOG_WARN, "Unexpected bytes written: %" PRIu32 "/%" PRIuz "",
		           written, Stream_GetPosition(s));
	}

out:
	Stream_Free(s, TRUE);
	return error;
}

static UINT audin_server_send_version(audin_server_context* context, const SNDIN_VERSION* version)
{
	audin_server* audin = (audin_server*)context;

	WINPR_ASSERT(context);
	WINPR_ASSERT(version);

	wStream* s = audin_server_packet_new(audin->log, 4, MSG_SNDIN_VERSION);
	if (!s)
		return ERROR_NOT_ENOUGH_MEMORY;

	Stream_Write_UINT32(s, version->Version);

	return audin_server_packet_send(context, s);
}

static UINT audin_server_send_formats(audin_server_context* context, const SNDIN_FORMATS* formats)
{
	audin_server* audin = (audin_server*)context;

	WINPR_ASSERT(audin);
	WINPR_ASSERT(formats);

	wStream* s = audin_server_packet_new(audin->log, 4 + 4 + 18, MSG_SNDIN_FORMATS);
	if (!s)
		return ERROR_NOT_ENOUGH_MEMORY;

	Stream_Write_UINT32(s, formats->NumFormats);
	Stream_Write_UINT32(s, formats->cbSizeFormatsPacket);

	for (UINT32 i = 0; i < formats->NumFormats; ++i)
	{
		AUDIO_FORMAT* format = &formats->SoundFormats[i];

		if (!audio_format_write(s, format))
		{
			WLog_Print(audin->log, WLOG_ERROR, "Failed to write audio format");
			Stream_Free(s, TRUE);
			return CHANNEL_RC_NO_MEMORY;
		}
	}

	return audin_server_packet_send(context, s);
}

static UINT audin_server_send_open(audin_server_context* context, const SNDIN_OPEN* open)
{
	audin_server* audin = (audin_server*)context;
	WINPR_ASSERT(audin);
	WINPR_ASSERT(open);

	wStream* s = audin_server_packet_new(audin->log, 4 + 4 + 18 + 22, MSG_SNDIN_OPEN);
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
	audin_server* audin = (audin_server*)context;

	WINPR_ASSERT(context);
	WINPR_ASSERT(format_change);

	wStream* s = audin_server_packet_new(audin->log, 4, MSG_SNDIN_FORMATCHANGE);
	if (!s)
		return ERROR_NOT_ENOUGH_MEMORY;

	Stream_Write_UINT32(s, format_change->NewFormat);

	return audin_server_packet_send(context, s);
}

static UINT audin_server_receive_version_default(audin_server_context* audin_ctx,
                                                 const SNDIN_VERSION* version)
{
	audin_server* audin = (audin_server*)audin_ctx;
	SNDIN_FORMATS formats = { 0 };

	WINPR_ASSERT(audin);
	WINPR_ASSERT(version);

	if (version->Version == 0)
	{
		WLog_Print(audin->log, WLOG_ERROR, "Received invalid AUDIO_INPUT version from client");
		return ERROR_INVALID_DATA;
	}

	WLog_Print(audin->log, WLOG_DEBUG, "AUDIO_INPUT version of client: %u", version->Version);

	formats.NumFormats = audin->audin_n_server_formats;
	formats.SoundFormats = audin->audin_server_formats;

	return audin->context.SendFormats(&audin->context, &formats);
}

static UINT send_open(audin_server* audin)
{
	SNDIN_OPEN open = { 0 };

	WINPR_ASSERT(audin);

	open.FramesPerPacket = 441;
	open.initialFormat = audin->audin_client_format_idx;
	open.captureFormat.wFormatTag = WAVE_FORMAT_PCM;
	open.captureFormat.nChannels = 2;
	open.captureFormat.nSamplesPerSec = 44100;
	open.captureFormat.nAvgBytesPerSec = 44100 * 2 * 2;
	open.captureFormat.nBlockAlign = 4;
	open.captureFormat.wBitsPerSample = 16;

	WINPR_ASSERT(audin->context.SendOpen);
	return audin->context.SendOpen(&audin->context, &open);
}

static UINT audin_server_receive_formats_default(audin_server_context* context,
                                                 const SNDIN_FORMATS* formats)
{
	audin_server* audin = (audin_server*)context;
	WINPR_ASSERT(audin);
	WINPR_ASSERT(formats);

	if (audin->audin_negotiated_format)
	{
		WLog_Print(audin->log, WLOG_ERROR,
		           "Received client formats, but negotiation was already done");
		return ERROR_INVALID_DATA;
	}

	for (UINT32 i = 0; i < audin->audin_n_server_formats; ++i)
	{
		for (UINT32 j = 0; j < formats->NumFormats; ++j)
		{
			if (audio_format_compatible(&audin->audin_server_formats[i], &formats->SoundFormats[j]))
			{
				audin->audin_negotiated_format = &audin->audin_server_formats[i];
				audin->audin_client_format_idx = i;
				return send_open(audin);
			}
		}
	}

	WLog_Print(audin->log, WLOG_ERROR, "Could not agree on a audio format with the server");

	return ERROR_INVALID_DATA;
}

static UINT audin_server_receive_format_change_default(audin_server_context* context,
                                                       const SNDIN_FORMATCHANGE* format_change)
{
	audin_server* audin = (audin_server*)context;

	WINPR_ASSERT(audin);
	WINPR_ASSERT(format_change);

	if (format_change->NewFormat != audin->audin_client_format_idx)
	{
		WLog_Print(audin->log, WLOG_ERROR,
		           "NewFormat in FormatChange differs from requested format");
		return ERROR_INVALID_DATA;
	}

	WLog_Print(audin->log, WLOG_DEBUG, "Received Format Change PDU: %u", format_change->NewFormat);

	return CHANNEL_RC_OK;
}

static UINT audin_server_incoming_data_default(audin_server_context* context,
                                               const SNDIN_DATA_INCOMING* data_incoming)
{
	audin_server* audin = (audin_server*)context;
	WINPR_ASSERT(audin);
	WINPR_ASSERT(data_incoming);

	/* TODO: Implement bandwidth measure of clients uplink */
	WLog_Print(audin->log, WLOG_DEBUG, "Received Incoming Data PDU");
	return CHANNEL_RC_OK;
}

static UINT audin_server_open_reply_default(audin_server_context* context,
                                            const SNDIN_OPEN_REPLY* open_reply)
{
	audin_server* audin = (audin_server*)context;
	WINPR_ASSERT(audin);
	WINPR_ASSERT(open_reply);

	/* TODO: Implement failure handling */
	WLog_Print(audin->log, WLOG_DEBUG, "Open Reply PDU: Result: %i", open_reply->Result);
	return CHANNEL_RC_OK;
}

audin_server_context* audin_server_context_new(HANDLE vcm)
{
	audin_server* audin = (audin_server*)calloc(1, sizeof(audin_server));

	if (!audin)
	{
		WLog_ERR(AUDIN_TAG, "calloc failed!");
		return NULL;
	}
	audin->log = WLog_Get(AUDIN_TAG);
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
	audin->context.ReceiveVersion = audin_server_receive_version_default;
	audin->context.ReceiveFormats = audin_server_receive_formats_default;
	audin->context.ReceiveFormatChange = audin_server_receive_format_change_default;
	audin->context.IncomingData = audin_server_incoming_data_default;
	audin->context.OpenReply = audin_server_open_reply_default;

	return &audin->context;
}

void audin_server_context_free(audin_server_context* context)
{
	audin_server* audin = (audin_server*)context;

	if (!audin)
		return;

	audin_server_close(context);
	audio_formats_free(audin->audin_server_formats, audin->audin_n_server_formats);
	audin->audin_server_formats = NULL;
	free(audin);
}

BOOL audin_server_set_formats(audin_server_context* context, SSIZE_T count,
                              const AUDIO_FORMAT* formats)
{
	audin_server* audin = (audin_server*)context;
	WINPR_ASSERT(audin);

	audio_formats_free(audin->audin_server_formats, audin->audin_n_server_formats);
	audin->audin_n_server_formats = 0;
	audin->audin_server_formats = NULL;
	audin->audin_negotiated_format = NULL;

	if (count < 0)
	{
		const size_t audin_n_server_formats =
		    server_audin_get_formats(&audin->audin_server_formats);
		WINPR_ASSERT(audin_n_server_formats <= UINT32_MAX);

		audin->audin_n_server_formats = (UINT32)audin_n_server_formats;
	}
	else
	{
		AUDIO_FORMAT* audin_server_formats = audio_formats_new(count);
		if (!audin_server_formats)
			return count == 0;

		for (SSIZE_T x = 0; x < count; x++)
		{
			if (!audio_format_copy(&formats[x], &audin_server_formats[x]))
			{
				audio_formats_free(audin_server_formats, count);
				return FALSE;
			}
		}

		WINPR_ASSERT(count <= UINT32_MAX);
		audin->audin_server_formats = audin_server_formats;
		audin->audin_n_server_formats = (UINT32)count;
	}
	return audin->audin_n_server_formats > 0;
}

const AUDIO_FORMAT* audin_server_get_negotiated_format(const audin_server_context* context)
{
	const audin_server* audin = (const audin_server*)context;
	WINPR_ASSERT(audin);

	return audin->audin_negotiated_format;
}
