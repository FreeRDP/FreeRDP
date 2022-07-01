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
#include <freerdp/constants.h>
#include <freerdp/client/cliprdr.h>

#include "cliprdr_main.h"
#include "cliprdr_format.h"
#include "../cliprdr_common.h"

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT cliprdr_process_format_list(cliprdrPlugin* cliprdr, wStream* s, UINT32 dataLen,
                                 UINT16 msgFlags)
{
	CLIPRDR_FORMAT_LIST formatList = { 0 };
	CliprdrClientContext* context = cliprdr_get_client_interface(cliprdr);
	UINT error = CHANNEL_RC_OK;

	formatList.common.msgType = CB_FORMAT_LIST;
	formatList.common.msgFlags = msgFlags;
	formatList.common.dataLen = dataLen;

	if ((error = cliprdr_read_format_list(s, &formatList, cliprdr->useLongFormatNames)))
		goto error_out;

	WLog_Print(cliprdr->log, WLOG_DEBUG, "ServerFormatList: numFormats: %" PRIu32 "",
	           formatList.numFormats);

	if (context->ServerFormatList)
	{
		if ((error = context->ServerFormatList(context, &formatList)))
			WLog_ERR(TAG, "ServerFormatList failed with error %" PRIu32 "", error);
	}

error_out:
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

	IFCALLRET(context->ServerFormatDataResponse, error, context, &formatDataResponse);
	if (error)
		WLog_ERR(TAG, "ServerFormatDataResponse failed with error %" PRIu32 "!", error);

	return error;
}
