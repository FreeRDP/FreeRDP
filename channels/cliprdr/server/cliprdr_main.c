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

#include <freerdp/channels/log.h>
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

/**
 *                                    Data Transfer Sequences\n
 *     Shared                                                                     Local\n
 *  Clipboard Owner                                                           Clipboard Owner\n
 *        |                                                                         |\n
 *        |-------------------------------------------------------------------------|\n _
 *        |-------------------------------Format List PDU-------------------------->|\n  |
 *        |<--------------------------Format List Response PDU----------------------|\n _| Copy Sequence
 *        |<---------------------Lock Clipboard Data PDU (Optional)-----------------|\n
 *        |-------------------------------------------------------------------------|\n
 *        |-------------------------------------------------------------------------|\n _
 *        |<--------------------------Format Data Request PDU-----------------------|\n  | Paste Sequence Palette,
 *        |---------------------------Format Data Response PDU--------------------->|\n _| Metafile, File List Data
 *        |-------------------------------------------------------------------------|\n
 *        |-------------------------------------------------------------------------|\n _
 *        |<------------------------Format Contents Request PDU---------------------|\n  | Paste Sequence
 *        |-------------------------Format Contents Response PDU------------------->|\n _| File Stream Data
 *        |<---------------------Lock Clipboard Data PDU (Optional)-----------------|\n
 *        |-------------------------------------------------------------------------|\n
 *
 */

static int cliprdr_server_send_capabilities(CliprdrServerContext* context)
{
	wStream* s;
	BOOL status;
	ULONG written;
	UINT32 generalFlags;
	CLIPRDR_HEADER header;
	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*) context->handle;

	WLog_DBG(TAG, "CliprdrServerCapabilities");

	header.msgType = CB_CLIP_CAPS;
	header.msgFlags = 0;
	header.dataLen = 16;

	generalFlags = 0;

	if (cliprdr->useLongFormatNames)
		generalFlags |= CB_USE_LONG_FORMAT_NAMES;

	s = Stream_New(NULL, header.dataLen + CLIPRDR_HEADER_LENGTH);

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

	status = WTSVirtualChannelWrite(cliprdr->ChannelHandle, (PCHAR) Stream_Buffer(s), Stream_Length(s), &written);

	Stream_Free(s, TRUE);

	return 1;
}

static int cliprdr_server_send_monitor_ready(CliprdrServerContext* context)
{
	wStream* s;
	BOOL status;
	ULONG written;
	CLIPRDR_HEADER header;
	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*) context->handle;

	WLog_DBG(TAG, "CliprdrServerMonitorReady");

	header.msgType = CB_MONITOR_READY;
	header.msgFlags = 0;
	header.dataLen = 0;

	s = Stream_New(NULL, header.dataLen + CLIPRDR_HEADER_LENGTH);

	Stream_Write_UINT16(s, header.msgType); /* msgType (2 bytes) */
	Stream_Write_UINT16(s, header.msgFlags); /* msgFlags (2 bytes) */
	Stream_Write_UINT32(s, header.dataLen); /* dataLen (4 bytes) */

	Stream_SealLength(s);

	status = WTSVirtualChannelWrite(cliprdr->ChannelHandle, (PCHAR) Stream_Buffer(s), Stream_Length(s), &written);

	Stream_Free(s, TRUE);

	return 1;
}

int cliprdr_server_send_format_list_response(CliprdrServerContext* context)
{
	wStream* s;
	BOOL status;
	ULONG written;
	CLIPRDR_HEADER header;
	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*) context->handle;

	WLog_DBG(TAG, "CliprdrServerFormatListResponse");

	header.msgType = CB_FORMAT_LIST_RESPONSE;
	header.msgFlags = CB_RESPONSE_OK;
	header.dataLen = 0;

	s = Stream_New(NULL, header.dataLen + CLIPRDR_HEADER_LENGTH);

	Stream_Write_UINT16(s, header.msgType); /* msgType (2 bytes) */
	Stream_Write_UINT16(s, header.msgFlags); /* msgFlags (2 bytes) */
	Stream_Write_UINT32(s, header.dataLen); /* dataLen (4 bytes) */

	Stream_SealLength(s);

	status = WTSVirtualChannelWrite(cliprdr->ChannelHandle, (PCHAR) Stream_Buffer(s), Stream_Length(s), &written);

	Stream_Free(s, TRUE);

	return 1;
}

