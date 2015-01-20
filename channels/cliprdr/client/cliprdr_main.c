/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Clipboard Virtual Channel
 *
 * Copyright 2009-2011 Jay Sorg
 * Copyright 2010-2011 Vic Lee
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

#include <freerdp/types.h>
#include <freerdp/constants.h>
#include <freerdp/client/cliprdr.h>

#include "cliprdr_main.h"
#include "cliprdr_format.h"

const char* const CB_MSG_TYPE_STRINGS[] =
{
	"",
	"CB_MONITOR_READY",
	"CB_FORMAT_LIST",
	"CB_FORMAT_LIST_RESPONSE",
	"CB_FORMAT_DATA_REQUEST",
	"CB_FORMAT_DATA_RESPONSE",
	"CB_TEMP_DIRECTORY",
	"CB_CLIP_CAPS",
	"CB_FILECONTENTS_REQUEST",
	"CB_FILECONTENTS_RESPONSE",
	"CB_LOCK_CLIPDATA"
	"CB_UNLOCK_CLIPDATA"
};

CliprdrClientContext* cliprdr_get_client_interface(cliprdrPlugin* cliprdr)
{
	CliprdrClientContext* pInterface;
	pInterface = (CliprdrClientContext*) cliprdr->channelEntryPoints.pInterface;
	return pInterface;
}

wStream* cliprdr_packet_new(UINT16 msgType, UINT16 msgFlags, UINT32 dataLen)
{
	wStream* s;

	s = Stream_New(NULL, dataLen + 8);

	Stream_Write_UINT16(s, msgType);
	Stream_Write_UINT16(s, msgFlags);

	/* Write actual length after the entire packet has been constructed. */
	Stream_Seek(s, 4);

	return s;
}

void cliprdr_packet_send(cliprdrPlugin* cliprdr, wStream* s)
{
	int pos;
	UINT32 dataLen;
	UINT32 status = 0;

	pos = Stream_GetPosition(s);

	dataLen = pos - 8;

	Stream_SetPosition(s, 4);
	Stream_Write_UINT32(s, dataLen);
	Stream_SetPosition(s, pos);

#ifdef WITH_DEBUG_CLIPRDR
	WLog_DBG(TAG, "Cliprdr Sending (%d bytes)", dataLen + 8);
	winpr_HexDump(TAG, WLOG_DEBUG, Stream_Buffer(s), dataLen + 8);
#endif

	if (!cliprdr)
	{
		status = CHANNEL_RC_BAD_INIT_HANDLE;
	}
	else
	{
		status = cliprdr->channelEntryPoints.pVirtualChannelWrite(cliprdr->OpenHandle,
			Stream_Buffer(s), (UINT32) Stream_GetPosition(s), s);
	}

	if (status != CHANNEL_RC_OK)
	{
		Stream_Free(s, TRUE);
		WLog_ERR(TAG,  "VirtualChannelWrite failed with %s [%08X]",
				 WTSErrorToString(status), status);
	}
}

void cliprdr_print_general_capability_flags(UINT32 flags)
{
	WLog_INFO(TAG,  "generalFlags (0x%08X) {", flags);

	if (flags & CB_USE_LONG_FORMAT_NAMES)
		WLog_INFO(TAG,  "\tCB_USE_LONG_FORMAT_NAMES");

	if (flags & CB_STREAM_FILECLIP_ENABLED)
		WLog_INFO(TAG,  "\tCB_STREAM_FILECLIP_ENABLED");

	if (flags & CB_FILECLIP_NO_FILE_PATHS)
		WLog_INFO(TAG,  "\tCB_FILECLIP_NO_FILE_PATHS");

	if (flags & CB_CAN_LOCK_CLIPDATA)
		WLog_INFO(TAG,  "\tCB_CAN_LOCK_CLIPDATA");

	WLog_INFO(TAG,  "}");
}

