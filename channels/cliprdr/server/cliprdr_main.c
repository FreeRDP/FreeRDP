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
	UINT rc;
	size_t pos, size;
	BOOL status;
	UINT32 dataLen;
	UINT32 written;
	pos = Stream_GetPosition(s);
	if ((pos < 8) || (pos > UINT32_MAX))
	{
		rc = ERROR_NO_DATA;
		goto fail;
	}

	dataLen = (UINT32)(pos - 8);
	Stream_SetPosition(s, 4);
	Stream_Write_UINT32(s, dataLen);
	Stream_SetPosition(s, pos);
	size = Stream_Length(s);
	if (size > UINT32_MAX)
	{
		rc = ERROR_INVALID_DATA;
		goto fail;
	}

	status = WTSVirtualChannelWrite(cliprdr->ChannelHandle, (PCHAR)Stream_Buffer(s), (UINT32)size,
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
	UINT32 x;
	wStream* s;
	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;

	if (capabilities->msgType != CB_CLIP_CAPS)
		WLog_WARN(TAG, "[%s] called with invalid type %08" PRIx32, __FUNCTION__,
		          capabilities->msgType);

	if (capabilities->cCapabilitiesSets > UINT16_MAX)
	{
		WLog_ERR(TAG, "Invalid number of capability sets in clipboard caps");
		return ERROR_INVALID_PARAMETER;
	}

	s = cliprdr_packet_new(CB_CLIP_CAPS, 0, 4 + CB_CAPSTYPE_GENERAL_LEN);

	if (!s)
	{
		WLog_ERR(TAG, "cliprdr_packet_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	Stream_Write_UINT16(s,
	                    (UINT16)capabilities->cCapabilitiesSets); /* cCapabilitiesSets (2 bytes) */
	Stream_Write_UINT16(s, 0);                                    /* pad1 (2 bytes) */
	for (x = 0; x < capabilities->cCapabilitiesSets; x++)
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
				WLog_WARN(TAG, "Unknown capability set type %08" PRIx16, cap->capabilitySetType);
				if (!Stream_SafeSeek(s, cap->capabilitySetLength))
				{
					WLog_ERR(TAG, "%s: short stream", __FUNCTION__);
					return ERROR_NO_DATA;
				}
				break;
		}
	}
	WLog_DBG(TAG, "ServerCapabilities");
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
	wStream* s;
	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;

	if (monitorReady->msgType != CB_MONITOR_READY)
		WLog_WARN(TAG, "[%s] called with invalid type %08" PRIx32, __FUNCTION__,
		          monitorReady->msgType);

	s = cliprdr_packet_new(CB_MONITOR_READY, monitorReady->msgFlags, monitorReady->dataLen);

	if (!s)
	{
		WLog_ERR(TAG, "cliprdr_packet_new failed!");
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
                                       const CLIPRDR_FORMAT_LIST* formatList)
{
	wStream* s;
	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;

	s = cliprdr_packet_format_list_new(formatList, context->useLongFormatNames);
	if (!s)
	{
		WLog_ERR(TAG, "cliprdr_packet_format_list_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	WLog_DBG(TAG, "ServerFormatList: numFormats: %" PRIu32 "", formatList->numFormats);
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
	wStream* s;
	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;
	if (formatListResponse->msgType != CB_FORMAT_LIST_RESPONSE)
		WLog_WARN(TAG, "[%s] called with invalid type %08" PRIx32, __FUNCTION__,
		          formatListResponse->msgType);

	s = cliprdr_packet_new(CB_FORMAT_LIST_RESPONSE, formatListResponse->msgFlags,
	                       formatListResponse->dataLen);

	if (!s)
	{
		WLog_ERR(TAG, "cliprdr_packet_new failed!");
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
                                               const CLIPRDR_LOCK_CLIPBOARD_DATA* lockClipboardData)
{
	wStream* s;
	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;
	if (lockClipboardData->msgType != CB_LOCK_CLIPDATA)
		WLog_WARN(TAG, "[%s] called with invalid type %08" PRIx32, __FUNCTION__,
		          lockClipboardData->msgType);

	s = cliprdr_packet_lock_clipdata_new(lockClipboardData);
	if (!s)
	{
		WLog_ERR(TAG, "cliprdr_packet_lock_clipdata_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	WLog_DBG(TAG, "ServerLockClipboardData: clipDataId: 0x%08" PRIX32 "",
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
	wStream* s;
	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;
	if (unlockClipboardData->msgType != CB_UNLOCK_CLIPDATA)
		WLog_WARN(TAG, "[%s] called with invalid type %08" PRIx32, __FUNCTION__,
		          unlockClipboardData->msgType);

	s = cliprdr_packet_unlock_clipdata_new(unlockClipboardData);

	if (!s)
	{
		WLog_ERR(TAG, "cliprdr_packet_unlock_clipdata_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	WLog_DBG(TAG, "ServerUnlockClipboardData: clipDataId: 0x%08" PRIX32 "",
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
	wStream* s;
	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;
	if (formatDataRequest->msgType != CB_FORMAT_DATA_REQUEST)
		WLog_WARN(TAG, "[%s] called with invalid type %08" PRIx32, __FUNCTION__,
		          formatDataRequest->msgType);

	s = cliprdr_packet_new(CB_FORMAT_DATA_REQUEST, formatDataRequest->msgFlags,
	                       formatDataRequest->dataLen);

	if (!s)
	{
		WLog_ERR(TAG, "cliprdr_packet_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	Stream_Write_UINT32(s, formatDataRequest->requestedFormatId); /* requestedFormatId (4 bytes) */
	WLog_DBG(TAG, "ClientFormatDataRequest");
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
	wStream* s;
	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;

	if (formatDataResponse->msgType != CB_FORMAT_DATA_RESPONSE)
		WLog_WARN(TAG, "[%s] called with invalid type %08" PRIx32, __FUNCTION__,
		          formatDataResponse->msgType);

	s = cliprdr_packet_new(CB_FORMAT_DATA_RESPONSE, formatDataResponse->msgFlags,
	                       formatDataResponse->dataLen);

	if (!s)
	{
		WLog_ERR(TAG, "cliprdr_packet_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	Stream_Write(s, formatDataResponse->requestedFormatData, formatDataResponse->dataLen);
	WLog_DBG(TAG, "ServerFormatDataResponse");
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
	wStream* s;
	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;

	if (fileContentsRequest->msgType != CB_FILECONTENTS_REQUEST)
		WLog_WARN(TAG, "[%s] called with invalid type %08" PRIx32, __FUNCTION__,
		          fileContentsRequest->msgType);

	s = cliprdr_packet_file_contents_request_new(fileContentsRequest);
	if (!s)
	{
		WLog_ERR(TAG, "cliprdr_packet_file_contents_request_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	WLog_DBG(TAG, "ServerFileContentsRequest: streamId: 0x%08" PRIX32 "",
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
	wStream* s;
	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;

	if (fileContentsResponse->msgType != CB_FILECONTENTS_RESPONSE)
		WLog_WARN(TAG, "[%s] called with invalid type %08" PRIx32, __FUNCTION__,
		          fileContentsResponse->msgType);

	s = cliprdr_packet_file_contents_response_new(fileContentsResponse);
	if (!s)
	{
		WLog_ERR(TAG, "cliprdr_packet_file_contents_response_new failed!");
		return ERROR_INTERNAL_ERROR;
	}

	WLog_DBG(TAG, "ServerFileContentsResponse: streamId: 0x%08" PRIX32 "",
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
	Stream_Read_UINT32(s, cap_set->version);      /* version (4 bytes) */
	Stream_Read_UINT32(s, cap_set->generalFlags); /* generalFlags (4 bytes) */

	if (context->useLongFormatNames)
		context->useLongFormatNames =
		    (cap_set->generalFlags & CB_USE_LONG_FORMAT_NAMES) ? TRUE : FALSE;

	if (context->streamFileClipEnabled)
		context->streamFileClipEnabled =
		    (cap_set->generalFlags & CB_STREAM_FILECLIP_ENABLED) ? TRUE : FALSE;

	if (context->fileClipNoFilePaths)
		context->fileClipNoFilePaths =
		    (cap_set->generalFlags & CB_FILECLIP_NO_FILE_PATHS) ? TRUE : FALSE;

	if (context->canLockClipData)
		context->canLockClipData = (cap_set->generalFlags & CB_CAN_LOCK_CLIPDATA) ? TRUE : FALSE;

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_server_receive_capabilities(CliprdrServerContext* context, wStream* s,
                                                const CLIPRDR_HEADER* header)
{
	UINT16 index;
	UINT16 capabilitySetType;
	UINT16 capabilitySetLength;
	UINT error = CHANNEL_RC_OK;
	size_t cap_sets_size = 0;
	CLIPRDR_CAPABILITIES capabilities;
	CLIPRDR_CAPABILITY_SET* capSet;
	void* tmp;

	WINPR_UNUSED(header);

	/* set `capabilitySets` to NULL so `realloc` will know to alloc the first block */
	capabilities.capabilitySets = NULL;

	WLog_DBG(TAG, "CliprdrClientCapabilities");
	Stream_Read_UINT16(s, capabilities.cCapabilitiesSets); /* cCapabilitiesSets (2 bytes) */
	Stream_Seek_UINT16(s);                                 /* pad1 (2 bytes) */

	for (index = 0; index < capabilities.cCapabilitiesSets; index++)
	{
		Stream_Read_UINT16(s, capabilitySetType);   /* capabilitySetType (2 bytes) */
		Stream_Read_UINT16(s, capabilitySetLength); /* capabilitySetLength (2 bytes) */

		cap_sets_size += capabilitySetLength;

		tmp = realloc(capabilities.capabilitySets, cap_sets_size);
		if (tmp == NULL)
		{
			WLog_ERR(TAG, "capabilities.capabilitySets realloc failed!");
			free(capabilities.capabilitySets);
			return CHANNEL_RC_NO_MEMORY;
		}

		capabilities.capabilitySets = (CLIPRDR_CAPABILITY_SET*)tmp;

		capSet = &(capabilities.capabilitySets[index]);

		capSet->capabilitySetType = capabilitySetType;
		capSet->capabilitySetLength = capabilitySetLength;

		switch (capSet->capabilitySetType)
		{
			case CB_CAPSTYPE_GENERAL:
				if ((error = cliprdr_server_receive_general_capability(
				         context, s, (CLIPRDR_GENERAL_CAPABILITY_SET*)capSet)))
				{
					WLog_ERR(TAG,
					         "cliprdr_server_receive_general_capability failed with error %" PRIu32
					         "",
					         error);
					goto out;
				}
				break;

			default:
				WLog_ERR(TAG, "unknown cliprdr capability set: %" PRIu16 "",
				         capSet->capabilitySetType);
				error = ERROR_INVALID_DATA;
				goto out;
		}
	}

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
	size_t length;
	WCHAR* wszTempDir;
	CLIPRDR_TEMP_DIRECTORY tempDirectory;
	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;
	size_t slength;
	UINT error = CHANNEL_RC_OK;

	WINPR_UNUSED(header);
	if ((slength = Stream_GetRemainingLength(s)) < 520)
	{
		WLog_ERR(TAG, "Stream_GetRemainingLength returned %" PRIuz " but should at least be 520",
		         slength);
		return CHANNEL_RC_NO_MEMORY;
	}

	wszTempDir = (WCHAR*)Stream_Pointer(s);

	if (wszTempDir[260] != 0)
	{
		WLog_ERR(TAG, "wszTempDir[260] was not 0");
		return ERROR_INVALID_DATA;
	}

	free(cliprdr->temporaryDirectory);
	cliprdr->temporaryDirectory = NULL;

	if (ConvertFromUnicode(CP_UTF8, 0, wszTempDir, -1, &(cliprdr->temporaryDirectory), 0, NULL,
	                       NULL) < 1)
	{
		WLog_ERR(TAG, "failed to convert temporary directory name");
		return ERROR_INVALID_DATA;
	}

	length = strnlen(cliprdr->temporaryDirectory, 520);

	if (length > 519)
		length = 519;

	CopyMemory(tempDirectory.szTempDir, cliprdr->temporaryDirectory, length);
	tempDirectory.szTempDir[length] = '\0';
	WLog_DBG(TAG, "CliprdrTemporaryDirectory: %s", cliprdr->temporaryDirectory);
	IFCALLRET(context->TempDirectory, error, context, &tempDirectory);

	if (error)
		WLog_ERR(TAG, "TempDirectory failed with error %" PRIu32 "!", error);

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
	CLIPRDR_FORMAT_LIST formatList;
	UINT error = CHANNEL_RC_OK;

	formatList.msgType = CB_FORMAT_LIST;
	formatList.msgFlags = header->msgFlags;
	formatList.dataLen = header->dataLen;

	if ((error = cliprdr_read_format_list(s, &formatList, context->useLongFormatNames)))
		goto out;

	WLog_DBG(TAG, "ClientFormatList: numFormats: %" PRIu32 "", formatList.numFormats);
	IFCALLRET(context->ClientFormatList, error, context, &formatList);

	if (error)
		WLog_ERR(TAG, "ClientFormatList failed with error %" PRIu32 "!", error);

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
	CLIPRDR_FORMAT_LIST_RESPONSE formatListResponse;
	UINT error = CHANNEL_RC_OK;

	WINPR_UNUSED(s);
	WLog_DBG(TAG, "CliprdrClientFormatListResponse");
	formatListResponse.msgType = CB_FORMAT_LIST_RESPONSE;
	formatListResponse.msgFlags = header->msgFlags;
	formatListResponse.dataLen = header->dataLen;
	IFCALLRET(context->ClientFormatListResponse, error, context, &formatListResponse);

	if (error)
		WLog_ERR(TAG, "ClientFormatListResponse failed with error %" PRIu32 "!", error);

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
		WLog_ERR(TAG, "ClientLockClipboardData failed with error %" PRIu32 "!", error);

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
	CLIPRDR_UNLOCK_CLIPBOARD_DATA unlockClipboardData;
	UINT error = CHANNEL_RC_OK;
	WLog_DBG(TAG, "CliprdrClientUnlockClipData");

	unlockClipboardData.msgType = CB_UNLOCK_CLIPDATA;
	unlockClipboardData.msgFlags = header->msgFlags;
	unlockClipboardData.dataLen = header->dataLen;

	if ((error = cliprdr_read_unlock_clipdata(s, &unlockClipboardData)))
		return error;

	IFCALLRET(context->ClientUnlockClipboardData, error, context, &unlockClipboardData);

	if (error)
		WLog_ERR(TAG, "ClientUnlockClipboardData failed with error %" PRIu32 "!", error);

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
	CLIPRDR_FORMAT_DATA_REQUEST formatDataRequest;
	UINT error = CHANNEL_RC_OK;
	WLog_DBG(TAG, "CliprdrClientFormatDataRequest");
	formatDataRequest.msgType = CB_FORMAT_DATA_REQUEST;
	formatDataRequest.msgFlags = header->msgFlags;
	formatDataRequest.dataLen = header->dataLen;

	if ((error = cliprdr_read_format_data_request(s, &formatDataRequest)))
		return error;

	context->lastRequestedFormatId = formatDataRequest.requestedFormatId;
	IFCALLRET(context->ClientFormatDataRequest, error, context, &formatDataRequest);

	if (error)
		WLog_ERR(TAG, "ClientFormatDataRequest failed with error %" PRIu32 "!", error);

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
	CLIPRDR_FORMAT_DATA_RESPONSE formatDataResponse;
	UINT error = CHANNEL_RC_OK;

	WLog_DBG(TAG, "CliprdrClientFormatDataResponse");
	formatDataResponse.msgType = CB_FORMAT_DATA_RESPONSE;
	formatDataResponse.msgFlags = header->msgFlags;
	formatDataResponse.dataLen = header->dataLen;

	if ((error = cliprdr_read_format_data_response(s, &formatDataResponse)))
		return error;

	IFCALLRET(context->ClientFormatDataResponse, error, context, &formatDataResponse);

	if (error)
		WLog_ERR(TAG, "ClientFormatDataResponse failed with error %" PRIu32 "!", error);

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
	CLIPRDR_FILE_CONTENTS_REQUEST request;
	UINT error = CHANNEL_RC_OK;
	WLog_DBG(TAG, "CliprdrClientFileContentsRequest");
	request.msgType = CB_FILECONTENTS_REQUEST;
	request.msgFlags = header->msgFlags;
	request.dataLen = header->dataLen;

	if ((error = cliprdr_read_file_contents_request(s, &request)))
		return error;

	IFCALLRET(context->ClientFileContentsRequest, error, context, &request);

	if (error)
		WLog_ERR(TAG, "ClientFileContentsRequest failed with error %" PRIu32 "!", error);

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
	CLIPRDR_FILE_CONTENTS_RESPONSE response;
	UINT error = CHANNEL_RC_OK;
	WLog_DBG(TAG, "CliprdrClientFileContentsResponse");

	response.msgType = CB_FILECONTENTS_RESPONSE;
	response.msgFlags = header->msgFlags;
	response.dataLen = header->dataLen;

	if ((error = cliprdr_read_file_contents_response(s, &response)))
		return error;

	IFCALLRET(context->ClientFileContentsResponse, error, context, &response);

	if (error)
		WLog_ERR(TAG, "ClientFileContentsResponse failed with error %" PRIu32 "!", error);

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
	UINT error;
	WLog_DBG(TAG,
	         "CliprdrServerReceivePdu: msgType: %" PRIu16 " msgFlags: 0x%04" PRIX16
	         " dataLen: %" PRIu32 "",
	         header->msgType, header->msgFlags, header->dataLen);

	switch (header->msgType)
	{
		case CB_CLIP_CAPS:
			if ((error = cliprdr_server_receive_capabilities(context, s, header)))
				WLog_ERR(TAG, "cliprdr_server_receive_capabilities failed with error %" PRIu32 "!",
				         error);

			break;

		case CB_TEMP_DIRECTORY:
			if ((error = cliprdr_server_receive_temporary_directory(context, s, header)))
				WLog_ERR(TAG,
				         "cliprdr_server_receive_temporary_directory failed with error %" PRIu32
				         "!",
				         error);

			break;

		case CB_FORMAT_LIST:
			if ((error = cliprdr_server_receive_format_list(context, s, header)))
				WLog_ERR(TAG, "cliprdr_server_receive_format_list failed with error %" PRIu32 "!",
				         error);

			break;

		case CB_FORMAT_LIST_RESPONSE:
			if ((error = cliprdr_server_receive_format_list_response(context, s, header)))
				WLog_ERR(TAG,
				         "cliprdr_server_receive_format_list_response failed with error %" PRIu32
				         "!",
				         error);

			break;

		case CB_LOCK_CLIPDATA:
			if ((error = cliprdr_server_receive_lock_clipdata(context, s, header)))
				WLog_ERR(TAG, "cliprdr_server_receive_lock_clipdata failed with error %" PRIu32 "!",
				         error);

			break;

		case CB_UNLOCK_CLIPDATA:
			if ((error = cliprdr_server_receive_unlock_clipdata(context, s, header)))
				WLog_ERR(TAG,
				         "cliprdr_server_receive_unlock_clipdata failed with error %" PRIu32 "!",
				         error);

			break;

		case CB_FORMAT_DATA_REQUEST:
			if ((error = cliprdr_server_receive_format_data_request(context, s, header)))
				WLog_ERR(TAG,
				         "cliprdr_server_receive_format_data_request failed with error %" PRIu32
				         "!",
				         error);

			break;

		case CB_FORMAT_DATA_RESPONSE:
			if ((error = cliprdr_server_receive_format_data_response(context, s, header)))
				WLog_ERR(TAG,
				         "cliprdr_server_receive_format_data_response failed with error %" PRIu32
				         "!",
				         error);

			break;

		case CB_FILECONTENTS_REQUEST:
			if ((error = cliprdr_server_receive_filecontents_request(context, s, header)))
				WLog_ERR(TAG,
				         "cliprdr_server_receive_filecontents_request failed with error %" PRIu32
				         "!",
				         error);

			break;

		case CB_FILECONTENTS_RESPONSE:
			if ((error = cliprdr_server_receive_filecontents_response(context, s, header)))
				WLog_ERR(TAG,
				         "cliprdr_server_receive_filecontents_response failed with error %" PRIu32
				         "!",
				         error);

			break;

		default:
			error = ERROR_INVALID_DATA;
			WLog_ERR(TAG, "Unexpected clipboard PDU type: %" PRIu16 "", header->msgType);
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
	CLIPRDR_GENERAL_CAPABILITY_SET generalCapabilitySet;
	UINT error;
	CLIPRDR_MONITOR_READY monitorReady = { 0 };
	CLIPRDR_CAPABILITIES capabilities = { 0 };

	generalFlags = 0;
	monitorReady.msgType = CB_MONITOR_READY;
	capabilities.msgType = CB_CLIP_CAPS;

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
	capabilities.capabilitySets = (CLIPRDR_CAPABILITY_SET*)&generalCapabilitySet;
	generalCapabilitySet.capabilitySetType = CB_CAPSTYPE_GENERAL;
	generalCapabilitySet.capabilitySetLength = CB_CAPSTYPE_GENERAL_LEN;
	generalCapabilitySet.version = CB_CAPS_VERSION_2;
	generalCapabilitySet.generalFlags = generalFlags;

	if ((error = context->ServerCapabilities(context, &capabilities)))
	{
		WLog_ERR(TAG, "ServerCapabilities failed with error %" PRIu32 "!", error);
		return error;
	}

	if ((error = context->MonitorReady(context, &monitorReady)))
	{
		WLog_ERR(TAG, "MonitorReady failed with error %" PRIu32 "!", error);
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
	wStream* s;
	size_t position;
	DWORD BytesToRead;
	DWORD BytesReturned;
	CLIPRDR_HEADER header;
	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;
	UINT error;
	DWORD status;
	s = cliprdr->s;

	if (Stream_GetPosition(s) < CLIPRDR_HEADER_LENGTH)
	{
		BytesReturned = 0;
		BytesToRead = (UINT32)(CLIPRDR_HEADER_LENGTH - Stream_GetPosition(s));
		status = WaitForSingleObject(cliprdr->ChannelEvent, 0);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %" PRIu32 "", error);
			return error;
		}

		if (status == WAIT_TIMEOUT)
			return CHANNEL_RC_OK;

		if (!WTSVirtualChannelRead(cliprdr->ChannelHandle, 0, (PCHAR)Stream_Pointer(s), BytesToRead,
		                           &BytesReturned))
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
		Stream_Read_UINT16(s, header.msgType);  /* msgType (2 bytes) */
		Stream_Read_UINT16(s, header.msgFlags); /* msgFlags (2 bytes) */
		Stream_Read_UINT32(s, header.dataLen);  /* dataLen (4 bytes) */

		if (!Stream_EnsureCapacity(s, (header.dataLen + CLIPRDR_HEADER_LENGTH)))
		{
			WLog_ERR(TAG, "Stream_EnsureCapacity failed!");
			return CHANNEL_RC_NO_MEMORY;
		}

		Stream_SetPosition(s, position);

		if (Stream_GetPosition(s) < (header.dataLen + CLIPRDR_HEADER_LENGTH))
		{
			BytesReturned = 0;
			BytesToRead =
			    (UINT32)((header.dataLen + CLIPRDR_HEADER_LENGTH) - Stream_GetPosition(s));
			status = WaitForSingleObject(cliprdr->ChannelEvent, 0);

			if (status == WAIT_FAILED)
			{
				error = GetLastError();
				WLog_ERR(TAG, "WaitForSingleObject failed with error %" PRIu32 "", error);
				return error;
			}

			if (status == WAIT_TIMEOUT)
				return CHANNEL_RC_OK;

			if (!WTSVirtualChannelRead(cliprdr->ChannelHandle, 0, (PCHAR)Stream_Pointer(s),
			                           BytesToRead, &BytesReturned))
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
				WLog_ERR(TAG, "cliprdr_server_receive_pdu failed with error code %" PRIu32 "!",
				         error);
				return error;
			}

			Stream_SetPosition(s, 0);
			/* check for trailing zero bytes */
			status = WaitForSingleObject(cliprdr->ChannelEvent, 0);

			if (status == WAIT_FAILED)
			{
				error = GetLastError();
				WLog_ERR(TAG, "WaitForSingleObject failed with error %" PRIu32 "", error);
				return error;
			}

			if (status == WAIT_TIMEOUT)
				return CHANNEL_RC_OK;

			BytesReturned = 0;
			BytesToRead = 4;

			if (!WTSVirtualChannelRead(cliprdr->ChannelHandle, 0, (PCHAR)Stream_Pointer(s),
			                           BytesToRead, &BytesReturned))
			{
				WLog_ERR(TAG, "WTSVirtualChannelRead failed!");
				return ERROR_INTERNAL_ERROR;
			}

			if (BytesReturned == 4)
			{
				Stream_Read_UINT16(s, header.msgType);  /* msgType (2 bytes) */
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
	CliprdrServerContext* context = (CliprdrServerContext*)arg;
	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;
	UINT error = CHANNEL_RC_OK;

	ChannelEvent = context->GetEventHandle(context);
	nCount = 0;
	events[nCount++] = cliprdr->StopEvent;
	events[nCount++] = ChannelEvent;

	if (context->autoInitializationSequence)
	{
		if ((error = cliprdr_server_init(context)))
		{
			WLog_ERR(TAG, "cliprdr_server_init failed with error %" PRIu32 "!", error);
			goto out;
		}
	}

	while (1)
	{
		status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForMultipleObjects failed with error %" PRIu32 "", error);
			goto out;
		}

		status = WaitForSingleObject(cliprdr->StopEvent, 0);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %" PRIu32 "", error);
			goto out;
		}

		if (status == WAIT_OBJECT_0)
			break;

		status = WaitForSingleObject(ChannelEvent, 0);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %" PRIu32 "", error);
			goto out;
		}

		if (status == WAIT_OBJECT_0)
		{
			if ((error = context->CheckEventHandle(context)))
			{
				WLog_ERR(TAG, "CheckEventHandle failed with error %" PRIu32 "!", error);
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
	void* buffer = NULL;
	DWORD BytesReturned = 0;
	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;
	cliprdr->ChannelHandle = WTSVirtualChannelOpen(cliprdr->vcm, WTS_CURRENT_SESSION, "cliprdr");

	if (!cliprdr->ChannelHandle)
	{
		WLog_ERR(TAG, "WTSVirtualChannelOpen for cliprdr failed!");
		return ERROR_INTERNAL_ERROR;
	}

	cliprdr->ChannelEvent = NULL;

	if (WTSVirtualChannelQuery(cliprdr->ChannelHandle, WTSVirtualEventHandle, &buffer,
	                           &BytesReturned))
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
	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;

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
	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;
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

	if (!(cliprdr->Thread = CreateThread(NULL, 0, cliprdr_server_thread, (void*)context, 0, NULL)))
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
	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;

	if (cliprdr->StopEvent)
	{
		SetEvent(cliprdr->StopEvent);

		if (WaitForSingleObject(cliprdr->Thread, INFINITE) == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %" PRIu32 "", error);
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
	CliprdrServerPrivate* cliprdr = (CliprdrServerPrivate*)context->handle;
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
	context = (CliprdrServerContext*)calloc(1, sizeof(CliprdrServerContext));

	if (context)
	{
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
		cliprdr = context->handle = (CliprdrServerPrivate*)calloc(1, sizeof(CliprdrServerPrivate));

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

	cliprdr = (CliprdrServerPrivate*)context->handle;

	if (cliprdr)
	{
		Stream_Free(cliprdr->s, TRUE);
		free(cliprdr->temporaryDirectory);
	}

	free(context->handle);
	free(context);
}
