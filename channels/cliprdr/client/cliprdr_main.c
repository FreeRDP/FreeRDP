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

#include <freerdp/config.h>

#include <winpr/wtypes.h>
#include <winpr/assert.h>
#include <winpr/crt.h>
#include <winpr/print.h>

#include <freerdp/types.h>
#include <freerdp/constants.h>
#include <freerdp/client/cliprdr.h>

#include "../../../channels/client/addin.h"

#include "cliprdr_main.h"
#include "cliprdr_format.h"
#include "../cliprdr_common.h"

static const char* CB_MSG_TYPE_STRINGS(UINT32 type)
{
	switch (type)
	{
		case CB_MONITOR_READY:
			return "CB_MONITOR_READY";
		case CB_FORMAT_LIST:
			return "CB_FORMAT_LIST";
		case CB_FORMAT_LIST_RESPONSE:
			return "CB_FORMAT_LIST_RESPONSE";
		case CB_FORMAT_DATA_REQUEST:
			return "CB_FORMAT_DATA_REQUEST";
		case CB_FORMAT_DATA_RESPONSE:
			return "CB_FORMAT_DATA_RESPONSE";
		case CB_TEMP_DIRECTORY:
			return "CB_TEMP_DIRECTORY";
		case CB_CLIP_CAPS:
			return "CB_CLIP_CAPS";
		case CB_FILECONTENTS_REQUEST:
			return "CB_FILECONTENTS_REQUEST";
		case CB_FILECONTENTS_RESPONSE:
			return "CB_FILECONTENTS_RESPONSE";
		case CB_LOCK_CLIPDATA:
			return "CB_LOCK_CLIPDATA";
		case CB_UNLOCK_CLIPDATA:
			return "CB_UNLOCK_CLIPDATA";
		default:
			return "UNKNOWN";
	}
}

