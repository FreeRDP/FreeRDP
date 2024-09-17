/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Cliprdr common
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
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

#include <winpr/crt.h>
#include <winpr/stream.h>
#include <freerdp/channels/log.h>

#define TAG CHANNELS_TAG("cliprdr.common")

#include "cliprdr_common.h"

static const char* CB_MSG_TYPE_STR(UINT32 type)
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

const char* CB_MSG_TYPE_STRING(UINT16 type, char* buffer, size_t size)
{
	(void)_snprintf(buffer, size, "%s [0x%04" PRIx16 "]", CB_MSG_TYPE_STR(type), type);
	return buffer;
}

const char* CB_MSG_FLAGS_STRING(UINT16 msgFlags, char* buffer, size_t size)
{
	if ((msgFlags & CB_RESPONSE_OK) != 0)
		winpr_str_append("CB_RESPONSE_OK", buffer, size, "|");
	if ((msgFlags & CB_RESPONSE_FAIL) != 0)
		winpr_str_append("CB_RESPONSE_FAIL", buffer, size, "|");
	if ((msgFlags & CB_ASCII_NAMES) != 0)
		winpr_str_append("CB_ASCII_NAMES", buffer, size, "|");

	const size_t len = strnlen(buffer, size);
	if (!len)
		winpr_str_append("NONE", buffer, size, "");

	char val[32] = { 0 };
	(void)_snprintf(val, sizeof(val), "[0x%04" PRIx16 "]", msgFlags);
	winpr_str_append(val, buffer, size, "|");
	return buffer;
}

static BOOL cliprdr_validate_file_contents_request(const CLIPRDR_FILE_CONTENTS_REQUEST* request)
{
	/*
	 * [MS-RDPECLIP] 2.2.5.3 File Contents Request PDU (CLIPRDR_FILECONTENTS_REQUEST).
	 *
	 * A request for the size of the file identified by the lindex field. The size MUST be
	 * returned as a 64-bit, unsigned integer. The cbRequested field MUST be set to
	 * 0x00000008 and both the nPositionLow and nPositionHigh fields MUST be
	 * set to 0x00000000.
	 */

	if (request->dwFlags & FILECONTENTS_SIZE)
	{
		if (request->cbRequested != sizeof(UINT64))
		{
			WLog_ERR(TAG, "cbRequested must be %" PRIu32 ", got %" PRIu32 "", sizeof(UINT64),
			         request->cbRequested);
			return FALSE;
		}

		if (request->nPositionHigh != 0 || request->nPositionLow != 0)
		{
			WLog_ERR(TAG, "nPositionHigh and nPositionLow must be set to 0");
			return FALSE;
		}
	}

	return TRUE;
}

wStream* cliprdr_packet_new(UINT16 msgType, UINT16 msgFlags, UINT32 dataLen)
{
	wStream* s = NULL;
	s = Stream_New(NULL, dataLen + 8);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return NULL;
	}

	Stream_Write_UINT16(s, msgType);
	Stream_Write_UINT16(s, msgFlags);
	/* Write actual length after the entire packet has been constructed. */
	Stream_Write_UINT32(s, 0);
	return s;
}

static void cliprdr_write_file_contents_request(wStream* s,
                                                const CLIPRDR_FILE_CONTENTS_REQUEST* request)
{
	Stream_Write_UINT32(s, request->streamId);      /* streamId (4 bytes) */
	Stream_Write_UINT32(s, request->listIndex);     /* listIndex (4 bytes) */
	Stream_Write_UINT32(s, request->dwFlags);       /* dwFlags (4 bytes) */
	Stream_Write_UINT32(s, request->nPositionLow);  /* nPositionLow (4 bytes) */
	Stream_Write_UINT32(s, request->nPositionHigh); /* nPositionHigh (4 bytes) */
	Stream_Write_UINT32(s, request->cbRequested);   /* cbRequested (4 bytes) */

	if (request->haveClipDataId)
		Stream_Write_UINT32(s, request->clipDataId); /* clipDataId (4 bytes) */
}

static INLINE void cliprdr_write_lock_unlock_clipdata(wStream* s, UINT32 clipDataId)
{
	Stream_Write_UINT32(s, clipDataId);
}

static void cliprdr_write_lock_clipdata(wStream* s,
                                        const CLIPRDR_LOCK_CLIPBOARD_DATA* lockClipboardData)
{
	cliprdr_write_lock_unlock_clipdata(s, lockClipboardData->clipDataId);
}