static int cliprdr_process_general_capability(cliprdrPlugin* cliprdr, wStream* s)
{
	UINT32 version;
	UINT32 generalFlags;
	CLIPRDR_CAPABILITIES capabilities;
	CLIPRDR_GENERAL_CAPABILITY_SET generalCapabilitySet;
	CliprdrClientContext* context = cliprdr_get_client_interface(cliprdr);
	
	Stream_Read_UINT32(s, version); /* version (4 bytes) */
	Stream_Read_UINT32(s, generalFlags); /* generalFlags (4 bytes) */
	
	DEBUG_CLIPRDR("Version: %d", version);
#ifdef WITH_DEBUG_CLIPRDR
	cliprdr_print_general_capability_flags(generalFlags);
#endif

	if (cliprdr->useLongFormatNames)
		cliprdr->useLongFormatNames = (generalFlags & CB_USE_LONG_FORMAT_NAMES) ? TRUE : FALSE;

	if (cliprdr->streamFileClipEnabled)
		cliprdr->streamFileClipEnabled = (generalFlags & CB_STREAM_FILECLIP_ENABLED) ? TRUE : FALSE;

	if (cliprdr->fileClipNoFilePaths)
		cliprdr->fileClipNoFilePaths = (generalFlags & CB_FILECLIP_NO_FILE_PATHS) ? TRUE : FALSE;

	if (cliprdr->canLockClipData)
		cliprdr->canLockClipData = (generalFlags & CB_CAN_LOCK_CLIPDATA) ? TRUE : FALSE;
	
	cliprdr->capabilitiesReceived = TRUE;

	if (!context->custom)
		return -1;
		
	capabilities.cCapabilitiesSets = 1;
	capabilities.capabilitySets = (CLIPRDR_CAPABILITY_SET*) &(generalCapabilitySet);
	generalCapabilitySet.capabilitySetType = CB_CAPSTYPE_GENERAL;
	generalCapabilitySet.capabilitySetLength = 12;
	generalCapabilitySet.version = version;
	generalCapabilitySet.generalFlags = generalFlags;

	if (context->ServerCapabilities)
		context->ServerCapabilities(context, &capabilities);

	return 1;
}

static int cliprdr_process_clip_caps(cliprdrPlugin* cliprdr, wStream* s, UINT16 length, UINT16 flags)
{
	UINT16 index;
	UINT16 lengthCapability;
	UINT16 cCapabilitiesSets;
	UINT16 capabilitySetType;
	
	Stream_Read_UINT16(s, cCapabilitiesSets); /* cCapabilitiesSets (2 bytes) */
	Stream_Seek_UINT16(s); /* pad1 (2 bytes) */

	WLog_Print(cliprdr->log, WLOG_DEBUG, "ServerCapabilities");

	for (index = 0; index < cCapabilitiesSets; index++)
	{
		Stream_Read_UINT16(s, capabilitySetType); /* capabilitySetType (2 bytes) */
		Stream_Read_UINT16(s, lengthCapability); /* lengthCapability (2 bytes) */

		switch (capabilitySetType)
		{
			case CB_CAPSTYPE_GENERAL:
				cliprdr_process_general_capability(cliprdr, s);
				break;
				
			default:
				WLog_ERR(TAG, "unknown cliprdr capability set: %d", capabilitySetType);
				break;
		}
	}

	return 1;
}

static int cliprdr_process_monitor_ready(cliprdrPlugin* cliprdr, wStream* s, UINT16 length, UINT16 flags)
{
	CLIPRDR_MONITOR_READY monitorReady;
	CliprdrClientContext* context = cliprdr_get_client_interface(cliprdr);

	WLog_Print(cliprdr->log, WLOG_DEBUG, "MonitorReady");

	if (!context->custom)
		return -1;

	if (!cliprdr->capabilitiesReceived)
	{
		/**
		  * The clipboard capabilities pdu from server to client is optional,
		  * but a server using it must send it before sending the monitor ready pdu.
		  * When the server capabilities pdu is not used, default capabilities
		  * corresponding to a generalFlags field set to zero are assumed.
		  */
		
		cliprdr->useLongFormatNames = FALSE;
		cliprdr->streamFileClipEnabled = FALSE;
		cliprdr->fileClipNoFilePaths = TRUE;
		cliprdr->canLockClipData = FALSE;
	}
	
	monitorReady.msgType = CB_MONITOR_READY;
	monitorReady.msgFlags = flags;
	monitorReady.dataLen = length;

	if (context->MonitorReady)
		context->MonitorReady(context, &monitorReady);

	return 1;
}

static int cliprdr_process_filecontents_request(cliprdrPlugin* cliprdr, wStream* s, UINT32 length, UINT16 flags)
{
	CLIPRDR_FILE_CONTENTS_REQUEST request;
	CliprdrClientContext* context = cliprdr_get_client_interface(cliprdr);

	WLog_Print(cliprdr->log, WLOG_DEBUG, "FileContentsRequest");

	if (!context->custom)
		return -1;

	if (Stream_GetRemainingLength(s) < 28)
		return -1;

	request.msgType = CB_FILECONTENTS_REQUEST;
	request.msgFlags = flags;
	request.dataLen = length;

	Stream_Read_UINT32(s, request.streamId); /* streamId (4 bytes) */
	Stream_Read_UINT32(s, request.listIndex); /* listIndex (4 bytes) */
	Stream_Read_UINT32(s, request.dwFlags); /* dwFlags (4 bytes) */
	Stream_Read_UINT32(s, request.nPositionLow); /* nPositionLow (4 bytes) */
	Stream_Read_UINT32(s, request.nPositionHigh); /* nPositionHigh (4 bytes) */
	Stream_Read_UINT32(s, request.cbRequested); /* cbRequested (4 bytes) */
	Stream_Read_UINT32(s, request.clipDataId); /* clipDataId (4 bytes) */

	if (context->ServerFileContentsRequest)
		context->ServerFileContentsRequest(context, &request);

	return 1;
}

