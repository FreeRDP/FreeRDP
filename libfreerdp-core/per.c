/**
 * FreeRDP: A Remote Desktop Protocol Client
 * ASN.1 Packed Encoding Rules (BER)
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

#include "per.h"

void
per_write_length(STREAM* s, int length)
{
	if (length > 0x7F)
		stream_write_uint16_be(s, (length | 0x8000));
	else
		stream_write_uint8(s, length);
}

void
per_write_choice(STREAM* s, uint8 choice)
{
	stream_write_uint8(s, choice);
}

void
per_write_selection(STREAM* s, uint8 selection)
{
	stream_write_uint8(s, selection);
}

void
per_write_number_of_sets(STREAM* s, uint8 number)
{
	stream_write_uint8(s, number);
}

void
per_write_padding(STREAM* s, int length)
{
	int i;

	for (i = 0; i < length; i++)
		stream_write_uint8(s, 0);
}

void
per_write_object_identifier(STREAM* s, uint8 oid[6])
{
	uint8 t12 = (oid[0] << 4) & (oid[1] & 0x0F);
	stream_write_uint8(s, 5); /* length */
	stream_write_uint8(s, t12); /* first two tuples */
	stream_write_uint8(s, oid[2]); /* tuple 3 */
	stream_write_uint8(s, oid[3]); /* tuple 4 */
	stream_write_uint8(s, oid[4]); /* tuple 5 */
	stream_write_uint8(s, oid[5]); /* tuple 6 */
}

void
per_write_string(STREAM* s, uint8* str, int length)
{
	int i;

	for (i = 0; i < length; i++)
		stream_write_uint8(s, str[i]);
}

void
per_write_octet_string(STREAM* s, uint8* oct_str, int length, int min)
{
	int i;
	int mlength;

	mlength = (length - min >= 0) ? length - min : min;

	per_write_length(s, mlength);

	for (i = 0; i < length; i++)
		stream_write_uint8(s, oct_str[i]);
}

void
per_write_numeric_string(STREAM* s, uint8* num_str, int length, int min)
{
	int i;
	int mlength;
	uint8 num, c1, c2;

	mlength = (length - min >= 0) ? length - min : min;

	per_write_length(s, mlength);

	for (i = 0; i < length; i += 2)
	{
		c1 = num_str[i];
		c2 = ((i + 1) < length) ? num_str[i + 1] : 0x30;

		c1 = (c1 - 0x30) % 10;
		c2 = (c2 - 0x30) % 10;
		num = (c1 << 4) | c2;

		stream_write_uint8(s, num); /* string */
	}
}
