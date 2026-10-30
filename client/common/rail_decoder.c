/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RAIL launch record decoder
 *
 * Copyright 2026 Tony Dursun <oraturk75@gmail.com>
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
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <winpr/assert.h>

#include "rail_decoder.h"

#define RAIL_DECODER_KNOWN_EXEC_FLAGS                                                \
	(TS_RAIL_EXEC_FLAG_EXPAND_WORKINGDIRECTORY | TS_RAIL_EXEC_FLAG_TRANSLATE_FILES | \
	 TS_RAIL_EXEC_FLAG_FILE | TS_RAIL_EXEC_FLAG_EXPAND_ARGUMENTS |                   \
	 TS_RAIL_EXEC_FLAG_APP_USER_MODEL_ID)

WINPR_ATTR_NODISCARD
static BOOL rail_decoder_parse_flags(const char* value, UINT16* flags)
{
	WINPR_ASSERT(value);
	WINPR_ASSERT(flags);

	if (value[0] == '\0')
	{
		*flags = 0;
		return TRUE;
	}
	if ((value[0] < '0') || (value[0] > '9'))
		return FALSE;

	const BOOL hexadecimal = (value[0] == '0') && ((value[1] == 'x') || (value[1] == 'X'));
	const char* start = hexadecimal ? &value[2] : value;
	const int base = hexadecimal ? 16 : 10;
	if (hexadecimal &&
	    !(((start[0] >= '0') && (start[0] <= '9')) || ((start[0] >= 'a') && (start[0] <= 'f')) ||
	      ((start[0] >= 'A') && (start[0] <= 'F'))))
		return FALSE;

	errno = 0;
	char* end = nullptr;
	const unsigned long parsed = strtoul(start, &end, base);
	if ((errno != 0) || !end || (end == start) || (*end != '\0') || (parsed > UINT16_MAX))
		return FALSE;
	const UINT16 converted = (UINT16)parsed;
	if ((converted & ~RAIL_DECODER_KNOWN_EXEC_FLAGS) != 0)
		return FALSE;
	if (((converted & TS_RAIL_EXEC_FLAG_TRANSLATE_FILES) != 0) &&
	    ((converted & TS_RAIL_EXEC_FLAG_FILE) == 0))
		return FALSE;
	*flags = converted;
	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL rail_decoder_is_valid_utf8(const char* value)
{
	WINPR_ASSERT(value);
	const BYTE* bytes = (const BYTE*)value;
	const size_t length = strlen(value);

	for (size_t x = 0; x < length;)
	{
		const BYTE first = bytes[x];
		if (first <= 0x7F)
		{
			x++;
			continue;
		}

		size_t count = 0;
		BYTE secondMinimum = 0x80;
		BYTE secondMaximum = 0xBF;
		if ((first >= 0xC2) && (first <= 0xDF))
			count = 2;
		else if ((first >= 0xE0) && (first <= 0xEF))
		{
			count = 3;
			if (first == 0xE0)
				secondMinimum = 0xA0;
			else if (first == 0xED)
				secondMaximum = 0x9F;
		}
		else if ((first >= 0xF0) && (first <= 0xF4))
		{
			count = 4;
			if (first == 0xF0)
				secondMinimum = 0x90;
			else if (first == 0xF4)
				secondMaximum = 0x8F;
		}
		else
			return FALSE;

		if ((count > length - x) || (bytes[x + 1U] < secondMinimum) ||
		    (bytes[x + 1U] > secondMaximum))
			return FALSE;
		for (size_t y = 2; y < count; y++)
		{
			if ((bytes[x + y] < 0x80) || (bytes[x + y] > 0xBF))
				return FALSE;
		}
		x += count;
	}
	return TRUE;
}

WINPR_ATTR_NODISCARD
static const char* rail_decoder_validate_order(const RAIL_EXEC_ORDER* order)
{
	WINPR_ASSERT(order);
	if (!order->RemoteApplicationProgram || (order->RemoteApplicationProgram[0] == '\0'))
		return "program is missing or empty";
	if (!rail_decoder_is_valid_utf8(order->RemoteApplicationProgram))
		return "program is not valid UTF-8";
	if (!rail_decoder_is_valid_utf8(order->RemoteApplicationWorkingDir))
		return "working directory is not valid UTF-8";
	if (!rail_decoder_is_valid_utf8(order->RemoteApplicationArguments))
		return "arguments are not valid UTF-8";

	RAIL_UNICODE_STRING program = WINPR_C_ARRAY_INIT;
	RAIL_UNICODE_STRING workingDirectory = WINPR_C_ARRAY_INIT;
	RAIL_UNICODE_STRING arguments = WINPR_C_ARRAY_INIT;
	const BOOL programConverted =
	    utf8_string_to_rail_string(order->RemoteApplicationProgram, &program);
	const BOOL workingDirectoryConverted =
	    utf8_string_to_rail_string(order->RemoteApplicationWorkingDir, &workingDirectory);
	const BOOL argumentsConverted =
	    utf8_string_to_rail_string(order->RemoteApplicationArguments, &arguments);
	const char* error = nullptr;
	if (!programConverted)
		error = "program could not be converted to RAIL encoding";
	else if (!workingDirectoryConverted)
		error = "working directory could not be converted to RAIL encoding";
	else if (!argumentsConverted)
		error = "arguments could not be converted to RAIL encoding";
	else if ((program.length == 0) || (program.length > 520))
		error = "converted program length is outside the RAIL limit";
	else if (workingDirectory.length > 520)
		error = "converted working-directory length exceeds the RAIL limit";
	else if (arguments.length > 16000)
		error = "converted argument length exceeds the RAIL limit";
	rail_unicode_string_free(&program);
	rail_unicode_string_free(&workingDirectory);
	rail_unicode_string_free(&arguments);
	return error;
}

WINPR_ATTR_NODISCARD
static const char* rail_decoder_parse_record(RailDecoder* dec, RAIL_EXEC_ORDER* result)
{
	WINPR_ASSERT(dec);
	WINPR_ASSERT(dec->record);
	WINPR_ASSERT(dec->recordBytes > 0);

	char* program = nullptr;
	char* workingDirectory = nullptr;
	char* arguments = nullptr;
	UINT16 flags = 0;
	BOOL haveProgram = FALSE;
	BOOL haveWorkingDirectory = FALSE;
	BOOL haveArguments = FALSE;
	BOOL haveFlags = FALSE;

	const size_t contentLength = dec->recordBytes - 1U;
	size_t offset = 0;
	while (offset < contentLength)
	{
		char* line = &dec->record[offset];
		char* newline = memchr(line, '\n', contentLength - offset);
		if (!newline)
		{
			return "line is not terminated";
		}
		*newline = '\0';
		offset = (size_t)(newline - dec->record) + 1U;

		char* separator = strchr(line, '=');
		if (!separator || (separator == line))
		{
			return "line has no valid key separator";
		}
		*separator = '\0';
		char* value = separator + 1U;
		if (strcmp(line, "program") == 0)
		{
			if (haveProgram)
			{
				return "program key is duplicated";
			}
			haveProgram = TRUE;
			program = value;
		}
		else if (strcmp(line, "working-directory") == 0)
		{
			if (haveWorkingDirectory)
			{
				return "working-directory key is duplicated";
			}
			haveWorkingDirectory = TRUE;
			workingDirectory = value;
		}
		else if (strcmp(line, "arguments") == 0)
		{
			if (haveArguments)
			{
				return "arguments key is duplicated";
			}
			haveArguments = TRUE;
			arguments = value;
		}
		else if (strcmp(line, "flags") == 0)
		{
			if (haveFlags)
			{
				return "flags key is duplicated";
			}
			haveFlags = TRUE;
			if (!rail_decoder_parse_flags(value, &flags))
			{
				return "flags value is invalid";
			}
		}
		else
		{
			return "record contains an unknown key";
		}
	}

	const RAIL_EXEC_ORDER order = { .flags = flags,
		                            .RemoteApplicationProgram = haveProgram ? program : "",
		                            .RemoteApplicationWorkingDir =
		                                haveWorkingDirectory ? workingDirectory : "",
		                            .RemoteApplicationArguments = haveArguments ? arguments : "" };
	*result = order;
	return rail_decoder_validate_order(result);
}

void rail_decoder_reset(RailDecoder* dec)
{
	WINPR_ASSERT(dec);
	dec->recordBytes = 0;
	dec->atLineStart = TRUE;
	dec->recordInvalid = FALSE;
	dec->recordError = nullptr;
}

BOOL rail_decoder_init(RailDecoder* dec, size_t maxRecord)
{
	WINPR_ASSERT(dec);
	*dec = (RailDecoder)WINPR_C_ARRAY_INIT;
	if ((maxRecord == 0) || (maxRecord == SIZE_MAX))
		return FALSE;
	dec->record = calloc(maxRecord + 1U, sizeof(char));
	if (!dec->record)
		return FALSE;
	dec->maxRecord = maxRecord;
	rail_decoder_reset(dec);
	return TRUE;
}

void rail_decoder_uninit(RailDecoder* dec)
{
	WINPR_ASSERT(dec);
	free(dec->record);
	*dec = (RailDecoder)WINPR_C_ARRAY_INIT;
}

RailDecoderResult rail_decoder_feed(RailDecoder* dec, const BYTE* data, size_t length,
                                    size_t* consumed, RAIL_EXEC_ORDER* order, const char** reason)
{
	WINPR_ASSERT(dec);
	WINPR_ASSERT(dec->record);
	WINPR_ASSERT(data || (length == 0));
	WINPR_ASSERT(consumed);
	WINPR_ASSERT(order);
	WINPR_ASSERT(reason);
	*consumed = 0;
	*reason = nullptr;
	*order = (RAIL_EXEC_ORDER)WINPR_C_ARRAY_INIT;

	for (size_t x = 0; x < length; x++)
	{
		const BYTE value = data[x];
		const BOOL terminator = (value == '\n') && dec->atLineStart;
		if (dec->recordBytes < dec->maxRecord)
			dec->record[dec->recordBytes] = (char)value;
		else if (!dec->recordInvalid)
		{
			dec->recordInvalid = TRUE;
			dec->recordError = "record exceeds the FIFO atomic-write limit";
		}
		if (dec->recordBytes <= dec->maxRecord)
			dec->recordBytes++;
		if ((value == '\0') && !dec->recordInvalid)
		{
			dec->recordInvalid = TRUE;
			dec->recordError = "record contains NUL";
		}
		dec->atLineStart = (value == '\n');

		if (!terminator)
			continue;
		*consumed = x + 1U;
		if (dec->recordInvalid)
			*reason = dec->recordError ? dec->recordError : "record is invalid";
		else
		{
			dec->record[dec->recordBytes] = '\0';
			*reason = rail_decoder_parse_record(dec, order);
		}
		/* Reset counters only: the caller still needs the borrowed strings. */
		rail_decoder_reset(dec);
		return *reason ? RAIL_DECODER_REJECTED : RAIL_DECODER_RECORD;
	}
	*consumed = length;
	return RAIL_DECODER_INCOMPLETE;
}