static int cliprdr_process_filecontents_response(cliprdrPlugin* cliprdr, wStream* s, UINT32 length, UINT16 flags)
{
	CLIPRDR_FILE_CONTENTS_RESPONSE response;
	CliprdrClientContext* context = cliprdr_get_client_interface(cliprdr);

	WLog_Print(cliprdr->log, WLOG_DEBUG, "FileContentsResponse");

	if (!context->custom)
		return -1;
		
	if (Stream_GetRemainingLength(s) < 4)
		return -1;

	response.msgType = CB_FILECONTENTS_RESPONSE;
	response.msgFlags = flags;
	response.dataLen = length;

	Stream_Read_UINT32(s, response.streamId); /* streamId (4 bytes) */
	
	response.cbRequested = length - 4;
	response.requestedData = Stream_Pointer(s); /* requestedFileContentsData */

	if (context->ServerFileContentsResponse)
		context->ServerFileContentsResponse(context, &response);

	return 1;
}

static int cliprdr_process_lock_clipdata(cliprdrPlugin* cliprdr, wStream* s, UINT32 length, UINT16 flags)
{
	CLIPRDR_LOCK_CLIPBOARD_DATA lockClipboardData;
	CliprdrClientContext* context = cliprdr_get_client_interface(cliprdr);

	WLog_Print(cliprdr->log, WLOG_DEBUG, "LockClipData");

	if (!context->custom)
		return -1;
	
	if (Stream_GetRemainingLength(s) < 4)
		return -1;

	lockClipboardData.msgType = CB_LOCK_CLIPDATA;
	lockClipboardData.msgFlags = flags;
	lockClipboardData.dataLen = length;

	Stream_Read_UINT32(s, lockClipboardData.clipDataId); /* clipDataId (4 bytes) */

	if (context->ServerLockClipboardData)
		context->ServerLockClipboardData(context, &lockClipboardData);

	return 1;
}

static int cliprdr_process_unlock_clipdata(cliprdrPlugin* cliprdr, wStream* s, UINT32 length, UINT16 flags)
{
	CLIPRDR_UNLOCK_CLIPBOARD_DATA unlockClipboardData;
	CliprdrClientContext* context = cliprdr_get_client_interface(cliprdr);

	WLog_Print(cliprdr->log, WLOG_DEBUG, "UnlockClipData");

	if (!context->custom)
		return -1;
	
	if (Stream_GetRemainingLength(s) < 4)
		return -1;

	unlockClipboardData.msgType = CB_UNLOCK_CLIPDATA;
	unlockClipboardData.msgFlags = flags;
	unlockClipboardData.dataLen = length;

	Stream_Read_UINT32(s, unlockClipboardData.clipDataId); /* clipDataId (4 bytes) */

	if (context->ServerUnlockClipboardData)
		context->ServerUnlockClipboardData(context, &unlockClipboardData);

	return 1;
}

