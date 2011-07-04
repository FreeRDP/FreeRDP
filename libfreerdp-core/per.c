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
