/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Clipboard Virtual Channel Extension
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2022 Armin Novak <anovak@thincast.com
 * Copyright 2022 Thincast Technologies GmbH
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <winpr/stream.h>
#include <freerdp/utils/cliprdr_utils.h>
#include <freerdp/channels/cliprdr.h>

#include <freerdp/log.h>
#define TAG FREERDP_TAG("utils." CLIPRDR_SVC_CHANNEL_NAME)

#define CLIPRDR_FILEDESCRIPTOR_SIZE (4 + 32 + 4 + 16 + 8 + 8 + 520)
#define CLIPRDR_MAX_FILE_SIZE (2U * 1024 * 1024 * 1024)

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
	wStream sbuffer;
	wStream* s = NULL;

	if (!format_data || !file_descriptor_array || !file_descriptor_count)
		return ERROR_BAD_ARGUMENTS;

	s = Stream_StaticConstInit(&sbuffer, format_data, format_data_length);
	if (!s)
		return ERROR_NOT_ENOUGH_MEMORY;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
	{
		result = ERROR_INCORRECT_SIZE;
		goto out;
	}

	Stream_Read_UINT32(s, count); /* cItems (4 bytes) */

	if (!Stream_CheckAndLogRequiredLength(TAG, s, CLIPRDR_FILEDESCRIPTOR_SIZE * count * 1ull))
	{
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
		UINT64 tmp;
		FILEDESCRIPTORW* file = &((*file_descriptor_array)[i]);

		Stream_Read_UINT32(s, file->dwFlags); /* flags (4 bytes) */
		Stream_Read_UINT32(s, file->clsid.Data1);
		Stream_Read_UINT16(s, file->clsid.Data2);
		Stream_Read_UINT16(s, file->clsid.Data3);
		Stream_Read(s, &file->clsid.Data4, sizeof(file->clsid.Data4));
		Stream_Read_INT32(s, file->sizel.cx);
		Stream_Read_INT32(s, file->sizel.cy);
		Stream_Read_INT32(s, file->pointl.x);
		Stream_Read_INT32(s, file->pointl.y);
		Stream_Read_UINT32(s, file->dwFileAttributes); /* fileAttributes (4 bytes) */
		Stream_Read_UINT64(s, tmp);                    /* ftCreationTime (8 bytes) */
		file->ftCreationTime = uint64_to_filetime(tmp);
		Stream_Read_UINT64(s, tmp); /* ftLastAccessTime (8 bytes) */
		file->ftLastAccessTime = uint64_to_filetime(tmp);
		Stream_Read_UINT64(s, tmp); /* lastWriteTime (8 bytes) */
		file->ftLastWriteTime = uint64_to_filetime(tmp);
		Stream_Read_UINT32(s, file->nFileSizeHigh); /* fileSizeHigh (4 bytes) */
		Stream_Read_UINT32(s, file->nFileSizeLow);  /* fileSizeLow (4 bytes) */
		Stream_Read_UTF16_String(s, file->cFileName,
		                         ARRAYSIZE(file->cFileName)); /* cFileName (520 bytes) */
	}

	if (Stream_GetRemainingLength(s) > 0)
		WLog_WARN(TAG, "packed file list has %" PRIuz " excess bytes",
		          Stream_GetRemainingLength(s));
out:

	return result;
}

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
	size_t len;
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

		Stream_Write_UINT32(s, file->clsid.Data1);
		Stream_Write_UINT16(s, file->clsid.Data2);
		Stream_Write_UINT16(s, file->clsid.Data3);
		WINPR_ASSERT(sizeof(file->clsid.Data4) == 8);
		Stream_Write(s, &file->clsid.Data4, sizeof(file->clsid.Data4));
		Stream_Write_INT32(s, file->sizel.cx);
		Stream_Write_INT32(s, file->sizel.cy);
		Stream_Write_INT32(s, file->pointl.x);
		Stream_Write_INT32(s, file->pointl.y);
		Stream_Write_UINT32(s, file->dwFileAttributes); /* fileAttributes (4 bytes) */
		Stream_Write_UINT64(s, filetime_to_uint64(file->ftCreationTime));
		Stream_Write_UINT64(s, filetime_to_uint64(file->ftLastAccessTime));
		Stream_Write_UINT64(
		    s, filetime_to_uint64(file->ftLastWriteTime)); /* lastWriteTime (8 bytes) */
		Stream_Write_UINT32(s, file->nFileSizeHigh); /* fileSizeHigh (4 bytes) */
		Stream_Write_UINT32(s, file->nFileSizeLow);  /* fileSizeLow (4 bytes) */
		Stream_Write_UTF16_String(s, file->cFileName,
		                          ARRAYSIZE(file->cFileName)); /* cFileName (520 bytes) */
	}

	Stream_SealLength(s);

	Stream_GetBuffer(s, *format_data);
	Stream_GetLength(s, len);
	if (len > UINT32_MAX)
		goto error;

	*format_data_length = (UINT32)len;

	Stream_Free(s, FALSE);

	return result;

error:
	Stream_Free(s, TRUE);

	return result;
}