static void cliprdr_order_recv(cliprdrPlugin* cliprdr, wStream* s)
{
	UINT16 msgType;
	UINT16 msgFlags;
	UINT32 dataLen;

	Stream_Read_UINT16(s, msgType); /* msgType (2 bytes) */
	Stream_Read_UINT16(s, msgFlags); /* msgFlags (2 bytes) */
	Stream_Read_UINT32(s, dataLen); /* dataLen (4 bytes) */

	DEBUG_CLIPRDR("msgType: %s (%d), msgFlags: %d dataLen: %d",
				  CB_MSG_TYPE_STRINGS[msgType], msgType, msgFlags, dataLen);
#ifdef WITH_DEBUG_CLIPRDR
	winpr_HexDump(TAG, WLOG_DEBUG, Stream_Buffer(s), dataLen + 8);
#endif

	switch (msgType)
	{
		case CB_CLIP_CAPS:
			cliprdr_process_clip_caps(cliprdr, s, dataLen, msgFlags);
			break;

		case CB_MONITOR_READY:
			cliprdr_process_monitor_ready(cliprdr, s, dataLen, msgFlags);
			break;

		case CB_FORMAT_LIST:
			cliprdr_process_format_list(cliprdr, s, dataLen, msgFlags);
			break;

		case CB_FORMAT_LIST_RESPONSE:
			cliprdr_process_format_list_response(cliprdr, s, dataLen, msgFlags);
			break;

		case CB_FORMAT_DATA_REQUEST:
			cliprdr_process_format_data_request(cliprdr, s, dataLen, msgFlags);
			break;

		case CB_FORMAT_DATA_RESPONSE:
			cliprdr_process_format_data_response(cliprdr, s, dataLen, msgFlags);
			break;

		case CB_FILECONTENTS_REQUEST:
			cliprdr_process_filecontents_request(cliprdr, s, dataLen, msgFlags);
			break;

		case CB_FILECONTENTS_RESPONSE:
			cliprdr_process_filecontents_response(cliprdr, s, dataLen, msgFlags);
			break;

		case CB_LOCK_CLIPDATA:
			cliprdr_process_lock_clipdata(cliprdr, s, dataLen, msgFlags);
			break;

		case CB_UNLOCK_CLIPDATA:
			cliprdr_process_unlock_clipdata(cliprdr, s, dataLen, msgFlags);
			break;

		default:
			WLog_ERR(TAG, "unknown msgType %d", msgType);
			break;
	}

	Stream_Free(s, TRUE);
}

/**
 * Callback Interface
 */

int cliprdr_client_capabilities(CliprdrClientContext* context, CLIPRDR_CAPABILITIES* capabilities)
{
	wStream* s;
	CLIPRDR_GENERAL_CAPABILITY_SET* generalCapabilitySet;
	cliprdrPlugin* cliprdr = (cliprdrPlugin*) context->handle;
	
	s = cliprdr_packet_new(CB_CLIP_CAPS, 0, 4 + CB_CAPSTYPE_GENERAL_LEN);

	Stream_Write_UINT16(s, 1); /* cCapabilitiesSets */
	Stream_Write_UINT16(s, 0); /* pad1 */

	generalCapabilitySet = (CLIPRDR_GENERAL_CAPABILITY_SET*) capabilities->capabilitySets;
	Stream_Write_UINT16(s, generalCapabilitySet->capabilitySetType); /* capabilitySetType */
	Stream_Write_UINT16(s, generalCapabilitySet->capabilitySetLength); /* lengthCapability */
	Stream_Write_UINT32(s, generalCapabilitySet->version); /* version */
	Stream_Write_UINT32(s, generalCapabilitySet->generalFlags); /* generalFlags */

	WLog_Print(cliprdr->log, WLOG_DEBUG, "ClientCapabilities");
	cliprdr_packet_send(cliprdr, s);

	return 0;
}

int cliprdr_temp_directory(CliprdrClientContext* context, CLIPRDR_TEMP_DIRECTORY* tempDirectory)
{
	int length;
	wStream* s;
	WCHAR* wszTempDir = NULL;
	cliprdrPlugin* cliprdr = (cliprdrPlugin*) context->handle;

	s = cliprdr_packet_new(CB_TEMP_DIRECTORY, 0, 520 * 2);

	length = ConvertToUnicode(CP_UTF8, 0, tempDirectory->szTempDir, -1, &wszTempDir, 0);

	if (length < 0)
		return -1;

	if (length > 520)
		length = 520;

	Stream_Write(s, tempDirectory->szTempDir, length * 2);
	Stream_Zero(s, (520 - length) * 2);

	free(wszTempDir);

	WLog_Print(cliprdr->log, WLOG_DEBUG, "TempDirectory: %s",
			tempDirectory->szTempDir);

	cliprdr_packet_send(cliprdr, s);

	return 1;
}