CliprdrClientContext* cliprdr_get_client_interface(cliprdrPlugin* cliprdr)
{
	CliprdrClientContext* pInterface;

	if (!cliprdr)
		return NULL;

	pInterface = (CliprdrClientContext*)cliprdr->channelEntryPoints.pInterface;
	return pInterface;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_packet_send(cliprdrPlugin* cliprdr, wStream* s)
{
	size_t pos;
	UINT32 dataLen;
	UINT status = CHANNEL_RC_OK;

	WINPR_ASSERT(cliprdr);
	WINPR_ASSERT(s);

	pos = Stream_GetPosition(s);
	dataLen = pos - 8;
	Stream_SetPosition(s, 4);
	Stream_Write_UINT32(s, dataLen);
	Stream_SetPosition(s, pos);

	WLog_DBG(TAG, "Cliprdr Sending (%" PRIu32 " bytes)", dataLen + 8);
#if defined(WITH_DEBUG_CLIPRDR)
	winpr_HexDump(TAG, WLOG_DEBUG, Stream_Buffer(s), dataLen + 8);
#endif

	if (!cliprdr)
	{
		status = CHANNEL_RC_BAD_INIT_HANDLE;
	}
	else
	{
		WINPR_ASSERT(cliprdr->channelEntryPoints.pVirtualChannelWriteEx);
		status = cliprdr->channelEntryPoints.pVirtualChannelWriteEx(
		    cliprdr->InitHandle, cliprdr->OpenHandle, Stream_Buffer(s),
		    (UINT32)Stream_GetPosition(s), s);
	}

	if (status != CHANNEL_RC_OK)
	{
		Stream_Free(s, TRUE);
		WLog_ERR(TAG, "VirtualChannelWrite failed with %s [%08" PRIX32 "]",
		         WTSErrorToString(status), status);
	}

	return status;
}

static void cliprdr_print_general_capability_flags(UINT32 flags)
{
	WLog_DBG(TAG, "generalFlags (0x%08" PRIX32 ") {", flags);

	if (flags & CB_USE_LONG_FORMAT_NAMES)
		WLog_DBG(TAG, "\tCB_USE_LONG_FORMAT_NAMES");

	if (flags & CB_STREAM_FILECLIP_ENABLED)
		WLog_DBG(TAG, "\tCB_STREAM_FILECLIP_ENABLED");

	if (flags & CB_FILECLIP_NO_FILE_PATHS)
		WLog_DBG(TAG, "\tCB_FILECLIP_NO_FILE_PATHS");

	if (flags & CB_CAN_LOCK_CLIPDATA)
		WLog_DBG(TAG, "\tCB_CAN_LOCK_CLIPDATA");

	if (flags & CB_HUGE_FILE_SUPPORT_ENABLED)
		WLog_DBG(TAG, "\tCB_HUGE_FILE_SUPPORT_ENABLED");

	WLog_DBG(TAG, "}");
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_process_general_capability(cliprdrPlugin* cliprdr, wStream* s)
{
	UINT32 version;
	UINT32 generalFlags;
	CLIPRDR_CAPABILITIES capabilities = { 0 };
	CLIPRDR_GENERAL_CAPABILITY_SET generalCapabilitySet = { 0 };
	CliprdrClientContext* context = cliprdr_get_client_interface(cliprdr);
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(cliprdr);
	WINPR_ASSERT(s);

	if (!context)
	{
		WLog_ERR(TAG, "cliprdr_get_client_interface failed!");
		return ERROR_INTERNAL_ERROR;
	}

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, version);      /* version (4 bytes) */
	Stream_Read_UINT32(s, generalFlags); /* generalFlags (4 bytes) */
	WLog_DBG(TAG, "[%s] Version: %" PRIu32 "", __FUNCTION__, version);

	cliprdr_print_general_capability_flags(generalFlags);

	cliprdr->useLongFormatNames = (generalFlags & CB_USE_LONG_FORMAT_NAMES);
	cliprdr->streamFileClipEnabled = (generalFlags & CB_STREAM_FILECLIP_ENABLED);
	cliprdr->fileClipNoFilePaths = (generalFlags & CB_FILECLIP_NO_FILE_PATHS);
	cliprdr->canLockClipData = (generalFlags & CB_CAN_LOCK_CLIPDATA);
	cliprdr->hasHugeFileSupport = (generalFlags & CB_HUGE_FILE_SUPPORT_ENABLED);
	cliprdr->capabilitiesReceived = TRUE;

	capabilities.common.msgType = CB_CLIP_CAPS;
	capabilities.cCapabilitiesSets = 1;
	capabilities.capabilitySets = (CLIPRDR_CAPABILITY_SET*)&(generalCapabilitySet);
	generalCapabilitySet.capabilitySetType = CB_CAPSTYPE_GENERAL;
	generalCapabilitySet.capabilitySetLength = 12;
	generalCapabilitySet.version = version;
	generalCapabilitySet.generalFlags = generalFlags;
	IFCALLRET(context->ServerCapabilities, error, context, &capabilities);

	if (error)
		WLog_ERR(TAG, "ServerCapabilities failed with error %" PRIu32 "!", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_process_clip_caps(cliprdrPlugin* cliprdr, wStream* s, UINT32 length,
                                      UINT16 flags)
{
	UINT16 index;
	UINT16 lengthCapability;
	UINT16 cCapabilitiesSets;
	UINT16 capabilitySetType;
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(cliprdr);
	WINPR_ASSERT(s);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT16(s, cCapabilitiesSets); /* cCapabilitiesSets (2 bytes) */
	Stream_Seek_UINT16(s);                    /* pad1 (2 bytes) */
	WLog_Print(cliprdr->log, WLOG_DEBUG, "ServerCapabilities");

	for (index = 0; index < cCapabilitiesSets; index++)
	{
		if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
			return ERROR_INVALID_DATA;

		Stream_Read_UINT16(s, capabilitySetType); /* capabilitySetType (2 bytes) */
		Stream_Read_UINT16(s, lengthCapability);  /* lengthCapability (2 bytes) */

		if ((lengthCapability < 4) ||
		    (!Stream_CheckAndLogRequiredLength(TAG, s, lengthCapability - 4U)))
			return ERROR_INVALID_DATA;

		switch (capabilitySetType)
		{
			case CB_CAPSTYPE_GENERAL:
				if ((error = cliprdr_process_general_capability(cliprdr, s)))
				{
					WLog_ERR(TAG,
					         "cliprdr_process_general_capability failed with error %" PRIu32 "!",
					         error);
					return error;
				}

				break;

			default:
				WLog_ERR(TAG, "unknown cliprdr capability set: %" PRIu16 "", capabilitySetType);
				return CHANNEL_RC_BAD_PROC;
		}
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_process_monitor_ready(cliprdrPlugin* cliprdr, wStream* s, UINT32 length,
                                          UINT16 flags)
{
	CLIPRDR_MONITOR_READY monitorReady = { 0 };
	CliprdrClientContext* context = cliprdr_get_client_interface(cliprdr);
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(cliprdr);
	WINPR_ASSERT(s);

	WLog_Print(cliprdr->log, WLOG_DEBUG, "MonitorReady");

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

	monitorReady.common.msgType = CB_MONITOR_READY;
	monitorReady.common.msgFlags = flags;
	monitorReady.common.dataLen = length;
	IFCALLRET(context->MonitorReady, error, context, &monitorReady);

	if (error)
		WLog_ERR(TAG, "MonitorReady failed with error %" PRIu32 "!", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_process_filecontents_request(cliprdrPlugin* cliprdr, wStream* s, UINT32 length,
                                                 UINT16 flags)
{
	CLIPRDR_FILE_CONTENTS_REQUEST request = { 0 };
	CliprdrClientContext* context = cliprdr_get_client_interface(cliprdr);
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(cliprdr);
	WINPR_ASSERT(s);

	WLog_Print(cliprdr->log, WLOG_DEBUG, "FileContentsRequest");

	request.common.msgType = CB_FILECONTENTS_REQUEST;
	request.common.msgFlags = flags;
	request.common.dataLen = length;

	if ((error = cliprdr_read_file_contents_request(s, &request)))
		return error;

	IFCALLRET(context->ServerFileContentsRequest, error, context, &request);

	if (error)
		WLog_ERR(TAG, "ServerFileContentsRequest failed with error %" PRIu32 "!", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_process_filecontents_response(cliprdrPlugin* cliprdr, wStream* s, UINT32 length,
                                                  UINT16 flags)
{
	CLIPRDR_FILE_CONTENTS_RESPONSE response = { 0 };
	CliprdrClientContext* context = cliprdr_get_client_interface(cliprdr);
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(cliprdr);
	WINPR_ASSERT(s);

	WLog_Print(cliprdr->log, WLOG_DEBUG, "FileContentsResponse");

	response.common.msgType = CB_FILECONTENTS_RESPONSE;
	response.common.msgFlags = flags;
	response.common.dataLen = length;

	if ((error = cliprdr_read_file_contents_response(s, &response)))
		return error;

	IFCALLRET(context->ServerFileContentsResponse, error, context, &response);

	if (error)
		WLog_ERR(TAG, "ServerFileContentsResponse failed with error %" PRIu32 "!", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_process_lock_clipdata(cliprdrPlugin* cliprdr, wStream* s, UINT32 length,
                                          UINT16 flags)
{
	CLIPRDR_LOCK_CLIPBOARD_DATA lockClipboardData = { 0 };
	CliprdrClientContext* context = cliprdr_get_client_interface(cliprdr);
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(cliprdr);
	WINPR_ASSERT(s);

	WLog_Print(cliprdr->log, WLOG_DEBUG, "LockClipData");

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return ERROR_INVALID_DATA;

	lockClipboardData.common.msgType = CB_LOCK_CLIPDATA;
	lockClipboardData.common.msgFlags = flags;
	lockClipboardData.common.dataLen = length;
	Stream_Read_UINT32(s, lockClipboardData.clipDataId); /* clipDataId (4 bytes) */
	IFCALLRET(context->ServerLockClipboardData, error, context, &lockClipboardData);

	if (error)
		WLog_ERR(TAG, "ServerLockClipboardData failed with error %" PRIu32 "!", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_process_unlock_clipdata(cliprdrPlugin* cliprdr, wStream* s, UINT32 length,
                                            UINT16 flags)
{
	CLIPRDR_UNLOCK_CLIPBOARD_DATA unlockClipboardData = { 0 };
	CliprdrClientContext* context = cliprdr_get_client_interface(cliprdr);
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(cliprdr);
	WINPR_ASSERT(s);

	WLog_Print(cliprdr->log, WLOG_DEBUG, "UnlockClipData");

	if ((error = cliprdr_read_unlock_clipdata(s, &unlockClipboardData)))
		return error;

	unlockClipboardData.common.msgType = CB_UNLOCK_CLIPDATA;
	unlockClipboardData.common.msgFlags = flags;
	unlockClipboardData.common.dataLen = length;

	IFCALLRET(context->ServerUnlockClipboardData, error, context, &unlockClipboardData);

	if (error)
		WLog_ERR(TAG, "ServerUnlockClipboardData failed with error %" PRIu32 "!", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_order_recv(LPVOID userdata, wStream* s)
{
	cliprdrPlugin* cliprdr = userdata;
	UINT16 msgType;
	UINT16 msgFlags;
	UINT32 dataLen;
	UINT error;

	WINPR_ASSERT(cliprdr);
	WINPR_ASSERT(s);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT16(s, msgType);  /* msgType (2 bytes) */
	Stream_Read_UINT16(s, msgFlags); /* msgFlags (2 bytes) */
	Stream_Read_UINT32(s, dataLen);  /* dataLen (4 bytes) */

	if (!Stream_CheckAndLogRequiredLength(TAG, s, dataLen))
		return ERROR_INVALID_DATA;

	WLog_DBG(TAG, "msgType: %s (%" PRIu16 "), msgFlags: %" PRIu16 " dataLen: %" PRIu32 "",
	         CB_MSG_TYPE_STRINGS(msgType), msgType, msgFlags, dataLen);
#if defined(WITH_DEBUG_CLIPRDR)
	winpr_HexDump(TAG, WLOG_DEBUG, Stream_Buffer(s), dataLen + 8);
#endif

	switch (msgType)
	{
		case CB_CLIP_CAPS:
			if ((error = cliprdr_process_clip_caps(cliprdr, s, dataLen, msgFlags)))
				WLog_ERR(TAG, "cliprdr_process_clip_caps failed with error %" PRIu32 "!", error);

			break;

		case CB_MONITOR_READY:
			if ((error = cliprdr_process_monitor_ready(cliprdr, s, dataLen, msgFlags)))
				WLog_ERR(TAG, "cliprdr_process_monitor_ready failed with error %" PRIu32 "!",
				         error);

			break;

		case CB_FORMAT_LIST:
			if ((error = cliprdr_process_format_list(cliprdr, s, dataLen, msgFlags)))
				WLog_ERR(TAG, "cliprdr_process_format_list failed with error %" PRIu32 "!", error);

			break;

		case CB_FORMAT_LIST_RESPONSE:
			if ((error = cliprdr_process_format_list_response(cliprdr, s, dataLen, msgFlags)))
				WLog_ERR(TAG, "cliprdr_process_format_list_response failed with error %" PRIu32 "!",
				         error);

			break;

		case CB_FORMAT_DATA_REQUEST:
			if ((error = cliprdr_process_format_data_request(cliprdr, s, dataLen, msgFlags)))
				WLog_ERR(TAG, "cliprdr_process_format_data_request failed with error %" PRIu32 "!",
				         error);

			break;

		case CB_FORMAT_DATA_RESPONSE:
			if ((error = cliprdr_process_format_data_response(cliprdr, s, dataLen, msgFlags)))
				WLog_ERR(TAG, "cliprdr_process_format_data_response failed with error %" PRIu32 "!",
				         error);

			break;

		case CB_FILECONTENTS_REQUEST:
			if ((error = cliprdr_process_filecontents_request(cliprdr, s, dataLen, msgFlags)))
				WLog_ERR(TAG, "cliprdr_process_filecontents_request failed with error %" PRIu32 "!",
				         error);

			break;

		case CB_FILECONTENTS_RESPONSE:
			if ((error = cliprdr_process_filecontents_response(cliprdr, s, dataLen, msgFlags)))
				WLog_ERR(TAG,
				         "cliprdr_process_filecontents_response failed with error %" PRIu32 "!",
				         error);

			break;

		case CB_LOCK_CLIPDATA:
			if ((error = cliprdr_process_lock_clipdata(cliprdr, s, dataLen, msgFlags)))
				WLog_ERR(TAG, "cliprdr_process_lock_clipdata failed with error %" PRIu32 "!",
				         error);

			break;

		case CB_UNLOCK_CLIPDATA:
			if ((error = cliprdr_process_unlock_clipdata(cliprdr, s, dataLen, msgFlags)))
				WLog_ERR(TAG, "cliprdr_process_unlock_clipdata failed with error %" PRIu32 "!",
				         error);

			break;

		default:
			error = CHANNEL_RC_BAD_PROC;
			WLog_ERR(TAG, "unknown msgType %" PRIu16 "", msgType);
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
                                        const CLIPRDR_CAPABILITIES* capabilities)
{
	wStream* s;
	UINT32 flags;
	const CLIPRDR_GENERAL_CAPABILITY_SET* generalCapabilitySet;
	cliprdrPlugin* cliprdr;

	WINPR_ASSERT(context);

	cliprdr = (cliprdrPlugin*)context->handle;
	WINPR_ASSERT(cliprdr);

	s = cliprdr_packet_new(CB_CLIP_CAPS, 0, 4 + CB_CAPSTYPE_GENERAL_LEN);

	if (!s)
	{
		WLog_ERR(TAG, "cliprdr_packet_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	Stream_Write_UINT16(s, 1); /* cCapabilitiesSets */
	Stream_Write_UINT16(s, 0); /* pad1 */
	generalCapabilitySet = (const CLIPRDR_GENERAL_CAPABILITY_SET*)capabilities->capabilitySets;
	Stream_Write_UINT16(s, generalCapabilitySet->capabilitySetType);   /* capabilitySetType */
	Stream_Write_UINT16(s, generalCapabilitySet->capabilitySetLength); /* lengthCapability */
	Stream_Write_UINT32(s, generalCapabilitySet->version);             /* version */
	flags = generalCapabilitySet->generalFlags;

	/* Client capabilities are sent in response to server capabilities.
	 * -> Do not request features the server does not support.
	 * -> Update clipboard context feature state to what was agreed upon.
	 */
	if (!cliprdr->useLongFormatNames)
		flags &= ~CB_USE_LONG_FORMAT_NAMES;
	if (!cliprdr->streamFileClipEnabled)
		flags &= ~CB_STREAM_FILECLIP_ENABLED;
	if (!cliprdr->fileClipNoFilePaths)
		flags &= ~CB_FILECLIP_NO_FILE_PATHS;
	if (!cliprdr->canLockClipData)
		flags &= ~CB_CAN_LOCK_CLIPDATA;
	if (!cliprdr->hasHugeFileSupport)
		flags &= ~CB_HUGE_FILE_SUPPORT_ENABLED;

	cliprdr->useLongFormatNames = flags & CB_USE_LONG_FORMAT_NAMES;
	cliprdr->streamFileClipEnabled = flags & CB_STREAM_FILECLIP_ENABLED;
	cliprdr->fileClipNoFilePaths = flags & CB_FILECLIP_NO_FILE_PATHS;
	cliprdr->canLockClipData = flags & CB_CAN_LOCK_CLIPDATA;
	cliprdr->hasHugeFileSupport = flags & CB_HUGE_FILE_SUPPORT_ENABLED;

	Stream_Write_UINT32(s, flags); /* generalFlags */
	WLog_Print(cliprdr->log, WLOG_DEBUG, "ClientCapabilities");
	return cliprdr_packet_send(cliprdr, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_temp_directory(CliprdrClientContext* context,
                                   const CLIPRDR_TEMP_DIRECTORY* tempDirectory)
{
	wStream* s;
	cliprdrPlugin* cliprdr;

	WINPR_ASSERT(context);
	WINPR_ASSERT(tempDirectory);

	cliprdr = (cliprdrPlugin*)context->handle;
	WINPR_ASSERT(cliprdr);

	const size_t tmpDirCharLen = sizeof(tempDirectory->szTempDir) / sizeof(WCHAR);
	s = cliprdr_packet_new(CB_TEMP_DIRECTORY, 0, tmpDirCharLen * sizeof(WCHAR));

	if (!s)
	{
		WLog_ERR(TAG, "cliprdr_packet_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	if (Stream_Write_UTF16_String_From_UTF8(s, tmpDirCharLen - 1, tempDirectory->szTempDir,
	                                        ARRAYSIZE(tempDirectory->szTempDir), TRUE) < 0)
		return ERROR_INTERNAL_ERROR;
	/* Path must be 260 UTF16 characters with '\0' termination.
	 * ensure this here */
	Stream_Write_UINT16(s, 0);

	WLog_Print(cliprdr->log, WLOG_DEBUG, "TempDirectory: %s", tempDirectory->szTempDir);
	return cliprdr_packet_send(cliprdr, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_client_format_list(CliprdrClientContext* context,
                                       const CLIPRDR_FORMAT_LIST* formatList)
{
	wStream* s;
	cliprdrPlugin* cliprdr;

	WINPR_ASSERT(context);

	cliprdr = (cliprdrPlugin*)context->handle;
	WINPR_ASSERT(cliprdr);

	s = cliprdr_packet_format_list_new(formatList, cliprdr->useLongFormatNames);
	if (!s)
	{
		WLog_ERR(TAG, "cliprdr_packet_format_list_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	WLog_Print(cliprdr->log, WLOG_DEBUG, "ClientFormatList: numFormats: %" PRIu32 "",
	           formatList->numFormats);
	return cliprdr_packet_send(cliprdr, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT
cliprdr_client_format_list_response(CliprdrClientContext* context,
                                    const CLIPRDR_FORMAT_LIST_RESPONSE* formatListResponse)
{
	wStream* s;
	cliprdrPlugin* cliprdr;

	WINPR_ASSERT(context);
	WINPR_ASSERT(formatListResponse);

	cliprdr = (cliprdrPlugin*)context->handle;
	WINPR_ASSERT(cliprdr);

	s = cliprdr_packet_new(CB_FORMAT_LIST_RESPONSE, formatListResponse->common.msgFlags, 0);

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
                                               const CLIPRDR_LOCK_CLIPBOARD_DATA* lockClipboardData)
{
	wStream* s;
	cliprdrPlugin* cliprdr;

	WINPR_ASSERT(context);
	WINPR_ASSERT(lockClipboardData);

	cliprdr = (cliprdrPlugin*)context->handle;
	WINPR_ASSERT(cliprdr);

	s = cliprdr_packet_lock_clipdata_new(lockClipboardData);

	if (!s)
	{
		WLog_ERR(TAG, "cliprdr_packet_lock_clipdata_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	WLog_Print(cliprdr->log, WLOG_DEBUG, "ClientLockClipboardData: clipDataId: 0x%08" PRIX32 "",
	           lockClipboardData->clipDataId);
	return cliprdr_packet_send(cliprdr, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT
cliprdr_client_unlock_clipboard_data(CliprdrClientContext* context,
                                     const CLIPRDR_UNLOCK_CLIPBOARD_DATA* unlockClipboardData)
{
	wStream* s;
	cliprdrPlugin* cliprdr;

	WINPR_ASSERT(context);
	WINPR_ASSERT(unlockClipboardData);

	cliprdr = (cliprdrPlugin*)context->handle;
	WINPR_ASSERT(cliprdr);

	s = cliprdr_packet_unlock_clipdata_new(unlockClipboardData);

	if (!s)
	{
		WLog_ERR(TAG, "cliprdr_packet_unlock_clipdata_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	WLog_Print(cliprdr->log, WLOG_DEBUG, "ClientUnlockClipboardData: clipDataId: 0x%08" PRIX32 "",
	           unlockClipboardData->clipDataId);
	return cliprdr_packet_send(cliprdr, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_client_format_data_request(CliprdrClientContext* context,
                                               const CLIPRDR_FORMAT_DATA_REQUEST* formatDataRequest)
{
	wStream* s;
	cliprdrPlugin* cliprdr;

	WINPR_ASSERT(context);
	WINPR_ASSERT(formatDataRequest);

	cliprdr = (cliprdrPlugin*)context->handle;
	WINPR_ASSERT(cliprdr);

	s = cliprdr_packet_new(CB_FORMAT_DATA_REQUEST, 0, 4);

	if (!s)
	{
		WLog_ERR(TAG, "cliprdr_packet_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	Stream_Write_UINT32(s, formatDataRequest->requestedFormatId); /* requestedFormatId (4 bytes) */
	WLog_Print(cliprdr->log, WLOG_DEBUG, "ClientFormatDataRequest");
	return cliprdr_packet_send(cliprdr, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT
cliprdr_client_format_data_response(CliprdrClientContext* context,
                                    const CLIPRDR_FORMAT_DATA_RESPONSE* formatDataResponse)
{
	wStream* s;
	cliprdrPlugin* cliprdr;

	WINPR_ASSERT(context);
	WINPR_ASSERT(formatDataResponse);

	cliprdr = (cliprdrPlugin*)context->handle;
	WINPR_ASSERT(cliprdr);

	s = cliprdr_packet_new(CB_FORMAT_DATA_RESPONSE, formatDataResponse->common.msgFlags,
	                       formatDataResponse->common.dataLen);

	if (!s)
	{
		WLog_ERR(TAG, "cliprdr_packet_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	Stream_Write(s, formatDataResponse->requestedFormatData, formatDataResponse->common.dataLen);
	WLog_Print(cliprdr->log, WLOG_DEBUG, "ClientFormatDataResponse");
	return cliprdr_packet_send(cliprdr, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT
cliprdr_client_file_contents_request(CliprdrClientContext* context,
                                     const CLIPRDR_FILE_CONTENTS_REQUEST* fileContentsRequest)
{
	wStream* s;
	cliprdrPlugin* cliprdr;

	WINPR_ASSERT(context);
	WINPR_ASSERT(fileContentsRequest);

	cliprdr = (cliprdrPlugin*)context->handle;
	if (!cliprdr)
		return ERROR_INTERNAL_ERROR;

	if (!cliprdr->hasHugeFileSupport)
	{
		if (((UINT64)fileContentsRequest->cbRequested + fileContentsRequest->nPositionLow) >
		    UINT32_MAX)
			return ERROR_INVALID_PARAMETER;
		if (fileContentsRequest->nPositionHigh != 0)
			return ERROR_INVALID_PARAMETER;
	}

	s = cliprdr_packet_file_contents_request_new(fileContentsRequest);

	if (!s)
	{
		WLog_ERR(TAG, "cliprdr_packet_file_contents_request_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	WLog_Print(cliprdr->log, WLOG_DEBUG, "ClientFileContentsRequest: streamId: 0x%08" PRIX32 "",
	           fileContentsRequest->streamId);
	return cliprdr_packet_send(cliprdr, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT
cliprdr_client_file_contents_response(CliprdrClientContext* context,
                                      const CLIPRDR_FILE_CONTENTS_RESPONSE* fileContentsResponse)
{
	wStream* s;
	cliprdrPlugin* cliprdr;

	WINPR_ASSERT(context);
	WINPR_ASSERT(fileContentsResponse);

	cliprdr = (cliprdrPlugin*)context->handle;
	WINPR_ASSERT(cliprdr);

	s = cliprdr_packet_file_contents_response_new(fileContentsResponse);

	if (!s)
	{
		WLog_ERR(TAG, "cliprdr_packet_file_contents_response_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	WLog_Print(cliprdr->log, WLOG_DEBUG, "ClientFileContentsResponse: streamId: 0x%08" PRIX32 "",
	           fileContentsResponse->streamId);
	return cliprdr_packet_send(cliprdr, s);
}

static VOID VCAPITYPE cliprdr_virtual_channel_open_event_ex(LPVOID lpUserParam, DWORD openHandle,
                                                            UINT event, LPVOID pData,
                                                            UINT32 dataLength, UINT32 totalLength,
                                                            UINT32 dataFlags)
{
	UINT error = CHANNEL_RC_OK;
	cliprdrPlugin* cliprdr = (cliprdrPlugin*)lpUserParam;

	switch (event)
	{
		case CHANNEL_EVENT_DATA_RECEIVED:
			if (!cliprdr || (cliprdr->OpenHandle != openHandle))
			{
				WLog_ERR(TAG, "error no match");
				return;
			}
			if ((error = channel_client_post_message(cliprdr->MsgsHandle, pData, dataLength,
			                                         totalLength, dataFlags)))
				WLog_ERR(TAG, "failed with error %" PRIu32 "", error);

			break;

		case CHANNEL_EVENT_WRITE_CANCELLED:
		case CHANNEL_EVENT_WRITE_COMPLETE:
		{
			wStream* s = (wStream*)pData;
			Stream_Free(s, TRUE);
		}
		break;

		case CHANNEL_EVENT_USER:
			break;
	}

	if (error && cliprdr && cliprdr->context->rdpcontext)
		setChannelError(cliprdr->context->rdpcontext, error,
		                "cliprdr_virtual_channel_open_event_ex reported an error");
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_virtual_channel_event_connected(cliprdrPlugin* cliprdr, LPVOID pData,
                                                    UINT32 dataLength)
{
	DWORD status;
	WINPR_ASSERT(cliprdr);
	WINPR_ASSERT(cliprdr->context);

	WINPR_ASSERT(cliprdr->channelEntryPoints.pVirtualChannelOpenEx);
	status = cliprdr->channelEntryPoints.pVirtualChannelOpenEx(
	    cliprdr->InitHandle, &cliprdr->OpenHandle, cliprdr->channelDef.name,
	    cliprdr_virtual_channel_open_event_ex);
	if (status != CHANNEL_RC_OK)
		return status;

	cliprdr->MsgsHandle = channel_client_create_handler(
	    cliprdr->context->rdpcontext, cliprdr, cliprdr_order_recv, CLIPRDR_SVC_CHANNEL_NAME);
	if (!cliprdr->MsgsHandle)
		return ERROR_INTERNAL_ERROR;

	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_virtual_channel_event_disconnected(cliprdrPlugin* cliprdr)
{
	UINT rc;

	WINPR_ASSERT(cliprdr);

	channel_client_quit_handler(cliprdr->MsgsHandle);
	cliprdr->MsgsHandle = NULL;

	if (cliprdr->OpenHandle == 0)
		return CHANNEL_RC_OK;

	WINPR_ASSERT(cliprdr->channelEntryPoints.pVirtualChannelCloseEx);
	rc = cliprdr->channelEntryPoints.pVirtualChannelCloseEx(cliprdr->InitHandle,
	                                                        cliprdr->OpenHandle);

	if (CHANNEL_RC_OK != rc)
	{
		WLog_ERR(TAG, "pVirtualChannelClose failed with %s [%08" PRIX32 "]", WTSErrorToString(rc),
		         rc);
		return rc;
	}

	cliprdr->OpenHandle = 0;

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_virtual_channel_event_terminated(cliprdrPlugin* cliprdr)
{
	WINPR_ASSERT(cliprdr);

	cliprdr->InitHandle = 0;
	free(cliprdr->context);
	free(cliprdr);
	return CHANNEL_RC_OK;
}

static VOID VCAPITYPE cliprdr_virtual_channel_init_event_ex(LPVOID lpUserParam, LPVOID pInitHandle,
                                                            UINT event, LPVOID pData,
                                                            UINT dataLength)
{
	UINT error = CHANNEL_RC_OK;
	cliprdrPlugin* cliprdr = (cliprdrPlugin*)lpUserParam;

	if (!cliprdr || (cliprdr->InitHandle != pInitHandle))
	{
		WLog_ERR(TAG, "error no match");
		return;
	}

	switch (event)
	{
		case CHANNEL_EVENT_CONNECTED:
			if ((error = cliprdr_virtual_channel_event_connected(cliprdr, pData, dataLength)))
				WLog_ERR(TAG,
				         "cliprdr_virtual_channel_event_connected failed with error %" PRIu32 "!",
				         error);

			break;

		case CHANNEL_EVENT_DISCONNECTED:
			if ((error = cliprdr_virtual_channel_event_disconnected(cliprdr)))
				WLog_ERR(TAG,
				         "cliprdr_virtual_channel_event_disconnected failed with error %" PRIu32
				         "!",
				         error);

			break;

		case CHANNEL_EVENT_TERMINATED:
			if ((error = cliprdr_virtual_channel_event_terminated(cliprdr)))
				WLog_ERR(TAG,
				         "cliprdr_virtual_channel_event_terminated failed with error %" PRIu32 "!",
				         error);

			break;
	}

	if (error && cliprdr->context->rdpcontext)
		setChannelError(cliprdr->context->rdpcontext, error,
		                "cliprdr_virtual_channel_init_event reported an error");
}

/* cliprdr is always built-in */
#define VirtualChannelEntryEx cliprdr_VirtualChannelEntryEx

BOOL VCAPITYPE VirtualChannelEntryEx(PCHANNEL_ENTRY_POINTS pEntryPoints, PVOID pInitHandle)
{
	UINT rc;
	cliprdrPlugin* cliprdr;
	CliprdrClientContext* context = NULL;
	CHANNEL_ENTRY_POINTS_FREERDP_EX* pEntryPointsEx;
	cliprdr = (cliprdrPlugin*)calloc(1, sizeof(cliprdrPlugin));

	if (!cliprdr)
	{
		WLog_ERR(TAG, "calloc failed!");
		return FALSE;
	}

	cliprdr->channelDef.options = CHANNEL_OPTION_INITIALIZED | CHANNEL_OPTION_ENCRYPT_RDP |
	                              CHANNEL_OPTION_COMPRESS_RDP | CHANNEL_OPTION_SHOW_PROTOCOL;
	sprintf_s(cliprdr->channelDef.name, ARRAYSIZE(cliprdr->channelDef.name),
	          CLIPRDR_SVC_CHANNEL_NAME);
	pEntryPointsEx = (CHANNEL_ENTRY_POINTS_FREERDP_EX*)pEntryPoints;
	WINPR_ASSERT(pEntryPointsEx);

	if ((pEntryPointsEx->cbSize >= sizeof(CHANNEL_ENTRY_POINTS_FREERDP_EX)) &&
	    (pEntryPointsEx->MagicNumber == FREERDP_CHANNEL_MAGIC_NUMBER))
	{
		context = (CliprdrClientContext*)calloc(1, sizeof(CliprdrClientContext));

		if (!context)
		{
			free(cliprdr);
			WLog_ERR(TAG, "calloc failed!");
			return FALSE;
		}

		context->handle = (void*)cliprdr;
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
	WLog_Print(cliprdr->log, WLOG_DEBUG, "VirtualChannelEntryEx");
	CopyMemory(&(cliprdr->channelEntryPoints), pEntryPoints,
	           sizeof(CHANNEL_ENTRY_POINTS_FREERDP_EX));
	cliprdr->InitHandle = pInitHandle;
	rc = cliprdr->channelEntryPoints.pVirtualChannelInitEx(
	    cliprdr, context, pInitHandle, &cliprdr->channelDef, 1, VIRTUAL_CHANNEL_VERSION_WIN2000,
	    cliprdr_virtual_channel_init_event_ex);

	if (CHANNEL_RC_OK != rc)
	{
		WLog_ERR(TAG, "pVirtualChannelInit failed with %s [%08" PRIX32 "]", WTSErrorToString(rc),
		         rc);
		free(cliprdr->context);
		free(cliprdr);
		return FALSE;
	}

	cliprdr->channelEntryPoints.pInterface = context;
	return TRUE;
}