static void cliprdr_write_unlock_clipdata(wStream* s,
                                          const CLIPRDR_UNLOCK_CLIPBOARD_DATA* unlockClipboardData)
{
	cliprdr_write_lock_unlock_clipdata(s, unlockClipboardData->clipDataId);
}

static void cliprdr_write_file_contents_response(wStream* s,
                                                 const CLIPRDR_FILE_CONTENTS_RESPONSE* response)
{
	Stream_Write_UINT32(s, response->streamId); /* streamId (4 bytes) */
	Stream_Write(s, response->requestedData, response->cbRequested);
}

wStream* cliprdr_packet_lock_clipdata_new(const CLIPRDR_LOCK_CLIPBOARD_DATA* lockClipboardData)
{
	wStream* s = NULL;

	if (!lockClipboardData)
		return NULL;

	s = cliprdr_packet_new(CB_LOCK_CLIPDATA, 0, 4);

	if (!s)
		return NULL;

	cliprdr_write_lock_clipdata(s, lockClipboardData);
	return s;
}

wStream*
cliprdr_packet_unlock_clipdata_new(const CLIPRDR_UNLOCK_CLIPBOARD_DATA* unlockClipboardData)
{
	wStream* s = NULL;

	if (!unlockClipboardData)
		return NULL;

	s = cliprdr_packet_new(CB_UNLOCK_CLIPDATA, 0, 4);

	if (!s)
		return NULL;

	cliprdr_write_unlock_clipdata(s, unlockClipboardData);
	return s;
}

wStream* cliprdr_packet_file_contents_request_new(const CLIPRDR_FILE_CONTENTS_REQUEST* request)
{
	wStream* s = NULL;

	if (!request)
		return NULL;

	s = cliprdr_packet_new(CB_FILECONTENTS_REQUEST, 0, 28);

	if (!s)
		return NULL;

	cliprdr_write_file_contents_request(s, request);
	return s;
}

wStream* cliprdr_packet_file_contents_response_new(const CLIPRDR_FILE_CONTENTS_RESPONSE* response)
{
	wStream* s = NULL;

	if (!response)
		return NULL;

	s = cliprdr_packet_new(CB_FILECONTENTS_RESPONSE, response->common.msgFlags,
	                       4 + response->cbRequested);

	if (!s)
		return NULL;

	cliprdr_write_file_contents_response(s, response);
	return s;
}

wStream* cliprdr_packet_format_list_new(const CLIPRDR_FORMAT_LIST* formatList,
                                        BOOL useLongFormatNames, BOOL useAsciiNames)
{
	WINPR_ASSERT(formatList);

	if (formatList->common.msgType != CB_FORMAT_LIST)
		WLog_WARN(TAG, "called with invalid type %08" PRIx32, formatList->common.msgType);

	if (useLongFormatNames && useAsciiNames)
		WLog_WARN(TAG, "called with invalid arguments useLongFormatNames=true && "
		               "useAsciiNames=true. useAsciiNames requires "
		               "useLongFormatNames=false, ignoring argument.");

	const UINT32 length = formatList->numFormats * 36;
	const size_t formatNameCharSize =
	    (useLongFormatNames || !useAsciiNames) ? sizeof(WCHAR) : sizeof(CHAR);

	wStream* s = cliprdr_packet_new(CB_FORMAT_LIST, 0, length);
	if (!s)
	{
		WLog_ERR(TAG, "cliprdr_packet_new failed!");
		return NULL;
	}

	for (UINT32 index = 0; index < formatList->numFormats; index++)
	{
		const CLIPRDR_FORMAT* format = &(formatList->formats[index]);

		const char* szFormatName = format->formatName;
		size_t formatNameLength = 0;
		if (szFormatName)
			formatNameLength = strlen(szFormatName);

		size_t formatNameMaxLength = formatNameLength + 1; /* Ensure '\0' termination in output */
		if (!Stream_EnsureRemainingCapacity(s,
		                                    4 + MAX(32, formatNameMaxLength * formatNameCharSize)))
			goto fail;

		Stream_Write_UINT32(s, format->formatId); /* formatId (4 bytes) */

		if (!useLongFormatNames)
		{
			formatNameMaxLength = useAsciiNames ? 32 : 16;
			formatNameLength = MIN(formatNameMaxLength - 1, formatNameLength);
		}

		if (szFormatName && (formatNameLength > 0))
		{
			if (useAsciiNames)
			{
				Stream_Write(s, szFormatName, formatNameLength);
				Stream_Zero(s, formatNameMaxLength - formatNameLength);
			}
			else
			{
				if (Stream_Write_UTF16_String_From_UTF8(s, formatNameMaxLength, szFormatName,
				                                        formatNameLength, TRUE) < 0)
					goto fail;
			}
		}
		else
			Stream_Zero(s, formatNameMaxLength * formatNameCharSize);
	}

	return s;

fail:
	Stream_Free(s, TRUE);
	return NULL;
}