int cliprdr_client_format_list(CliprdrClientContext* context, CLIPRDR_FORMAT_LIST* formatList)
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
	cliprdrPlugin* cliprdr = (cliprdrPlugin*) context->handle;

	if (!cliprdr->useLongFormatNames)
	{
		length = formatList->numFormats * 36;
		
		s = cliprdr_packet_new(CB_FORMAT_LIST, 0, length);
		
		for (index = 0; index < formatList->numFormats; index++)
		{
			format = (CLIPRDR_FORMAT*) &(formatList->formats[index]);
			
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
					formatNameSize = ConvertToUnicode(CP_UTF8, 0, szFormatName, -1, &wszFormatName, 0);
				
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
			format = (CLIPRDR_FORMAT*) &(formatList->formats[index]);
			length += 4;
			formatNameSize = 2;

			if (format->formatName)
				formatNameSize = MultiByteToWideChar(CP_UTF8, 0, format->formatName, -1, NULL, 0) * 2;

			length += formatNameSize;
		}

		s = cliprdr_packet_new(CB_FORMAT_LIST, 0, length);

		for (index = 0; index < formatList->numFormats; index++)
		{
			format = (CLIPRDR_FORMAT*) &(formatList->formats[index]);
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

	WLog_Print(cliprdr->log, WLOG_DEBUG, "ClientFormatList: numFormats: %d",
			formatList->numFormats);
	
	cliprdr_packet_send(cliprdr, s);

	return 0;
}

int cliprdr_client_format_list_response(CliprdrClientContext* context, CLIPRDR_FORMAT_LIST_RESPONSE* formatListResponse)
{
	wStream* s;
	cliprdrPlugin* cliprdr = (cliprdrPlugin*) context->handle;

	formatListResponse->msgType = CB_FORMAT_LIST_RESPONSE;
	formatListResponse->dataLen = 0;

	s = cliprdr_packet_new(formatListResponse->msgType, formatListResponse->msgFlags, formatListResponse->dataLen);

	WLog_Print(cliprdr->log, WLOG_DEBUG, "ClientFormatListResponse");
	cliprdr_packet_send(cliprdr, s);

	return 0;
}

int cliprdr_client_lock_clipboard_data(CliprdrClientContext* context, CLIPRDR_LOCK_CLIPBOARD_DATA* lockClipboardData)
{
	wStream* s;
	cliprdrPlugin* cliprdr = (cliprdrPlugin*) context->handle;

	s = cliprdr_packet_new(CB_LOCK_CLIPDATA, 0, 4);

	Stream_Write_UINT32(s, lockClipboardData->clipDataId); /* clipDataId (4 bytes) */

	WLog_Print(cliprdr->log, WLOG_DEBUG, "ClientLockClipboardData: clipDataId: 0x%04X",
			lockClipboardData->clipDataId);

	cliprdr_packet_send(cliprdr, s);

	return 1;
}

int cliprdr_client_unlock_clipboard_data(CliprdrClientContext* context, CLIPRDR_UNLOCK_CLIPBOARD_DATA* unlockClipboardData)
{
	wStream* s;
	cliprdrPlugin* cliprdr = (cliprdrPlugin*) context->handle;

	s = cliprdr_packet_new(CB_UNLOCK_CLIPDATA, 0, 4);

	Stream_Write_UINT32(s, unlockClipboardData->clipDataId); /* clipDataId (4 bytes) */

	WLog_Print(cliprdr->log, WLOG_DEBUG, "ClientUnlockClipboardData: clipDataId: 0x%04X",
			unlockClipboardData->clipDataId);

	cliprdr_packet_send(cliprdr, s);

	return 1;
}

int cliprdr_client_format_data_request(CliprdrClientContext* context, CLIPRDR_FORMAT_DATA_REQUEST* formatDataRequest)
{
	wStream* s;
	cliprdrPlugin* cliprdr = (cliprdrPlugin*) context->handle;

	formatDataRequest->msgType = CB_FORMAT_DATA_REQUEST;
	formatDataRequest->msgFlags = 0;
	formatDataRequest->dataLen = 4;

	s = cliprdr_packet_new(formatDataRequest->msgType, formatDataRequest->msgFlags, formatDataRequest->dataLen);
	Stream_Write_UINT32(s, formatDataRequest->requestedFormatId); /* requestedFormatId (4 bytes) */

	WLog_Print(cliprdr->log, WLOG_DEBUG, "ClientFormatDataRequest");
	cliprdr_packet_send(cliprdr, s);

	return 0;
}

int cliprdr_client_format_data_response(CliprdrClientContext* context, CLIPRDR_FORMAT_DATA_RESPONSE* formatDataResponse)
{
	wStream* s;
	cliprdrPlugin* cliprdr = (cliprdrPlugin*) context->handle;

	formatDataResponse->msgType = CB_FORMAT_DATA_RESPONSE;

	s = cliprdr_packet_new(formatDataResponse->msgType, formatDataResponse->msgFlags, formatDataResponse->dataLen);

	Stream_Write(s, formatDataResponse->requestedFormatData, formatDataResponse->dataLen);

	WLog_Print(cliprdr->log, WLOG_DEBUG, "ClientFormatDataResponse");
	cliprdr_packet_send(cliprdr, s);

	return 0;
}

int cliprdr_client_file_contents_request(CliprdrClientContext* context, CLIPRDR_FILE_CONTENTS_REQUEST* fileContentsRequest)
{
	wStream* s;
	cliprdrPlugin* cliprdr = (cliprdrPlugin*) context->handle;

	s = cliprdr_packet_new(CB_FILECONTENTS_REQUEST, 0, 28);

	Stream_Write_UINT32(s, fileContentsRequest->streamId); /* streamId (4 bytes) */
	Stream_Write_UINT32(s, fileContentsRequest->listIndex); /* listIndex (4 bytes) */
	Stream_Write_UINT32(s, fileContentsRequest->dwFlags); /* dwFlags (4 bytes) */
	Stream_Write_UINT32(s, fileContentsRequest->nPositionLow); /* nPositionLow (4 bytes) */
	Stream_Write_UINT32(s, fileContentsRequest->nPositionHigh); /* nPositionHigh (4 bytes) */
	Stream_Write_UINT32(s, fileContentsRequest->cbRequested); /* cbRequested (4 bytes) */
	Stream_Write_UINT32(s, fileContentsRequest->clipDataId); /* clipDataId (4 bytes) */

	WLog_Print(cliprdr->log, WLOG_DEBUG, "ClientFileContentsRequest: streamId: 0x%04X",
		fileContentsRequest->streamId);

	cliprdr_packet_send(cliprdr, s);

	return 1;
}

int cliprdr_client_file_contents_response(CliprdrClientContext* context, CLIPRDR_FILE_CONTENTS_RESPONSE* fileContentsResponse)
{
	wStream* s;
	cliprdrPlugin* cliprdr = (cliprdrPlugin*) context->handle;

	if (fileContentsResponse->dwFlags & FILECONTENTS_SIZE)
		fileContentsResponse->cbRequested = sizeof(UINT64);

	s = cliprdr_packet_new(CB_FILECONTENTS_REQUEST, 0,
			4 + fileContentsResponse->cbRequested);

	Stream_Write_UINT32(s, fileContentsResponse->streamId); /* streamId (4 bytes) */

	/**
	 * requestedFileContentsData:
	 * FILECONTENTS_SIZE: file size as UINT64
	 * FILECONTENTS_RANGE: file data from requested range
	 */

	Stream_Write(s, fileContentsResponse->requestedData, fileContentsResponse->cbRequested);

	WLog_Print(cliprdr->log, WLOG_DEBUG, "ClientFileContentsResponse: streamId: 0x%04X",
		fileContentsResponse->streamId);

	cliprdr_packet_send(cliprdr, s);

	return 1;
}

/****************************************************************************************/

static wListDictionary* g_InitHandles = NULL;
static wListDictionary* g_OpenHandles = NULL;

void cliprdr_add_init_handle_data(void* pInitHandle, void* pUserData)
{
	if (!g_InitHandles)
		g_InitHandles = ListDictionary_New(TRUE);

	ListDictionary_Add(g_InitHandles, pInitHandle, pUserData);
}

void* cliprdr_get_init_handle_data(void* pInitHandle)
{
	void* pUserData = NULL;
	pUserData = ListDictionary_GetItemValue(g_InitHandles, pInitHandle);
	return pUserData;
}

void cliprdr_remove_init_handle_data(void* pInitHandle)
{
	ListDictionary_Remove(g_InitHandles, pInitHandle);
	if (ListDictionary_Count(g_InitHandles) < 1)
	{
		ListDictionary_Free(g_InitHandles);
		g_InitHandles = NULL;
	}
}

void cliprdr_add_open_handle_data(DWORD openHandle, void* pUserData)
{
	void* pOpenHandle = (void*) (size_t) openHandle;

	if (!g_OpenHandles)
		g_OpenHandles = ListDictionary_New(TRUE);

	ListDictionary_Add(g_OpenHandles, pOpenHandle, pUserData);
}

void* cliprdr_get_open_handle_data(DWORD openHandle)
{
	void* pUserData = NULL;
	void* pOpenHandle = (void*) (size_t) openHandle;
	pUserData = ListDictionary_GetItemValue(g_OpenHandles, pOpenHandle);
	return pUserData;
}

void cliprdr_remove_open_handle_data(DWORD openHandle)
{
	void* pOpenHandle = (void*) (size_t) openHandle;
	ListDictionary_Remove(g_OpenHandles, pOpenHandle);

	if (ListDictionary_Count(g_OpenHandles) < 1)
	{
		ListDictionary_Free(g_OpenHandles);
		g_OpenHandles = NULL;
	}
}

static void cliprdr_virtual_channel_event_data_received(cliprdrPlugin* cliprdr,
		void* pData, UINT32 dataLength, UINT32 totalLength, UINT32 dataFlags)
{
	wStream* data_in;

	if ((dataFlags & CHANNEL_FLAG_SUSPEND) || (dataFlags & CHANNEL_FLAG_RESUME))
	{
		return;
	}

	if (dataFlags & CHANNEL_FLAG_FIRST)
	{
		if (cliprdr->data_in)
			Stream_Free(cliprdr->data_in, TRUE);

		cliprdr->data_in = Stream_New(NULL, totalLength);
	}

	data_in = cliprdr->data_in;
	Stream_EnsureRemainingCapacity(data_in, (int) dataLength);
	Stream_Write(data_in, pData, dataLength);

	if (dataFlags & CHANNEL_FLAG_LAST)
	{
		if (Stream_Capacity(data_in) != Stream_GetPosition(data_in))
		{
			WLog_ERR(TAG,  "cliprdr_plugin_process_received: read error");
		}

		cliprdr->data_in = NULL;
		Stream_SealLength(data_in);
		Stream_SetPosition(data_in, 0);

		MessageQueue_Post(cliprdr->queue, NULL, 0, (void*) data_in, NULL);
	}
}

static VOID VCAPITYPE cliprdr_virtual_channel_open_event(DWORD openHandle, UINT event,
		LPVOID pData, UINT32 dataLength, UINT32 totalLength, UINT32 dataFlags)
{
	cliprdrPlugin* cliprdr;

	cliprdr = (cliprdrPlugin*) cliprdr_get_open_handle_data(openHandle);

	if (!cliprdr)
	{
		WLog_ERR(TAG,  "cliprdr_virtual_channel_open_event: error no match");
		return;
	}

	switch (event)
	{
		case CHANNEL_EVENT_DATA_RECEIVED:
			cliprdr_virtual_channel_event_data_received(cliprdr, pData, dataLength, totalLength, dataFlags);
			break;

		case CHANNEL_EVENT_WRITE_COMPLETE:
			Stream_Free((wStream*) pData, TRUE);
			break;

		case CHANNEL_EVENT_USER:
			break;
	}
}

static void* cliprdr_virtual_channel_client_thread(void* arg)
{
	wStream* data;
	wMessage message;
	cliprdrPlugin* cliprdr = (cliprdrPlugin*) arg;

	while (1)
	{
		if (!MessageQueue_Wait(cliprdr->queue))
			break;

		if (MessageQueue_Peek(cliprdr->queue, &message, TRUE))
		{
			if (message.id == WMQ_QUIT)
				break;

			if (message.id == 0)
			{
				data = (wStream*) message.wParam;
				cliprdr_order_recv(cliprdr, data);
			}
		}
	}

	ExitThread(0);
	return NULL;
}

static void cliprdr_virtual_channel_event_connected(cliprdrPlugin* cliprdr, LPVOID pData, UINT32 dataLength)
{
	UINT32 status;

	status = cliprdr->channelEntryPoints.pVirtualChannelOpen(cliprdr->InitHandle,
		&cliprdr->OpenHandle, cliprdr->channelDef.name, cliprdr_virtual_channel_open_event);

	cliprdr_add_open_handle_data(cliprdr->OpenHandle, cliprdr);

	if (status != CHANNEL_RC_OK)
	{
		WLog_ERR(TAG,  "pVirtualChannelOpen failed with %s [%08X]",
				 WTSErrorToString(status), status);
		return;
	}

	cliprdr->queue = MessageQueue_New(NULL);

	cliprdr->thread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE) cliprdr_virtual_channel_client_thread, (void*) cliprdr, 0, NULL);
}

