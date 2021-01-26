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

	if (!context->custom)
	{
		WLog_ERR(TAG, "context->custom not set!");
		return ERROR_INTERNAL_ERROR;
	}

	formatList.msgType = CB_FORMAT_LIST;
	formatList.msgFlags = msgFlags;
	formatList.dataLen = dataLen;

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

	if (!context->custom)
	{
		WLog_ERR(TAG, "context->custom not set!");
		return ERROR_INTERNAL_ERROR;
	}

	formatListResponse.msgType = CB_FORMAT_LIST_RESPONSE;
	formatListResponse.msgFlags = msgFlags;
	formatListResponse.dataLen = dataLen;

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
	CLIPRDR_FORMAT_DATA_REQUEST formatDataRequest;
	CliprdrClientContext* context = cliprdr_get_client_interface(cliprdr);
	UINT error = CHANNEL_RC_OK;

	WLog_Print(cliprdr->log, WLOG_DEBUG, "ServerFormatDataRequest");

	if (!context->custom)
	{
		WLog_ERR(TAG, "context->custom not set!");
		return ERROR_INTERNAL_ERROR;
	}

	formatDataRequest.msgType = CB_FORMAT_DATA_REQUEST;
	formatDataRequest.msgFlags = msgFlags;
	formatDataRequest.dataLen = dataLen;

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
	CLIPRDR_FORMAT_DATA_RESPONSE formatDataResponse;
	CliprdrClientContext* context = cliprdr_get_client_interface(cliprdr);
	UINT error = CHANNEL_RC_OK;

	WLog_Print(cliprdr->log, WLOG_DEBUG, "ServerFormatDataResponse");

	if (!context->custom)
	{
		WLog_ERR(TAG, "context->custom not set!");
		return ERROR_INTERNAL_ERROR;
	}

	formatDataResponse.msgType = CB_FORMAT_DATA_RESPONSE;
	formatDataResponse.msgFlags = msgFlags;
	formatDataResponse.dataLen = dataLen;

	if ((error = cliprdr_read_format_data_response(s, &formatDataResponse)))
		return error;

	IFCALLRET(context->ServerFormatDataResponse, error, context, &formatDataResponse);
	if (error)
		WLog_ERR(TAG, "ServerFormatDataResponse failed with error %" PRIu32 "!", error);

	return error;
}

static UINT64 filetime_to_uint64(FILETIME value)
{
	UINT64 converted = 0;
	converted |= (UINT32)value.dwHighDateTime;
	converted <<= 32;
	converted |= (UINT32)value.dwLowDateTime;
	return converted;
}

static FILETIME uint64_to_filetime(UINT64 value)
{
	FILETIME converted;
	converted.dwLowDateTime = (UINT32)(value >> 0);
	converted.dwHighDateTime = (UINT32)(value >> 32);
	return converted;
}

#define CLIPRDR_FILEDESCRIPTOR_SIZE (4 + 32 + 4 + 16 + 8 + 8 + 520)

/**
 * Parse a packed file list.
 *
 * The resulting array must be freed with the `free()` function.
 *
 * @param [in]  format_data            packed `CLIPRDR_FILELIST` to parse.
 * @param [in]  format_data_length     length of `format_data` in bytes.
 * @param [out] file_descriptor_array  parsed array of `FILEDESCRIPTOR` structs.
 * @param [out] file_descriptor_count  number of elements in `file_descriptor_array`.
 *
 * @returns 0 on success, otherwise a Win32 error code.
 */
UINT cliprdr_parse_file_list(const BYTE* format_data, UINT32 format_data_length,
                             FILEDESCRIPTORW** file_descriptor_array, UINT32* file_descriptor_count)
{
	UINT result = NO_ERROR;
	UINT32 i;
	UINT32 count = 0;
	wStream* s = NULL;

	if (!format_data || !file_descriptor_array || !file_descriptor_count)
		return ERROR_BAD_ARGUMENTS;

	s = Stream_New((BYTE*)format_data, format_data_length);
	if (!s)
		return ERROR_NOT_ENOUGH_MEMORY;

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_ERR(TAG, "invalid packed file list");

		result = ERROR_INCORRECT_SIZE;
		goto out;
	}

	Stream_Read_UINT32(s, count); /* cItems (4 bytes) */

	if (Stream_GetRemainingLength(s) / CLIPRDR_FILEDESCRIPTOR_SIZE < count)
	{
		WLog_ERR(TAG, "packed file list is too short: expected %" PRIuz ", have %" PRIuz,
		         ((size_t)count) * CLIPRDR_FILEDESCRIPTOR_SIZE, Stream_GetRemainingLength(s));

		result = ERROR_INCORRECT_SIZE;
		goto out;
	}

	*file_descriptor_count = count;
	*file_descriptor_array = calloc(count, sizeof(FILEDESCRIPTORW));
	if (!*file_descriptor_array)
	{
		result = ERROR_NOT_ENOUGH_MEMORY;
		goto out;
	}

	for (i = 0; i < count; i++)
	{
		int c;
		UINT64 lastWriteTime;
		FILEDESCRIPTORW* file = &((*file_descriptor_array)[i]);

		Stream_Read_UINT32(s, file->dwFlags);          /* flags (4 bytes) */
		Stream_Seek(s, 32);                            /* reserved1 (32 bytes) */
		Stream_Read_UINT32(s, file->dwFileAttributes); /* fileAttributes (4 bytes) */
		Stream_Seek(s, 16);                            /* reserved2 (16 bytes) */
		Stream_Read_UINT64(s, lastWriteTime);          /* lastWriteTime (8 bytes) */
		file->ftLastWriteTime = uint64_to_filetime(lastWriteTime);
		Stream_Read_UINT32(s, file->nFileSizeHigh); /* fileSizeHigh (4 bytes) */
		Stream_Read_UINT32(s, file->nFileSizeLow);  /* fileSizeLow (4 bytes) */
		for (c = 0; c < 260; c++)                   /* cFileName (520 bytes) */
			Stream_Read_UINT16(s, file->cFileName[c]);
	}

	if (Stream_GetRemainingLength(s) > 0)
		WLog_WARN(TAG, "packed file list has %" PRIuz " excess bytes",
		          Stream_GetRemainingLength(s));
