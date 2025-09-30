/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2025 Thincast Technologies GmbH
 * Copyright 2025 Armin Novak <anovak@thincast.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <winpr/wtypes.h>
#include <freerdp/codec/color.h>

typedef struct
{
	uint32_t val;
	const char* str;
} test_t;

#define XSTR(s) STR(s)
#define STR(x) #x
#define ENTRY(x) { x, #x }
static const test_t testcases[] = { ENTRY(PIXEL_FORMAT_ARGB32),
	                                ENTRY(PIXEL_FORMAT_XRGB32),
	                                ENTRY(PIXEL_FORMAT_ABGR32),
	                                ENTRY(PIXEL_FORMAT_XBGR32),
	                                ENTRY(PIXEL_FORMAT_BGRA32),
	                                ENTRY(PIXEL_FORMAT_BGRX32),
	                                ENTRY(PIXEL_FORMAT_RGBA32),
	                                ENTRY(PIXEL_FORMAT_RGBX32),
	                                ENTRY(PIXEL_FORMAT_BGRX32_DEPTH30),
	                                ENTRY(PIXEL_FORMAT_RGBX32_DEPTH30),
	                                ENTRY(PIXEL_FORMAT_RGB24),
	                                ENTRY(PIXEL_FORMAT_BGR24),
	                                ENTRY(PIXEL_FORMAT_RGB16),
	                                ENTRY(PIXEL_FORMAT_BGR16),
	                                ENTRY(PIXEL_FORMAT_ARGB15),
	                                ENTRY(PIXEL_FORMAT_RGB15),
	                                ENTRY(PIXEL_FORMAT_ABGR15),
	                                ENTRY(PIXEL_FORMAT_BGR15),
	                                ENTRY(PIXEL_FORMAT_RGB8),
	                                ENTRY(PIXEL_FORMAT_A4),
	                                ENTRY(PIXEL_FORMAT_MONO) };
#undef ENTRY
#undef STR

int TestFreeRDPCodecColor(WINPR_ATTR_UNUSED int argc, WINPR_ATTR_UNUSED char* argv[])
{
	const size_t count = ARRAYSIZE(testcases);
	for (size_t x = 0; x < count; x++)
	{
		const test_t* cur = &testcases[x];

		const char* cmp = cur->str;
		const uint32_t val = FreeRDPGetColorFromatFromName(cmp);
		if (val != cur->val)
			return -1;

		const char* str = FreeRDPGetColorFormatName(cur->val);
		if (!str || (strcmp(str, cmp) != 0))
			return -2;
	}

	return 0;
}