static void cliprdr_virtual_channel_event_disconnected(cliprdrPlugin* cliprdr)
{
	UINT rc;

	MessageQueue_PostQuit(cliprdr->queue, 0);
	WaitForSingleObject(cliprdr->thread, INFINITE);

	MessageQueue_Free(cliprdr->queue);
	CloseHandle(cliprdr->thread);

	rc = cliprdr->channelEntryPoints.pVirtualChannelClose(cliprdr->OpenHandle);
	if (CHANNEL_RC_OK != rc)
	{
		WLog_ERR(TAG, "pVirtualChannelClose failed with %s [%08X]",
				 WTSErrorToString(rc), rc);
	}

	if (cliprdr->data_in)
	{
		Stream_Free(cliprdr->data_in, TRUE);
		cliprdr->data_in = NULL;
	}

	cliprdr_remove_open_handle_data(cliprdr->OpenHandle);
}

static void cliprdr_virtual_channel_event_terminated(cliprdrPlugin* cliprdr)
{
	cliprdr_remove_init_handle_data(cliprdr->InitHandle);

	free(cliprdr);
}

static VOID VCAPITYPE cliprdr_virtual_channel_init_event(LPVOID pInitHandle, UINT event, LPVOID pData, UINT dataLength)
{
	cliprdrPlugin* cliprdr;

	cliprdr = (cliprdrPlugin*) cliprdr_get_init_handle_data(pInitHandle);

	if (!cliprdr)
	{
		WLog_ERR(TAG,  "cliprdr_virtual_channel_init_event: error no match");
		return;
	}

	switch (event)
	{
		case CHANNEL_EVENT_CONNECTED:
			cliprdr_virtual_channel_event_connected(cliprdr, pData, dataLength);
			break;

		case CHANNEL_EVENT_DISCONNECTED:
			cliprdr_virtual_channel_event_disconnected(cliprdr);
			break;

		case CHANNEL_EVENT_TERMINATED:
			cliprdr_virtual_channel_event_terminated(cliprdr);
			break;
	}
}

