/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RAIL launch record decoder tests
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

#include <stdio.h>
#include <string.h>
#include <winpr/crt.h>

#include "../rail_decoder.h"

static BOOL append(RailDecoder* dec, const char* data, size_t length)
{
	if (length > RAIL_DECODER_CAPACITY - dec->used)
		return FALSE;
	memcpy(dec->buffer + dec->used, data, length);
	dec->used += length;
	return TRUE;
}

static BOOL expect_record(RailDecoder* dec, const char* reason, const RAIL_EXEC_ORDER* expected)
{
	size_t consumed = 0;
	const char* actualReason = nullptr;
	RAIL_EXEC_ORDER order = WINPR_C_ARRAY_INIT;
	RailDecoderResult result = RAIL_DECODER_INCOMPLETE;
	do
	{
		result = rail_decoder_next(dec, &consumed, &order, &actualReason);
		if ((result != RAIL_DECODER_REJECTED) || actualReason)
			break;
		rail_decoder_consume(dec, consumed);
	} while (dec->used > 0);
	BOOL ok = reason ? ((result == RAIL_DECODER_REJECTED) && actualReason &&
	                    (strcmp(reason, actualReason) == 0))
	                 : ((result == RAIL_DECODER_RECORD) && !actualReason);
	if (ok && !reason && expected)
		ok = (order.flags == expected->flags) &&
		     (strcmp(order.RemoteApplicationProgram, expected->RemoteApplicationProgram) == 0) &&
		     (strcmp(order.RemoteApplicationWorkingDir, expected->RemoteApplicationWorkingDir) ==
		      0) &&
		     (strcmp(order.RemoteApplicationArguments, expected->RemoteApplicationArguments) == 0);
	if (!ok)
		(void)fprintf(stderr, "decoder: result=%d, reason=%s\n", result,
		              actualReason ? actualReason : "none");
	/* A skipped span was already consumed above. */
	if ((result != RAIL_DECODER_REJECTED) || actualReason)
		rail_decoder_consume(dec, consumed);
	return ok;
}

static BOOL test_records(RailDecoder* dec)
{
	const struct
	{
		const char* record;
		const char* reason;
		RAIL_EXEC_ORDER order;
	} cases[] = {
		{ "program=app\n\n", nullptr, { 0, "app", "", "" } },
		{ "\nprogram=b\n\n", nullptr, { 0, "b", "", "" } },
		{ "arguments=a=b\nflags=0x4\nworking-directory=dir\nprogram=app\n\n",
		  nullptr,
		  { 4, "app", "dir", "a=b" } },
		{ "program=app\nworking-directory=\narguments=\nflags=\n\n",
		  nullptr,
		  { 0, "app", "", "" } },
		{ "program=app\nunknown=x\n\n", "record contains an unknown key", { 0 } },
		{ "program=a\nprogram=b\n\n", "record contains a duplicate key", { 0 } },
		{ "program=a\narguments=\narguments=\n\n", "record contains a duplicate key", { 0 } },
		{ "program\n\n", "line has no valid key separator", { 0 } },
		{ "=app\n\n", "line has no valid key separator", { 0 } },
		{ "program=app\nflags=6\n\n", nullptr, { 6, "app", "", "" } },
		{ "program=app\nflags=+6\n\n", "flags value is invalid", { 0 } },
		{ "program=app\nflags=08\n\n", "flags value is invalid", { 0 } },
		{ "program=app\nflags=016\n\n", nullptr, { 14, "app", "", "" } },
		{ "program=app\nflags=010\n\n", nullptr, { 8, "app", "", "" } },
		{ "program=app\nflags=0x0x1\n\n", "flags value is invalid", { 0 } },
		{ "program=app\nflags=65536\n\n", "flags value is invalid", { 0 } },
		{ "program=app\nflags=32\n\n", "flags value is invalid", { 0 } },
		{ "program=app\nflags=2\n\n", "flags value is invalid", { 0 } },
		{ "program=app\nflags=x\n\n", "flags value is invalid", { 0 } },
		{ "program=app\nflags=0x\n\n", "flags value is invalid", { 0 } },
		{ "program=\n\n", "program is missing or empty", { 0 } },
		{ "arguments=a\n\n", "program is missing or empty", { 0 } },
	};
	for (size_t x = 0; x < ARRAYSIZE(cases); x++)
	{
		if (!append(dec, cases[x].record, strlen(cases[x].record)) ||
		    !expect_record(dec, cases[x].reason, &cases[x].order))
		{
			(void)fprintf(stderr, "record case %" PRIuz " failed\n", x);
			return FALSE;
		}
	}
	const char nul[] = "program=a\0b\n\n";
	return append(dec, nul, sizeof(nul) - 1U) && expect_record(dec, "record contains NUL", nullptr);
}

static BOOL test_buffer(RailDecoder* dec)
{
	const RAIL_EXEC_ORDER a = { 0, "a", "", "" };
	const RAIL_EXEC_ORDER b = { 0, "b", "", "" };
	const char first[] = "program=a\n";
	const char rest[] = "\nprogram=b\n\n";
	size_t consumed = 0;
	const char* reason = nullptr;
	RAIL_EXEC_ORDER order = WINPR_C_ARRAY_INIT;
	if (!append(dec, first, sizeof(first) - 1U) ||
	    (rail_decoder_next(dec, &consumed, &order, &reason) != RAIL_DECODER_INCOMPLETE) ||
	    (consumed != 0) || !append(dec, rest, sizeof(rest) - 1U) ||
	    !expect_record(dec, nullptr, &a) || !expect_record(dec, nullptr, &b) || (dec->used != 0))
		return FALSE;

	char full[RAIL_DECODER_CAPACITY] = WINPR_C_ARRAY_INIT;
	const char prefix[] = "program=a\narguments=";
	memset(full, 'x', sizeof(full));
	memcpy(full, prefix, sizeof(prefix) - 1U);
	full[sizeof(full) - 2U] = '\n';
	full[sizeof(full) - 1U] = '\n';
	if (!append(dec, full, sizeof(full)) ||
	    (rail_decoder_next(dec, &consumed, &order, &reason) != RAIL_DECODER_RECORD) ||
	    (consumed != sizeof(full)) || (strcmp(order.RemoteApplicationProgram, "a") != 0) ||
	    (strlen(order.RemoteApplicationArguments) != sizeof(full) - sizeof(prefix) - 1U))
		return FALSE;
	rail_decoder_consume(dec, consumed);

	/* Oversize rejection once, then discard through a delimiter split across reads. */
	memset(full, 'x', sizeof(full));
	if (!append(dec, full, sizeof(full)) ||
	    !expect_record(dec, "record exceeds 4096-byte limit", nullptr) ||
	    !append(dec, full, sizeof(full)) ||
	    (rail_decoder_next(dec, &consumed, &order, &reason) != RAIL_DECODER_REJECTED) || reason ||
	    (consumed != sizeof(full)))
		return FALSE;
	rail_decoder_consume(dec, consumed);
	if (!append(dec, "tail\n", 5) ||
	    (rail_decoder_next(dec, &consumed, &order, &reason) != RAIL_DECODER_REJECTED) || reason ||
	    (consumed != 4))
		return FALSE;
	rail_decoder_consume(dec, consumed);
	return append(dec, rest, sizeof(rest) - 1U) && expect_record(dec, nullptr, &b) &&
	       (dec->used == 0) && !dec->discarding;
}

int TestClientRailDecoder(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	RailDecoder dec = WINPR_C_ARRAY_INIT;
	return (test_records(&dec) && test_buffer(&dec)) ? 0 : -1;
}