static int cliprdr_server_receive_general_capability(CliprdrServerContext* context, wStream* s)
{
	UINT32 version;
	UINT32 generalFlags;
	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*) context->handle;

	Stream_Read_UINT32(s, version); /* version (4 bytes) */
	Stream_Read_UINT32(s, generalFlags); /* generalFlags (4 bytes) */

	if (generalFlags & CB_USE_LONG_FORMAT_NAMES)
		cliprdr->useLongFormatNames = TRUE;

	if (generalFlags & CB_STREAM_FILECLIP_ENABLED)
		cliprdr->streamFileClipEnabled = TRUE;

	if (generalFlags & CB_FILECLIP_NO_FILE_PATHS)
		cliprdr->fileClipNoFilePaths = TRUE;

	if (generalFlags & CB_CAN_LOCK_CLIPDATA)
		cliprdr->canLockClipData = TRUE;

	return 1;
}

static int cliprdr_server_receive_capabilities(CliprdrServerContext* context, wStream* s, CLIPRDR_HEADER* header)
{
	UINT16 index;
	UINT16 cCapabilitiesSets;
	UINT16 capabilitySetType;
	UINT16 lengthCapability;

	WLog_DBG(TAG, "CliprdrClientCapabilities");

	Stream_Read_UINT16(s, cCapabilitiesSets); /* cCapabilitiesSets (2 bytes) */
	Stream_Seek_UINT16(s); /* pad1 (2 bytes) */

	for (index = 0; index < cCapabilitiesSets; index++)
	{
		Stream_Read_UINT16(s, capabilitySetType); /* capabilitySetType (2 bytes) */
		Stream_Read_UINT16(s, lengthCapability); /* lengthCapability (2 bytes) */

		switch (capabilitySetType)
		{
			case CB_CAPSTYPE_GENERAL:
				cliprdr_server_receive_general_capability(context, s);
				break;

			default:
				WLog_ERR(TAG, "unknown cliprdr capability set: %d", capabilitySetType);
				break;
		}
	}

	return 1;
}

static int cliprdr_server_receive_temporary_directory(CliprdrServerContext* context, wStream* s, CLIPRDR_HEADER* header)
{
	int length;
	WCHAR* wszTempDir;
	CLIPRDR_TEMP_DIRECTORY tempDirectory;
	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*) context->handle;

	if (Stream_GetRemainingLength(s) < 520)
		return -1;

	wszTempDir = (WCHAR*) Stream_Pointer(s);

	if (wszTempDir[260] != 0)
		return -1;

	free(cliprdr->temporaryDirectory);
	cliprdr->temporaryDirectory = NULL;

	ConvertFromUnicode(CP_UTF8, 0, wszTempDir, -1,
			&(cliprdr->temporaryDirectory), 0, NULL, NULL);

	length = strlen(cliprdr->temporaryDirectory);

	if (length > 519)
		length = 519;

	CopyMemory(tempDirectory.szTempDir, cliprdr->temporaryDirectory, length);
	tempDirectory.szTempDir[length] = '\0';

	WLog_DBG(TAG, "CliprdrTemporaryDirectory: %s", cliprdr->temporaryDirectory);

	if (context->TempDirectory)
		context->TempDirectory(context, &tempDirectory);

	return 1;
}

