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
#include <winpr/string.h>

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

	errno = 0;
	char* end = nullptr;
	const unsigned long parsed = strtoul(value, &end, 0);
	if ((errno != 0) || !end || (end == value) || (*end != '\0') || (parsed > UINT16_MAX))
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
static const char* rail_decoder_parse_record(char* record, RAIL_EXEC_ORDER* order)
{
	char* program = nullptr;
	char* workingDirectory = nullptr;
	char* arguments = nullptr;
	char* flags = nullptr;
	char* line = nullptr;
	while ((line = StrSep(&record, "\n")))
	{
		char* separator = strchr(line, '=');
		if (!separator || (separator == line))
			return "line has no valid key separator";
		*separator = '\0';
		char** field = nullptr;
		if (strcmp(line, "program") == 0)
			field = &program;
		else if (strcmp(line, "working-directory") == 0)
			field = &workingDirectory;
		else if (strcmp(line, "arguments") == 0)
			field = &arguments;
		else if (strcmp(line, "flags") == 0)
			field = &flags;
		else
			return "record contains an unknown key";
		if (*field)
			return "record contains a duplicate key";
		*field = separator + 1;
	}
	order->RemoteApplicationProgram = program ? program : "";
	order->RemoteApplicationWorkingDir = workingDirectory ? workingDirectory : "";
	order->RemoteApplicationArguments = arguments ? arguments : "";
	if (order->RemoteApplicationProgram[0] == '\0')
		return "program is missing or empty";
	if (flags && !rail_decoder_parse_flags(flags, &order->flags))
		return "flags value is invalid";
	return nullptr;
}

void rail_decoder_consume(RailDecoder* dec, size_t consumed)
{
	WINPR_ASSERT(dec);
	WINPR_ASSERT(consumed <= dec->used);
	dec->used -= consumed;
	memmove(dec->buffer, dec->buffer + consumed, dec->used);
}

RailDecoderResult rail_decoder_next(RailDecoder* dec, size_t* consumed, RAIL_EXEC_ORDER* order,
                                    const char** reason)
{
	WINPR_ASSERT(dec);
	WINPR_ASSERT(consumed);
	WINPR_ASSERT(order);
	WINPR_ASSERT(reason);
	WINPR_ASSERT(dec->used <= RAIL_DECODER_CAPACITY);
	*consumed = 0;
	*reason = nullptr;
	*order = (RAIL_EXEC_ORDER)WINPR_C_ARRAY_INIT;

	/* Extra blank lines between records do not belong to the next launch. */
	if (!dec->discarding)
	{
		while ((*consumed < dec->used) && (dec->buffer[*consumed] == '\n'))
			(*consumed)++;
		if (*consumed > 0)
			return RAIL_DECODER_REJECTED;
	}

	char* end = nullptr;
	for (size_t offset = 0; offset + 1 < dec->used;)
	{
		char* newline = memchr(dec->buffer + offset, '\n', dec->used - offset - 1);
		if (!newline)
			break;
		if (newline[1] == '\n')
		{
			end = newline;
			break;
		}
		offset = (size_t)(newline - dec->buffer) + 1;
	}
	if (end)
	{
		*consumed = (size_t)(end - dec->buffer) + 2;
		if (dec->discarding)
		{
			dec->discarding = FALSE;
			return RAIL_DECODER_REJECTED;
		}
		if (memchr(dec->buffer, '\0', *consumed))
			*reason = "record contains NUL";
		else
		{
			*end = '\0';
			*reason = rail_decoder_parse_record(dec->buffer, order);
		}
		return *reason ? RAIL_DECODER_REJECTED : RAIL_DECODER_RECORD;
	}
	if (dec->used == RAIL_DECODER_CAPACITY)
	{
		if (!dec->discarding)
			*reason = "record exceeds 4096-byte limit";
		dec->discarding = TRUE;
	}
	if (dec->discarding && (dec->used > 0))
	{
		/* Keep a possible first half of the delimiter for the next read. */
		*consumed = dec->used - (dec->buffer[dec->used - 1] == '\n' ? 1U : 0U);
		if (*consumed > 0)
			return RAIL_DECODER_REJECTED;
	}
	return RAIL_DECODER_INCOMPLETE;
}
