/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Stream Unit Tests
 *
 * Copyright 2011 Vic Lee
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
#include <stdlib.h>
#include <freerdp/freerdp.h>
#include <freerdp/utils/hexdump.h>
#include <freerdp/utils/stream.h>

#include "test_stream.h"

int init_stream_suite(void)
{
	return 0;
}

int clean_stream_suite(void)
{
	return 0;
}

int add_stream_suite(void)
{
	add_test_suite(stream);

	add_test_function(stream);

	return 0;
}

void test_stream(void)
{
	STREAM * stream;
	int pos;
	uint32 n;
	uint64 n64;

	stream = stream_new(1);
	pos = stream_get_pos(stream);

	stream_write_uint8(stream, 0xFE);

	stream_check_size(stream, 14);
	stream_write_uint16(stream, 0x0102);
	stream_write_uint32(stream, 0x03040506);
	stream_write_uint64(stream, 0x0708091011121314LL);

	/* freerdp_hexdump(stream->buffer, 15); */

	stream_set_pos(stream, pos);
	stream_seek(stream, 3);
	stream_read_uint32(stream, n);
	stream_read_uint64(stream, n64);

	CU_ASSERT(n == 0x03040506);
	CU_ASSERT(n64 == 0x0708091011121314LL);

	stream_free(stream);
}