/* cliprdr is always built-in */
#define VirtualChannelEntry	cliprdr_VirtualChannelEntry

BOOL VCAPITYPE VirtualChannelEntry(PCHANNEL_ENTRY_POINTS pEntryPoints)
{
	UINT rc;

	cliprdrPlugin* cliprdr;
	CliprdrClientContext* context;
	CHANNEL_ENTRY_POINTS_FREERDP* pEntryPointsEx;

	cliprdr = (cliprdrPlugin*) calloc(1, sizeof(cliprdrPlugin));

	cliprdr->channelDef.options =
		CHANNEL_OPTION_INITIALIZED |
		CHANNEL_OPTION_ENCRYPT_RDP |
		CHANNEL_OPTION_COMPRESS_RDP |
		CHANNEL_OPTION_SHOW_PROTOCOL;

	strcpy(cliprdr->channelDef.name, "cliprdr");

	pEntryPointsEx = (CHANNEL_ENTRY_POINTS_FREERDP*) pEntryPoints;

	if ((pEntryPointsEx->cbSize >= sizeof(CHANNEL_ENTRY_POINTS_FREERDP)) &&
			(pEntryPointsEx->MagicNumber == FREERDP_CHANNEL_MAGIC_NUMBER))
	{
		context = (CliprdrClientContext*) calloc(1, sizeof(CliprdrClientContext));

		context->handle = (void*) cliprdr;
		context->custom = NULL;

		context->ClientCapabilities = cliprdr_client_capabilities;
		context->TempDirectory = cliprdr_temp_directory;
		context->ClientFormatList = cliprdr_client_format_list;
		context->ClientFormatListResponse = cliprdr_client_format_list_response;
		context->ClientLockClipboardData = cliprdr_client_lock_clipboard_data;
		context->ClientUnlockClipboardData = cliprdr_client_unlock_clipboard_data;
		context->ClientFormatDataRequest = cliprdr_client_format_data_request;
		context->ClientFormatDataResponse = cliprdr_client_format_data_response;
		context->ClientFileContentsRequest = cliprdr_client_file_contents_request;
		context->ClientFileContentsResponse = cliprdr_client_file_contents_response;

		*(pEntryPointsEx->ppInterface) = (void*) context;
		cliprdr->context = context;
	}

	cliprdr->log = WLog_Get("com.freerdp.channels.cliprdr.client");

	cliprdr->useLongFormatNames = TRUE;
	cliprdr->streamFileClipEnabled = FALSE;
	cliprdr->fileClipNoFilePaths = TRUE;
	cliprdr->canLockClipData = FALSE;
	
	WLog_Print(cliprdr->log, WLOG_DEBUG, "VirtualChannelEntry");

	CopyMemory(&(cliprdr->channelEntryPoints), pEntryPoints, sizeof(CHANNEL_ENTRY_POINTS_FREERDP));

	rc = cliprdr->channelEntryPoints.pVirtualChannelInit(&cliprdr->InitHandle,
		&cliprdr->channelDef, 1, VIRTUAL_CHANNEL_VERSION_WIN2000, cliprdr_virtual_channel_init_event);
	if (CHANNEL_RC_OK != rc)
	{
		WLog_ERR(TAG, "pVirtualChannelInit failed with %s [%08X]",
				 WTSErrorToString(rc), rc);
		free(cliprdr);
		return -1;
	}

	cliprdr->channelEntryPoints.pInterface = *(cliprdr->channelEntryPoints.ppInterface);
	cliprdr->channelEntryPoints.ppInterface = &(cliprdr->channelEntryPoints.pInterface);

	cliprdr_add_init_handle_data(cliprdr->InitHandle, (void*) cliprdr);

	return 1;
}
