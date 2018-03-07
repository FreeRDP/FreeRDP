/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Clipboard Virtual Channel Extension
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

wStream* cliprdr_server_packet_new(UINT16 msgType, UINT16 msgFlags,
                                   UINT32 dataLen)
{
	wStream* s;
	s = Stream_New(NULL, dataLen + 8);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return NULL;
	}

	Stream_Write_UINT16(s, msgType);
	Stream_Write_UINT16(s, msgFlags);
	/* Write actual length after the entire packet has been constructed. */
	Stream_Seek(s, 4);
	return s;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT cliprdr_server_packet_send(CliprdrServerPrivate* cliprdr, wStream* s)
{
	size_t pos;
	BOOL status;
	UINT32 dataLen;
	UINT32 written;
	pos = Stream_GetPosition(s);
	dataLen = pos - 8;
	Stream_SetPosition(s, 4);
	Stream_Write_UINT32(s, dataLen);
	Stream_SetPosition(s, pos);
	status = WTSVirtualChannelWrite(cliprdr->ChannelHandle,
	                                (PCHAR) Stream_Buffer(s), Stream_Length(s), &written);
	Stream_Free(s, TRUE);
	return status ? CHANNEL_RC_OK : ERROR_INTERNAL_ERROR;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_capabilities(CliprdrServerContext* context,
                                        CLIPRDR_CAPABILITIES* capabilities)
{
	wStream* s;
	CLIPRDR_GENERAL_CAPABILITY_SET* generalCapabilitySet;
	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*) context->handle;
	capabilities->msgType = CB_CLIP_CAPS;
	capabilities->msgFlags = 0;
	s = cliprdr_server_packet_new(CB_CLIP_CAPS, 0, 4 + CB_CAPSTYPE_GENERAL_LEN);

	if (!s)
	{
		WLog_ERR(TAG, "cliprdr_server_packet_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	Stream_Write_UINT16(s, 1); /* cCapabilitiesSets (2 bytes) */
	Stream_Write_UINT16(s, 0); /* pad1 (2 bytes) */
	generalCapabilitySet = (CLIPRDR_GENERAL_CAPABILITY_SET*)
	                       capabilities->capabilitySets;
	Stream_Write_UINT16(s,
	                    generalCapabilitySet->capabilitySetType); /* capabilitySetType (2 bytes) */
	Stream_Write_UINT16(s,
	                    generalCapabilitySet->capabilitySetLength); /* lengthCapability (2 bytes) */
	Stream_Write_UINT32(s, generalCapabilitySet->version); /* version (4 bytes) */
	Stream_Write_UINT32(s,
	                    generalCapabilitySet->generalFlags); /* generalFlags (4 bytes) */
	WLog_DBG(TAG, "ServerCapabilities");
	return cliprdr_server_packet_send(cliprdr, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_monitor_ready(CliprdrServerContext* context,
        CLIPRDR_MONITOR_READY* monitorReady)
{
	wStream* s;
	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*) context->handle;
	monitorReady->msgType = CB_MONITOR_READY;
	monitorReady->msgFlags = 0;
	monitorReady->dataLen = 0;
	s = cliprdr_server_packet_new(monitorReady->msgType,
	                              monitorReady->msgFlags, monitorReady->dataLen);

	if (!s)
	{
		WLog_ERR(TAG, "cliprdr_server_packet_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	WLog_DBG(TAG, "ServerMonitorReady");
	return cliprdr_server_packet_send(cliprdr, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_format_list(CliprdrServerContext* context,
                                       CLIPRDR_FORMAT_LIST* formatList)
{
	wStream* s;
	UINT32 index;
	int length = 0;
	int cchWideChar;
	LPWSTR lpWideCharStr;
	int formatNameSize;
	int formatNameLength;
	char* szFormatName;
	WCHAR* wszFormatName;
	BOOL asciiNames = FALSE;
	CLIPRDR_FORMAT* format;
	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*) context->handle;

	if (!context->useLongFormatNames)
	{
		length = formatList->numFormats * 36;
		s = cliprdr_server_packet_new(CB_FORMAT_LIST, 0, length);

		if (!s)
		{
			WLog_ERR(TAG, "cliprdr_server_packet_new failed!");
			return ERROR_INTERNAL_ERROR;
		}

		for (index = 0; index < formatList->numFormats; index++)
		{
			format = (CLIPRDR_FORMAT*) & (formatList->formats[index]);
			Stream_Write_UINT32(s, format->formatId); /* formatId (4 bytes) */
			formatNameSize = 0;
			formatNameLength = 0;
			szFormatName = format->formatName;

			if (asciiNames)
			{
				if (szFormatName)
					formatNameLength = strlen(szFormatName);

				if (formatNameLength > 31)
					formatNameLength = 31;

				Stream_Write(s, szFormatName, formatNameLength);
				Stream_Zero(s, 32 - formatNameLength);
			}
			else
			{
				wszFormatName = NULL;

				if (szFormatName)
					formatNameSize = ConvertToUnicode(CP_UTF8, 0, szFormatName, -1, &wszFormatName,
					                                  0);

				if (formatNameSize > 15)
					formatNameSize = 15;

				if (wszFormatName)
					Stream_Write(s, wszFormatName, formatNameSize * 2);

				Stream_Zero(s, 32 - (formatNameSize * 2));
				free(wszFormatName);
			}
		}
	}
	else
	{
		for (index = 0; index < formatList->numFormats; index++)
		{
			format = (CLIPRDR_FORMAT*) & (formatList->formats[index]);
			length += 4;
			formatNameSize = 2;

			if (format->formatName)
				formatNameSize = MultiByteToWideChar(CP_UTF8, 0, format->formatName, -1, NULL,
				                                     0) * 2;

			length += formatNameSize;
		}

		s = cliprdr_server_packet_new(CB_FORMAT_LIST, 0, length);

		if (!s)
		{
			WLog_ERR(TAG, "cliprdr_server_packet_new failed!");
			return ERROR_INTERNAL_ERROR;
		}

		for (index = 0; index < formatList->numFormats; index++)
		{
			format = (CLIPRDR_FORMAT*) & (formatList->formats[index]);
			Stream_Write_UINT32(s, format->formatId); /* formatId (4 bytes) */

			if (format->formatName)
			{
				lpWideCharStr = (LPWSTR) Stream_Pointer(s);
				cchWideChar = (Stream_Capacity(s) - Stream_GetPosition(s)) / 2;
				formatNameSize = MultiByteToWideChar(CP_UTF8, 0,
				                                     format->formatName, -1, lpWideCharStr, cchWideChar) * 2;
				Stream_Seek(s, formatNameSize);
			}
			else
			{
				Stream_Write_UINT16(s, 0);
			}
		}
	}

	WLog_DBG(TAG, "ServerFormatList: numFormats: %"PRIu32"",
	         formatList->numFormats);
	return cliprdr_server_packet_send(cliprdr, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_format_list_response(CliprdrServerContext* context,
        CLIPRDR_FORMAT_LIST_RESPONSE* formatListResponse)
{
	wStream* s;
	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*) context->handle;
	formatListResponse->msgType = CB_FORMAT_LIST_RESPONSE;
	formatListResponse->dataLen = 0;
	s = cliprdr_server_packet_new(formatListResponse->msgType,
	                              formatListResponse->msgFlags, formatListResponse->dataLen);

	if (!s)
	{
		WLog_ERR(TAG, "cliprdr_server_packet_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	WLog_DBG(TAG, "ServerFormatListResponse");
	return cliprdr_server_packet_send(cliprdr, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_lock_clipboard_data(CliprdrServerContext* context,
        CLIPRDR_LOCK_CLIPBOARD_DATA* lockClipboardData)
{
	wStream* s;
	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*) context->handle;
	s = cliprdr_server_packet_new(CB_LOCK_CLIPDATA, 0, 4);

	if (!s)
	{
		WLog_ERR(TAG, "cliprdr_server_packet_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	Stream_Write_UINT32(s,
	                    lockClipboardData->clipDataId); /* clipDataId (4 bytes) */
	WLog_DBG(TAG, "ServerLockClipboardData: clipDataId: 0x%08"PRIX32"",
	         lockClipboardData->clipDataId);
	return cliprdr_server_packet_send(cliprdr, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_unlock_clipboard_data(CliprdrServerContext* context,
        CLIPRDR_UNLOCK_CLIPBOARD_DATA* unlockClipboardData)
{
	wStream* s;
	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*) context->handle;
	s = cliprdr_server_packet_new(CB_UNLOCK_CLIPDATA, 0, 4);

	if (!s)
	{
		WLog_ERR(TAG, "cliprdr_server_packet_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	Stream_Write_UINT32(s,
	                    unlockClipboardData->clipDataId); /* clipDataId (4 bytes) */
	WLog_DBG(TAG, "ServerUnlockClipboardData: clipDataId: 0x%08"PRIX32"",
	         unlockClipboardData->clipDataId);
	return cliprdr_server_packet_send(cliprdr, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_format_data_request(CliprdrServerContext* context,
        CLIPRDR_FORMAT_DATA_REQUEST* formatDataRequest)
{
	wStream* s;
	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*) context->handle;
	formatDataRequest->msgType = CB_FORMAT_DATA_REQUEST;
	formatDataRequest->msgFlags = 0;
	formatDataRequest->dataLen = 4;
	s = cliprdr_server_packet_new(formatDataRequest->msgType,
	                              formatDataRequest->msgFlags, formatDataRequest->dataLen);

	if (!s)
	{
		WLog_ERR(TAG, "cliprdr_server_packet_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	Stream_Write_UINT32(s,
	                    formatDataRequest->requestedFormatId); /* requestedFormatId (4 bytes) */
	WLog_DBG(TAG, "ClientFormatDataRequest");
	return cliprdr_server_packet_send(cliprdr, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_format_data_response(CliprdrServerContext* context,
        CLIPRDR_FORMAT_DATA_RESPONSE* formatDataResponse)
{
	wStream* s;
	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*) context->handle;
	formatDataResponse->msgType = CB_FORMAT_DATA_RESPONSE;
	s = cliprdr_server_packet_new(formatDataResponse->msgType,
	                              formatDataResponse->msgFlags, formatDataResponse->dataLen);

	if (!s)
	{
		WLog_ERR(TAG, "cliprdr_server_packet_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	Stream_Write(s, formatDataResponse->requestedFormatData,
	             formatDataResponse->dataLen);
	WLog_DBG(TAG, "ServerFormatDataResponse");
	return cliprdr_server_packet_send(cliprdr, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_file_contents_request(CliprdrServerContext* context,
        CLIPRDR_FILE_CONTENTS_REQUEST* fileContentsRequest)
{
	wStream* s;
	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*) context->handle;
	s = cliprdr_server_packet_new(CB_FILECONTENTS_REQUEST, 0, 28);

	if (!s)
	{
		WLog_ERR(TAG, "cliprdr_server_packet_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	Stream_Write_UINT32(s, fileContentsRequest->streamId); /* streamId (4 bytes) */
	Stream_Write_UINT32(s,
	                    fileContentsRequest->listIndex); /* listIndex (4 bytes) */
	Stream_Write_UINT32(s, fileContentsRequest->dwFlags); /* dwFlags (4 bytes) */
	Stream_Write_UINT32(s,
	                    fileContentsRequest->nPositionLow); /* nPositionLow (4 bytes) */
	Stream_Write_UINT32(s,
	                    fileContentsRequest->nPositionHigh); /* nPositionHigh (4 bytes) */
	Stream_Write_UINT32(s,
	                    fileContentsRequest->cbRequested); /* cbRequested (4 bytes) */
	Stream_Write_UINT32(s,
	                    fileContentsRequest->clipDataId); /* clipDataId (4 bytes) */
	WLog_DBG(TAG, "ServerFileContentsRequest: streamId: 0x%08"PRIX32"",
	         fileContentsRequest->streamId);
	return cliprdr_server_packet_send(cliprdr, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_file_contents_response(CliprdrServerContext* context,
        CLIPRDR_FILE_CONTENTS_RESPONSE* fileContentsResponse)
{
	wStream* s;
	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*) context->handle;

	if (fileContentsResponse->dwFlags & FILECONTENTS_SIZE)
		fileContentsResponse->cbRequested = sizeof(UINT64);

	s = cliprdr_server_packet_new(CB_FILECONTENTS_RESPONSE,
	                              fileContentsResponse->msgFlags,
	                              4 + fileContentsResponse->cbRequested);

	if (!s)
	{
		WLog_ERR(TAG, "cliprdr_server_packet_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	Stream_Write_UINT32(s, fileContentsResponse->streamId); /* streamId (4 bytes) */
	/**
	 * requestedFileContentsData:
	 * FILECONTENTS_SIZE: file size as UINT64
	 * FILECONTENTS_RANGE: file data from requested range
	 */
	Stream_Write(s, fileContentsResponse->requestedData,
	             fileContentsResponse->cbRequested);
	WLog_DBG(TAG, "ServerFileContentsResponse: streamId: 0x%08"PRIX32"",
	         fileContentsResponse->streamId);
	return cliprdr_server_packet_send(cliprdr, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_receive_general_capability(CliprdrServerContext*
        context, wStream* s)
{
	UINT32 version;
	UINT32 generalFlags;
	Stream_Read_UINT32(s, version); /* version (4 bytes) */
	Stream_Read_UINT32(s, generalFlags); /* generalFlags (4 bytes) */

	if (context->useLongFormatNames)
		context->useLongFormatNames = (generalFlags & CB_USE_LONG_FORMAT_NAMES) ? TRUE :
		                              FALSE;

	if (context->streamFileClipEnabled)
		context->streamFileClipEnabled = (generalFlags & CB_STREAM_FILECLIP_ENABLED) ?
		                                 TRUE : FALSE;

	if (context->fileClipNoFilePaths)
		context->fileClipNoFilePaths = (generalFlags & CB_FILECLIP_NO_FILE_PATHS) ?
		                               TRUE : FALSE;

	if (context->canLockClipData)
		context->canLockClipData = (generalFlags & CB_CAN_LOCK_CLIPDATA) ? TRUE : FALSE;

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_receive_capabilities(CliprdrServerContext* context,
        wStream* s, CLIPRDR_HEADER* header)
{
	UINT16 index;
	UINT16 cCapabilitiesSets;
	UINT16 capabilitySetType;
	UINT16 lengthCapability;
	UINT error;
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
				if ((error = cliprdr_server_receive_general_capability(context, s)))
				{
					WLog_ERR(TAG, "cliprdr_server_receive_general_capability failed with error %"PRIu32"",
					         error);
					return error;
				}

				break;

			default:
				WLog_ERR(TAG, "unknown cliprdr capability set: %"PRIu16"", capabilitySetType);
				return ERROR_INVALID_DATA;
				break;
		}
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_receive_temporary_directory(CliprdrServerContext*
        context, wStream* s, CLIPRDR_HEADER* header)
{
	int length;
	WCHAR* wszTempDir;
	CLIPRDR_TEMP_DIRECTORY tempDirectory;
	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*) context->handle;
	size_t slength;
	UINT error = CHANNEL_RC_OK;

	if ((slength = Stream_GetRemainingLength(s)) < 520)
	{
		WLog_ERR(TAG,
		         "Stream_GetRemainingLength returned %"PRIuz" but should at least be 520", slength);
		return CHANNEL_RC_NO_MEMORY;
	}

	wszTempDir = (WCHAR*) Stream_Pointer(s);

	if (wszTempDir[260] != 0)
	{
		WLog_ERR(TAG, "wszTempDir[260] was not 0");
		return ERROR_INVALID_DATA;
	}

	free(cliprdr->temporaryDirectory);
	cliprdr->temporaryDirectory = NULL;

	if (ConvertFromUnicode(CP_UTF8, 0, wszTempDir, -1,
	                       &(cliprdr->temporaryDirectory), 0, NULL, NULL) < 1)
	{
		WLog_ERR(TAG, "failed to convert temporary directory name");
		return ERROR_INVALID_DATA;
	}

	length = strlen(cliprdr->temporaryDirectory);

	if (length > 519)
		length = 519;

	CopyMemory(tempDirectory.szTempDir, cliprdr->temporaryDirectory, length);
	tempDirectory.szTempDir[length] = '\0';
	WLog_DBG(TAG, "CliprdrTemporaryDirectory: %s", cliprdr->temporaryDirectory);
	IFCALLRET(context->TempDirectory, error, context, &tempDirectory);

	if (error)
		WLog_ERR(TAG, "TempDirectory failed with error %"PRIu32"!", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_receive_format_list(CliprdrServerContext* context,
        wStream* s, CLIPRDR_HEADER* header)
{
	UINT32 index;
	UINT32 dataLen;
	size_t position;
	BOOL asciiNames;
	int formatNameLength;
	char* szFormatName;
	WCHAR* wszFormatName;
	CLIPRDR_FORMAT* formats = NULL;
	CLIPRDR_FORMAT_LIST formatList;
	UINT error = CHANNEL_RC_OK;
	dataLen = header->dataLen;
	asciiNames = (header->msgFlags & CB_ASCII_NAMES) ? TRUE : FALSE;
	formatList.msgType = CB_FORMAT_LIST;
	formatList.msgFlags = header->msgFlags;
	formatList.dataLen = header->dataLen;
	index = 0;
	formatList.numFormats = 0;
	position = Stream_GetPosition(s);

	if (!header->dataLen)
	{
		/* empty format list */
		formatList.formats = NULL;
		formatList.numFormats = 0;
	}
	else if (!context->useLongFormatNames)
	{
		formatList.numFormats = (dataLen / 36);

		if ((formatList.numFormats * 36) != dataLen)
		{
			WLog_ERR(TAG, "Invalid short format list length: %"PRIu32"", dataLen);
			return ERROR_INVALID_PARAMETER;
		}

		if (formatList.numFormats)
			formats = (CLIPRDR_FORMAT*) calloc(formatList.numFormats,
			                                   sizeof(CLIPRDR_FORMAT));

		if (!formats)
		{
			WLog_ERR(TAG, "calloc failed!");
			return CHANNEL_RC_NO_MEMORY;
		}

		formatList.formats = formats;

		while (dataLen)
		{
			Stream_Read_UINT32(s, formats[index].formatId); /* formatId (4 bytes) */
			dataLen -= 4;
			formats[index].formatName = NULL;

			/* According to MS-RDPECLIP 2.2.3.1.1.1 formatName is "a 32-byte block containing
			 * the *null-terminated* name assigned to the Clipboard Format: (32 ASCII 8 characters
			 * or 16 Unicode characters)"
			 * However, both Windows RDSH and mstsc violate this specs as seen in the following
			 * example of a transferred short format name string: [R.i.c.h. .T.e.x.t. .F.o.r.m.a.t.]
			 * These are 16 unicode charaters - *without* terminating null !
			 */

			if (asciiNames)
			{
				szFormatName = (char*) Stream_Pointer(s);

				if (szFormatName[0])
				{
					/* ensure null termination */
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
					/* ConvertFromUnicode always returns a null-terminated
					 * string on success, even if the source string isn't.
					 */
					if (ConvertFromUnicode(CP_UTF8, 0, wszFormatName, 16,
					                       &(formats[index].formatName), 0, NULL, NULL) < 1)
					{
						WLog_ERR(TAG, "failed to convert short clipboard format name");
						error = ERROR_INVALID_DATA;
						goto out;
					}
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
			formats = (CLIPRDR_FORMAT*) calloc(formatList.numFormats,
			                                   sizeof(CLIPRDR_FORMAT));

		if (!formats)
		{
			WLog_ERR(TAG, "calloc failed!");
			return CHANNEL_RC_NO_MEMORY;
		}

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
				if (ConvertFromUnicode(CP_UTF8, 0, wszFormatName, -1,
				                       &(formats[index].formatName), 0, NULL, NULL) < 1)
				{
					WLog_ERR(TAG, "failed to convert long clipboard format name");
					error = ERROR_INVALID_DATA;
					goto out;
				}
			}

			Stream_Seek(s, (formatNameLength + 1) * 2);
			dataLen -= ((formatNameLength + 1) * 2);
			index++;
		}
	}

	WLog_DBG(TAG, "ClientFormatList: numFormats: %"PRIu32"",
	         formatList.numFormats);
	IFCALLRET(context->ClientFormatList, error, context, &formatList);

	if (error)
		WLog_ERR(TAG, "ClientFormatList failed with error %"PRIu32"!", error);

out:

	for (index = 0; index < formatList.numFormats; index++)
	{
		free(formatList.formats[index].formatName);
	}

	free(formatList.formats);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_receive_format_list_response(
    CliprdrServerContext* context, wStream* s, CLIPRDR_HEADER* header)
{
	CLIPRDR_FORMAT_LIST_RESPONSE formatListResponse;
	UINT error = CHANNEL_RC_OK;
	WLog_DBG(TAG, "CliprdrClientFormatListResponse");
	formatListResponse.msgType = CB_FORMAT_LIST_RESPONSE;
	formatListResponse.msgFlags = header->msgFlags;
	formatListResponse.dataLen = header->dataLen;
	IFCALLRET(context->ClientFormatListResponse, error, context,
	          &formatListResponse);

	if (error)
		WLog_ERR(TAG, "ClientFormatListResponse failed with error %"PRIu32"!", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_receive_lock_clipdata(CliprdrServerContext* context,
        wStream* s, CLIPRDR_HEADER* header)
{
	CLIPRDR_LOCK_CLIPBOARD_DATA lockClipboardData;
	UINT error = CHANNEL_RC_OK;
	WLog_DBG(TAG, "CliprdrClientLockClipData");

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_ERR(TAG, "not enough data in stream!");
		return ERROR_INVALID_DATA;
	}

	lockClipboardData.msgType = CB_LOCK_CLIPDATA;
	lockClipboardData.msgFlags = header->msgFlags;
	lockClipboardData.dataLen = header->dataLen;
	Stream_Read_UINT32(s, lockClipboardData.clipDataId); /* clipDataId (4 bytes) */
	IFCALLRET(context->ClientLockClipboardData, error, context, &lockClipboardData);

	if (error)
		WLog_ERR(TAG, "ClientLockClipboardData failed with error %"PRIu32"!", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_receive_unlock_clipdata(CliprdrServerContext*
        context, wStream* s, CLIPRDR_HEADER* header)
{
	CLIPRDR_UNLOCK_CLIPBOARD_DATA unlockClipboardData;
	UINT error = CHANNEL_RC_OK;
	WLog_DBG(TAG, "CliprdrClientUnlockClipData");
	unlockClipboardData.msgType = CB_UNLOCK_CLIPDATA;
	unlockClipboardData.msgFlags = header->msgFlags;
	unlockClipboardData.dataLen = header->dataLen;

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_ERR(TAG, "not enough data in stream!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s,
	                   unlockClipboardData.clipDataId); /* clipDataId (4 bytes) */
	IFCALLRET(context->ClientUnlockClipboardData, error, context,
	          &unlockClipboardData);

	if (error)
		WLog_ERR(TAG, "ClientUnlockClipboardData failed with error %"PRIu32"!", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_receive_format_data_request(CliprdrServerContext*
        context, wStream* s, CLIPRDR_HEADER* header)
{
	CLIPRDR_FORMAT_DATA_REQUEST formatDataRequest;
	UINT error = CHANNEL_RC_OK;
	WLog_DBG(TAG, "CliprdrClientFormatDataRequest");
	formatDataRequest.msgType = CB_FORMAT_DATA_REQUEST;
	formatDataRequest.msgFlags = header->msgFlags;
	formatDataRequest.dataLen = header->dataLen;

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_ERR(TAG, "not enough data in stream!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s,
	                   formatDataRequest.requestedFormatId); /* requestedFormatId (4 bytes) */
	IFCALLRET(context->ClientFormatDataRequest, error, context, &formatDataRequest);

	if (error)
		WLog_ERR(TAG, "ClientFormatDataRequest failed with error %"PRIu32"!", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_receive_format_data_response(
    CliprdrServerContext* context, wStream* s, CLIPRDR_HEADER* header)
{
	CLIPRDR_FORMAT_DATA_RESPONSE formatDataResponse;
	UINT error = CHANNEL_RC_OK;
	WLog_DBG(TAG, "CliprdrClientFormatDataResponse");
	formatDataResponse.msgType = CB_FORMAT_DATA_RESPONSE;
	formatDataResponse.msgFlags = header->msgFlags;
	formatDataResponse.dataLen = header->dataLen;
	formatDataResponse.requestedFormatData = NULL;

	if (Stream_GetRemainingLength(s) < header->dataLen)
	{
		WLog_ERR(TAG, "not enough data in stream!");
		return ERROR_INVALID_DATA;
	}

	if (header->dataLen)
	{
		formatDataResponse.requestedFormatData = (BYTE*) malloc(header->dataLen);
		Stream_Read(s, formatDataResponse.requestedFormatData, header->dataLen);
	}

	IFCALLRET(context->ClientFormatDataResponse, error, context,
	          &formatDataResponse);

	if (error)
		WLog_ERR(TAG, "ClientFormatDataResponse failed with error %"PRIu32"!", error);

	free(formatDataResponse.requestedFormatData);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_receive_filecontents_request(
    CliprdrServerContext* context, wStream* s, CLIPRDR_HEADER* header)
{
	CLIPRDR_FILE_CONTENTS_REQUEST request;
	UINT error = CHANNEL_RC_OK;
	WLog_DBG(TAG, "CliprdrClientFileContentsRequest");
	request.msgType = CB_FILECONTENTS_REQUEST;
	request.msgFlags = header->msgFlags;
	request.dataLen = header->dataLen;

	if (Stream_GetRemainingLength(s) < 24)
	{
		WLog_ERR(TAG, "not enough data in stream!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, request.streamId); /* streamId (4 bytes) */
	Stream_Read_UINT32(s, request.listIndex); /* listIndex (4 bytes) */
	Stream_Read_UINT32(s, request.dwFlags); /* dwFlags (4 bytes) */
	Stream_Read_UINT32(s, request.nPositionLow); /* nPositionLow (4 bytes) */
	Stream_Read_UINT32(s, request.nPositionHigh); /* nPositionHigh (4 bytes) */
	Stream_Read_UINT32(s, request.cbRequested); /* cbRequested (4 bytes) */

	if (Stream_GetRemainingLength(s) < 4) /* clipDataId (4 bytes) optional */
		request.clipDataId = 0;
	else
		Stream_Read_UINT32(s, request.clipDataId);

	IFCALLRET(context->ClientFileContentsRequest, error, context, &request);

	if (error)
		WLog_ERR(TAG, "ClientFileContentsRequest failed with error %"PRIu32"!", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_receive_filecontents_response(
    CliprdrServerContext* context, wStream* s, CLIPRDR_HEADER* header)
{
	CLIPRDR_FILE_CONTENTS_RESPONSE response;
	UINT error = CHANNEL_RC_OK;
	WLog_DBG(TAG, "CliprdrClientFileContentsResponse");
	response.msgType = CB_FILECONTENTS_RESPONSE;
	response.msgFlags = header->msgFlags;
	response.dataLen = header->dataLen;

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_ERR(TAG, "not enough data in stream!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, response.streamId); /* streamId (4 bytes) */
	response.cbRequested = header->dataLen - 4;
	response.requestedData = Stream_Pointer(s); /* requestedFileContentsData */
	IFCALLRET(context->ClientFileContentsResponse, error, context, &response);

	if (error)
		WLog_ERR(TAG, "ClientFileContentsResponse failed with error %"PRIu32"!", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_receive_pdu(CliprdrServerContext* context,
                                       wStream* s, CLIPRDR_HEADER* header)
{
	UINT error;
	WLog_DBG(TAG,
	         "CliprdrServerReceivePdu: msgType: %"PRIu16" msgFlags: 0x%04"PRIX16" dataLen: %"PRIu32"",
	         header->msgType, header->msgFlags, header->dataLen);

	switch (header->msgType)
	{
		case CB_CLIP_CAPS:
			if ((error = cliprdr_server_receive_capabilities(context, s, header)))
				WLog_ERR(TAG, "cliprdr_server_receive_capabilities failed with error %"PRIu32"!",
				         error);

			break;

		case CB_TEMP_DIRECTORY:
			if ((error = cliprdr_server_receive_temporary_directory(context, s, header)))
				WLog_ERR(TAG,
				         "cliprdr_server_receive_temporary_directory failed with error %"PRIu32"!", error);

			break;

		case CB_FORMAT_LIST:
			if ((error = cliprdr_server_receive_format_list(context, s, header)))
				WLog_ERR(TAG, "cliprdr_server_receive_format_list failed with error %"PRIu32"!",
				         error);

			break;

		case CB_FORMAT_LIST_RESPONSE:
			if ((error = cliprdr_server_receive_format_list_response(context, s, header)))
				WLog_ERR(TAG,
				         "cliprdr_server_receive_format_list_response failed with error %"PRIu32"!", error);

			break;

		case CB_LOCK_CLIPDATA:
			if ((error = cliprdr_server_receive_lock_clipdata(context, s, header)))
				WLog_ERR(TAG, "cliprdr_server_receive_lock_clipdata failed with error %"PRIu32"!",
				         error);

			break;

		case CB_UNLOCK_CLIPDATA:
			if ((error = cliprdr_server_receive_unlock_clipdata(context, s, header)))
				WLog_ERR(TAG, "cliprdr_server_receive_unlock_clipdata failed with error %"PRIu32"!",
				         error);

			break;

		case CB_FORMAT_DATA_REQUEST:
			if ((error = cliprdr_server_receive_format_data_request(context, s, header)))
				WLog_ERR(TAG,
				         "cliprdr_server_receive_format_data_request failed with error %"PRIu32"!", error);

			break;

		case CB_FORMAT_DATA_RESPONSE:
			if ((error = cliprdr_server_receive_format_data_response(context, s, header)))
				WLog_ERR(TAG,
				         "cliprdr_server_receive_format_data_response failed with error %"PRIu32"!", error);

			break;

		case CB_FILECONTENTS_REQUEST:
			if ((error = cliprdr_server_receive_filecontents_request(context, s, header)))
				WLog_ERR(TAG,
				         "cliprdr_server_receive_filecontents_request failed with error %"PRIu32"!", error);

			break;

		case CB_FILECONTENTS_RESPONSE:
			if ((error = cliprdr_server_receive_filecontents_response(context, s, header)))
				WLog_ERR(TAG,
				         "cliprdr_server_receive_filecontents_response failed with error %"PRIu32"!", error);

			break;

		default:
			error = ERROR_INVALID_DATA;
			WLog_DBG(TAG, "Unexpected clipboard PDU type: %"PRIu16"", header->msgType);
			break;
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_init(CliprdrServerContext* context)
{
	UINT32 generalFlags;
	CLIPRDR_CAPABILITIES capabilities;
	CLIPRDR_MONITOR_READY monitorReady;
	CLIPRDR_GENERAL_CAPABILITY_SET generalCapabilitySet;
	UINT error;
	ZeroMemory(&capabilities, sizeof(capabilities));
	ZeroMemory(&monitorReady, sizeof(monitorReady));
	generalFlags = 0;

	if (context->useLongFormatNames)
		generalFlags |= CB_USE_LONG_FORMAT_NAMES;

	if (context->streamFileClipEnabled)
		generalFlags |= CB_STREAM_FILECLIP_ENABLED;

	if (context->fileClipNoFilePaths)
		generalFlags |= CB_FILECLIP_NO_FILE_PATHS;

	if (context->canLockClipData)
		generalFlags |= CB_CAN_LOCK_CLIPDATA;

	capabilities.msgType = CB_CLIP_CAPS;
	capabilities.msgFlags = 0;
	capabilities.dataLen = 4 + CB_CAPSTYPE_GENERAL_LEN;
	capabilities.cCapabilitiesSets = 1;
	capabilities.capabilitySets = (CLIPRDR_CAPABILITY_SET*) &generalCapabilitySet;
	generalCapabilitySet.capabilitySetType = CB_CAPSTYPE_GENERAL;
	generalCapabilitySet.capabilitySetLength = CB_CAPSTYPE_GENERAL_LEN;
	generalCapabilitySet.version = CB_CAPS_VERSION_2;
	generalCapabilitySet.generalFlags = generalFlags;

	if ((error = context->ServerCapabilities(context, &capabilities)))
	{
		WLog_ERR(TAG, "ServerCapabilities failed with error %"PRIu32"!", error);
		return error;
	}

	if ((error = context->MonitorReady(context, &monitorReady)))
	{
		WLog_ERR(TAG, "MonitorReady failed with error %"PRIu32"!", error);
		return error;
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT cliprdr_server_read(CliprdrServerContext* context)
{
	wStream* s;
	size_t position;
	DWORD BytesToRead;
	DWORD BytesReturned;
	CLIPRDR_HEADER header;
	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*) context->handle;
	UINT error;
	DWORD status;
	s = cliprdr->s;

	if (Stream_GetPosition(s) < CLIPRDR_HEADER_LENGTH)
	{
		BytesReturned = 0;
		BytesToRead = CLIPRDR_HEADER_LENGTH - Stream_GetPosition(s);
		status = WaitForSingleObject(cliprdr->ChannelEvent, 0);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"", error);
			return error;
		}

		if (status == WAIT_TIMEOUT)
			return CHANNEL_RC_OK;

		if (!WTSVirtualChannelRead(cliprdr->ChannelHandle, 0,
		                           (PCHAR) Stream_Pointer(s), BytesToRead, &BytesReturned))
		{
			WLog_ERR(TAG, "WTSVirtualChannelRead failed!");
			return ERROR_INTERNAL_ERROR;
		}

		Stream_Seek(s, BytesReturned);
	}

	if (Stream_GetPosition(s) >= CLIPRDR_HEADER_LENGTH)
	{
		position = Stream_GetPosition(s);
		Stream_SetPosition(s, 0);
		Stream_Read_UINT16(s, header.msgType); /* msgType (2 bytes) */
		Stream_Read_UINT16(s, header.msgFlags); /* msgFlags (2 bytes) */
		Stream_Read_UINT32(s, header.dataLen); /* dataLen (4 bytes) */

		if (!Stream_EnsureCapacity(s, (header.dataLen + CLIPRDR_HEADER_LENGTH)))
		{
			WLog_ERR(TAG, "Stream_EnsureCapacity failed!");
			return CHANNEL_RC_NO_MEMORY;
		}

		Stream_SetPosition(s, position);

		if (Stream_GetPosition(s) < (header.dataLen + CLIPRDR_HEADER_LENGTH))
		{
			BytesReturned = 0;
			BytesToRead = (header.dataLen + CLIPRDR_HEADER_LENGTH) - Stream_GetPosition(s);
			status = WaitForSingleObject(cliprdr->ChannelEvent, 0);

			if (status == WAIT_FAILED)
			{
				error = GetLastError();
				WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"", error);
				return error;
			}

			if (status == WAIT_TIMEOUT)
				return CHANNEL_RC_OK;

			if (!WTSVirtualChannelRead(cliprdr->ChannelHandle, 0,
			                           (PCHAR) Stream_Pointer(s), BytesToRead, &BytesReturned))
			{
				WLog_ERR(TAG, "WTSVirtualChannelRead failed!");
				return ERROR_INTERNAL_ERROR;
			}

			Stream_Seek(s, BytesReturned);
		}

		if (Stream_GetPosition(s) >= (header.dataLen + CLIPRDR_HEADER_LENGTH))
		{
			Stream_SetPosition(s, (header.dataLen + CLIPRDR_HEADER_LENGTH));
			Stream_SealLength(s);
			Stream_SetPosition(s, CLIPRDR_HEADER_LENGTH);

			if ((error = cliprdr_server_receive_pdu(context, s, &header)))
			{
				WLog_ERR(TAG, "cliprdr_server_receive_pdu failed with error code %"PRIu32"!", error);
				return error;
			}

			Stream_SetPosition(s, 0);
			/* check for trailing zero bytes */
			status = WaitForSingleObject(cliprdr->ChannelEvent, 0);

			if (status == WAIT_FAILED)
			{
				error = GetLastError();
				WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"", error);
				return error;
			}

			if (status == WAIT_TIMEOUT)
				return CHANNEL_RC_OK;

			BytesReturned = 0;
			BytesToRead = 4;

			if (!WTSVirtualChannelRead(cliprdr->ChannelHandle, 0,
			                           (PCHAR) Stream_Pointer(s), BytesToRead, &BytesReturned))
			{
				WLog_ERR(TAG, "WTSVirtualChannelRead failed!");
				return ERROR_INTERNAL_ERROR;
			}

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

	return CHANNEL_RC_OK;
}

static DWORD WINAPI cliprdr_server_thread(LPVOID arg)
{
	DWORD status;
	DWORD nCount;
	HANDLE events[8];
	HANDLE ChannelEvent;
	CliprdrServerContext* context = (CliprdrServerContext*) arg;
	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*) context->handle;
	UINT error;

	ChannelEvent = context->GetEventHandle(context);
	nCount = 0;
	events[nCount++] = cliprdr->StopEvent;
	events[nCount++] = ChannelEvent;

	if ((error = cliprdr_server_init(context)))
	{
		WLog_ERR(TAG, "cliprdr_server_init failed with error %"PRIu32"!", error);
		goto out;
	}

	while (1)
	{
		status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForMultipleObjects failed with error %"PRIu32"", error);
			goto out;
		}

		status = WaitForSingleObject(cliprdr->StopEvent, 0);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"", error);
			goto out;
		}

		if (status == WAIT_OBJECT_0)
			break;

		status = WaitForSingleObject(ChannelEvent, 0);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"", error);
			goto out;
		}

		if (status == WAIT_OBJECT_0)
		{
			if ((error = context->CheckEventHandle(context)))
			{
				WLog_ERR(TAG, "CheckEventHandle failed with error %"PRIu32"!", error);
				break;
			}
		}
	}

out:

	if (error && context->rdpcontext)
		setChannelError(context->rdpcontext, error,
		                "cliprdr_server_thread reported an error");

	ExitThread(error);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_open(CliprdrServerContext* context)
{
	void* buffer = NULL;
	DWORD BytesReturned = 0;
	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*) context->handle;
	cliprdr->ChannelHandle = WTSVirtualChannelOpen(cliprdr->vcm,
	                         WTS_CURRENT_SESSION, "cliprdr");

	if (!cliprdr->ChannelHandle)
	{
		WLog_ERR(TAG, "WTSVirtualChannelOpen for cliprdr failed!");
		return ERROR_INTERNAL_ERROR;
	}

	cliprdr->ChannelEvent = NULL;

	if (WTSVirtualChannelQuery(cliprdr->ChannelHandle, WTSVirtualEventHandle,
	                           &buffer, &BytesReturned))
	{
		if (BytesReturned != sizeof(HANDLE))
		{
			WLog_ERR(TAG, "BytesReturned has not size of HANDLE!");
			return ERROR_INTERNAL_ERROR;
		}

		CopyMemory(&(cliprdr->ChannelEvent), buffer, sizeof(HANDLE));
		WTSFreeMemory(buffer);
	}

	if (!cliprdr->ChannelEvent)
	{
		WLog_ERR(TAG, "WTSVirtualChannelQuery for cliprdr failed!");
		return ERROR_INTERNAL_ERROR;
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_close(CliprdrServerContext* context)
{
	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*) context->handle;

	if (cliprdr->ChannelHandle)
	{
		WTSVirtualChannelClose(cliprdr->ChannelHandle);
		cliprdr->ChannelHandle = NULL;
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_start(CliprdrServerContext* context)
{
	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*) context->handle;
	UINT error;

	if (!cliprdr->ChannelHandle)
	{
		if ((error = context->Open(context)))
		{
			WLog_ERR(TAG, "Open failed!");
			return error;
		}
	}

	if (!(cliprdr->StopEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
	{
		WLog_ERR(TAG, "CreateEvent failed!");
		return ERROR_INTERNAL_ERROR;
	}

	if (!(cliprdr->Thread = CreateThread(NULL, 0, cliprdr_server_thread, (void*) context, 0, NULL)))
	{
		WLog_ERR(TAG, "CreateThread failed!");
		CloseHandle(cliprdr->StopEvent);
		cliprdr->StopEvent = NULL;
		return ERROR_INTERNAL_ERROR;
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_stop(CliprdrServerContext* context)
{
	UINT error = CHANNEL_RC_OK;
	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*) context->handle;

	if (cliprdr->StopEvent)
	{
		SetEvent(cliprdr->StopEvent);

		if (WaitForSingleObject(cliprdr->Thread, INFINITE) == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"", error);
			return error;
		}

		CloseHandle(cliprdr->Thread);
		CloseHandle(cliprdr->StopEvent);
	}

	if (cliprdr->ChannelHandle)
		return context->Close(context);

	return error;
}

static HANDLE cliprdr_server_get_event_handle(CliprdrServerContext* context)
{
	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*) context->handle;
	return cliprdr->ChannelEvent;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_check_event_handle(CliprdrServerContext* context)
{
	return cliprdr_server_read(context);
}

CliprdrServerContext* cliprdr_server_context_new(HANDLE vcm)
{
	CliprdrServerContext* context;
	CliprdrServerPrivate* cliprdr;
	context = (CliprdrServerContext*) calloc(1, sizeof(CliprdrServerContext));

	if (context)
	{
		context->Open = cliprdr_server_open;
		context->Close = cliprdr_server_close;
		context->Start = cliprdr_server_start;
		context->Stop = cliprdr_server_stop;
		context->GetEventHandle = cliprdr_server_get_event_handle;
		context->CheckEventHandle = cliprdr_server_check_event_handle;
		context->ServerCapabilities = cliprdr_server_capabilities;
		context->MonitorReady = cliprdr_server_monitor_ready;
		context->ServerFormatList = cliprdr_server_format_list;
		context->ServerFormatListResponse = cliprdr_server_format_list_response;
		context->ServerLockClipboardData = cliprdr_server_lock_clipboard_data;
		context->ServerUnlockClipboardData = cliprdr_server_unlock_clipboard_data;
		context->ServerFormatDataRequest = cliprdr_server_format_data_request;
		context->ServerFormatDataResponse = cliprdr_server_format_data_response;
		context->ServerFileContentsRequest = cliprdr_server_file_contents_request;
		context->ServerFileContentsResponse = cliprdr_server_file_contents_response;
		cliprdr = context->handle = (CliprdrServerPrivate*) calloc(1,
		                            sizeof(CliprdrServerPrivate));

		if (cliprdr)
		{
			cliprdr->vcm = vcm;
			cliprdr->s = Stream_New(NULL, 4096);

			if (!cliprdr->s)
			{
				WLog_ERR(TAG, "Stream_New failed!");
				free(context->handle);
				free(context);
				return NULL;
			}
		}
		else
		{
			WLog_ERR(TAG, "calloc failed!");
			free(context);
			return NULL;
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
		Stream_Free(cliprdr->s, TRUE);
		free(cliprdr->temporaryDirectory);
	}

	free(context->handle);
	free(context);
}