out:
	Stream_Free(s, FALSE);

	return result;
}

#define CLIPRDR_MAX_FILE_SIZE (2U * 1024 * 1024 * 1024)

/**
 * Serialize a packed file list.
 *
 * The resulting format data must be freed with the `free()` function.
 *
 * @param [in]  file_descriptor_array  array of `FILEDESCRIPTOR` structs to serialize.
 * @param [in]  file_descriptor_count  number of elements in `file_descriptor_array`.
 * @param [out] format_data            serialized CLIPRDR_FILELIST.
 * @param [out] format_data_length     length of `format_data` in bytes.
 *
 * @returns 0 on success, otherwise a Win32 error code.
 */
UINT cliprdr_serialize_file_list(const FILEDESCRIPTORW* file_descriptor_array,
                                 UINT32 file_descriptor_count, BYTE** format_data,
                                 UINT32* format_data_length)
{
	return cliprdr_serialize_file_list_ex(CB_STREAM_FILECLIP_ENABLED, file_descriptor_array,
	                                      file_descriptor_count, format_data, format_data_length);
}

UINT cliprdr_serialize_file_list_ex(UINT32 flags, const FILEDESCRIPTORW* file_descriptor_array,
                                    UINT32 file_descriptor_count, BYTE** format_data,
                                    UINT32* format_data_length)
{
	UINT result = NO_ERROR;
	UINT32 i;
	wStream* s = NULL;

	if (!file_descriptor_array || !format_data || !format_data_length)
		return ERROR_BAD_ARGUMENTS;

	if ((flags & CB_STREAM_FILECLIP_ENABLED) == 0)
	{
		WLog_WARN(TAG, "No file clipboard support annouonced!");
		return ERROR_BAD_ARGUMENTS;
	}

	s = Stream_New(NULL, 4 + file_descriptor_count * CLIPRDR_FILEDESCRIPTOR_SIZE);
	if (!s)
		return ERROR_NOT_ENOUGH_MEMORY;

	Stream_Write_UINT32(s, file_descriptor_count); /* cItems (4 bytes) */

	for (i = 0; i < file_descriptor_count; i++)
	{
		int c;
		UINT64 lastWriteTime;
		const FILEDESCRIPTORW* file = &file_descriptor_array[i];

		/*
		 * There is a known issue with Windows server getting stuck in
		 * an infinite loop when downloading files that are larger than
		 * 2 gigabytes. Do not allow clients to send such file lists.
		 *
		 * https://support.microsoft.com/en-us/help/2258090
		 */
		if ((flags & CB_HUGE_FILE_SUPPORT_ENABLED) == 0)
		{
			if ((file->nFileSizeHigh > 0) || (file->nFileSizeLow >= CLIPRDR_MAX_FILE_SIZE))
			{
				WLog_ERR(TAG, "cliprdr does not support files over 2 GB");
				result = ERROR_FILE_TOO_LARGE;
				goto error;
			}
		}

		Stream_Write_UINT32(s, file->dwFlags);          /* flags (4 bytes) */
		Stream_Zero(s, 32);                             /* reserved1 (32 bytes) */
		Stream_Write_UINT32(s, file->dwFileAttributes); /* fileAttributes (4 bytes) */
		Stream_Zero(s, 16);                             /* reserved2 (16 bytes) */
		lastWriteTime = filetime_to_uint64(file->ftLastWriteTime);
		Stream_Write_UINT64(s, lastWriteTime);       /* lastWriteTime (8 bytes) */
		Stream_Write_UINT32(s, file->nFileSizeHigh); /* fileSizeHigh (4 bytes) */
		Stream_Write_UINT32(s, file->nFileSizeLow);  /* fileSizeLow (4 bytes) */
		for (c = 0; c < 260; c++)                    /* cFileName (520 bytes) */
			Stream_Write_UINT16(s, file->cFileName[c]);
	}

	Stream_SealLength(s);

	Stream_GetBuffer(s, *format_data);
	Stream_GetLength(s, *format_data_length);

	Stream_Free(s, FALSE);

	return result;

error:
	Stream_Free(s, TRUE);

	return result;
}