static int cliprdr_server_receive_format_list(CliprdrServerContext* context, wStream* s, CLIPRDR_HEADER* header)
{
	UINT32 index;
	UINT32 dataLen;
	UINT32 position;
	BOOL asciiNames;
	int formatNameLength;
	char* szFormatName;
	WCHAR* wszFormatName;
	CLIPRDR_FORMAT* formats = NULL;
	CLIPRDR_FORMAT_LIST formatList;
	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*) context->handle;

	dataLen = header->dataLen;
	asciiNames = (header->msgFlags & CB_ASCII_NAMES) ? TRUE : FALSE;

	formatList.msgType = CB_FORMAT_LIST;
	formatList.msgFlags = header->msgFlags;
	formatList.dataLen = header->dataLen;

	index = 0;
	formatList.numFormats = 0;
	position = Stream_GetPosition(s);

	if (!cliprdr->useLongFormatNames)
	{
		formatList.numFormats = (dataLen / 36);

		if ((formatList.numFormats * 36) != dataLen)
		{
			WLog_ERR(TAG, "Invalid short format list length: %d", dataLen);
			return -1;
		}

		if (formatList.numFormats)
			formats = (CLIPRDR_FORMAT*) calloc(formatList.numFormats, sizeof(CLIPRDR_FORMAT));

		if (!formats)
			return -1;

		formatList.formats = formats;

		while (dataLen)
		{
			Stream_Read_UINT32(s, formats[index].formatId); /* formatId (4 bytes) */
			dataLen -= 4;

			formats[index].formatName = NULL;

			if (asciiNames)
			{
				szFormatName = (char*) Stream_Pointer(s);

				if (szFormatName[0])
				{
					formats[index].formatName = (char*) malloc(32 + 1);
					CopyMemory(formats[index].formatName, szFormatName, 32);
					formats[index].formatName[32] = '\0';
				}
			}
			else
			{
				wszFormatName = (WCHAR*) Stream_Pointer(s);

				if (wszFormatName[0])
				{
					ConvertFromUnicode(CP_UTF8, 0, wszFormatName,
						16, &(formats[index].formatName), 0, NULL, NULL);
				}
			}

			Stream_Seek(s, 32);
			dataLen -= 32;
			index++;
		}
	}
	else
	{
		while (dataLen)
		{
			Stream_Seek(s, 4); /* formatId (4 bytes) */
			dataLen -= 4;

			wszFormatName = (WCHAR*) Stream_Pointer(s);

			if (!wszFormatName[0])
				formatNameLength = 0;
			else
				formatNameLength = _wcslen(wszFormatName);

			Stream_Seek(s, (formatNameLength + 1) * 2);
			dataLen -= ((formatNameLength + 1) * 2);

			formatList.numFormats++;
		}

		dataLen = formatList.dataLen;
		Stream_SetPosition(s, position);

		if (formatList.numFormats)
			formats = (CLIPRDR_FORMAT*) calloc(formatList.numFormats, sizeof(CLIPRDR_FORMAT));

		if (!formats)
			return -1;

		formatList.formats = formats;

		while (dataLen)
		{
			Stream_Read_UINT32(s, formats[index].formatId); /* formatId (4 bytes) */
			dataLen -= 4;

			formats[index].formatName = NULL;

			wszFormatName = (WCHAR*) Stream_Pointer(s);

			if (!wszFormatName[0])
				formatNameLength = 0;
			else
				formatNameLength = _wcslen(wszFormatName);

			if (formatNameLength)
			{
				ConvertFromUnicode(CP_UTF8, 0, wszFormatName,
					-1, &(formats[index].formatName), 0, NULL, NULL);
			}

			Stream_Seek(s, (formatNameLength + 1) * 2);
			dataLen -= ((formatNameLength + 1) * 2);

			index++;
		}
	}

	WLog_DBG(TAG, "ClientFormatList: numFormats: %d",
			formatList.numFormats);

	if (context->ClientFormatList)
		context->ClientFormatList(context, &formatList);

	for (index = 0; index < formatList.numFormats; index++)
	{
		if (formats[index].formatName)
			free(formats[index].formatName);
	}

	free(formats);

	return 1;
}

static int cliprdr_server_receive_format_list_response(CliprdrServerContext* context, wStream* s, CLIPRDR_HEADER* header)
{
	CLIPRDR_FORMAT_LIST_RESPONSE formatListResponse;

	WLog_DBG(TAG, "CliprdrClientFormatListResponse");

	formatListResponse.msgType = CB_FORMAT_LIST_RESPONSE;
	formatListResponse.msgFlags = header->msgFlags;
	formatListResponse.dataLen = header->dataLen;

	if (context->ClientFormatListResponse)
		context->ClientFormatListResponse(context, &formatListResponse);

	return 1;
}

static int cliprdr_server_receive_lock_clipdata(CliprdrServerContext* context, wStream* s, CLIPRDR_HEADER* header)
{
	CLIPRDR_LOCK_CLIPBOARD_DATA lockClipboardData;

	WLog_DBG(TAG, "CliprdrClientLockClipData");

	if (Stream_GetRemainingLength(s) < 4)
		return -1;

	lockClipboardData.msgType = CB_LOCK_CLIPDATA;
	lockClipboardData.msgFlags = header->msgFlags;
	lockClipboardData.dataLen = header->dataLen;

	Stream_Read_UINT32(s, lockClipboardData.clipDataId); /* clipDataId (4 bytes) */

	if (context->ClientLockClipboardData)
		context->ClientLockClipboardData(context, &lockClipboardData);

	return 1;
}

