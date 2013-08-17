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
#include <winpr/print.h>
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
	wStream* s;
	BOOL status;
	UINT32 generalFlags;
	CLIPRDR_HEADER header;

	printf("CliprdrServerSendCapabilities\n");

	header.msgType = CB_CLIP_CAPS;
	header.msgFlags = 0;
	header.dataLen = 24;

	generalFlags = 0;

	if (context->priv->UseLongFormatNames)
		generalFlags |= CB_USE_LONG_FORMAT_NAMES;

	s = Stream_New(NULL, header.dataLen);

	Stream_Write_UINT16(s, header.msgType); /* msgType (2 bytes) */
	Stream_Write_UINT16(s, header.msgFlags); /* msgFlags (2 bytes) */
	Stream_Write_UINT32(s, header.dataLen); /* dataLen (4 bytes) */

	Stream_Write_UINT16(s, 1); /* cCapabilitiesSets (2 bytes) */
	Stream_Write_UINT16(s, 0); /* pad1 (2 bytes) */

	Stream_Write_UINT16(s, CB_CAPSTYPE_GENERAL); /* capabilitySetType (2 bytes) */
	Stream_Write_UINT16(s, CB_CAPSTYPE_GENERAL_LEN); /* lengthCapability (2 bytes) */
	Stream_Write_UINT32(s, CB_CAPS_VERSION_2); /* version (4 bytes) */
	Stream_Write_UINT32(s, generalFlags); /* generalFlags (4 bytes) */

	Stream_SealLength(s);

	status = WTSVirtualChannelWrite(context->priv->ChannelHandle, Stream_Buffer(s), Stream_Length(s), NULL);

	Stream_Free(s, TRUE);

	return 0;
}

static int cliprdr_server_send_monitor_ready(CliprdrServerContext* context)
{
	wStream* s;
	BOOL status;
	CLIPRDR_HEADER header;

	printf("CliprdrServerSendMonitorReady\n");

	header.msgType = CB_MONITOR_READY;
	header.msgFlags = 0;
	header.dataLen = 8;

	s = Stream_New(NULL, header.dataLen);

	Stream_Write_UINT16(s, header.msgType); /* msgType (2 bytes) */
	Stream_Write_UINT16(s, header.msgFlags); /* msgFlags (2 bytes) */
	Stream_Write_UINT32(s, header.dataLen); /* dataLen (4 bytes) */

	Stream_SealLength(s);

	status = WTSVirtualChannelWrite(context->priv->ChannelHandle, Stream_Buffer(s), Stream_Length(s), NULL);

	Stream_Free(s, TRUE);

	return 0;
}

static int cliprdr_server_send_format_list_response(CliprdrServerContext* context)
{
	return 0;
}

static int cliprdr_server_receive_capabilities(CliprdrServerContext* context, wStream* s, CLIPRDR_HEADER* header)
{
	UINT32 version;
	UINT32 generalFlags;
	UINT16 cCapabilitiesSets;
	UINT16 capabilitySetType;
	UINT16 lengthCapability;

	Stream_Read_UINT16(s, cCapabilitiesSets); /* cCapabilitiesSets (2 bytes) */
	Stream_Seek_UINT16(s); /* pad1 (2 bytes) */

	Stream_Read_UINT16(s, capabilitySetType); /* capabilitySetType (2 bytes) */
	Stream_Read_UINT16(s, lengthCapability); /* lengthCapability (2 bytes) */

	Stream_Read_UINT32(s, version); /* version (4 bytes) */
	Stream_Read_UINT32(s, generalFlags); /* generalFlags (4 bytes) */

	if (generalFlags & CB_USE_LONG_FORMAT_NAMES)
		context->priv->UseLongFormatNames = TRUE;

	if (generalFlags & CB_STREAM_FILECLIP_ENABLED)
		context->priv->StreamFileClipEnabled = TRUE;

	if (generalFlags & CB_FILECLIP_NO_FILE_PATHS)
		context->priv->FileClipNoFilePaths = TRUE;

	if (generalFlags & CB_CAN_LOCK_CLIPDATA)
		context->priv->CanLockClipData = TRUE;

	return 0;
}

int cliprdr_wcslen(const WCHAR* str, const WCHAR* end)
{
	WCHAR* p = (WCHAR*) str;

	if (!p)
		return -1;

	while (*p)
	{
		if (p == end)
			return -1;

		p++;
	}

	return (p - str);
}

