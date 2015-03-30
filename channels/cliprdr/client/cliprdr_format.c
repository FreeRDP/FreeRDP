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

void cliprdr_process_format_list(cliprdrPlugin* cliprdr, wStream* s, UINT32 dataLen, UINT16 msgFlags)
{
	UINT32 index;
	UINT32 position;
	BOOL asciiNames;
	int formatNameLength;
	char* szFormatName;
	WCHAR* wszFormatName;
	CLIPRDR_FORMAT* formats = NULL;
	CLIPRDR_FORMAT_LIST formatList;
	CliprdrClientContext* context = cliprdr_get_client_interface(cliprdr);

	if (!context->custom)
		return;

	asciiNames = (msgFlags & CB_ASCII_NAMES) ? TRUE : FALSE;

	formatList.msgType = CB_FORMAT_LIST;
	formatList.msgFlags = msgFlags;
	formatList.dataLen = dataLen;

	index = 0;
	formatList.numFormats = 0;
	position = Stream_GetPosition(s);

	if (!formatList.dataLen)
	{
		/* empty format list */
		formatList.formats = NULL;
		formatList.numFormats = 0;
	}
	else if (!cliprdr->useLongFormatNames)
	{
		formatList.numFormats = (dataLen / 36);

		if ((formatList.numFormats * 36) != dataLen)
		{
			WLog_ERR(TAG, "Invalid short format list length: %d", dataLen);
			return;
		}

		if (formatList.numFormats)
			formats = (CLIPRDR_FORMAT*) calloc(formatList.numFormats, sizeof(CLIPRDR_FORMAT));

		if (!formats)
			return;

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
			return;

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

	WLog_Print(cliprdr->log, WLOG_DEBUG, "ServerFormatList: numFormats: %d",
			formatList.numFormats);

	if (context->ServerFormatList)
		context->ServerFormatList(context, &formatList);

	if (formats)
	{
		for (index = 0; index < formatList.numFormats; index++)
		{
			free(formats[index].formatName);
		}

		free(formats);
	}
}

void cliprdr_process_format_list_response(cliprdrPlugin* cliprdr, wStream* s, UINT32 dataLen, UINT16 msgFlags)
{
	CLIPRDR_FORMAT_LIST_RESPONSE formatListResponse;
	CliprdrClientContext* context = cliprdr_get_client_interface(cliprdr);

	WLog_Print(cliprdr->log, WLOG_DEBUG, "ServerFormatListResponse");

	if (!context->custom)
		return;

	formatListResponse.msgType = CB_FORMAT_LIST_RESPONSE;
	formatListResponse.msgFlags = msgFlags;
	formatListResponse.dataLen = dataLen;

	if (context->ServerFormatListResponse)
		context->ServerFormatListResponse(context, &formatListResponse);
}

void cliprdr_process_format_data_request(cliprdrPlugin* cliprdr, wStream* s, UINT32 dataLen, UINT16 msgFlags)
{
	CLIPRDR_FORMAT_DATA_REQUEST formatDataRequest;
	CliprdrClientContext* context = cliprdr_get_client_interface(cliprdr);

	WLog_Print(cliprdr->log, WLOG_DEBUG, "ServerFormatDataRequest");

	if (!context->custom)
		return;

	formatDataRequest.msgType = CB_FORMAT_DATA_REQUEST;
	formatDataRequest.msgFlags = msgFlags;
	formatDataRequest.dataLen = dataLen;

	Stream_Read_UINT32(s, formatDataRequest.requestedFormatId); /* requestedFormatId (4 bytes) */

	if (context->ServerFormatDataRequest)
		context->ServerFormatDataRequest(context, &formatDataRequest);
}

void cliprdr_process_format_data_response(cliprdrPlugin* cliprdr, wStream* s, UINT32 dataLen, UINT16 msgFlags)
{
	CLIPRDR_FORMAT_DATA_RESPONSE formatDataResponse;
	CliprdrClientContext* context = cliprdr_get_client_interface(cliprdr);

	WLog_Print(cliprdr->log, WLOG_DEBUG, "ServerFormatDataResponse");

	if (!context->custom)
		return;

	formatDataResponse.msgType = CB_FORMAT_DATA_RESPONSE;
	formatDataResponse.msgFlags = msgFlags;
	formatDataResponse.dataLen = dataLen;
	formatDataResponse.requestedFormatData = NULL;

	if (dataLen)
	{
		formatDataResponse.requestedFormatData = (BYTE*) malloc(dataLen);
		Stream_Read(s, formatDataResponse.requestedFormatData, dataLen);
	}

	if (context->ServerFormatDataResponse)
		context->ServerFormatDataResponse(context, &formatDataResponse);

	free(formatDataResponse.requestedFormatData);
}