static int cliprdr_server_receive_unlock_clipdata(CliprdrServerContext* context, wStream* s, CLIPRDR_HEADER* header)
{
	CLIPRDR_UNLOCK_CLIPBOARD_DATA unlockClipboardData;

	WLog_DBG(TAG, "CliprdrClientUnlockClipData");

	unlockClipboardData.msgType = CB_UNLOCK_CLIPDATA;
	unlockClipboardData.msgFlags = header->msgFlags;
	unlockClipboardData.dataLen = header->dataLen;

	if (Stream_GetRemainingLength(s) < 4)
		return -1;

	Stream_Read_UINT32(s, unlockClipboardData.clipDataId); /* clipDataId (4 bytes) */

	if (context->ClientUnlockClipboardData)
		context->ClientUnlockClipboardData(context, &unlockClipboardData);

	return 1;
}

static int cliprdr_server_receive_format_data_request(CliprdrServerContext* context, wStream* s, CLIPRDR_HEADER* header)
{
	CLIPRDR_FORMAT_DATA_REQUEST formatDataRequest;

	WLog_DBG(TAG, "CliprdrClientFormatDataRequest");

	formatDataRequest.msgType = CB_FORMAT_DATA_REQUEST;
	formatDataRequest.msgFlags = header->msgFlags;
	formatDataRequest.dataLen = header->dataLen;

	if (Stream_GetRemainingLength(s) < 4)
		return -1;

	Stream_Read_UINT32(s, formatDataRequest.requestedFormatId); /* requestedFormatId (4 bytes) */

	if (context->ClientFormatDataRequest)
		context->ClientFormatDataRequest(context, &formatDataRequest);

	return 1;
}

static int cliprdr_server_receive_format_data_response(CliprdrServerContext* context, wStream* s, CLIPRDR_HEADER* header)
{
	CLIPRDR_FORMAT_DATA_RESPONSE formatDataResponse;

	WLog_DBG(TAG, "CliprdrClientFormatDataResponse");

	formatDataResponse.msgType = CB_FORMAT_DATA_RESPONSE;
	formatDataResponse.msgFlags = header->msgFlags;
	formatDataResponse.dataLen = header->dataLen;
	formatDataResponse.requestedFormatData = NULL;

	if (Stream_GetRemainingLength(s) < header->dataLen)
		return -1;

	if (header->dataLen)
	{
		formatDataResponse.requestedFormatData = (BYTE*) malloc(header->dataLen);
		Stream_Read(s, formatDataResponse.requestedFormatData, header->dataLen);
	}

	if (context->ClientFormatDataResponse)
		context->ClientFormatDataResponse(context, &formatDataResponse);

	free(formatDataResponse.requestedFormatData);

	return 1;
}

static int cliprdr_server_receive_filecontents_request(CliprdrServerContext* context, wStream* s, CLIPRDR_HEADER* header)
{
	CLIPRDR_FILE_CONTENTS_REQUEST request;

	WLog_DBG(TAG, "CliprdrClientFileContentsRequest");

	request.msgType = CB_FILECONTENTS_REQUEST;
	request.msgFlags = header->msgFlags;
	request.dataLen = header->dataLen;

	if (Stream_GetRemainingLength(s) < 28)
		return -1;

	Stream_Read_UINT32(s, request.streamId); /* streamId (4 bytes) */
	Stream_Read_UINT32(s, request.listIndex); /* listIndex (4 bytes) */
	Stream_Read_UINT32(s, request.dwFlags); /* dwFlags (4 bytes) */
	Stream_Read_UINT32(s, request.nPositionLow); /* nPositionLow (4 bytes) */
	Stream_Read_UINT32(s, request.nPositionHigh); /* nPositionHigh (4 bytes) */
	Stream_Read_UINT32(s, request.cbRequested); /* cbRequested (4 bytes) */
	Stream_Read_UINT32(s, request.clipDataId); /* clipDataId (4 bytes) */

	if (context->ClientFileContentsRequest)
		context->ClientFileContentsRequest(context, &request);

	return 1;
}

