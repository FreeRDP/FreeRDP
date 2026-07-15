/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDSTLS unit test
 *
 * Copyright 2026 Armin Novak <anovak@thincast.com>
 * Copyright 2026 Thincast Technologies GmbH
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

#include <winpr/wtypes.h>
#include <winpr/wlog.h>

#include "../rdstls.h"

/* PDU structure:
 *
 * - 2 bytes version
 * - 2 bytes type
 */
struct test_case_t
{
	SSIZE_T expected;
	size_t len;
	char* data;
};

static const struct test_case_t tests[] = {
	{ 0, 1, "a" },
	{ 0, 2, "\x01\x00" },
	{ 0, 2, "\x02\x00" },
	{ -1, 2, "\x00\x00" },
	{ -1, 2, "\x03\x00" },
	{ 0, 3, "\x01\x00\x00" },
	{ -1, 4, "\x01\x00\x00\x00" },
	{ 8, 4, "\x01\x00\x01\x00" },
	{ 0, 4, "\x01\x00\x02\x00" },
	{ -1, 4, "\x01\x00\x03\x00" },
	{ 10, 4, "\x01\x00\x04\x00" },
	{ -1, 4, "\x01\x00\x05\x00" },
	{ -1, 4, "\x01\x00\x06\x00" },
	{ 0, 5, "\x01\x00\x02\x00\x00" },
	{ -1, 6, "\x01\x00\x02\x00\x00\x00" },
	{ -1, 6, "\x01\x00\x02\x00\x04\x00" },
	{ -1, 6, "\x01\x00\x02\x00\x05\x00" },
	{ -1, 6, "\x01\x00\x02\x00\x06\x00" },
	{ 0, 6, "\x01\x00\x02\x00\x01\x00" },
	{ 0, 6, "\x01\x00\x02\x00\x02\x00" },
	{ 0, 6, "\x01\x00\x02\x00\x03\x00" },
	{ -1, 12, "\x01\x00\x02\x00\x03\x00\x00\x00\x00\x00\x00\x00" },
	{ 12 + 1, 12, "\x01\x00\x02\x00\x03\x00\x00\x00\x00\x00\x01\x00" },
	{ 12 + 0x100, 12, "\x01\x00\x02\x00\x03\x00\x00\x00\x00\x00\x00\x01" },
	{ 12 + 0x3412, 12, "\x01\x00\x02\x00\x03\x00\x00\x00\x00\x00\x12\x34" },
	{ -1, 12, "\x01\x00\x02\x00\x02\x00\x00\x00\x00\x00\x00\x00" },
	{ 12 + 1, 12, "\x01\x00\x02\x00\x02\x00\x00\x00\x00\x00\x01\x00" },
	{ 12 + 0x100, 12, "\x01\x00\x02\x00\x02\x00\x00\x00\x00\x00\x00\x01" },
	{ 12 + 0x3412, 12, "\x01\x00\x02\x00\x02\x00\x00\x00\x00\x00\x12\x34" },
	{ -1, 14, "\x01\x00\x02\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00" },
	{ 14 + 1, 15, "\x01\x00\x02\x00\x01\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00" },
	{ -1, 15, "\x01\x00\x02\x00\x01\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00" },
	{ 14 + 2, 16, "\x01\x00\x02\x00\x01\x00\x00\x00\x00\x00\x01\x00\x00\x01\x00\x00" },
	{ 14 + 2, 16, "\x01\x00\x02\x00\x01\x00\x00\x00\x01\x00\x00\x00\x00\x01\x00\x00" },
	{ 14 + 2, 16, "\x01\x00\x02\x00\x01\x00\x01\x00\x00\x00\x00\x00\x00\x01\x00\x00" },
};

#define RDSTLS_DATA_PASSWORD_CREDS 0x01
#define RDSTLS_DATA_AUTORECONNECT_COOKIE 0x02
#define RDSTLS_DATA_FEDAUTH_TOKEN 0x03

static bool test_parse(void)
{
	wLog* log = WLog_Get(__func__);

	for (size_t x = 0; x < ARRAYSIZE(tests); x++)
	{
		wStream buffer = WINPR_C_ARRAY_INIT;
		const struct test_case_t* cur = &tests[x];
		wStream* s = Stream_StaticConstInit(&buffer, cur->data, cur->len);
		const SSIZE_T rc = rdstls_parse_pdu(log, s);
		if (rc != cur->expected)
		{
			WLog_Print(log, WLOG_ERROR, "Test case %" PRIuz " failed", x);
			return false;
		}
	}
	return true;
}

int TestRdstls(WINPR_ATTR_UNUSED int argc, WINPR_ATTR_UNUSED char* argv[])
{
	if (!test_parse())
		return -1;

	return 0;
}
