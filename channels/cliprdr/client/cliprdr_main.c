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
#include <freerdp/freerdp.h>
#include <freerdp/client/cliprdr.h>

#include "../../../channels/client/addin.h"

#include "cliprdr_main.h"
#include "cliprdr_format.h"
#include "../cliprdr_common.h"

const char type_FileGroupDescriptorW[] = "FileGroupDescriptorW";
const char type_FileContents[] = "FileContents";

CliprdrClientContext* cliprdr_get_client_interface(cliprdrPlugin* cliprdr)
{
	CliprdrClientContext* pInterface = NULL;

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
	size_t pos = 0;
	UINT32 dataLen = 0;
	UINT status = CHANNEL_RC_OK;

	WINPR_ASSERT(cliprdr);
	WINPR_ASSERT(s);

	pos = Stream_GetPosition(s);
	dataLen = pos - 8;
	Stream_SetPosition(s, 4);
	Stream_Write_UINT32(s, dataLen);
	Stream_SetPosition(s, pos);

	WLog_DBG(TAG, "Cliprdr Sending (%" PRIu32 " bytes)", dataLen + 8);

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

UINT cliprdr_send_error_response(cliprdrPlugin* cliprdr, UINT16 type)
{
	wStream* s = cliprdr_packet_new(type, CB_RESPONSE_FAIL, 0);
	if (!s)
	{
		WLog_ERR(TAG, "cliprdr_packet_new failed!");
		return ERROR_OUTOFMEMORY;
	}

	return cliprdr_packet_send(cliprdr, s);
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
	UINT32 version = 0;
	UINT32 generalFlags = 0;
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
	WLog_DBG(TAG, "Version: %" PRIu32 "", version);

	cliprdr_print_general_capability_flags(generalFlags);

	cliprdr->useLongFormatNames = (generalFlags & CB_USE_LONG_FORMAT_NAMES) ? TRUE : FALSE;
	cliprdr->streamFileClipEnabled = (generalFlags & CB_STREAM_FILECLIP_ENABLED) ? TRUE : FALSE;
	cliprdr->fileClipNoFilePaths = (generalFlags & CB_FILECLIP_NO_FILE_PATHS) ? TRUE : FALSE;
	cliprdr->canLockClipData = (generalFlags & CB_CAN_LOCK_CLIPDATA) ? TRUE : FALSE;
	cliprdr->hasHugeFileSupport = (generalFlags & CB_HUGE_FILE_SUPPORT_ENABLED) ? TRUE : FALSE;
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
	UINT16 lengthCapability = 0;
	UINT16 cCapabilitiesSets = 0;
	UINT16 capabilitySetType = 0;
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(cliprdr);
	WINPR_ASSERT(s);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT16(s, cCapabilitiesSets); /* cCapabilitiesSets (2 bytes) */
	Stream_Seek_UINT16(s);                    /* pad1 (2 bytes) */
	WLog_Print(cliprdr->log, WLOG_DEBUG, "ServerCapabilities");

	for (UINT16 index = 0; index < cCapabilitiesSets; index++)
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

	const UINT32 mask =
	    freerdp_settings_get_uint32(context->rdpcontext->settings, FreeRDP_ClipboardFeatureMask);
	if ((mask & (CLIPRDR_FLAG_LOCAL_TO_REMOTE_FILES)) == 0)
	{
		WLog_WARN(TAG, "local -> remote file copy disabled, ignoring request");
		return cliprdr_send_error_response(cliprdr, CB_FILECONTENTS_RESPONSE);
	}
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
	UINT16 msgType = 0;
	UINT16 msgFlags = 0;
	UINT32 dataLen = 0;
	UINT error = 0;

	WINPR_ASSERT(cliprdr);
	WINPR_ASSERT(s);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT16(s, msgType);  /* msgType (2 bytes) */
	Stream_Read_UINT16(s, msgFlags); /* msgFlags (2 bytes) */
	Stream_Read_UINT32(s, dataLen);  /* dataLen (4 bytes) */

	if (!Stream_CheckAndLogRequiredLength(TAG, s, dataLen))
		return ERROR_INVALID_DATA;

	char buffer1[64] = { 0 };
	char buffer2[64] = { 0 };
	WLog_DBG(TAG, "msgType: %s (%" PRIu16 "), msgFlags: %s dataLen: %" PRIu32 "",
	         CB_MSG_TYPE_STRING(msgType, buffer1, sizeof(buffer1)), msgType,
	         CB_MSG_FLAGS_STRING(msgFlags, buffer2, sizeof(buffer2)), dataLen);

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
	wStream* s = NULL;
	UINT32 flags = 0;
	const CLIPRDR_GENERAL_CAPABILITY_SET* generalCapabilitySet = NULL;
	cliprdrPlugin* cliprdr = NULL;

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

	cliprdr->useLongFormatNames = (flags & CB_USE_LONG_FORMAT_NAMES) ? TRUE : FALSE;
	cliprdr->streamFileClipEnabled = (flags & CB_STREAM_FILECLIP_ENABLED) ? TRUE : FALSE;
	cliprdr->fileClipNoFilePaths = (flags & CB_FILECLIP_NO_FILE_PATHS) ? TRUE : FALSE;
	cliprdr->canLockClipData = (flags & CB_CAN_LOCK_CLIPDATA) ? TRUE : FALSE;
	cliprdr->hasHugeFileSupport = (flags & CB_HUGE_FILE_SUPPORT_ENABLED) ? TRUE : FALSE;

	Stream_Write_UINT32(s, flags); /* generalFlags */
	WLog_Print(cliprdr->log, WLOG_DEBUG, "ClientCapabilities");

	cliprdr->initialFormatListSent = FALSE;

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
	wStream* s = NULL;
	cliprdrPlugin* cliprdr = NULL;

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
	{
		Stream_Free(s, TRUE);
		return ERROR_INTERNAL_ERROR;
	}
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
	wStream* s = NULL;
	cliprdrPlugin* cliprdr = NULL;

	WINPR_ASSERT(context);
	WINPR_ASSERT(formatList);

	cliprdr = (cliprdrPlugin*)context->handle;
	WINPR_ASSERT(cliprdr);

	{
		const UINT32 mask = CB_RESPONSE_OK | CB_RESPONSE_FAIL;
		if ((formatList->common.msgFlags & mask) != 0)
			WLog_WARN(TAG,
			          "Sending clipboard request with invalid flags msgFlags = 0x%08" PRIx32
			          ". Correct in your client!",
			          formatList->common.msgFlags & mask);
	}

	const UINT32 mask =
	    freerdp_settings_get_uint32(context->rdpcontext->settings, FreeRDP_ClipboardFeatureMask);
	CLIPRDR_FORMAT_LIST filterList = cliprdr_filter_format_list(
	    formatList, mask, CLIPRDR_FLAG_LOCAL_TO_REMOTE | CLIPRDR_FLAG_LOCAL_TO_REMOTE_FILES);

	/* Allow initial format list from monitor ready, but ignore later attempts */
	if ((filterList.numFormats == 0) && cliprdr->initialFormatListSent)
	{
		cliprdr_free_format_list(&filterList);
		return CHANNEL_RC_OK;
	}
	cliprdr->initialFormatListSent = TRUE;

	s = cliprdr_packet_format_list_new(&filterList, cliprdr->useLongFormatNames, FALSE);
	cliprdr_free_format_list(&filterList);

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
	wStream* s = NULL;
	cliprdrPlugin* cliprdr = NULL;

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
	wStream* s = NULL;
	cliprdrPlugin* cliprdr = NULL;

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
	wStream* s = NULL;
	cliprdrPlugin* cliprdr = NULL;

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
	WINPR_ASSERT(context);
	WINPR_ASSERT(formatDataRequest);

	cliprdrPlugin* cliprdr = (cliprdrPlugin*)context->handle;
	WINPR_ASSERT(cliprdr);

	const UINT32 mask =
	    freerdp_settings_get_uint32(context->rdpcontext->settings, FreeRDP_ClipboardFeatureMask);
	if ((mask & (CLIPRDR_FLAG_REMOTE_TO_LOCAL | CLIPRDR_FLAG_REMOTE_TO_LOCAL_FILES)) == 0)
	{
		WLog_WARN(TAG, "remote -> local copy disabled, ignoring request");
		return CHANNEL_RC_OK;
	}

	wStream* s = cliprdr_packet_new(CB_FORMAT_DATA_REQUEST, 0, 4);
	if (!s)
	{
		WLog_ERR(TAG, "cliprdr_packet_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	Stream_Write_UINT32(s, formatDataRequest->requestedFormatId); /* requestedFormatId (4 bytes) */
	WLog_Print(cliprdr->log, WLOG_DEBUG, "ClientFormatDataRequest(0x%08" PRIx32 ")",
	           formatDataRequest->requestedFormatId);
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
	WINPR_ASSERT(context);
	WINPR_ASSERT(formatDataResponse);

	cliprdrPlugin* cliprdr = (cliprdrPlugin*)context->handle;
	WINPR_ASSERT(cliprdr);

	WINPR_ASSERT(
	    (freerdp_settings_get_uint32(context->rdpcontext->settings, FreeRDP_ClipboardFeatureMask) &
	     (CLIPRDR_FLAG_LOCAL_TO_REMOTE | CLIPRDR_FLAG_LOCAL_TO_REMOTE_FILES)) != 0);

	wStream* s = cliprdr_packet_new(CB_FORMAT_DATA_RESPONSE, formatDataResponse->common.msgFlags,
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
	wStream* s = NULL;
	cliprdrPlugin* cliprdr = NULL;

	WINPR_ASSERT(context);
	WINPR_ASSERT(fileContentsRequest);

	const UINT32 mask =
	    freerdp_settings_get_uint32(context->rdpcontext->settings, FreeRDP_ClipboardFeatureMask);
	if ((mask & CLIPRDR_FLAG_REMOTE_TO_LOCAL_FILES) == 0)
	{
		WLog_WARN(TAG, "remote -> local file copy disabled, ignoring request");
		return CHANNEL_RC_OK;
	}

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
	wStream* s = NULL;
	cliprdrPlugin* cliprdr = NULL;

	WINPR_ASSERT(context);
	WINPR_ASSERT(fileContentsResponse);

	cliprdr = (cliprdrPlugin*)context->handle;
	WINPR_ASSERT(cliprdr);

	const UINT32 mask =
	    freerdp_settings_get_uint32(context->rdpcontext->settings, FreeRDP_ClipboardFeatureMask);
	if ((mask & CLIPRDR_FLAG_LOCAL_TO_REMOTE_FILES) == 0)
		return cliprdr_send_error_response(cliprdr, CB_FILECONTENTS_RESPONSE);

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
	DWORD status = 0;
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
	UINT rc = 0;

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

FREERDP_ENTRY_POINT(BOOL VCAPITYPE VirtualChannelEntryEx(PCHANNEL_ENTRY_POINTS pEntryPoints,
                                                         PVOID pInitHandle))
{
	UINT rc = 0;
	cliprdrPlugin* cliprdr = NULL;
	CliprdrClientContext* context = NULL;
	CHANNEL_ENTRY_POINTS_FREERDP_EX* pEntryPointsEx = NULL;
	cliprdr = (cliprdrPlugin*)calloc(1, sizeof(cliprdrPlugin));

	if (!cliprdr)
	{
		WLog_ERR(TAG, "calloc failed!");
		return FALSE;
	}

	cliprdr->channelDef.options = CHANNEL_OPTION_INITIALIZED | CHANNEL_OPTION_ENCRYPT_RDP |
	                              CHANNEL_OPTION_COMPRESS_RDP | CHANNEL_OPTION_SHOW_PROTOCOL;
	(void)sprintf_s(cliprdr->channelDef.name, ARRAYSIZE(cliprdr->channelDef.name),
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

	cliprdr->log = WLog_Get(CHANNELS_TAG("channels.cliprdr.client"));
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