static int cliprdr_server_receive_filecontents_response(CliprdrServerContext* context, wStream* s, CLIPRDR_HEADER* header)
{
	CLIPRDR_FILE_CONTENTS_RESPONSE response;

	WLog_DBG(TAG, "CliprdrClientFileContentsResponse");

	response.msgType = CB_FILECONTENTS_RESPONSE;
	response.msgFlags = header->msgFlags;
	response.dataLen = header->dataLen;

	if (Stream_GetRemainingLength(s) < 4)
		return -1;

	Stream_Read_UINT32(s, response.streamId); /* streamId (4 bytes) */

	response.cbRequested = header->dataLen - 4;
	response.requestedData = Stream_Pointer(s); /* requestedFileContentsData */

	if (context->ServerFileContentsResponse)
		context->ServerFileContentsResponse(context, &response);

	return 1;
}

static int cliprdr_server_receive_pdu(CliprdrServerContext* context, wStream* s, CLIPRDR_HEADER* header)
{
	WLog_DBG(TAG, "CliprdrServerReceivePdu: msgType: %d msgFlags: 0x%08X dataLen: %d",
			 header->msgType, header->msgFlags, header->dataLen);

	switch (header->msgType)
	{
		case CB_CLIP_CAPS:
			cliprdr_server_receive_capabilities(context, s, header);
			break;

		case CB_TEMP_DIRECTORY:
			cliprdr_server_receive_temporary_directory(context, s, header);
			break;

		case CB_FORMAT_LIST:
			cliprdr_server_receive_format_list(context, s, header);
			break;

		case CB_FORMAT_LIST_RESPONSE:
			cliprdr_server_receive_format_list_response(context, s, header);
			break;

		case CB_LOCK_CLIPDATA:
			cliprdr_server_receive_lock_clipdata(context, s, header);
			break;

		case CB_UNLOCK_CLIPDATA:
			cliprdr_server_receive_unlock_clipdata(context, s, header);
			break;

		case CB_FORMAT_DATA_REQUEST:
			cliprdr_server_receive_format_data_request(context, s, header);
			break;

		case CB_FORMAT_DATA_RESPONSE:
			cliprdr_server_receive_format_data_response(context, s, header);
			break;

		case CB_FILECONTENTS_REQUEST:
			cliprdr_server_receive_filecontents_request(context, s, header);
			break;

		case CB_FILECONTENTS_RESPONSE:
			cliprdr_server_receive_filecontents_response(context, s, header);
			break;

		default:
			WLog_DBG(TAG, "Unexpected clipboard PDU type: %d", header->msgType);
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
	int position;
	HANDLE events[8];
	HANDLE ChannelEvent;
	DWORD BytesReturned;
	DWORD BytesToRead;
	CLIPRDR_HEADER header;
	CliprdrServerContext* context = (CliprdrServerContext*) arg;
	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*) context->handle;

	buffer = NULL;
	BytesReturned = 0;
	ChannelEvent = NULL;

	s = Stream_New(NULL, 4096);

	if (WTSVirtualChannelQuery(cliprdr->ChannelHandle, WTSVirtualEventHandle, &buffer, &BytesReturned) == TRUE)
	{
		if (BytesReturned == sizeof(HANDLE))
			CopyMemory(&ChannelEvent, buffer, sizeof(HANDLE));

		WTSFreeMemory(buffer);
	}

	nCount = 0;
	events[nCount++] = ChannelEvent;
	events[nCount++] = cliprdr->StopEvent;

	cliprdr_server_send_capabilities(context);
	cliprdr_server_send_monitor_ready(context);

	while (1)
	{
		status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);

		if (WaitForSingleObject(cliprdr->StopEvent, 0) == WAIT_OBJECT_0)
		{
			break;
		}

		if (Stream_GetPosition(s) < CLIPRDR_HEADER_LENGTH)
		{
			BytesReturned = 0;
			BytesToRead = CLIPRDR_HEADER_LENGTH - Stream_GetPosition(s);

			if (!WTSVirtualChannelRead(cliprdr->ChannelHandle, 0,
				(PCHAR) Stream_Pointer(s), BytesToRead, &BytesReturned))
			{
				break;
			}

			if (BytesReturned < 0)
				break;

			Stream_Seek(s, BytesReturned);
		}

		if (Stream_GetPosition(s) >= CLIPRDR_HEADER_LENGTH)
		{
			position = Stream_GetPosition(s);
			Stream_SetPosition(s, 0);

			Stream_Read_UINT16(s, header.msgType); /* msgType (2 bytes) */
			Stream_Read_UINT16(s, header.msgFlags); /* msgFlags (2 bytes) */
			Stream_Read_UINT32(s, header.dataLen); /* dataLen (4 bytes) */

			Stream_EnsureCapacity(s, (header.dataLen + CLIPRDR_HEADER_LENGTH));
			Stream_SetPosition(s, position);

			if (Stream_GetPosition(s) < (header.dataLen + CLIPRDR_HEADER_LENGTH))
			{
				BytesReturned = 0;
				BytesToRead = (header.dataLen + CLIPRDR_HEADER_LENGTH) - Stream_GetPosition(s);

				if (!WTSVirtualChannelRead(cliprdr->ChannelHandle, 0,
					(PCHAR) Stream_Pointer(s), BytesToRead, &BytesReturned))
				{
					break;
				}

				if (BytesReturned < 0)
					break;

				Stream_Seek(s, BytesReturned);
			}

			if (Stream_GetPosition(s) >= (header.dataLen + CLIPRDR_HEADER_LENGTH))
			{
				Stream_SetPosition(s, (header.dataLen + CLIPRDR_HEADER_LENGTH));
				Stream_SealLength(s);
				Stream_SetPosition(s, CLIPRDR_HEADER_LENGTH);

				cliprdr_server_receive_pdu(context, s, &header);

				/* check for trailing zero bytes */

				Stream_SetPosition(s, 0);

				BytesReturned = 0;
				BytesToRead = 4;

				if (!WTSVirtualChannelRead(cliprdr->ChannelHandle, 0,
					(PCHAR) Stream_Pointer(s), BytesToRead, &BytesReturned))
				{
					break;
				}

				if (BytesReturned < 0)
					break;

				if (BytesReturned == 4)
				{
					Stream_Read_UINT16(s, header.msgType); /* msgType (2 bytes) */
					Stream_Read_UINT16(s, header.msgFlags); /* msgFlags (2 bytes) */

					if (!header.msgType)
					{
						/* ignore trailing bytes */
						Stream_SetPosition(s, 0);
					}
				}
				else
				{
					Stream_Seek(s, BytesReturned);
				}
			}
		}
	}

	Stream_Free(s, TRUE);

	return NULL;
}