UINT cliprdr_read_unlock_clipdata(wStream* s, CLIPRDR_UNLOCK_CLIPBOARD_DATA* unlockClipboardData)
{
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, unlockClipboardData->clipDataId); /* clipDataId (4 bytes) */
	return CHANNEL_RC_OK;
}

UINT cliprdr_read_format_data_request(wStream* s, CLIPRDR_FORMAT_DATA_REQUEST* request)
{
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, request->requestedFormatId); /* requestedFormatId (4 bytes) */
	return CHANNEL_RC_OK;
}

UINT cliprdr_read_format_data_response(wStream* s, CLIPRDR_FORMAT_DATA_RESPONSE* response)
{
	response->requestedFormatData = NULL;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, response->common.dataLen))
		return ERROR_INVALID_DATA;

	if (response->common.dataLen)
		response->requestedFormatData = Stream_ConstPointer(s);

	return CHANNEL_RC_OK;
}

UINT cliprdr_read_file_contents_request(wStream* s, CLIPRDR_FILE_CONTENTS_REQUEST* request)
{
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 24))
		return ERROR_INVALID_DATA;

	request->haveClipDataId = FALSE;
	Stream_Read_UINT32(s, request->streamId);      /* streamId (4 bytes) */
	Stream_Read_UINT32(s, request->listIndex);     /* listIndex (4 bytes) */
	Stream_Read_UINT32(s, request->dwFlags);       /* dwFlags (4 bytes) */
	Stream_Read_UINT32(s, request->nPositionLow);  /* nPositionLow (4 bytes) */
	Stream_Read_UINT32(s, request->nPositionHigh); /* nPositionHigh (4 bytes) */
	Stream_Read_UINT32(s, request->cbRequested);   /* cbRequested (4 bytes) */

	if (Stream_GetRemainingLength(s) >= 4)
	{
		Stream_Read_UINT32(s, request->clipDataId); /* clipDataId (4 bytes) */
		request->haveClipDataId = TRUE;
	}

	if (!cliprdr_validate_file_contents_request(request))
		return ERROR_BAD_ARGUMENTS;

	return CHANNEL_RC_OK;
}

UINT cliprdr_read_file_contents_response(wStream* s, CLIPRDR_FILE_CONTENTS_RESPONSE* response)
{
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, response->streamId);   /* streamId (4 bytes) */
	response->requestedData = Stream_ConstPointer(s); /* requestedFileContentsData */

	WINPR_ASSERT(response->common.dataLen >= 4);
	response->cbRequested = response->common.dataLen - 4;
	return CHANNEL_RC_OK;
}

