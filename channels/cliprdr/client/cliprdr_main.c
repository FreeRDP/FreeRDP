/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Clipboard Virtual Channel
 *
 * Copyright 2009-2011 Jay Sorg
 * Copyright 2010-2011 Vic Lee
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

#include <freerdp/types.h>
#include <freerdp/constants.h>
#include <freerdp/client/cliprdr.h>

#include "cliprdr_main.h"
#include "cliprdr_format.h"

static const char* const CB_MSG_TYPE_STRINGS[] =
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
	"CB_LOCK_CLIPDATA",
	"CB_UNLOCK_CLIPDATA"
};

CliprdrClientContext* cliprdr_get_client_interface(cliprdrPlugin* cliprdr)
{
	CliprdrClientContext* pInterface;

	if (!cliprdr)
		return NULL;

	pInterface = (CliprdrClientContext*) cliprdr->channelEntryPoints.pInterface;
	return pInterface;
}

static wStream* cliprdr_packet_new(UINT16 msgType, UINT16 msgFlags,
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
static UINT cliprdr_packet_send(cliprdrPlugin* cliprdr, wStream* s)
{
	UINT32 pos;
	UINT32 dataLen;
	UINT status = CHANNEL_RC_OK;
	pos = Stream_GetPosition(s);
	dataLen = pos - 8;
	Stream_SetPosition(s, 4);
	Stream_Write_UINT32(s, dataLen);
	Stream_SetPosition(s, pos);
#ifdef WITH_DEBUG_CLIPRDR
	WLog_DBG(TAG, "Cliprdr Sending (%"PRIu32" bytes)", dataLen + 8);
	winpr_HexDump(TAG, WLOG_DEBUG, Stream_Buffer(s), dataLen + 8);
#endif

	if (!cliprdr)
	{
		status = CHANNEL_RC_BAD_INIT_HANDLE;
	}
	else
	{
		status = cliprdr->channelEntryPoints.pVirtualChannelWriteEx(cliprdr->InitHandle,
		         cliprdr->OpenHandle,
		         Stream_Buffer(s), (UINT32) Stream_GetPosition(s), s);
	}

	if (status != CHANNEL_RC_OK)
		WLog_ERR(TAG, "VirtualChannelWrite failed with %s [%08"PRIX32"]",
		         WTSErrorToString(status), status);

	return status;
}

#ifdef WITH_DEBUG_CLIPRDR
static void cliprdr_print_general_capability_flags(UINT32 flags)
{
	WLog_INFO(TAG,  "generalFlags (0x%08"PRIX32") {", flags);

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
#endif

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_process_general_capability(cliprdrPlugin* cliprdr,
        wStream* s)
{
	UINT32 version;
	UINT32 generalFlags;
	CLIPRDR_CAPABILITIES capabilities;
	CLIPRDR_GENERAL_CAPABILITY_SET generalCapabilitySet;
	CliprdrClientContext* context = cliprdr_get_client_interface(cliprdr);
	UINT error = CHANNEL_RC_OK;

	if (!context)
	{
		WLog_ERR(TAG, "cliprdr_get_client_interface failed!");
		return ERROR_INTERNAL_ERROR;
	}

	Stream_Read_UINT32(s, version); /* version (4 bytes) */
	Stream_Read_UINT32(s, generalFlags); /* generalFlags (4 bytes) */
	DEBUG_CLIPRDR("Version: %"PRIu32"", version);
#ifdef WITH_DEBUG_CLIPRDR
	cliprdr_print_general_capability_flags(generalFlags);
#endif

	if (cliprdr->useLongFormatNames)
		cliprdr->useLongFormatNames = (generalFlags & CB_USE_LONG_FORMAT_NAMES) ? TRUE :
		                              FALSE;

	if (cliprdr->streamFileClipEnabled)
		cliprdr->streamFileClipEnabled = (generalFlags & CB_STREAM_FILECLIP_ENABLED) ?
		                                 TRUE : FALSE;

	if (cliprdr->fileClipNoFilePaths)
		cliprdr->fileClipNoFilePaths = (generalFlags & CB_FILECLIP_NO_FILE_PATHS) ?
		                               TRUE : FALSE;

	if (cliprdr->canLockClipData)
		cliprdr->canLockClipData = (generalFlags & CB_CAN_LOCK_CLIPDATA) ? TRUE : FALSE;

	cliprdr->capabilitiesReceived = TRUE;

	if (!context->custom)
	{
		WLog_ERR(TAG, "context->custom not set!");
		return ERROR_INTERNAL_ERROR;
	}

	capabilities.cCapabilitiesSets = 1;
	capabilities.capabilitySets = (CLIPRDR_CAPABILITY_SET*) &
	                              (generalCapabilitySet);
	generalCapabilitySet.capabilitySetType = CB_CAPSTYPE_GENERAL;
	generalCapabilitySet.capabilitySetLength = 12;
	generalCapabilitySet.version = version;
	generalCapabilitySet.generalFlags = generalFlags;
	IFCALLRET(context->ServerCapabilities, error, context, &capabilities);

	if (error)
		WLog_ERR(TAG, "ServerCapabilities failed with error %"PRIu32"!", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_process_clip_caps(cliprdrPlugin* cliprdr, wStream* s,
                                      UINT16 length, UINT16 flags)
{
	UINT16 index;
	UINT16 lengthCapability;
	UINT16 cCapabilitiesSets;
	UINT16 capabilitySetType;
	UINT error = CHANNEL_RC_OK;
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
				if ((error = cliprdr_process_general_capability(cliprdr, s)))
				{
					WLog_ERR(TAG, "cliprdr_process_general_capability failed with error %"PRIu32"!",
					         error);
					return error;
				}

				break;

			default:
				WLog_ERR(TAG, "unknown cliprdr capability set: %"PRIu16"", capabilitySetType);
				return CHANNEL_RC_BAD_PROC;
				break;
		}
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_process_monitor_ready(cliprdrPlugin* cliprdr, wStream* s,
        UINT16 length, UINT16 flags)
{
	CLIPRDR_MONITOR_READY monitorReady;
	CliprdrClientContext* context = cliprdr_get_client_interface(cliprdr);
	UINT error = CHANNEL_RC_OK;
	WLog_Print(cliprdr->log, WLOG_DEBUG, "MonitorReady");

	if (!context->custom)
	{
		WLog_ERR(TAG, "context->custom not set!");
		return ERROR_INTERNAL_ERROR;
	}

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
	IFCALLRET(context->MonitorReady, error, context, &monitorReady);

	if (error)
		WLog_ERR(TAG, "MonitorReady failed with error %"PRIu32"!", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_process_filecontents_request(cliprdrPlugin* cliprdr,
        wStream* s, UINT32 length, UINT16 flags)
{
	CLIPRDR_FILE_CONTENTS_REQUEST request;
	CliprdrClientContext* context = cliprdr_get_client_interface(cliprdr);
	UINT error = CHANNEL_RC_OK;
	WLog_Print(cliprdr->log, WLOG_DEBUG, "FileContentsRequest");

	if (!context->custom)
	{
		WLog_ERR(TAG, "context->custom not set!");
		return ERROR_INTERNAL_ERROR;
	}

	if (Stream_GetRemainingLength(s) < 24)
	{
		WLog_ERR(TAG, "not enough remaining data");
		return ERROR_INVALID_DATA;
	}

	request.msgType = CB_FILECONTENTS_REQUEST;
	request.msgFlags = flags;
	request.dataLen = length;
	request.haveClipDataId = FALSE;

	Stream_Read_UINT32(s, request.streamId); /* streamId (4 bytes) */
	Stream_Read_UINT32(s, request.listIndex); /* listIndex (4 bytes) */
	Stream_Read_UINT32(s, request.dwFlags); /* dwFlags (4 bytes) */
	Stream_Read_UINT32(s, request.nPositionLow); /* nPositionLow (4 bytes) */
	Stream_Read_UINT32(s, request.nPositionHigh); /* nPositionHigh (4 bytes) */
	Stream_Read_UINT32(s, request.cbRequested); /* cbRequested (4 bytes) */

	if (Stream_GetRemainingLength(s) >= 4)
	{
		Stream_Read_UINT32(s, request.clipDataId); /* clipDataId (4 bytes) */
		request.haveClipDataId = TRUE;
	}

	IFCALLRET(context->ServerFileContentsRequest, error, context, &request);

	if (error)
		WLog_ERR(TAG, "ServerFileContentsRequest failed with error %"PRIu32"!", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_process_filecontents_response(cliprdrPlugin* cliprdr,
        wStream* s, UINT32 length, UINT16 flags)
{
	CLIPRDR_FILE_CONTENTS_RESPONSE response;
	CliprdrClientContext* context = cliprdr_get_client_interface(cliprdr);
	UINT error = CHANNEL_RC_OK;
	WLog_Print(cliprdr->log, WLOG_DEBUG, "FileContentsResponse");

	if (!context->custom)
	{
		WLog_ERR(TAG, "context->custom not set!");
		return ERROR_INTERNAL_ERROR;
	}

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_ERR(TAG, "not enough remaining data");
		return ERROR_INVALID_DATA;
	}

	response.msgType = CB_FILECONTENTS_RESPONSE;
	response.msgFlags = flags;
	response.dataLen = length;
	Stream_Read_UINT32(s, response.streamId); /* streamId (4 bytes) */
	response.cbRequested = length - 4;
	response.requestedData = Stream_Pointer(s); /* requestedFileContentsData */
	IFCALLRET(context->ServerFileContentsResponse, error, context, &response);

	if (error)
		WLog_ERR(TAG, "ServerFileContentsResponse failed with error %"PRIu32"!", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_process_lock_clipdata(cliprdrPlugin* cliprdr, wStream* s,
        UINT32 length, UINT16 flags)
{
	CLIPRDR_LOCK_CLIPBOARD_DATA lockClipboardData;
	CliprdrClientContext* context = cliprdr_get_client_interface(cliprdr);
	UINT error = CHANNEL_RC_OK;
	WLog_Print(cliprdr->log, WLOG_DEBUG, "LockClipData");

	if (!context->custom)
	{
		WLog_ERR(TAG, "context->custom not set!");
		return ERROR_INTERNAL_ERROR;
	}

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_ERR(TAG, "not enough remaining data");
		return ERROR_INVALID_DATA;
	}

	lockClipboardData.msgType = CB_LOCK_CLIPDATA;
	lockClipboardData.msgFlags = flags;
	lockClipboardData.dataLen = length;
	Stream_Read_UINT32(s, lockClipboardData.clipDataId); /* clipDataId (4 bytes) */
	IFCALLRET(context->ServerLockClipboardData, error, context, &lockClipboardData);

	if (error)
		WLog_ERR(TAG, "ServerLockClipboardData failed with error %"PRIu32"!", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_process_unlock_clipdata(cliprdrPlugin* cliprdr, wStream* s,
        UINT32 length, UINT16 flags)
{
	CLIPRDR_UNLOCK_CLIPBOARD_DATA unlockClipboardData;
	CliprdrClientContext* context = cliprdr_get_client_interface(cliprdr);
	UINT error = CHANNEL_RC_OK;
	WLog_Print(cliprdr->log, WLOG_DEBUG, "UnlockClipData");

	if (!context->custom)
	{
		WLog_ERR(TAG, "context->custom not set!");
		return ERROR_INTERNAL_ERROR;
	}

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_ERR(TAG, "not enough remaining data");
		return ERROR_INVALID_DATA;
	}

	unlockClipboardData.msgType = CB_UNLOCK_CLIPDATA;
	unlockClipboardData.msgFlags = flags;
	unlockClipboardData.dataLen = length;
	Stream_Read_UINT32(s,
	                   unlockClipboardData.clipDataId); /* clipDataId (4 bytes) */
	IFCALLRET(context->ServerUnlockClipboardData, error, context,
	          &unlockClipboardData);

	if (error)
		WLog_ERR(TAG, "ServerUnlockClipboardData failed with error %"PRIu32"!", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_order_recv(cliprdrPlugin* cliprdr, wStream* s)
{
	UINT16 msgType;
	UINT16 msgFlags;
	UINT32 dataLen;
	UINT error;
	Stream_Read_UINT16(s, msgType); /* msgType (2 bytes) */
	Stream_Read_UINT16(s, msgFlags); /* msgFlags (2 bytes) */
	Stream_Read_UINT32(s, dataLen); /* dataLen (4 bytes) */
#ifdef WITH_DEBUG_CLIPRDR
	WLog_DBG(TAG, "msgType: %s (%"PRIu16"), msgFlags: %"PRIu16" dataLen: %"PRIu32"",
	         CB_MSG_TYPE_STRINGS[msgType], msgType, msgFlags, dataLen);
	winpr_HexDump(TAG, WLOG_DEBUG, Stream_Buffer(s), dataLen + 8);
#endif

	switch (msgType)
	{
		case CB_CLIP_CAPS:
			if ((error = cliprdr_process_clip_caps(cliprdr, s, dataLen, msgFlags)))
				WLog_ERR(TAG, "cliprdr_process_clip_caps failed with error %"PRIu32"!", error);

			break;

		case CB_MONITOR_READY:
			if ((error = cliprdr_process_monitor_ready(cliprdr, s, dataLen, msgFlags)))
				WLog_ERR(TAG, "cliprdr_process_monitor_ready failed with error %"PRIu32"!", error);

			break;

		case CB_FORMAT_LIST:
			if ((error = cliprdr_process_format_list(cliprdr, s, dataLen, msgFlags)))
				WLog_ERR(TAG, "cliprdr_process_format_list failed with error %"PRIu32"!", error);

			break;

		case CB_FORMAT_LIST_RESPONSE:
			if ((error = cliprdr_process_format_list_response(cliprdr, s, dataLen,
			             msgFlags)))
				WLog_ERR(TAG, "cliprdr_process_format_list_response failed with error %"PRIu32"!",
				         error);

			break;

		case CB_FORMAT_DATA_REQUEST:
			if ((error = cliprdr_process_format_data_request(cliprdr, s, dataLen,
			             msgFlags)))
				WLog_ERR(TAG, "cliprdr_process_format_data_request failed with error %"PRIu32"!",
				         error);

			break;

		case CB_FORMAT_DATA_RESPONSE:
			if ((error = cliprdr_process_format_data_response(cliprdr, s, dataLen,
			             msgFlags)))
				WLog_ERR(TAG, "cliprdr_process_format_data_response failed with error %"PRIu32"!",
				         error);

			break;

		case CB_FILECONTENTS_REQUEST:
			if ((error = cliprdr_process_filecontents_request(cliprdr, s, dataLen,
			             msgFlags)))
				WLog_ERR(TAG, "cliprdr_process_filecontents_request failed with error %"PRIu32"!",
				         error);

			break;

		case CB_FILECONTENTS_RESPONSE:
			if ((error = cliprdr_process_filecontents_response(cliprdr, s, dataLen,
			             msgFlags)))
				WLog_ERR(TAG, "cliprdr_process_filecontents_response failed with error %"PRIu32"!",
				         error);

			break;

		case CB_LOCK_CLIPDATA:
			if ((error = cliprdr_process_lock_clipdata(cliprdr, s, dataLen, msgFlags)))
				WLog_ERR(TAG, "cliprdr_process_lock_clipdata failed with error %"PRIu32"!", error);

			break;

		case CB_UNLOCK_CLIPDATA:
			if ((error = cliprdr_process_unlock_clipdata(cliprdr, s, dataLen, msgFlags)))
				WLog_ERR(TAG, "cliprdr_process_lock_clipdata failed with error %"PRIu32"!", error);

			break;

		default:
			error = CHANNEL_RC_BAD_PROC;
			WLog_ERR(TAG, "unknown msgType %"PRIu16"", msgType);
			break;
	}

	Stream_Free(s, TRUE);
	return error;
}

/**
 * Callback Interface
 */

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_client_capabilities(CliprdrClientContext* context,
                                        CLIPRDR_CAPABILITIES* capabilities)
{
	wStream* s;
	CLIPRDR_GENERAL_CAPABILITY_SET* generalCapabilitySet;
	cliprdrPlugin* cliprdr = (cliprdrPlugin*) context->handle;
	s = cliprdr_packet_new(CB_CLIP_CAPS, 0, 4 + CB_CAPSTYPE_GENERAL_LEN);

	if (!s)
	{
		WLog_ERR(TAG, "cliprdr_packet_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	Stream_Write_UINT16(s, 1); /* cCapabilitiesSets */
	Stream_Write_UINT16(s, 0); /* pad1 */
	generalCapabilitySet = (CLIPRDR_GENERAL_CAPABILITY_SET*)
	                       capabilities->capabilitySets;
	Stream_Write_UINT16(s,
	                    generalCapabilitySet->capabilitySetType); /* capabilitySetType */
	Stream_Write_UINT16(s,
	                    generalCapabilitySet->capabilitySetLength); /* lengthCapability */
	Stream_Write_UINT32(s, generalCapabilitySet->version); /* version */
	Stream_Write_UINT32(s, generalCapabilitySet->generalFlags); /* generalFlags */
	WLog_Print(cliprdr->log, WLOG_DEBUG, "ClientCapabilities");
	return cliprdr_packet_send(cliprdr, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_temp_directory(CliprdrClientContext* context,
                                   CLIPRDR_TEMP_DIRECTORY* tempDirectory)
{
	int length;
	wStream* s;
	WCHAR* wszTempDir = NULL;
	cliprdrPlugin* cliprdr = (cliprdrPlugin*) context->handle;
	s = cliprdr_packet_new(CB_TEMP_DIRECTORY, 0, 520 * 2);

	if (!s)
	{
		WLog_ERR(TAG, "cliprdr_packet_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	length = ConvertToUnicode(CP_UTF8, 0, tempDirectory->szTempDir, -1, &wszTempDir,
	                          0);

	if (length < 0)
		return ERROR_INTERNAL_ERROR;

	if (length > 520)
		length = 520;

	Stream_Write(s, wszTempDir, length * 2);
	Stream_Zero(s, (520 - length) * 2);
	free(wszTempDir);
	WLog_Print(cliprdr->log, WLOG_DEBUG, "TempDirectory: %s",
	           tempDirectory->szTempDir);
	return cliprdr_packet_send(cliprdr, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_client_format_list(CliprdrClientContext* context,
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
	cliprdrPlugin* cliprdr = (cliprdrPlugin*) context->handle;

	if (!cliprdr->useLongFormatNames)
	{
		length = formatList->numFormats * 36;
		s = cliprdr_packet_new(CB_FORMAT_LIST, 0, length);

		if (!s)
		{
			WLog_ERR(TAG, "cliprdr_packet_new failed!");
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

		s = cliprdr_packet_new(CB_FORMAT_LIST, 0, length);

		if (!s)
		{
			WLog_ERR(TAG, "cliprdr_packet_new failed!");
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

	WLog_Print(cliprdr->log, WLOG_DEBUG, "ClientFormatList: numFormats: %"PRIu32"",
	           formatList->numFormats);
	return cliprdr_packet_send(cliprdr, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_client_format_list_response(CliprdrClientContext* context,
        CLIPRDR_FORMAT_LIST_RESPONSE* formatListResponse)
{
	wStream* s;
	cliprdrPlugin* cliprdr = (cliprdrPlugin*) context->handle;
	formatListResponse->msgType = CB_FORMAT_LIST_RESPONSE;
	formatListResponse->dataLen = 0;
	s = cliprdr_packet_new(formatListResponse->msgType,
	                       formatListResponse->msgFlags, formatListResponse->dataLen);

	if (!s)
	{
		WLog_ERR(TAG, "cliprdr_packet_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	WLog_Print(cliprdr->log, WLOG_DEBUG, "ClientFormatListResponse");
	return cliprdr_packet_send(cliprdr, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_client_lock_clipboard_data(CliprdrClientContext* context,
        CLIPRDR_LOCK_CLIPBOARD_DATA* lockClipboardData)
{
	wStream* s;
	cliprdrPlugin* cliprdr = (cliprdrPlugin*) context->handle;
	s = cliprdr_packet_new(CB_LOCK_CLIPDATA, 0, 4);

	if (!s)
	{
		WLog_ERR(TAG, "cliprdr_packet_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	Stream_Write_UINT32(s,
	                    lockClipboardData->clipDataId); /* clipDataId (4 bytes) */
	WLog_Print(cliprdr->log, WLOG_DEBUG,
	           "ClientLockClipboardData: clipDataId: 0x%08"PRIX32"",
	           lockClipboardData->clipDataId);
	return cliprdr_packet_send(cliprdr, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_client_unlock_clipboard_data(CliprdrClientContext* context,
        CLIPRDR_UNLOCK_CLIPBOARD_DATA* unlockClipboardData)
{
	wStream* s;
	cliprdrPlugin* cliprdr = (cliprdrPlugin*) context->handle;
	s = cliprdr_packet_new(CB_UNLOCK_CLIPDATA, 0, 4);

	if (!s)
	{
		WLog_ERR(TAG, "cliprdr_packet_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	Stream_Write_UINT32(s,
	                    unlockClipboardData->clipDataId); /* clipDataId (4 bytes) */
	WLog_Print(cliprdr->log, WLOG_DEBUG,
	           "ClientUnlockClipboardData: clipDataId: 0x%08"PRIX32"",
	           unlockClipboardData->clipDataId);
	return cliprdr_packet_send(cliprdr, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_client_format_data_request(CliprdrClientContext* context,
        CLIPRDR_FORMAT_DATA_REQUEST* formatDataRequest)
{
	wStream* s;
	cliprdrPlugin* cliprdr = (cliprdrPlugin*) context->handle;
	formatDataRequest->msgType = CB_FORMAT_DATA_REQUEST;
	formatDataRequest->msgFlags = 0;
	formatDataRequest->dataLen = 4;
	s = cliprdr_packet_new(formatDataRequest->msgType, formatDataRequest->msgFlags,
	                       formatDataRequest->dataLen);

	if (!s)
	{
		WLog_ERR(TAG, "cliprdr_packet_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	Stream_Write_UINT32(s,
	                    formatDataRequest->requestedFormatId); /* requestedFormatId (4 bytes) */
	WLog_Print(cliprdr->log, WLOG_DEBUG, "ClientFormatDataRequest");
	return cliprdr_packet_send(cliprdr, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_client_format_data_response(CliprdrClientContext* context,
        CLIPRDR_FORMAT_DATA_RESPONSE* formatDataResponse)
{
	wStream* s;
	cliprdrPlugin* cliprdr = (cliprdrPlugin*) context->handle;
	formatDataResponse->msgType = CB_FORMAT_DATA_RESPONSE;
	s = cliprdr_packet_new(formatDataResponse->msgType,
	                       formatDataResponse->msgFlags, formatDataResponse->dataLen);

	if (!s)
	{
		WLog_ERR(TAG, "cliprdr_packet_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	Stream_Write(s, formatDataResponse->requestedFormatData,
	             formatDataResponse->dataLen);
	WLog_Print(cliprdr->log, WLOG_DEBUG, "ClientFormatDataResponse");
	return cliprdr_packet_send(cliprdr, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_client_file_contents_request(CliprdrClientContext* context,
        CLIPRDR_FILE_CONTENTS_REQUEST* fileContentsRequest)
{
	wStream* s;
	cliprdrPlugin* cliprdr = (cliprdrPlugin*) context->handle;
	s = cliprdr_packet_new(CB_FILECONTENTS_REQUEST, 0, 28);

	if (!s)
	{
		WLog_ERR(TAG, "cliprdr_packet_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	Stream_Write_UINT32(s, fileContentsRequest->streamId); /* streamId (4 bytes) */
	Stream_Write_UINT32(s, fileContentsRequest->listIndex); /* listIndex (4 bytes) */
	Stream_Write_UINT32(s, fileContentsRequest->dwFlags); /* dwFlags (4 bytes) */
	Stream_Write_UINT32(s, fileContentsRequest->nPositionLow); /* nPositionLow (4 bytes) */
	Stream_Write_UINT32(s, fileContentsRequest->nPositionHigh); /* nPositionHigh (4 bytes) */
	Stream_Write_UINT32(s, fileContentsRequest->cbRequested); /* cbRequested (4 bytes) */

	if (fileContentsRequest->haveClipDataId)
		Stream_Write_UINT32(s, fileContentsRequest->clipDataId); /* clipDataId (4 bytes) */

	WLog_Print(cliprdr->log, WLOG_DEBUG,
	           "ClientFileContentsRequest: streamId: 0x%08"PRIX32"",
	           fileContentsRequest->streamId);

	return cliprdr_packet_send(cliprdr, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_client_file_contents_response(CliprdrClientContext* context,
        CLIPRDR_FILE_CONTENTS_RESPONSE* fileContentsResponse)
{
	wStream* s;
	cliprdrPlugin* cliprdr = (cliprdrPlugin*) context->handle;

	s = cliprdr_packet_new(CB_FILECONTENTS_RESPONSE, fileContentsResponse->msgFlags,
	                       4 + fileContentsResponse->cbRequested);

	if (!s)
	{
		WLog_ERR(TAG, "cliprdr_packet_new failed!");
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
	WLog_Print(cliprdr->log, WLOG_DEBUG,
	           "ClientFileContentsResponse: streamId: 0x%08"PRIX32"",
	           fileContentsResponse->streamId);
	return cliprdr_packet_send(cliprdr, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_virtual_channel_event_data_received(cliprdrPlugin* cliprdr,
        void* pData, UINT32 dataLength, UINT32 totalLength, UINT32 dataFlags)
{
	wStream* data_in;

	if ((dataFlags & CHANNEL_FLAG_SUSPEND) || (dataFlags & CHANNEL_FLAG_RESUME))
	{
		return CHANNEL_RC_OK;
	}

	if (dataFlags & CHANNEL_FLAG_FIRST)
	{
		if (cliprdr->data_in)
			Stream_Free(cliprdr->data_in, TRUE);

		cliprdr->data_in = Stream_New(NULL, totalLength);
	}

	if (!(data_in = cliprdr->data_in))
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	if (!Stream_EnsureRemainingCapacity(data_in, (int) dataLength))
	{
		Stream_Free(cliprdr->data_in, TRUE);
		cliprdr->data_in = NULL;
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write(data_in, pData, dataLength);

	if (dataFlags & CHANNEL_FLAG_LAST)
	{
		if (Stream_Capacity(data_in) != Stream_GetPosition(data_in))
		{
			WLog_ERR(TAG, "cliprdr_plugin_process_received: read error");
			return ERROR_INTERNAL_ERROR;
		}

		cliprdr->data_in = NULL;
		Stream_SealLength(data_in);
		Stream_SetPosition(data_in, 0);

		if (!MessageQueue_Post(cliprdr->queue, NULL, 0, (void*) data_in, NULL))
		{
			WLog_ERR(TAG, "MessageQueue_Post failed!");
			return ERROR_INTERNAL_ERROR;
		}
	}

	return CHANNEL_RC_OK;
}

static VOID VCAPITYPE cliprdr_virtual_channel_open_event_ex(LPVOID lpUserParam, DWORD openHandle,
        UINT event,
        LPVOID pData, UINT32 dataLength, UINT32 totalLength, UINT32 dataFlags)
{
	UINT error = CHANNEL_RC_OK;
	cliprdrPlugin* cliprdr = (cliprdrPlugin*) lpUserParam;

	if (!cliprdr || (cliprdr->OpenHandle != openHandle))
	{
		WLog_ERR(TAG, "error no match");
		return;
	}

	switch (event)
	{
		case CHANNEL_EVENT_DATA_RECEIVED:
			if ((error = cliprdr_virtual_channel_event_data_received(cliprdr, pData, dataLength,
			             totalLength, dataFlags)))
				WLog_ERR(TAG, "failed with error %"PRIu32"", error);

			break;

		case CHANNEL_EVENT_WRITE_COMPLETE:
			Stream_Free((wStream*) pData, TRUE);
			break;

		case CHANNEL_EVENT_USER:
			break;
	}

	if (error && cliprdr->context->rdpcontext)
		setChannelError(cliprdr->context->rdpcontext, error,
		                "cliprdr_virtual_channel_open_event_ex reported an error");
}

static void* cliprdr_virtual_channel_client_thread(void* arg)
{
	wStream* data;
	wMessage message;
	cliprdrPlugin* cliprdr = (cliprdrPlugin*) arg;
	UINT error = CHANNEL_RC_OK;

	while (1)
	{
		if (!MessageQueue_Wait(cliprdr->queue))
		{
			WLog_ERR(TAG, "MessageQueue_Wait failed!");
			error = ERROR_INTERNAL_ERROR;
			break;
		}

		if (!MessageQueue_Peek(cliprdr->queue, &message, TRUE))
		{
			WLog_ERR(TAG, "MessageQueue_Peek failed!");
			error = ERROR_INTERNAL_ERROR;
			break;
		}

		if (message.id == WMQ_QUIT)
			break;

		if (message.id == 0)
		{
			data = (wStream*) message.wParam;

			if ((error = cliprdr_order_recv(cliprdr, data)))
			{
				WLog_ERR(TAG, "cliprdr_order_recv failed with error %"PRIu32"!", error);
				break;
			}
		}
	}

	if (error && cliprdr->context->rdpcontext)
		setChannelError(cliprdr->context->rdpcontext, error,
		                "cliprdr_virtual_channel_client_thread reported an error");

	ExitThread((DWORD)error);
	return NULL;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_virtual_channel_event_connected(cliprdrPlugin* cliprdr,
        LPVOID pData, UINT32 dataLength)
{
	UINT32 status;
	status = cliprdr->channelEntryPoints.pVirtualChannelOpenEx(cliprdr->InitHandle,
	         &cliprdr->OpenHandle, cliprdr->channelDef.name,
	         cliprdr_virtual_channel_open_event_ex);

	if (status != CHANNEL_RC_OK)
	{
		WLog_ERR(TAG, "pVirtualChannelOpen failed with %s [%08"PRIX32"]",
		         WTSErrorToString(status), status);
		return status;
	}

	cliprdr->queue = MessageQueue_New(NULL);

	if (!cliprdr->queue)
	{
		WLog_ERR(TAG, "MessageQueue_New failed!");
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	if (!(cliprdr->thread = CreateThread(NULL, 0,
	                                     (LPTHREAD_START_ROUTINE) cliprdr_virtual_channel_client_thread, (void*) cliprdr,
	                                     0, NULL)))
	{
		WLog_ERR(TAG, "CreateThread failed!");
		MessageQueue_Free(cliprdr->queue);
		cliprdr->queue = NULL;
		return ERROR_INTERNAL_ERROR;
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_virtual_channel_event_disconnected(cliprdrPlugin* cliprdr)
{
	UINT rc;

	if (MessageQueue_PostQuit(cliprdr->queue, 0)
	    && (WaitForSingleObject(cliprdr->thread, INFINITE) == WAIT_FAILED))
	{
		rc = GetLastError();
		WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"", rc);
		return rc;
	}

	MessageQueue_Free(cliprdr->queue);
	CloseHandle(cliprdr->thread);
	rc = cliprdr->channelEntryPoints.pVirtualChannelCloseEx(cliprdr->InitHandle, cliprdr->OpenHandle);

	if (CHANNEL_RC_OK != rc)
	{
		WLog_ERR(TAG, "pVirtualChannelClose failed with %s [%08"PRIX32"]",
		         WTSErrorToString(rc), rc);
		return rc;
	}

	cliprdr->OpenHandle = 0;

	if (cliprdr->data_in)
	{
		Stream_Free(cliprdr->data_in, TRUE);
		cliprdr->data_in = NULL;
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_virtual_channel_event_terminated(cliprdrPlugin* cliprdr)
{
	cliprdr->InitHandle = 0;
	free(cliprdr);
	return CHANNEL_RC_OK;
}

static VOID VCAPITYPE cliprdr_virtual_channel_init_event_ex(LPVOID lpUserParam, LPVOID pInitHandle,
        UINT event, LPVOID pData, UINT dataLength)
{
	UINT error = CHANNEL_RC_OK;
	cliprdrPlugin* cliprdr = (cliprdrPlugin*) lpUserParam;

	if (!cliprdr || (cliprdr->InitHandle != pInitHandle))
	{
		WLog_ERR(TAG, "error no match");
		return;
	}

	switch (event)
	{
		case CHANNEL_EVENT_CONNECTED:
			if ((error = cliprdr_virtual_channel_event_connected(cliprdr, pData,
			             dataLength)))
				WLog_ERR(TAG, "cliprdr_virtual_channel_event_connected failed with error %"PRIu32"!",
				         error);

			break;

		case CHANNEL_EVENT_DISCONNECTED:
			if ((error = cliprdr_virtual_channel_event_disconnected(cliprdr)))
				WLog_ERR(TAG,
				         "cliprdr_virtual_channel_event_disconnected failed with error %"PRIu32"!", error);

			break;

		case CHANNEL_EVENT_TERMINATED:
			if ((error = cliprdr_virtual_channel_event_terminated(cliprdr)))
				WLog_ERR(TAG, "cliprdr_virtual_channel_event_terminated failed with error %"PRIu32"!",
				         error);

			break;
	}

	if (error && cliprdr->context->rdpcontext)
		setChannelError(cliprdr->context->rdpcontext, error,
		                "cliprdr_virtual_channel_init_event reported an error");
}

/* cliprdr is always built-in */
#define VirtualChannelEntryEx	cliprdr_VirtualChannelEntryEx

BOOL VCAPITYPE VirtualChannelEntryEx(PCHANNEL_ENTRY_POINTS pEntryPoints, PVOID pInitHandle)
{
	UINT rc;
	cliprdrPlugin* cliprdr;
	CliprdrClientContext* context = NULL;
	CHANNEL_ENTRY_POINTS_FREERDP_EX* pEntryPointsEx;
	cliprdr = (cliprdrPlugin*) calloc(1, sizeof(cliprdrPlugin));

	if (!cliprdr)
	{
		WLog_ERR(TAG, "calloc failed!");
		return FALSE;
	}

	cliprdr->channelDef.options =
	    CHANNEL_OPTION_INITIALIZED |
	    CHANNEL_OPTION_ENCRYPT_RDP |
	    CHANNEL_OPTION_COMPRESS_RDP |
	    CHANNEL_OPTION_SHOW_PROTOCOL;
	strcpy(cliprdr->channelDef.name, "cliprdr");
	pEntryPointsEx = (CHANNEL_ENTRY_POINTS_FREERDP_EX*) pEntryPoints;

	if ((pEntryPointsEx->cbSize >= sizeof(CHANNEL_ENTRY_POINTS_FREERDP_EX)) &&
	    (pEntryPointsEx->MagicNumber == FREERDP_CHANNEL_MAGIC_NUMBER))
	{
		context = (CliprdrClientContext*) calloc(1, sizeof(CliprdrClientContext));

		if (!context)
		{
			free(cliprdr);
			WLog_ERR(TAG, "calloc failed!");
			return FALSE;
		}

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
		cliprdr->context = context;
		context->rdpcontext = pEntryPointsEx->context;
	}

	cliprdr->log = WLog_Get("com.freerdp.channels.cliprdr.client");
	cliprdr->useLongFormatNames = TRUE;
	cliprdr->streamFileClipEnabled = FALSE;
	cliprdr->fileClipNoFilePaths = TRUE;
	cliprdr->canLockClipData = FALSE;
	WLog_Print(cliprdr->log, WLOG_DEBUG, "VirtualChannelEntryEx");
	CopyMemory(&(cliprdr->channelEntryPoints), pEntryPoints,
	           sizeof(CHANNEL_ENTRY_POINTS_FREERDP_EX));
	cliprdr->InitHandle = pInitHandle;
	rc = cliprdr->channelEntryPoints.pVirtualChannelInitEx(cliprdr, context, pInitHandle,
	        &cliprdr->channelDef, 1, VIRTUAL_CHANNEL_VERSION_WIN2000,
	        cliprdr_virtual_channel_init_event_ex);

	if (CHANNEL_RC_OK != rc)
	{
		WLog_ERR(TAG, "pVirtualChannelInit failed with %s [%08"PRIX32"]",
		         WTSErrorToString(rc), rc);
		free(cliprdr->context);
		free(cliprdr);
		return FALSE;
	}

	cliprdr->channelEntryPoints.pInterface = context;
	return TRUE;
}
