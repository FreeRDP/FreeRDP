/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Packed Encoding Rules (PER) Unit Tests
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <freerdp/freerdp.h>
#include <freerdp/utils/hexdump.h>
#include <freerdp/utils/stream.h>

#include "test_per.h"
#include "libfreerdp-core/per.h"

int init_per_suite(void)
{
	return 0;
}

int clean_per_suite(void)
{
	return 0;
}

int add_per_suite(void)
{
	add_test_suite(per);

	add_test_function(per_write_length);
	add_test_function(per_write_object_identifier);

	return 0;
}

uint8 per_length_expected[2] = "\x81\x2A";

void test_per_write_length(void)
{
	STREAM* s = stream_new(2);
	per_write_length(s, 298);
	ASSERT_STREAM(s, (uint8*) per_length_expected, sizeof(per_length_expected));
}

uint8 per_oid[6] = { 0, 0, 20, 124, 0, 1 };
uint8 per_oid_expected[6] = "\x05\x00\x14\x7C\x00\x01";

void test_per_write_object_identifier(void)
{
	STREAM* s = stream_new(6);
	per_write_object_identifier(s, per_oid);
	ASSERT_STREAM(s, (uint8*) per_oid_expected, sizeof(per_oid_expected));
}
