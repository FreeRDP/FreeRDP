/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Basic Encoding Rules (BER) Unit Tests
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

#include "test_ber.h"
#include "libfreerdp-core/ber.h"

int init_ber_suite(void)
{
	return 0;
}

int clean_ber_suite(void)
{
	return 0;
}

int add_ber_suite(void)
{
	add_test_suite(ber);

	add_test_function(ber_write_length);
	add_test_function(ber_write_universal_tag);
	add_test_function(ber_write_application_tag);

	return 0;
}

uint8 ber_length_expected_1[1] = "\x40"; /* 64 */
uint8 ber_length_expected_2[3] = "\x82\x01\x94"; /* 404 */

void test_ber_write_length(void)
{
	STREAM *s1, *s2;

	s1 = stream_new(sizeof(ber_length_expected_1));
	s2 = stream_new(sizeof(ber_length_expected_2));

	ber_write_length(s1, 64);
	ASSERT_STREAM(s1, (uint8*) ber_length_expected_1, sizeof(ber_length_expected_1));

	ber_write_length(s2, 404);
	ASSERT_STREAM(s2, (uint8*) ber_length_expected_2, sizeof(ber_length_expected_2));

	stream_free(s1);
	stream_free(s2);
}

/* BOOLEAN, length 1, without value */
uint8 ber_universal_tag_expected[1] = "\x01";

void test_ber_write_universal_tag(void)
{
	STREAM* s;

	s = stream_new(sizeof(ber_universal_tag_expected));
	ber_write_universal_tag(s, 1, false);

	ASSERT_STREAM(s, (uint8*) ber_universal_tag_expected, sizeof(ber_universal_tag_expected));

	stream_free(s);
}

/* T.125 MCS Application 101 (Connect-Initial), length 404 */
uint8 ber_application_tag_expected[5] = "\x7F\x65\x82\x01\x94";

void test_ber_write_application_tag(void)
{
	STREAM* s;

	s = stream_new(sizeof(ber_application_tag_expected));
	ber_write_application_tag(s, 101, 404);

	ASSERT_STREAM(s, (uint8*) ber_application_tag_expected, sizeof(ber_application_tag_expected));

	stream_free(s);
}
