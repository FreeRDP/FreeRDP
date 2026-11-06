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

static BOOL expect_record(RailDecoder* dec, const char* data, size_t length, const char* reason,
                          const RAIL_EXEC_ORDER* expected)
{
	size_t consumed = 0;
	const char* actualReason = nullptr;
	RAIL_EXEC_ORDER order = WINPR_C_ARRAY_INIT;
	const RailDecoderResult result =
	    rail_decoder_feed(dec, (const BYTE*)data, length, &consumed, &order, &actualReason);
	BOOL ok = (consumed == length);
	if (reason)
		ok = ok && (result == RAIL_DECODER_REJECTED) && actualReason &&
		     (strcmp(reason, actualReason) == 0);
	else
	{
		ok = ok && (result == RAIL_DECODER_RECORD) && !actualReason;
		if (ok && expected)
			ok =
			    (order.flags == expected->flags) &&
			    (strcmp(order.RemoteApplicationProgram, expected->RemoteApplicationProgram) == 0) &&
			    (strcmp(order.RemoteApplicationWorkingDir, expected->RemoteApplicationWorkingDir) ==
			     0) &&
			    (strcmp(order.RemoteApplicationArguments, expected->RemoteApplicationArguments) ==
			     0);
	}
	if (!ok)
		(void)fprintf(stderr, "decoder: result=%d, consumed=%" PRIuz "/%" PRIuz ", reason=%s\n",
		              result, consumed, length, actualReason ? actualReason : "none");
	return ok;
}

/* D1-D4: complete records use the same feed path as the transport. */
static BOOL test_records(RailDecoder* dec)
{
	const struct
	{
		const char* record;
		const char* reason;
		RAIL_EXEC_ORDER order;
	} cases[] = {
		{ "program=app\n\n", nullptr, { 0, "app", "", "" } },
		{ "arguments=a=b\nflags=0x4\nworking-directory=dir\nprogram=app\n\n",
		  nullptr,
		  { 4, "app", "dir", "a=b" } },
		{ "program=app\nworking-directory=\narguments=\nflags=\n\n",
		  nullptr,
		  { 0, "app", "", "" } },
		{ "program=app\nunknown=x\n\n", "record contains an unknown key", { 0 } },
		{ "program=a\nprogram=b\n\n", "program key is duplicated", { 0 } },
		{ "program=a\nworking-directory=\nworking-directory=\n\n",
		  "working-directory key is duplicated",
		  { 0 } },
		{ "program=a\narguments=\narguments=\n\n", "arguments key is duplicated", { 0 } },
		{ "program=a\nflags=\nflags=\n\n", "flags key is duplicated", { 0 } },
		{ "program\n\n", "line has no valid key separator", { 0 } },
		{ "=app\n\n", "line has no valid key separator", { 0 } },
		{ "program=app\nflags=6\n\n", nullptr, { 6, "app", "", "" } },
		{ "program=app\nflags=08\n\n", nullptr, { 8, "app", "", "" } },
		{ "program=app\nflags=016\n\n", nullptr, { 16, "app", "", "" } },
		{ "program=app\nflags=010\n\n", "flags value is invalid", { 0 } },
		{ "program=app\nflags=65536\n\n", "flags value is invalid", { 0 } },
		{ "program=app\nflags=32\n\n", "flags value is invalid", { 0 } },
		{ "program=app\nflags=2\n\n", "flags value is invalid", { 0 } },
		{ "program=app\nflags=x\n\n", "flags value is invalid", { 0 } },
		{ "program=app\nflags=0x\n\n", "flags value is invalid", { 0 } },
		{ "program=\n\n", "program is missing or empty", { 0 } },
		{ "arguments=a\n\n", "program is missing or empty", { 0 } },
		{ "program=\xc0\xaf\n\n", "program is not valid UTF-8", { 0 } },
		{ "program=app\nworking-directory=\xed\xa0\x80\n\n",
		  "working directory is not valid UTF-8",
		  { 0 } },
		{ "program=app\narguments=\xf4\x90\x80\x80\n\n", "arguments are not valid UTF-8", { 0 } },
	};
	for (size_t x = 0; x < ARRAYSIZE(cases); x++)
	{
		if (!expect_record(dec, cases[x].record, strlen(cases[x].record), cases[x].reason,
		                   &cases[x].order))
		{
			(void)fprintf(stderr, "record case %" PRIuz " failed\n", x);
			return FALSE;
		}
	}
	const char nul[] = "program=a\0b\n\n";
	return expect_record(dec, nul, sizeof(nul) - 1U, "record contains NUL", nullptr);
}