UINT cliprdr_read_format_list(wStream* s, CLIPRDR_FORMAT_LIST* formatList, BOOL useLongFormatNames)
{
	UINT32 index = 0;
	size_t formatNameLength = 0;
	const char* szFormatName = NULL;
	const WCHAR* wszFormatName = NULL;
	wStream sub1buffer = { 0 };
	CLIPRDR_FORMAT* formats = NULL;
	UINT error = ERROR_INTERNAL_ERROR;

	const BOOL asciiNames = (formatList->common.msgFlags & CB_ASCII_NAMES) ? TRUE : FALSE;

	index = 0;
	/* empty format list */
	formatList->formats = NULL;
	formatList->numFormats = 0;

	wStream* sub1 =
	    Stream_StaticConstInit(&sub1buffer, Stream_ConstPointer(s), formatList->common.dataLen);
	if (!Stream_SafeSeek(s, formatList->common.dataLen))
		return ERROR_INVALID_DATA;

	if (!formatList->common.dataLen)
	{
	}
	else if (!useLongFormatNames)
	{
		const size_t cap = Stream_Capacity(sub1) / 36ULL;
		if (cap > UINT32_MAX)
		{
			WLog_ERR(TAG, "Invalid short format list length: %" PRIuz "", cap);
			return ERROR_INTERNAL_ERROR;
		}
		formatList->numFormats = (UINT32)cap;

		if (formatList->numFormats)
			formats = (CLIPRDR_FORMAT*)calloc(formatList->numFormats, sizeof(CLIPRDR_FORMAT));

		if (!formats)
		{
			WLog_ERR(TAG, "calloc failed!");
			return CHANNEL_RC_NO_MEMORY;
		}

		formatList->formats = formats;

		while (Stream_GetRemainingLength(sub1) >= 4)
		{
			CLIPRDR_FORMAT* format = &formats[index];

			Stream_Read_UINT32(sub1, format->formatId); /* formatId (4 bytes) */

			/* According to MS-RDPECLIP 2.2.3.1.1.1 formatName is "a 32-byte block containing
			 * the *null-terminated* name assigned to the Clipboard Format: (32 ASCII 8 characters
			 * or 16 Unicode characters)"
			 * However, both Windows RDSH and mstsc violate this specs as seen in the following
			 * example of a transferred short format name string: [R.i.c.h. .T.e.x.t. .F.o.r.m.a.t.]
			 * These are 16 unicode charaters - *without* terminating null !
			 */

			szFormatName = Stream_ConstPointer(sub1);
			wszFormatName = Stream_ConstPointer(sub1);
			if (!Stream_SafeSeek(sub1, 32))
				goto error_out;

			free(format->formatName);
			format->formatName = NULL;

			if (asciiNames)
			{
				if (szFormatName[0])
				{
					/* ensure null termination */
					format->formatName = strndup(szFormatName, 31);
					if (!format->formatName)
					{
						WLog_ERR(TAG, "malloc failed!");
						error = CHANNEL_RC_NO_MEMORY;
						goto error_out;
					}
				}
			}
			else
			{
				if (wszFormatName[0])
				{
					format->formatName = ConvertWCharNToUtf8Alloc(wszFormatName, 16, NULL);
					if (!format->formatName)
						goto error_out;
				}
			}

			index++;
		}
	}
	else
	{
		wStream sub2buffer = sub1buffer;
		wStream* sub2 = &sub2buffer;

		while (Stream_GetRemainingLength(sub1) > 0)
		{
			size_t rest = 0;
			if (!Stream_SafeSeek(sub1, 4)) /* formatId (4 bytes) */
				goto error_out;

			wszFormatName = Stream_ConstPointer(sub1);
			rest = Stream_GetRemainingLength(sub1);
			formatNameLength = _wcsnlen(wszFormatName, rest / sizeof(WCHAR));

			if (!Stream_SafeSeek(sub1, (formatNameLength + 1) * sizeof(WCHAR)))
				goto error_out;
			formatList->numFormats++;
		}

		if (formatList->numFormats)
			formats = (CLIPRDR_FORMAT*)calloc(formatList->numFormats, sizeof(CLIPRDR_FORMAT));

		if (!formats)
		{
			WLog_ERR(TAG, "calloc failed!");
			return CHANNEL_RC_NO_MEMORY;
		}

		formatList->formats = formats;

		while (Stream_GetRemainingLength(sub2) >= 4)
		{
			size_t rest = 0;
			CLIPRDR_FORMAT* format = &formats[index];

			Stream_Read_UINT32(sub2, format->formatId); /* formatId (4 bytes) */

			free(format->formatName);
			format->formatName = NULL;

			wszFormatName = Stream_ConstPointer(sub2);
			rest = Stream_GetRemainingLength(sub2);
			formatNameLength = _wcsnlen(wszFormatName, rest / sizeof(WCHAR));
			if (!Stream_SafeSeek(sub2, (formatNameLength + 1) * sizeof(WCHAR)))
				goto error_out;

			if (formatNameLength)
			{
				format->formatName =
				    ConvertWCharNToUtf8Alloc(wszFormatName, formatNameLength, NULL);
				if (!format->formatName)
					goto error_out;
			}

			index++;
		}
	}

	return CHANNEL_RC_OK;

error_out:
	cliprdr_free_format_list(formatList);
	return error;
}

void cliprdr_free_format_list(CLIPRDR_FORMAT_LIST* formatList)
{
	if (formatList == NULL)
		return;

	if (formatList->formats)
	{
		for (UINT32 index = 0; index < formatList->numFormats; index++)
		{
			free(formatList->formats[index].formatName);
		}

		free(formatList->formats);
		formatList->formats = NULL;
		formatList->numFormats = 0;
	}
}
