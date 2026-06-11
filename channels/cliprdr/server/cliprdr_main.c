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

#include <freerdp/config.h>

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/print.h>
#include <winpr/stream.h>

#include <freerdp/freerdp.h>
#include <freerdp/channels/log.h>
#include "cliprdr_main.h"
#include "../cliprdr_common.h"

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
 *        |<--------------------------Format List Response PDU----------------------|\n _| Copy
 * Sequence
 *        |<---------------------Lock Clipboard Data PDU (Optional)-----------------|\n
 *        |-------------------------------------------------------------------------|\n
 *        |-------------------------------------------------------------------------|\n _
 *        |<--------------------------Format Data Request PDU-----------------------|\n  | Paste
 * Sequence Palette,
 *        |---------------------------Format Data Response PDU--------------------->|\n _| Metafile,
 * File List Data
 *        |-------------------------------------------------------------------------|\n
 *        |-------------------------------------------------------------------------|\n _
 *        |<------------------------Format Contents Request PDU---------------------|\n  | Paste
 * Sequence
 *        |-------------------------Format Contents Response PDU------------------->|\n _| File
 * Stream Data
 *        |<---------------------Lock Clipboard Data PDU (Optional)-----------------|\n
 *        |-------------------------------------------------------------------------|\n
 *
 */

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_packet_send(CliprdrServerPrivate* cliprdr, wStream* s)
{
	UINT rc = 0;
	BOOL status = 0;
	ULONG written = 0;

	WINPR_ASSERT(cliprdr);

	const size_t pos = Stream_GetPosition(s);
	if ((pos < 8) || (pos > UINT32_MAX))
	{
		rc = ERROR_NO_DATA;
		goto fail;
	}

	const UINT32 dataLen = (UINT32)(pos - 8);
	if (!Stream_SetPosition(s, 4))
		goto fail;
	Stream_Write_UINT32(s, dataLen);

	WINPR_ASSERT(pos <= UINT32_MAX);
	status = WTSVirtualChannelWrite(cliprdr->ChannelHandle, Stream_BufferAs(s, char), (UINT32)pos,
	                                &written);
	rc = status ? CHANNEL_RC_OK : ERROR_INTERNAL_ERROR;
fail:
	Stream_Free(s, TRUE);
	return rc;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_capabilities(CliprdrServerContext* context,
                                        const CLIPRDR_CAPABILITIES* capabilities)
{
	size_t offset = 0;

	WINPR_ASSERT(context);
	WINPR_ASSERT(capabilities);

	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;

	if (capabilities->common.msgType != CB_CLIP_CAPS)
		WLog_Print(cliprdr->log, WLOG_WARN, "called with invalid type %08" PRIx32,
		           capabilities->common.msgType);

	if (capabilities->cCapabilitiesSets > UINT16_MAX)
	{
		WLog_Print(cliprdr->log, WLOG_ERROR, "Invalid number of capability sets in clipboard caps");
		return ERROR_INVALID_PARAMETER;
	}

	wStream* s = cliprdr_packet_new(CB_CLIP_CAPS, 0, 4 + CB_CAPSTYPE_GENERAL_LEN);

	if (!s)
	{
		WLog_Print(cliprdr->log, WLOG_ERROR, "cliprdr_packet_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	Stream_Write_UINT16(s,
	                    (UINT16)capabilities->cCapabilitiesSets); /* cCapabilitiesSets (2 bytes) */
	Stream_Write_UINT16(s, 0);                                    /* pad1 (2 bytes) */
	for (UINT32 x = 0; x < capabilities->cCapabilitiesSets; x++)
	{
		const CLIPRDR_CAPABILITY_SET* cap =
		    (const CLIPRDR_CAPABILITY_SET*)(((const BYTE*)capabilities->capabilitySets) + offset);
		offset += cap->capabilitySetLength;

		switch (cap->capabilitySetType)
		{
			case CB_CAPSTYPE_GENERAL:
			{
				const CLIPRDR_GENERAL_CAPABILITY_SET* generalCapabilitySet =
				    (const CLIPRDR_GENERAL_CAPABILITY_SET*)cap;
				Stream_Write_UINT16(
				    s, generalCapabilitySet->capabilitySetType); /* capabilitySetType (2 bytes) */
				Stream_Write_UINT16(
				    s, generalCapabilitySet->capabilitySetLength); /* lengthCapability (2 bytes) */
				Stream_Write_UINT32(s, generalCapabilitySet->version); /* version (4 bytes) */
				Stream_Write_UINT32(
				    s, generalCapabilitySet->generalFlags); /* generalFlags (4 bytes) */
			}
			break;

			default:
				WLog_Print(cliprdr->log, WLOG_WARN, "Unknown capability set type %08" PRIx16,
				           cap->capabilitySetType);
				if (!Stream_SafeSeek(s, cap->capabilitySetLength))
				{
					WLog_Print(cliprdr->log, WLOG_ERROR, "short stream");
					Stream_Free(s, TRUE);
					return ERROR_NO_DATA;
				}
				break;
		}
	}
	WLog_Print(cliprdr->log, WLOG_DEBUG, "ServerCapabilities");
	return cliprdr_server_packet_send(cliprdr, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_monitor_ready(CliprdrServerContext* context,
                                         const CLIPRDR_MONITOR_READY* monitorReady)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(monitorReady);

	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;

	if (monitorReady->common.msgType != CB_MONITOR_READY)
		WLog_Print(cliprdr->log, WLOG_WARN, "called with invalid type %08" PRIx32,
		           monitorReady->common.msgType);

	wStream* s = cliprdr_packet_new(CB_MONITOR_READY, monitorReady->common.msgFlags,
	                                monitorReady->common.dataLen);

	if (!s)
	{
		WLog_Print(cliprdr->log, WLOG_ERROR, "cliprdr_packet_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	WLog_Print(cliprdr->log, WLOG_DEBUG, "ServerMonitorReady");
	return cliprdr_server_packet_send(cliprdr, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_format_list(CliprdrServerContext* context,
                                       const CLIPRDR_FORMAT_LIST* formatList)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(formatList);

	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;

	wStream* s = cliprdr_packet_format_list_new(formatList, context->useLongFormatNames, FALSE);
	if (!s)
	{
		WLog_Print(cliprdr->log, WLOG_ERROR, "cliprdr_packet_format_list_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	WLog_Print(cliprdr->log, WLOG_DEBUG, "ServerFormatList: numFormats: %" PRIu32 "",
	           formatList->numFormats);
	return cliprdr_server_packet_send(cliprdr, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT
cliprdr_server_format_list_response(CliprdrServerContext* context,
                                    const CLIPRDR_FORMAT_LIST_RESPONSE* formatListResponse)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(formatListResponse);

	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;
	if (formatListResponse->common.msgType != CB_FORMAT_LIST_RESPONSE)
		WLog_Print(cliprdr->log, WLOG_WARN, "called with invalid type %08" PRIx32,
		           formatListResponse->common.msgType);

	wStream* s = cliprdr_packet_new(CB_FORMAT_LIST_RESPONSE, formatListResponse->common.msgFlags,
	                                formatListResponse->common.dataLen);

	if (!s)
	{
		WLog_Print(cliprdr->log, WLOG_ERROR, "cliprdr_packet_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	WLog_Print(cliprdr->log, WLOG_DEBUG, "ServerFormatListResponse");
	return cliprdr_server_packet_send(cliprdr, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_lock_clipboard_data(CliprdrServerContext* context,
                                               const CLIPRDR_LOCK_CLIPBOARD_DATA* lockClipboardData)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(lockClipboardData);

	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;
	if (lockClipboardData->common.msgType != CB_LOCK_CLIPDATA)
		WLog_Print(cliprdr->log, WLOG_WARN, "called with invalid type %08" PRIx32,
		           lockClipboardData->common.msgType);

	wStream* s = cliprdr_packet_lock_clipdata_new(lockClipboardData);
	if (!s)
	{
		WLog_Print(cliprdr->log, WLOG_ERROR, "cliprdr_packet_lock_clipdata_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	WLog_Print(cliprdr->log, WLOG_DEBUG, "ServerLockClipboardData: clipDataId: 0x%08" PRIX32 "",
	           lockClipboardData->clipDataId);
	return cliprdr_server_packet_send(cliprdr, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT
cliprdr_server_unlock_clipboard_data(CliprdrServerContext* context,
                                     const CLIPRDR_UNLOCK_CLIPBOARD_DATA* unlockClipboardData)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(unlockClipboardData);

	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;
	if (unlockClipboardData->common.msgType != CB_UNLOCK_CLIPDATA)
		WLog_Print(cliprdr->log, WLOG_WARN, "called with invalid type %08" PRIx32,
		           unlockClipboardData->common.msgType);

	wStream* s = cliprdr_packet_unlock_clipdata_new(unlockClipboardData);

	if (!s)
	{
		WLog_Print(cliprdr->log, WLOG_ERROR, "cliprdr_packet_unlock_clipdata_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	WLog_Print(cliprdr->log, WLOG_DEBUG, "ServerUnlockClipboardData: clipDataId: 0x%08" PRIX32 "",
	           unlockClipboardData->clipDataId);
	return cliprdr_server_packet_send(cliprdr, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_format_data_request(CliprdrServerContext* context,
                                               const CLIPRDR_FORMAT_DATA_REQUEST* formatDataRequest)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(formatDataRequest);

	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;
	if (formatDataRequest->common.msgType != CB_FORMAT_DATA_REQUEST)
		WLog_Print(cliprdr->log, WLOG_WARN, "called with invalid type %08" PRIx32,
		           formatDataRequest->common.msgType);

	wStream* s = cliprdr_packet_new(CB_FORMAT_DATA_REQUEST, formatDataRequest->common.msgFlags,
	                                formatDataRequest->common.dataLen);

	if (!s)
	{
		WLog_Print(cliprdr->log, WLOG_ERROR, "cliprdr_packet_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	Stream_Write_UINT32(s, formatDataRequest->requestedFormatId); /* requestedFormatId (4 bytes) */
	WLog_Print(cliprdr->log, WLOG_DEBUG, "ClientFormatDataRequest");
	return cliprdr_server_packet_send(cliprdr, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT
cliprdr_server_format_data_response(CliprdrServerContext* context,
                                    const CLIPRDR_FORMAT_DATA_RESPONSE* formatDataResponse)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(formatDataResponse);

	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;

	if (formatDataResponse->common.msgType != CB_FORMAT_DATA_RESPONSE)
		WLog_Print(cliprdr->log, WLOG_WARN, "called with invalid type %08" PRIx32,
		           formatDataResponse->common.msgType);

	wStream* s = cliprdr_packet_new(CB_FORMAT_DATA_RESPONSE, formatDataResponse->common.msgFlags,
	                                formatDataResponse->common.dataLen);

	if (!s)
	{
		WLog_Print(cliprdr->log, WLOG_ERROR, "cliprdr_packet_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	Stream_Write(s, formatDataResponse->requestedFormatData, formatDataResponse->common.dataLen);
	WLog_Print(cliprdr->log, WLOG_DEBUG, "ServerFormatDataResponse");
	return cliprdr_server_packet_send(cliprdr, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT
cliprdr_server_file_contents_request(CliprdrServerContext* context,
                                     const CLIPRDR_FILE_CONTENTS_REQUEST* fileContentsRequest)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(fileContentsRequest);

	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;

	if (fileContentsRequest->common.msgType != CB_FILECONTENTS_REQUEST)
		WLog_Print(cliprdr->log, WLOG_WARN, "called with invalid type %08" PRIx32,
		           fileContentsRequest->common.msgType);

	wStream* s = cliprdr_packet_file_contents_request_new(fileContentsRequest);
	if (!s)
	{
		WLog_Print(cliprdr->log, WLOG_ERROR, "cliprdr_packet_file_contents_request_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	WLog_Print(cliprdr->log, WLOG_DEBUG, "ServerFileContentsRequest: streamId: 0x%08" PRIX32 "",
	           fileContentsRequest->streamId);
	return cliprdr_server_packet_send(cliprdr, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT
cliprdr_server_file_contents_response(CliprdrServerContext* context,
                                      const CLIPRDR_FILE_CONTENTS_RESPONSE* fileContentsResponse)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(fileContentsResponse);

	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;
	WINPR_ASSERT(cliprdr);

	if (fileContentsResponse->common.msgType != CB_FILECONTENTS_RESPONSE)
		WLog_Print(cliprdr->log, WLOG_WARN, "called with invalid type %08" PRIx32,
		           fileContentsResponse->common.msgType);

	wStream* s = cliprdr_packet_file_contents_response_new(fileContentsResponse);
	if (!s)
	{
		WLog_Print(cliprdr->log, WLOG_ERROR, "cliprdr_packet_file_contents_response_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	WLog_Print(cliprdr->log, WLOG_DEBUG, "ServerFileContentsResponse: streamId: 0x%08" PRIX32 "",
	           fileContentsResponse->streamId);
	return cliprdr_server_packet_send(cliprdr, s);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_receive_general_capability(CliprdrServerContext* context, wStream* s,
                                                      CLIPRDR_GENERAL_CAPABILITY_SET* cap_set)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(cap_set);

	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;
	WINPR_ASSERT(cliprdr);

	if (cap_set->capabilitySetLength != sizeof(CLIPRDR_GENERAL_CAPABILITY_SET))
	{
		WLog_Print(cliprdr->log, WLOG_ERROR,
		           "invalid capabilitySetLength %" PRIu16 " != %" PRIuz
		           " (capabilitySetType=CB_CAPSTYPE_GENERAL)",
		           cap_set->capabilitySetLength, sizeof(CLIPRDR_GENERAL_CAPABILITY_SET));
		return ERROR_INVALID_DATA;
	}

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, cap_set->version);      /* version (4 bytes) */
	Stream_Read_UINT32(s, cap_set->generalFlags); /* generalFlags (4 bytes) */

	if (context->useLongFormatNames)
		context->useLongFormatNames = (cap_set->generalFlags & CB_USE_LONG_FORMAT_NAMES) != 0;

	if (context->streamFileClipEnabled)
		context->streamFileClipEnabled = (cap_set->generalFlags & CB_STREAM_FILECLIP_ENABLED) != 0;

	if (context->fileClipNoFilePaths)
		context->fileClipNoFilePaths = (cap_set->generalFlags & CB_FILECLIP_NO_FILE_PATHS) != 0;

	if (context->canLockClipData)
		context->canLockClipData = (cap_set->generalFlags & CB_CAN_LOCK_CLIPDATA) != 0;

	if (context->hasHugeFileSupport)
		context->hasHugeFileSupport = (cap_set->generalFlags & CB_HUGE_FILE_SUPPORT_ENABLED) != 0;

	return CHANNEL_RC_OK;
}

WINPR_ATTR_NODISCARD
static BOOL cliprdr_capabilities_contain_capset(wLog* log, const CLIPRDR_CAPABILITIES* capabilities,
                                                size_t capabilitiesSize,
                                                const CLIPRDR_CAPABILITY_SET* capset)
{
	WINPR_ASSERT(capabilities);

	const BYTE* offset = (const BYTE*)capabilities->capabilitySets;
	size_t byteOffset = 0;
	for (UINT32 x = 0; x < capabilities->cCapabilitiesSets; x++)
	{
		const CLIPRDR_CAPABILITY_SET* cur = (const CLIPRDR_CAPABILITY_SET*)offset;
		byteOffset += cur->capabilitySetLength;
		if (byteOffset > capabilitiesSize)
			return FALSE;

		offset += cur->capabilitySetLength;

		if (cur->capabilitySetType == capset->capabilitySetType)
		{
			WLog_Print(log, WLOG_ERROR, "duplicate  CLIPRDR_CAPABILITY_SET %" PRIu16,
			           cur->capabilitySetType);
			return TRUE;
		}
	}
	return FALSE;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_receive_capabilities(CliprdrServerContext* context, wStream* s,
                                                const CLIPRDR_HEADER* header)
{
	UINT error = ERROR_INVALID_DATA;
	size_t cap_sets_size = 0;
	CLIPRDR_CAPABILITIES capabilities = WINPR_C_ARRAY_INIT;

	WINPR_ASSERT(context);
	WINPR_UNUSED(header);

	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;
	WINPR_ASSERT(cliprdr);

	WLog_Print(cliprdr->log, WLOG_DEBUG, "CliprdrClientCapabilities");
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return ERROR_INVALID_DATA;

	const UINT16 cCapabilitiesSets = Stream_Get_UINT16(s); /* cCapabilitiesSets (2 bytes) */
	Stream_Seek_UINT16(s);                                 /* pad1 (2 bytes) */

	for (size_t index = 0; index < cCapabilitiesSets; index++)
	{
		const size_t cap_set_offset = cap_sets_size;
		if (!Stream_CheckAndLogRequiredLength(TAG, s, sizeof(CLIPRDR_CAPABILITY_SET)))
			goto out;
		const UINT16 capabilitySetType = Stream_Get_UINT16(s);   /* capabilitySetType (2 bytes) */
		const UINT16 capabilitySetLength = Stream_Get_UINT16(s); /* capabilitySetLength (2 bytes) */

		if (capabilitySetLength < sizeof(CLIPRDR_CAPABILITY_SET))
		{
			WLog_Print(cliprdr->log, WLOG_ERROR,
			           "invalid capabilitySetLength %" PRIu16 " < %" PRIuz
			           " (capabilitySetType=%" PRIu16 ")",
			           capabilitySetLength, sizeof(CLIPRDR_CAPABILITY_SET), capabilitySetType);
			goto out;
		}

		if (!Stream_CheckAndLogRequiredLength(
		        TAG, s, (size_t)capabilitySetLength - sizeof(CLIPRDR_CAPABILITY_SET)))
			goto out;

		cap_sets_size += capabilitySetLength;

		CLIPRDR_CAPABILITY_SET* tmp = realloc(capabilities.capabilitySets, cap_sets_size);
		if (tmp == nullptr)
		{
			WLog_Print(cliprdr->log, WLOG_ERROR, "capabilities.capabilitySets realloc failed!");
			free(capabilities.capabilitySets);
			return CHANNEL_RC_NO_MEMORY;
		}

		capabilities.capabilitySets = tmp;

		CLIPRDR_CAPABILITY_SET* capSet =
		    (CLIPRDR_CAPABILITY_SET*)(((BYTE*)capabilities.capabilitySets) + cap_set_offset);

		capSet->capabilitySetType = capabilitySetType;
		capSet->capabilitySetLength = capabilitySetLength;

		if (cliprdr_capabilities_contain_capset(cliprdr->log, &capabilities, cap_sets_size, capSet))
			goto out;

		capabilities.cCapabilitiesSets++;

		switch (capSet->capabilitySetType)
		{
			case CB_CAPSTYPE_GENERAL:
				error = cliprdr_server_receive_general_capability(
				    context, s, (CLIPRDR_GENERAL_CAPABILITY_SET*)capSet);
				if (error)
				{
					WLog_Print(
					    cliprdr->log, WLOG_ERROR,
					    "cliprdr_server_receive_general_capability failed with error %" PRIu32 "",
					    error);
					goto out;
				}
				break;

			default:
				WLog_Print(cliprdr->log, WLOG_ERROR, "unknown cliprdr capability set: %" PRIu16 "",
				           capSet->capabilitySetType);
				goto out;
		}
	}

	error = CHANNEL_RC_OK;
	IFCALLRET(context->ClientCapabilities, error, context, &capabilities);
out:
	free(capabilities.capabilitySets);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_receive_temporary_directory(CliprdrServerContext* context, wStream* s,
                                                       const CLIPRDR_HEADER* header)
{
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(context);
	WINPR_UNUSED(header);

	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;
	WINPR_ASSERT(cliprdr);

	if (!Stream_CheckAndLogRequiredLength(TAG, s,
	                                      ARRAYSIZE(cliprdr->temporaryDirectory) * sizeof(WCHAR)))
		return CHANNEL_RC_NO_MEMORY;

	const WCHAR* wszTempDir = Stream_ConstPointer(s);

	if (wszTempDir[ARRAYSIZE(cliprdr->temporaryDirectory) - 1] != 0)
	{
		WLog_Print(cliprdr->log, WLOG_ERROR, "wszTempDir[259] was not 0");
		return ERROR_INVALID_DATA;
	}

	if (ConvertWCharNToUtf8(wszTempDir, ARRAYSIZE(cliprdr->temporaryDirectory),
	                        cliprdr->temporaryDirectory,
	                        ARRAYSIZE(cliprdr->temporaryDirectory)) < 0)
	{
		WLog_Print(cliprdr->log, WLOG_ERROR, "failed to convert temporary directory name");
		return ERROR_INVALID_DATA;
	}

	size_t length = strnlen(cliprdr->temporaryDirectory, ARRAYSIZE(cliprdr->temporaryDirectory));

	if (length >= ARRAYSIZE(cliprdr->temporaryDirectory))
		length = ARRAYSIZE(cliprdr->temporaryDirectory) - 1;

	CLIPRDR_TEMP_DIRECTORY tempDirectory = WINPR_C_ARRAY_INIT;
	CopyMemory(tempDirectory.szTempDir, cliprdr->temporaryDirectory, length);
	tempDirectory.szTempDir[length] = '\0';
	WLog_Print(cliprdr->log, WLOG_DEBUG, "CliprdrTemporaryDirectory: %s",
	           cliprdr->temporaryDirectory);
	IFCALLRET(context->TempDirectory, error, context, &tempDirectory);

	if (error)
		WLog_Print(cliprdr->log, WLOG_ERROR, "TempDirectory failed with error %" PRIu32 "!", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_receive_format_list(CliprdrServerContext* context, wStream* s,
                                               const CLIPRDR_HEADER* header)
{
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(context);
	WINPR_ASSERT(header);

	CLIPRDR_FORMAT_LIST formatList = { .common = { .msgType = CB_FORMAT_LIST,
		                                           .msgFlags = header->msgFlags,
		                                           .dataLen = header->dataLen },
		                               .numFormats = 0,
		                               .formats = nullptr };
	wLog* log = WLog_Get(TAG);
	if ((error = cliprdr_read_format_list(log, s, &formatList, context->useLongFormatNames)))
		goto out;

	WLog_Print(log, WLOG_DEBUG, "ClientFormatList: numFormats: %" PRIu32 "", formatList.numFormats);
	IFCALLRET(context->ClientFormatList, error, context, &formatList);

	if (error)
		WLog_Print(log, WLOG_ERROR, "ClientFormatList failed with error %" PRIu32 "!", error);

out:
	cliprdr_free_format_list(&formatList);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_receive_format_list_response(CliprdrServerContext* context, wStream* s,
                                                        const CLIPRDR_HEADER* header)
{
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(context);
	WINPR_ASSERT(header);

	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;
	WINPR_ASSERT(cliprdr);

	WINPR_UNUSED(s);
	WLog_Print(cliprdr->log, WLOG_DEBUG, "CliprdrClientFormatListResponse");

	CLIPRDR_FORMAT_LIST_RESPONSE formatListResponse = { .common = { .msgType =
		                                                                CB_FORMAT_LIST_RESPONSE,
		                                                            .msgFlags = header->msgFlags,
		                                                            .dataLen = header->dataLen } };
	IFCALLRET(context->ClientFormatListResponse, error, context, &formatListResponse);

	if (error)
		WLog_Print(cliprdr->log, WLOG_ERROR,
		           "ClientFormatListResponse failed with error %" PRIu32 "!", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_receive_lock_clipdata(CliprdrServerContext* context, wStream* s,
                                                 const CLIPRDR_HEADER* header)
{
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(context);
	WINPR_ASSERT(header);

	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;
	WINPR_ASSERT(cliprdr);

	WLog_Print(cliprdr->log, WLOG_DEBUG, "CliprdrClientLockClipData");

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return ERROR_INVALID_DATA;

	CLIPRDR_LOCK_CLIPBOARD_DATA lockClipboardData = { .common = { .msgType = CB_LOCK_CLIPDATA,
		                                                          .msgFlags = header->msgFlags,
		                                                          .dataLen = header->dataLen },
		                                              .clipDataId = 0 };

	Stream_Read_UINT32(s, lockClipboardData.clipDataId); /* clipDataId (4 bytes) */
	IFCALLRET(context->ClientLockClipboardData, error, context, &lockClipboardData);

	if (error)
		WLog_Print(cliprdr->log, WLOG_ERROR,
		           "ClientLockClipboardData failed with error %" PRIu32 "!", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_receive_unlock_clipdata(CliprdrServerContext* context, wStream* s,
                                                   const CLIPRDR_HEADER* header)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(header);

	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;
	WINPR_ASSERT(cliprdr);

	WLog_Print(cliprdr->log, WLOG_DEBUG, "CliprdrClientUnlockClipData");

	CLIPRDR_UNLOCK_CLIPBOARD_DATA unlockClipboardData = { .common = { .msgType = CB_UNLOCK_CLIPDATA,
		                                                              .msgFlags = header->msgFlags,
		                                                              .dataLen = header->dataLen },
		                                                  .clipDataId = 0 };

	UINT error = cliprdr_read_unlock_clipdata(s, &unlockClipboardData);
	if (error)
		return error;

	IFCALLRET(context->ClientUnlockClipboardData, error, context, &unlockClipboardData);

	if (error)
		WLog_Print(cliprdr->log, WLOG_ERROR,
		           "ClientUnlockClipboardData failed with error %" PRIu32 "!", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_receive_format_data_request(CliprdrServerContext* context, wStream* s,
                                                       const CLIPRDR_HEADER* header)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(header);

	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;
	WINPR_ASSERT(cliprdr);

	WLog_Print(cliprdr->log, WLOG_DEBUG, "CliprdrClientFormatDataRequest");

	CLIPRDR_FORMAT_DATA_REQUEST formatDataRequest = { .common = { .msgType = CB_FORMAT_DATA_REQUEST,
		                                                          .msgFlags = header->msgFlags,
		                                                          .dataLen = header->dataLen },
		                                              .requestedFormatId = 0 };

	UINT error = cliprdr_read_format_data_request(s, &formatDataRequest);
	if (error)
		return error;

	context->lastRequestedFormatId = formatDataRequest.requestedFormatId;
	IFCALLRET(context->ClientFormatDataRequest, error, context, &formatDataRequest);

	if (error)
		WLog_Print(cliprdr->log, WLOG_ERROR,
		           "ClientFormatDataRequest failed with error %" PRIu32 "!", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_receive_format_data_response(CliprdrServerContext* context, wStream* s,
                                                        const CLIPRDR_HEADER* header)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(header);

	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;
	WINPR_ASSERT(cliprdr);

	WLog_Print(cliprdr->log, WLOG_DEBUG, "CliprdrClientFormatDataResponse");

	CLIPRDR_FORMAT_DATA_RESPONSE formatDataResponse = { .common = { .msgType =
		                                                                CB_FORMAT_DATA_RESPONSE,
		                                                            .msgFlags = header->msgFlags,
		                                                            .dataLen = header->dataLen },
		                                                .requestedFormatData = nullptr };

	UINT error = cliprdr_read_format_data_response(s, &formatDataResponse);
	if (error)
		return error;

	IFCALLRET(context->ClientFormatDataResponse, error, context, &formatDataResponse);

	if (error)
		WLog_Print(cliprdr->log, WLOG_ERROR,
		           "ClientFormatDataResponse failed with error %" PRIu32 "!", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_receive_filecontents_request(CliprdrServerContext* context, wStream* s,
                                                        const CLIPRDR_HEADER* header)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(header);

	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;
	WINPR_ASSERT(cliprdr);

	WLog_Print(cliprdr->log, WLOG_DEBUG, "CliprdrClientFileContentsRequest");

	CLIPRDR_FILE_CONTENTS_REQUEST request = { .common = { .msgType = CB_FILECONTENTS_REQUEST,
		                                                  .msgFlags = header->msgFlags,
		                                                  .dataLen = header->dataLen },
		                                      .streamId = 0,
		                                      .listIndex = 0,
		                                      .dwFlags = 0,
		                                      .nPositionLow = 0,
		                                      .nPositionHigh = 0,
		                                      .cbRequested = 0,
		                                      .haveClipDataId = FALSE,
		                                      .clipDataId = 0 };

	UINT error = cliprdr_read_file_contents_request(s, &request);
	if (error)
		return error;

	if (!context->hasHugeFileSupport)
	{
		if (request.nPositionHigh > 0)
			return ERROR_INVALID_DATA;
		if ((UINT64)request.nPositionLow + request.cbRequested > UINT32_MAX)
			return ERROR_INVALID_DATA;
	}
	IFCALLRET(context->ClientFileContentsRequest, error, context, &request);

	if (error)
		WLog_Print(cliprdr->log, WLOG_ERROR,
		           "ClientFileContentsRequest failed with error %" PRIu32 "!", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_receive_filecontents_response(CliprdrServerContext* context, wStream* s,
                                                         const CLIPRDR_HEADER* header)
{
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(context);
	WINPR_ASSERT(header);

	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;
	WINPR_ASSERT(cliprdr);

	WLog_Print(cliprdr->log, WLOG_DEBUG, "CliprdrClientFileContentsResponse");

	CLIPRDR_FILE_CONTENTS_RESPONSE response = { .common = { .msgType = CB_FILECONTENTS_RESPONSE,
		                                                    .msgFlags = header->msgFlags,
		                                                    .dataLen = header->dataLen },
		                                        .streamId = 0,
		                                        .cbRequested = 0,
		                                        .requestedData = nullptr };

	if ((error = cliprdr_read_file_contents_response(s, &response)))
		return error;

	IFCALLRET(context->ClientFileContentsResponse, error, context, &response);

	if (error)
		WLog_Print(cliprdr->log, WLOG_ERROR,
		           "ClientFileContentsResponse failed with error %" PRIu32 "!", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_receive_pdu(CliprdrServerContext* context, wStream* s,
                                       const CLIPRDR_HEADER* header)
{
	UINT error = 0;

	WINPR_ASSERT(context);
	WINPR_ASSERT(header);

	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;
	WINPR_ASSERT(cliprdr);

	{
		char buffer1[64] = WINPR_C_ARRAY_INIT;
		char buffer2[64] = WINPR_C_ARRAY_INIT;
		WLog_Print(cliprdr->log, WLOG_DEBUG,
		           "CliprdrServerReceivePdu: msgType: %s, msgFlags: %s dataLen: %" PRIu32 "",
		           CB_MSG_TYPE_STRING(header->msgType, buffer1, sizeof(buffer1)),
		           CB_MSG_FLAGS_STRING(header->msgFlags, buffer2, sizeof(buffer2)),
		           header->dataLen);
	}

	const size_t start = Stream_GetPosition(s);
	switch (header->msgType)
	{
		case CB_CLIP_CAPS:
			error = cliprdr_server_receive_capabilities(context, s, header);
			break;

		case CB_TEMP_DIRECTORY:
			error = cliprdr_server_receive_temporary_directory(context, s, header);
			break;

		case CB_FORMAT_LIST:
			error = cliprdr_server_receive_format_list(context, s, header);
			break;

		case CB_FORMAT_LIST_RESPONSE:
			error = cliprdr_server_receive_format_list_response(context, s, header);
			break;

		case CB_LOCK_CLIPDATA:
			error = cliprdr_server_receive_lock_clipdata(context, s, header);
			break;

		case CB_UNLOCK_CLIPDATA:
			error = cliprdr_server_receive_unlock_clipdata(context, s, header);
			break;

		case CB_FORMAT_DATA_REQUEST:
			error = cliprdr_server_receive_format_data_request(context, s, header);
			break;

		case CB_FORMAT_DATA_RESPONSE:
			error = cliprdr_server_receive_format_data_response(context, s, header);
			break;

		case CB_FILECONTENTS_REQUEST:
			error = cliprdr_server_receive_filecontents_request(context, s, header);
			break;

		case CB_FILECONTENTS_RESPONSE:
			error = cliprdr_server_receive_filecontents_response(context, s, header);
			break;

		default:
			error = ERROR_INVALID_DATA;
			WLog_Print(cliprdr->log, WLOG_ERROR, "Unexpected clipboard PDU type: %" PRIu16 "",
			           header->msgType);
			break;
	}

	const size_t end = Stream_GetPosition(s);
	const size_t diff = end - start;
	if (diff != header->dataLen)
	{
		char buffer1[64] = WINPR_C_ARRAY_INIT;
		WLog_WARN(
		    TAG, "[%s] Mismatch: header told length is %" PRIu32 ", but actually read %" PRIuz,
		    CB_MSG_TYPE_STRING(header->msgType, buffer1, sizeof(buffer1)), header->dataLen, diff);
	}

	const size_t rem = Stream_GetRemainingLength(s);
	if (rem > 0)
	{
		char buffer1[64] = WINPR_C_ARRAY_INIT;
		char buffer2[64] = WINPR_C_ARRAY_INIT;
		WLog_WARN(
		    TAG, "msgType: %s, msgFlags: %s dataLen: %" PRIu32 ": %" PRIuz " bytes not parsed",
		    CB_MSG_TYPE_STRING(header->msgType, buffer1, sizeof(buffer1)),
		    CB_MSG_FLAGS_STRING(header->msgFlags, buffer2, sizeof(buffer2)), header->dataLen, rem);
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
	UINT32 generalFlags = 0;
	UINT error = 0;
	CLIPRDR_MONITOR_READY monitorReady = WINPR_C_ARRAY_INIT;

	WINPR_ASSERT(context);

	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;
	WINPR_ASSERT(cliprdr);

	monitorReady.common.msgType = CB_MONITOR_READY;

	if (context->useLongFormatNames)
		generalFlags |= CB_USE_LONG_FORMAT_NAMES;

	if (context->streamFileClipEnabled)
		generalFlags |= CB_STREAM_FILECLIP_ENABLED;

	if (context->fileClipNoFilePaths)
		generalFlags |= CB_FILECLIP_NO_FILE_PATHS;

	if (context->canLockClipData)
		generalFlags |= CB_CAN_LOCK_CLIPDATA;

	if (context->hasHugeFileSupport)
		generalFlags |= CB_HUGE_FILE_SUPPORT_ENABLED;

	CLIPRDR_GENERAL_CAPABILITY_SET generalCapabilitySet = { .capabilitySetType =
		                                                        CB_CAPSTYPE_GENERAL,
		                                                    .capabilitySetLength =
		                                                        CB_CAPSTYPE_GENERAL_LEN,
		                                                    .version = CB_CAPS_VERSION_2,
		                                                    .generalFlags = generalFlags };

	CLIPRDR_CAPABILITIES capabilities = { .common = { .msgType = CB_CLIP_CAPS,
		                                              .msgFlags = 0,
		                                              .dataLen = 4 + CB_CAPSTYPE_GENERAL_LEN },
		                                  .cCapabilitiesSets = 1,
		                                  .capabilitySets =
		                                      (CLIPRDR_CAPABILITY_SET*)&generalCapabilitySet };

	if ((error = context->ServerCapabilities(context, &capabilities)))
	{
		WLog_Print(cliprdr->log, WLOG_ERROR, "ServerCapabilities failed with error %" PRIu32 "!",
		           error);
		return error;
	}

	if ((error = context->MonitorReady(context, &monitorReady)))
	{
		WLog_Print(cliprdr->log, WLOG_ERROR, "MonitorReady failed with error %" PRIu32 "!", error);
		return error;
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_read(CliprdrServerContext* context)
{
	WINPR_ASSERT(context);

	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;
	WINPR_ASSERT(cliprdr);

	wStream* s = cliprdr->s;

	const size_t spos = Stream_GetPosition(s);
	WINPR_STATIC_ASSERT(CLIPRDR_HEADER_LENGTH == sizeof(CLIPRDR_HEADER));

	/* Read data.
	 * first the header, then the data expected from header supplied length */
	const BOOL noHeader = spos < sizeof(CLIPRDR_HEADER);
	if (noHeader || (spos < cliprdr->totalExpectedBytes))
	{
		DWORD BytesReturned = 0;
		const size_t BytesToRead =
		    (noHeader ? sizeof(CLIPRDR_HEADER) : cliprdr->totalExpectedBytes) - spos;
		const DWORD status = WaitForSingleObject(cliprdr->ChannelEvent, 0);

		if (status == WAIT_FAILED)
		{
			const UINT error = GetLastError();
			WLog_Print(cliprdr->log, WLOG_ERROR,
			           "WaitForSingleObject failed with error %" PRIu32 "", error);
			return error;
		}

		if (status == WAIT_TIMEOUT)
			return CHANNEL_RC_OK;

		if (!WTSVirtualChannelRead(cliprdr->ChannelHandle, 0, Stream_Pointer(s),
		                           WINPR_ASSERTING_INT_CAST(DWORD, BytesToRead), &BytesReturned))
		{
			WLog_Print(cliprdr->log, WLOG_ERROR, "WTSVirtualChannelRead failed!");
			return ERROR_INTERNAL_ERROR;
		}

		if (!Stream_SafeSeek(s, BytesReturned))
			return ERROR_INTERNAL_ERROR;
	}

	/* Abort early if the packet is not fully available */
	const size_t epos = Stream_GetPosition(s);
	if ((epos < sizeof(CLIPRDR_HEADER)) || (epos < cliprdr->totalExpectedBytes))
		return CHANNEL_RC_OK;

	// Got enough bytes to extract header, do so.
	if (cliprdr->totalExpectedBytes == 0)
	{
		const size_t position = Stream_GetPosition(s);
		Stream_ResetPosition(s);
		Stream_Read_UINT16(s, cliprdr->header.msgType);  /* msgType (2 bytes) */
		Stream_Read_UINT16(s, cliprdr->header.msgFlags); /* msgFlags (2 bytes) */
		Stream_Read_UINT32(s, cliprdr->header.dataLen);  /* dataLen (4 bytes) */

		if (!Stream_EnsureRemainingCapacity(s, cliprdr->header.dataLen))
		{
			WLog_Print(cliprdr->log, WLOG_ERROR, "Stream_EnsureCapacity failed!");
			return CHANNEL_RC_NO_MEMORY;
		}

		if (!Stream_SetPosition(s, position))
			return ERROR_INVALID_DATA;

		cliprdr->totalExpectedBytes = cliprdr->header.dataLen + CLIPRDR_HEADER_LENGTH;
	}
	/* Read completed, process the data */
	else
	{
		if (epos > cliprdr->totalExpectedBytes)
		{
			char buffer1[128] = WINPR_C_ARRAY_INIT;
			char buffer2[128] = WINPR_C_ARRAY_INIT;
			WLog_Print(cliprdr->log, WLOG_WARN,
			           "Received excess bytes for [%s|%s]: Expected %" PRIuz ", but got %" PRIuz,
			           CB_MSG_TYPE_STRING(cliprdr->header.msgType, buffer1, sizeof(buffer1)),
			           CB_MSG_FLAGS_STRING(cliprdr->header.msgFlags, buffer2, sizeof(buffer2)),
			           cliprdr->totalExpectedBytes, epos);
		}
		Stream_SealLength(s);
		if (!Stream_SetPosition(s, CLIPRDR_HEADER_LENGTH))
			return ERROR_INVALID_DATA;

		const UINT error = cliprdr_server_receive_pdu(context, s, &cliprdr->header);
		Stream_ResetPosition(s);
		if (!Stream_SetLength(s, Stream_Capacity(s)))
			return ERROR_INTERNAL_ERROR;

		cliprdr->totalExpectedBytes = 0;

		const CLIPRDR_HEADER empty = WINPR_C_ARRAY_INIT;
		cliprdr->header = empty;
		if (error)
		{
			char buffer1[128] = WINPR_C_ARRAY_INIT;
			char buffer2[128] = WINPR_C_ARRAY_INIT;
			WLog_Print(cliprdr->log, WLOG_ERROR,
			           "cliprdr_server_receive_pdu [%s|%s] failed with error code %" PRIu32 "!",
			           CB_MSG_TYPE_STRING(cliprdr->header.msgType, buffer1, sizeof(buffer1)),
			           CB_MSG_FLAGS_STRING(cliprdr->header.msgFlags, buffer2, sizeof(buffer2)),
			           error);
			return error;
		}
	}

	return CHANNEL_RC_OK;
}

static DWORD WINAPI cliprdr_server_thread(LPVOID arg)
{
	DWORD status = 0;
	DWORD nCount = 0;
	HANDLE events[MAXIMUM_WAIT_OBJECTS] = WINPR_C_ARRAY_INIT;
	CliprdrServerContext* context = (CliprdrServerContext*)arg;
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(context);

	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;
	WINPR_ASSERT(cliprdr);

	HANDLE ChannelEvent = context->GetEventHandle(context);

	events[nCount++] = cliprdr->StopEvent;
	events[nCount++] = ChannelEvent;

	if (context->autoInitializationSequence)
	{
		if ((error = cliprdr_server_init(context)))
		{
			WLog_Print(cliprdr->log, WLOG_ERROR,
			           "cliprdr_server_init failed with error %" PRIu32 "!", error);
			goto out;
		}
	}

	while (1)
	{
		status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_Print(cliprdr->log, WLOG_ERROR,
			           "WaitForMultipleObjects failed with error %" PRIu32 "", error);
			goto out;
		}

		status = WaitForSingleObject(cliprdr->StopEvent, 0);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_Print(cliprdr->log, WLOG_ERROR,
			           "WaitForSingleObject failed with error %" PRIu32 "", error);
			goto out;
		}

		if (status == WAIT_OBJECT_0)
			break;

		status = WaitForSingleObject(ChannelEvent, 0);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_Print(cliprdr->log, WLOG_ERROR,
			           "WaitForSingleObject failed with error %" PRIu32 "", error);
			goto out;
		}

		if (status == WAIT_OBJECT_0)
		{
			if ((error = context->CheckEventHandle(context)))
			{
				WLog_Print(cliprdr->log, WLOG_ERROR,
				           "CheckEventHandle failed with error %" PRIu32 "!", error);
				break;
			}
		}
	}

out:

	if (error && context->rdpcontext)
		setChannelError(context->rdpcontext, error, "cliprdr_server_thread reported an error");

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
	void* buffer = nullptr;
	DWORD BytesReturned = 0;

	WINPR_ASSERT(context);

	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;
	WINPR_ASSERT(cliprdr);

	cliprdr->ChannelHandle =
	    WTSVirtualChannelOpen(cliprdr->vcm, WTS_CURRENT_SESSION, CLIPRDR_SVC_CHANNEL_NAME);

	if (!cliprdr->ChannelHandle)
	{
		WLog_Print(cliprdr->log, WLOG_ERROR, "WTSVirtualChannelOpen for cliprdr failed!");
		return ERROR_INTERNAL_ERROR;
	}

	cliprdr->ChannelEvent = nullptr;

	if (WTSVirtualChannelQuery(cliprdr->ChannelHandle, WTSVirtualEventHandle, &buffer,
	                           &BytesReturned))
	{
		if (BytesReturned != sizeof(HANDLE))
		{
			WLog_Print(cliprdr->log, WLOG_ERROR, "BytesReturned has not size of HANDLE!");
			return ERROR_INTERNAL_ERROR;
		}

		cliprdr->ChannelEvent = *(HANDLE*)buffer;
		WTSFreeMemory(buffer);
	}

	if (!cliprdr->ChannelEvent)
	{
		WLog_Print(cliprdr->log, WLOG_ERROR, "WTSVirtualChannelQuery for cliprdr failed!");
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
	WINPR_ASSERT(context);

	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;
	WINPR_ASSERT(cliprdr);

	if (cliprdr->ChannelHandle)
	{
		(void)WTSVirtualChannelClose(cliprdr->ChannelHandle);
		cliprdr->ChannelHandle = nullptr;
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
	UINT error = 0;

	WINPR_ASSERT(context);

	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;
	WINPR_ASSERT(cliprdr);

	if (!cliprdr->ChannelHandle)
	{
		error = context->Open(context);
		if (error)
		{
			WLog_Print(cliprdr->log, WLOG_ERROR, "Open failed!");
			return error;
		}
	}

	cliprdr->StopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	if (!cliprdr->StopEvent)
	{
		WLog_Print(cliprdr->log, WLOG_ERROR, "CreateEvent failed!");
		return ERROR_INTERNAL_ERROR;
	}

	cliprdr->Thread = CreateThread(nullptr, 0, cliprdr_server_thread, (void*)context, 0, nullptr);
	if (!cliprdr->Thread)
	{
		WLog_Print(cliprdr->log, WLOG_ERROR, "CreateThread failed!");
		(void)CloseHandle(cliprdr->StopEvent);
		cliprdr->StopEvent = nullptr;
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

	WINPR_ASSERT(context);

	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;
	WINPR_ASSERT(cliprdr);

	if (cliprdr->StopEvent)
	{
		(void)SetEvent(cliprdr->StopEvent);

		if (WaitForSingleObject(cliprdr->Thread, INFINITE) == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_Print(cliprdr->log, WLOG_ERROR,
			           "WaitForSingleObject failed with error %" PRIu32 "", error);
			return error;
		}

		(void)CloseHandle(cliprdr->Thread);
		(void)CloseHandle(cliprdr->StopEvent);
	}

	if (cliprdr->ChannelHandle)
		return context->Close(context);

	return error;
}

static HANDLE cliprdr_server_get_event_handle(CliprdrServerContext* context)
{
	WINPR_ASSERT(context);

	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;
	WINPR_ASSERT(cliprdr);
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
	CliprdrServerContext* context = (CliprdrServerContext*)calloc(1, sizeof(CliprdrServerContext));

	if (!context)
		goto fail;

	context->autoInitializationSequence = TRUE;
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

	CliprdrServerPrivate* cliprdr = context->handle =
	    (CliprdrServerPrivate*)calloc(1, sizeof(CliprdrServerPrivate));
	if (!cliprdr)
		goto fail;

	cliprdr->log = WLog_Get(TAG);
	cliprdr->vcm = vcm;
	cliprdr->s = Stream_New(nullptr, 4096);

	if (!cliprdr->s)
		goto fail;

	return context;

fail:
	cliprdr_server_context_free(context);
	return nullptr;
}

void cliprdr_server_context_free(CliprdrServerContext* context)
{
	if (!context)
		return;

	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;

	if (cliprdr)
		Stream_Free(cliprdr->s, TRUE);

	free(context->handle);
	free(context);
}