/* D4: the decoder's 16384-byte frame permits every RAIL boundary to be reached. */
static BOOL test_rail_limits(RailDecoder* dec)
{
	const struct
	{
		const char* prefix;
		size_t limit;
		const char* reason;
	} cases[] = {
		{ "program=", 260, "converted program length is outside the RAIL limit" },
		{ "program=app\nworking-directory=", 260,
		  "converted working-directory length exceeds the RAIL limit" },
		{ "program=app\narguments=", 8000, "converted argument length exceeds the RAIL limit" },
	};
	char record[16384] = WINPR_C_ARRAY_INIT;
	for (size_t x = 0; x < ARRAYSIZE(cases); x++)
	{
		const size_t prefix = strlen(cases[x].prefix);
		memcpy(record, cases[x].prefix, prefix);
		for (size_t extra = 0; extra <= 1; extra++)
		{
			const size_t end = prefix + cases[x].limit + extra;
			memset(&record[prefix], 'a', cases[x].limit + extra);
			record[end] = '\n';
			record[end + 1U] = '\n';
			if (!expect_record(dec, record, end + 2U, extra ? cases[x].reason : nullptr, nullptr))
				return FALSE;
		}
	}
	return TRUE;
}

/* D5: partial feeds, consumed at the first terminator, and over-limit resync. */
static BOOL test_framing(void)
{
	RailDecoder dec = WINPR_C_ARRAY_INIT;
	const char record[] = "program=a\n\n";
	const char two[] = "program=a\n\nprogram=a\n\n";
	const RAIL_EXEC_ORDER expected = { 0, "a", "", "" };
	const size_t size = sizeof(record) - 1U;
	size_t consumed = 0;
	const char* reason = nullptr;
	RAIL_EXEC_ORDER order = WINPR_C_ARRAY_INIT;
	BOOL ok = FALSE;
	if (!rail_decoder_init(&dec, size))
		return FALSE;
	if ((rail_decoder_feed(&dec, (const BYTE*)record, 4, &consumed, &order, &reason) !=
	     RAIL_DECODER_INCOMPLETE) ||
	    (consumed != 4) || reason ||
	    !expect_record(&dec, &record[4], size - 4U, nullptr, &expected))
		goto out;
	if ((rail_decoder_feed(&dec, (const BYTE*)two, sizeof(two) - 1U, &consumed, &order, &reason) !=
	     RAIL_DECODER_RECORD) ||
	    (consumed != size) || reason || (strcmp(order.RemoteApplicationProgram, "a") != 0) ||
	    !expect_record(&dec, &two[consumed], sizeof(two) - 1U - consumed, nullptr, &expected))
		goto out;
	if (!expect_record(&dec, record, size, nullptr, &expected) ||
	    !expect_record(&dec, "program=ab\n\n", size + 1U,
	                   "record exceeds the FIFO atomic-write limit", nullptr) ||
	    !expect_record(&dec, record, size, nullptr, &expected))
		goto out;
	ok = TRUE;
out:
	rail_decoder_uninit(&dec);
	return ok;
}

int TestClientRailDecoder(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	RailDecoder dec = WINPR_C_ARRAY_INIT;
	if (!rail_decoder_init(&dec, 16384))
		return -1;
	const BOOL ok = test_records(&dec) && test_rail_limits(&dec) && test_framing();
	rail_decoder_uninit(&dec);
	return ok ? 0 : -1;
}
