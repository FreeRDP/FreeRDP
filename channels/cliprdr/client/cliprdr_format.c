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

#include <winpr/crt.h>
#include <winpr/print.h>

#include <freerdp/types.h>
#include <freerdp/freerdp.h>
#include <freerdp/settings.h>
#include <freerdp/constants.h>
#include <freerdp/client/cliprdr.h>

#include "cliprdr_main.h"
#include "cliprdr_format.h"
#include "../cliprdr_common.h"

CLIPRDR_FORMAT_LIST cliprdr_filter_format_list(const CLIPRDR_FORMAT_LIST* list, const UINT32 mask,
                                               const UINT32 checkMask)
{
	const UINT32 maskData =
	    checkMask & (CLIPRDR_FLAG_LOCAL_TO_REMOTE | CLIPRDR_FLAG_REMOTE_TO_LOCAL);
	const UINT32 maskFiles =
	    checkMask & (CLIPRDR_FLAG_LOCAL_TO_REMOTE_FILES | CLIPRDR_FLAG_REMOTE_TO_LOCAL_FILES);
	WINPR_ASSERT(list);

	CLIPRDR_FORMAT_LIST filtered = { 0 };
	filtered.common.msgType = CB_FORMAT_LIST;
	filtered.numFormats = list->numFormats;
	filtered.formats = calloc(filtered.numFormats, sizeof(CLIPRDR_FORMAT));

	size_t wpos = 0;
	if ((mask & checkMask) == checkMask)
	{
		for (size_t x = 0; x < list->numFormats; x++)
		{
			const CLIPRDR_FORMAT* format = &list->formats[x];
			CLIPRDR_FORMAT* cur = &filtered.formats[x];
			cur->formatId = format->formatId;
			if (format->formatName)
				cur->formatName = _strdup(format->formatName);
			wpos++;
		}
	}
	else if ((mask & maskFiles) != 0)
	{
		for (size_t x = 0; x < list->numFormats; x++)
		{
			const CLIPRDR_FORMAT* format = &list->formats[x];
			CLIPRDR_FORMAT* cur = &filtered.formats[wpos];

			if (!format->formatName)
				continue;
			if (strcmp(format->formatName, type_FileGroupDescriptorW) == 0 ||
			    strcmp(format->formatName, type_FileContents) == 0)
			{
				cur->formatId = format->formatId;
				cur->formatName = _strdup(format->formatName);
				wpos++;
			}
		}
	}
	else if ((mask & maskData) != 0)
	{
		for (size_t x = 0; x < list->numFormats; x++)
		{
			const CLIPRDR_FORMAT* format = &list->formats[x];
			CLIPRDR_FORMAT* cur = &filtered.formats[wpos];

			if (!format->formatName ||
			    (strcmp(format->formatName, type_FileGroupDescriptorW) != 0 &&
			     strcmp(format->formatName, type_FileContents) != 0))
			{
				cur->formatId = format->formatId;
				if (format->formatName)
					cur->formatName = _strdup(format->formatName);
				wpos++;
			}
		}
	}
	filtered.numFormats = wpos;
	return filtered;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT cliprdr_process_format_list(cliprdrPlugin* cliprdr, wStream* s, UINT32 dataLen,
                                 UINT16 msgFlags)
{
	CLIPRDR_FORMAT_LIST formatList = { 0 };
	CLIPRDR_FORMAT_LIST filteredFormatList = { 0 };
	CliprdrClientContext* context = cliprdr_get_client_interface(cliprdr);
	UINT error = CHANNEL_RC_OK;

	formatList.common.msgType = CB_FORMAT_LIST;
	formatList.common.msgFlags = msgFlags;
	formatList.common.dataLen = dataLen;

	if ((error = cliprdr_read_format_list(s, &formatList, cliprdr->useLongFormatNames)))
		goto error_out;

	const UINT32 mask =
	    freerdp_settings_get_uint32(context->rdpcontext->settings, FreeRDP_ClipboardFeatureMask);
	filteredFormatList = cliprdr_filter_format_list(
	    &formatList, mask, CLIPRDR_FLAG_REMOTE_TO_LOCAL | CLIPRDR_FLAG_REMOTE_TO_LOCAL_FILES);
	if (filteredFormatList.numFormats == 0)
		goto error_out;

	WLog_Print(cliprdr->log, WLOG_DEBUG, "ServerFormatList: numFormats: %" PRIu32 "",
	           filteredFormatList.numFormats);

	if (context->ServerFormatList)
	{
		if ((error = context->ServerFormatList(context, &filteredFormatList)))
			WLog_ERR(TAG, "ServerFormatList failed with error %" PRIu32 "", error);
	}

error_out:
	cliprdr_free_format_list(&filteredFormatList);
	cliprdr_free_format_list(&formatList);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT cliprdr_process_format_list_response(cliprdrPlugin* cliprdr, wStream* s, UINT32 dataLen,
                                          UINT16 msgFlags)
{
	CLIPRDR_FORMAT_LIST_RESPONSE formatListResponse = { 0 };
	CliprdrClientContext* context = cliprdr_get_client_interface(cliprdr);
	UINT error = CHANNEL_RC_OK;

	WLog_Print(cliprdr->log, WLOG_DEBUG, "ServerFormatListResponse");

	formatListResponse.common.msgType = CB_FORMAT_LIST_RESPONSE;
	formatListResponse.common.msgFlags = msgFlags;
	formatListResponse.common.dataLen = dataLen;

	IFCALLRET(context->ServerFormatListResponse, error, context, &formatListResponse);
	if (error)
		WLog_ERR(TAG, "ServerFormatListResponse failed with error %" PRIu32 "!", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT cliprdr_process_format_data_request(cliprdrPlugin* cliprdr, wStream* s, UINT32 dataLen,
                                         UINT16 msgFlags)
{
	CLIPRDR_FORMAT_DATA_REQUEST formatDataRequest = { 0 };
	CliprdrClientContext* context = cliprdr_get_client_interface(cliprdr);
	UINT error = CHANNEL_RC_OK;

	WLog_Print(cliprdr->log, WLOG_DEBUG, "ServerFormatDataRequest");

	formatDataRequest.common.msgType = CB_FORMAT_DATA_REQUEST;
	formatDataRequest.common.msgFlags = msgFlags;
	formatDataRequest.common.dataLen = dataLen;

	if ((error = cliprdr_read_format_data_request(s, &formatDataRequest)))
		return error;

	const UINT32 mask =
	    freerdp_settings_get_uint32(context->rdpcontext->settings, FreeRDP_ClipboardFeatureMask);
	if ((mask & (CLIPRDR_FLAG_LOCAL_TO_REMOTE | CLIPRDR_FLAG_LOCAL_TO_REMOTE_FILES)) == 0)
	{
		return cliprdr_send_error_response(cliprdr, CB_FORMAT_DATA_RESPONSE);
	}

	context->lastRequestedFormatId = formatDataRequest.requestedFormatId;
	IFCALLRET(context->ServerFormatDataRequest, error, context, &formatDataRequest);
	if (error)
		WLog_ERR(TAG, "ServerFormatDataRequest failed with error %" PRIu32 "!", error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT cliprdr_process_format_data_response(cliprdrPlugin* cliprdr, wStream* s, UINT32 dataLen,
                                          UINT16 msgFlags)
{
	CLIPRDR_FORMAT_DATA_RESPONSE formatDataResponse = { 0 };
	CliprdrClientContext* context = cliprdr_get_client_interface(cliprdr);
	UINT error = CHANNEL_RC_OK;

	WLog_Print(cliprdr->log, WLOG_DEBUG, "ServerFormatDataResponse");

	formatDataResponse.common.msgType = CB_FORMAT_DATA_RESPONSE;
	formatDataResponse.common.msgFlags = msgFlags;
	formatDataResponse.common.dataLen = dataLen;

	if ((error = cliprdr_read_format_data_response(s, &formatDataResponse)))
		return error;

	const UINT32 mask =
	    freerdp_settings_get_uint32(context->rdpcontext->settings, FreeRDP_ClipboardFeatureMask);
	if ((mask & (CLIPRDR_FLAG_REMOTE_TO_LOCAL | CLIPRDR_FLAG_REMOTE_TO_LOCAL_FILES)) == 0)
	{
		WLog_WARN(TAG,
		          "Received ServerFormatDataResponse but remote -> local clipboard is disabled");
		return CHANNEL_RC_OK;
	}

	IFCALLRET(context->ServerFormatDataResponse, error, context, &formatDataResponse);
	if (error)
		WLog_ERR(TAG, "ServerFormatDataResponse failed with error %" PRIu32 "!", error);

	return error;
}
