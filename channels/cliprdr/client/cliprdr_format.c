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

void cliprdr_process_short_format_names(cliprdrPlugin* cliprdr, wStream* s, UINT32 length, UINT16 flags)
{
	int i;
	BOOL ascii;
	int num_formats;
	CLIPRDR_FORMAT_NAME* format_name;

	num_formats = length / 36;

	if (num_formats <= 0)
	{
		cliprdr->format_names = NULL;
		cliprdr->num_format_names = 0;
		return;
	}

	if (num_formats * 36 != length)
		WLog_ERR(TAG, "dataLen %d not divided by 36!", length);

	ascii = (flags & CB_ASCII_NAMES) ? TRUE : FALSE;

	cliprdr->format_names = (CLIPRDR_FORMAT_NAME*) malloc(sizeof(CLIPRDR_FORMAT_NAME) * num_formats);
	cliprdr->num_format_names = num_formats;

	for (i = 0; i < num_formats; i++)
	{
		format_name = &cliprdr->format_names[i];

		Stream_Read_UINT32(s, format_name->id);

		if (ascii)
		{
			format_name->name = _strdup((char*) s->pointer);
			format_name->length = strlen(format_name->name);
		}
		else
		{
			format_name->name = NULL;
			format_name->length = ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) s->pointer, 32 / 2, &format_name->name, 0, NULL, NULL);
		}

		Stream_Seek(s, 32);
	}
}

void cliprdr_process_long_format_names(cliprdrPlugin* cliprdr, wStream* s, UINT32 length, UINT16 flags)
{
	int allocated_formats = 8;
	BYTE* end_mark;
	CLIPRDR_FORMAT_NAME* format_name;
	
	Stream_GetPointer(s, end_mark);
	end_mark += length;
		
	cliprdr->format_names = (CLIPRDR_FORMAT_NAME*) malloc(sizeof(CLIPRDR_FORMAT_NAME) * allocated_formats);
	cliprdr->num_format_names = 0;

	while (Stream_GetRemainingLength(s) >= 6)
	{
		BYTE* p;
		int name_len;
		
		if (cliprdr->num_format_names >= allocated_formats)
		{
			allocated_formats *= 2;
			cliprdr->format_names = (CLIPRDR_FORMAT_NAME*) realloc(cliprdr->format_names,
					sizeof(CLIPRDR_FORMAT_NAME) * allocated_formats);
		}
		
		format_name = &cliprdr->format_names[cliprdr->num_format_names++];
		Stream_Read_UINT32(s, format_name->id);
		
		format_name->name = NULL;
		format_name->length = 0;

		for (p = Stream_Pointer(s), name_len = 0; p + 1 < end_mark; p += 2, name_len += 2)
		{
			if (*((unsigned short*) p) == 0)
				break;
		}
		
		format_name->length = ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) Stream_Pointer(s), name_len / 2, &format_name->name, 0, NULL, NULL);

		Stream_Seek(s, name_len + 2);
	}
}

void cliprdr_process_format_list(cliprdrPlugin* cliprdr, wStream* s, UINT32 dataLen, UINT16 msgFlags)
{
	CliprdrClientContext* context = cliprdr_get_client_interface(cliprdr);

	if (context->custom)
	{
		UINT32 index;
		int formatNameLength;
		CLIPRDR_FORMAT* formats;
		CLIPRDR_FORMAT_LIST formatList;

		formatList.msgType = CB_FORMAT_LIST;
		formatList.msgFlags = msgFlags;
		formatList.dataLen = dataLen;

		formatList.numFormats = 0;

		while (dataLen)
		{
			Stream_Seek(s, 4); /* formatId */
			dataLen -= 4;

			formatNameLength = _wcslen((WCHAR*) Stream_Pointer(s));
			Stream_Seek(s, (formatNameLength + 1) * 2);
			dataLen -= ((formatNameLength + 1) * 2);
			formatList.numFormats++;
		}

		index = 0;
		dataLen = formatList.dataLen;
		Stream_Rewind(s, dataLen);

		formats = (CLIPRDR_FORMAT*) malloc(sizeof(CLIPRDR_FORMAT) * formatList.numFormats);
		formatList.formats = formats;

		while (dataLen)
		{
			Stream_Read_UINT32(s, formats[index].formatId); /* formatId */
			dataLen -= 4;

			formats[index].formatName = NULL;
			formatNameLength = _wcslen((WCHAR*) Stream_Pointer(s));

			if (formatNameLength)
			{
				formatNameLength = ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) Stream_Pointer(s),
					-1, &(formats[index].formatName), 0, NULL, NULL);

				Stream_Seek(s, formatNameLength * 2);
				dataLen -= (formatNameLength * 2);
			}
			else
			{
				Stream_Seek(s, 2);
				dataLen -= 2;
			}

			index++;
		}

		WLog_Print(cliprdr->log, WLOG_DEBUG, "ServerFormatList: numFormats: %d",
				formatList.numFormats);

		if (context->ServerFormatList)
			context->ServerFormatList(context, &formatList);

		for (index = 0; index < formatList.numFormats; index++)
			free(formats[index].formatName);

		free(formats);
	}
}

void cliprdr_process_format_list_response(cliprdrPlugin* cliprdr, wStream* s, UINT32 dataLen, UINT16 msgFlags)
{
	CliprdrClientContext* context = cliprdr_get_client_interface(cliprdr);

	WLog_Print(cliprdr->log, WLOG_DEBUG, "ServerFormatListResponse");

	if (context->custom)
	{
		CLIPRDR_FORMAT_LIST_RESPONSE formatListResponse;

		formatListResponse.msgType = CB_FORMAT_LIST_RESPONSE;
		formatListResponse.msgFlags = msgFlags;
		formatListResponse.dataLen = dataLen;

		if (context->ServerFormatListResponse)
			context->ServerFormatListResponse(context, &formatListResponse);
	}
}

void cliprdr_process_format_data_request(cliprdrPlugin* cliprdr, wStream* s, UINT32 dataLen, UINT16 msgFlags)
{
	CliprdrClientContext* context = cliprdr_get_client_interface(cliprdr);

	WLog_Print(cliprdr->log, WLOG_DEBUG, "ServerFormatDataRequest");

	if (context->custom)
	{
		CLIPRDR_FORMAT_DATA_REQUEST formatDataRequest;

		formatDataRequest.msgType = CB_FORMAT_DATA_REQUEST;
		formatDataRequest.msgFlags = msgFlags;
		formatDataRequest.dataLen = dataLen;

		Stream_Read_UINT32(s, formatDataRequest.requestedFormatId); /* requestedFormatId (4 bytes) */

		if (context->ServerFormatDataRequest)
			context->ServerFormatDataRequest(context, &formatDataRequest);
	}
}

void cliprdr_process_format_data_response(cliprdrPlugin* cliprdr, wStream* s, UINT32 dataLen, UINT16 msgFlags)
{
	CliprdrClientContext* context = cliprdr_get_client_interface(cliprdr);
	
	WLog_Print(cliprdr->log, WLOG_DEBUG, "ServerFormatDataResponse");

	if (context->custom)
	{
		CLIPRDR_FORMAT_DATA_RESPONSE formatDataResponse;
		
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
}