static int cliprdr_server_start(CliprdrServerContext* context)
{
	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*) context->handle;

	cliprdr->ChannelHandle = WTSVirtualChannelOpen(cliprdr->vcm, WTS_CURRENT_SESSION, "cliprdr");

	if (!cliprdr->ChannelHandle)
		return -1;

	cliprdr->StopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	cliprdr->Thread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE) cliprdr_server_thread, (void*) context, 0, NULL);

	return 0;
}

static int cliprdr_server_stop(CliprdrServerContext* context)
{
	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*) context->handle;

	SetEvent(cliprdr->StopEvent);
	WaitForSingleObject(cliprdr->Thread, INFINITE);
	CloseHandle(cliprdr->Thread);

	return 0;
}

CliprdrServerContext* cliprdr_server_context_new(HANDLE vcm)
{
	CliprdrServerContext* context;
	CliprdrServerPrivate* cliprdr;

	context = (CliprdrServerContext*) calloc(1, sizeof(CliprdrServerContext));

	if (context)
	{
		context->Start = cliprdr_server_start;
		context->Stop = cliprdr_server_stop;

		cliprdr = context->handle = (CliprdrServerPrivate*) calloc(1, sizeof(CliprdrServerPrivate));

		if (cliprdr)
		{
			cliprdr->vcm = vcm;
			cliprdr->useLongFormatNames = TRUE;
			cliprdr->streamFileClipEnabled = TRUE;
			cliprdr->fileClipNoFilePaths = TRUE;
			cliprdr->canLockClipData = TRUE;
		}
	}

	return context;
}

void cliprdr_server_context_free(CliprdrServerContext* context)
{
	CliprdrServerPrivate* cliprdr;

	if (!context)
		return;

	cliprdr = (CliprdrServerPrivate*) context->handle;

	if (cliprdr)
	{
		free(cliprdr->temporaryDirectory);
	}

	free(context);
}