static int cliprdr_server_receive_long_format_list(CliprdrServerContext* context, wStream* s, CLIPRDR_HEADER* header)
{
	int i;
	WCHAR* end;
	int length;
	int position;

	position = Stream_GetPosition(s);
	end = (WCHAR*) &(Stream_Buffer(s)[Stream_Length(s)]);

	winpr_HexDump(Stream_Buffer(s), Stream_Length(s));

	context->priv->ClientFormatNameCount = 0;

	while (Stream_GetRemainingLength(s) >= 6)
	{
		Stream_Seek(s, 4); /* formatId (4 bytes) */

		length = cliprdr_wcslen((WCHAR*) Stream_Pointer(s), end);

		if (length < 0)
			return -1;

		Stream_Seek(s, (length + 1) * 2); /* wszFormatName */

		context->priv->ClientFormatNameCount = 0;
	}

	context->priv->ClientFormatNames = (CLIPRDR_FORMAT_NAME*)
			malloc(sizeof(CLIPRDR_FORMAT_NAME) * context->priv->ClientFormatNameCount);

	Stream_SetPosition(s, position);

	for (i = 0; i < context->priv->ClientFormatNameCount; i++)
	{
		Stream_Read_UINT32(s, context->priv->ClientFormatNames[i].id); /* formatId (4 bytes) */

		context->priv->ClientFormatNames[i].length = ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) Stream_Pointer(s),
				-1, &context->priv->ClientFormatNames[i].name, 0, NULL, NULL);

		Stream_Seek(s, (context->priv->ClientFormatNames[i].length + 1) * 2);
	}

	for (i = 0; i < context->priv->ClientFormatNameCount; i++)
	{
		printf("Format %d: Id: 0x%04X Name: %s\n", i,
				context->priv->ClientFormatNames[i].id,
				context->priv->ClientFormatNames[i].name);
	}

	return 0;
}

static int cliprdr_server_receive_short_format_list(CliprdrServerContext* context, wStream* s, CLIPRDR_HEADER* header)
{
	printf("%s: unimplemented\n", __FUNCTION__);
	return 0;
}

static int cliprdr_server_receive_format_list(CliprdrServerContext* context, wStream* s, CLIPRDR_HEADER* header)
{
	int status;

	if (context->priv->UseLongFormatNames)
	{
		status = cliprdr_server_receive_long_format_list(context, s, header);
	}
	else
	{
		status = cliprdr_server_receive_short_format_list(context, s, header);
	}

	return status;
}

static int cliprdr_server_receive_pdu(CliprdrServerContext* context, wStream* s)
{
	CLIPRDR_HEADER header;

	Stream_Read_UINT16(s, header.msgType); /* msgType (2 bytes) */
	Stream_Read_UINT16(s, header.msgFlags); /* msgFlags (2 bytes) */
	Stream_Read_UINT32(s, header.dataLen); /* dataLen (4 bytes) */

	printf("CliprdrServerReceivePdu: msgType: %d msgFlags: 0x%08X dataLen: %d\n",
			header.msgType, header.msgFlags, header.dataLen);

	switch (header.msgType)
	{
		case CB_CLIP_CAPS:
			cliprdr_server_receive_capabilities(context, s, &header);
			break;

		case CB_TEMP_DIRECTORY:
			break;

		case CB_FORMAT_LIST:
			cliprdr_server_receive_format_list(context, s, &header);
			break;

		case CB_FORMAT_LIST_RESPONSE:
			break;

		case CB_LOCK_CLIPDATA:
			break;

		case CB_UNLOCK_CLIPDATA:
			break;

		case CB_FORMAT_DATA_REQUEST:
			break;

		case CB_FORMAT_DATA_RESPONSE:
			break;

		case CB_FILECONTENTS_REQUEST:
			break;

		case CB_FILECONTENTS_RESPONSE:
			break;

		default:
			printf("Unexpected clipboard PDU type: %d\n", header.msgType);
			break;
	}

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

	context->SendCapabilities(context);
	context->SendMonitorReady(context);

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
			if (!BytesReturned)
				break;

			Stream_EnsureRemainingCapacity(s, BytesReturned);

			if (WTSVirtualChannelRead(context->priv->ChannelHandle, 0,
					Stream_Buffer(s), Stream_Capacity(s), &BytesReturned) == FALSE)
			{
				break;
			}

			Stream_Seek(s, BytesReturned);
		}

		Stream_SealLength(s);
		Stream_SetPosition(s, 0);

		cliprdr_server_receive_pdu(context, s);
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

			context->priv->UseLongFormatNames = TRUE;
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
