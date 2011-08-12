/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Remote Applications Integrated Locally (RAIL) Utils
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

#include <freerdp/types.h>
#include <freerdp/utils/memory.h>

#include <freerdp/utils/rail.h>

void rail_unicode_string_alloc(UNICODE_STRING* unicode_string, uint16 cbString)
{
	unicode_string->length = cbString;
	unicode_string->string = xzalloc(cbString);
}

void rail_unicode_string_free(UNICODE_STRING* unicode_string)
{
	unicode_string->length = 0;

	if (unicode_string->string != NULL)
		xfree(unicode_string->string);
}

void rail_read_unicode_string(STREAM* s, UNICODE_STRING* unicode_string)
{
	stream_read_uint16(s, unicode_string->length); /* cbString (2 bytes) */

	if (unicode_string->string == NULL)
		unicode_string->string = (uint8*) xmalloc(unicode_string->length);
	else
		unicode_string->string = (uint8*) xrealloc(unicode_string->string, unicode_string->length);

	stream_read(s, unicode_string->string, unicode_string->length);
}

void rail_write_unicode_string(STREAM* s, UNICODE_STRING* unicode_string)
{
	stream_check_size(s, 2 + unicode_string->length);
	stream_write_uint16(s, unicode_string->length); /* cbString (2 bytes) */
	stream_write(s, unicode_string->string, unicode_string->length); /* string */
}

void rail_write_unicode_string_value(STREAM* s, UNICODE_STRING* unicode_string)
{
	if (unicode_string->length > 0)
	{
		stream_check_size(s, unicode_string->length);
		stream_write(s, unicode_string->string, unicode_string->length); /* string */
	}
}

void rail_read_rectangle_16(STREAM* s, RECTANGLE_16* rectangle_16)
{
	stream_read_uint16(s, rectangle_16->left); /* left (2 bytes) */
	stream_read_uint16(s, rectangle_16->top); /* top (2 bytes) */
	stream_read_uint16(s, rectangle_16->right); /* right (2 bytes) */
	stream_read_uint16(s, rectangle_16->bottom); /* bottom (2 bytes) */
}

void rail_write_rectangle_16(STREAM* s, RECTANGLE_16* rectangle_16)
{
	stream_write_uint16(s, rectangle_16->left); /* left (2 bytes) */
	stream_write_uint16(s, rectangle_16->top); /* top (2 bytes) */
	stream_write_uint16(s, rectangle_16->right); /* right (2 bytes) */
	stream_write_uint16(s, rectangle_16->bottom); /* bottom (2 bytes) */
}

