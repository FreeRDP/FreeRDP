/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Rectangle Utils
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

#include <freerdp/utils/rect.h>

void freerdp_read_rectangle_16(STREAM* s, RECTANGLE_16* rectangle_16)
{
	stream_read_uint16(s, rectangle_16->left); /* left (2 bytes) */
	stream_read_uint16(s, rectangle_16->top); /* top (2 bytes) */
	stream_read_uint16(s, rectangle_16->right); /* right (2 bytes) */
	stream_read_uint16(s, rectangle_16->bottom); /* bottom (2 bytes) */
}

void freerdp_write_rectangle_16(STREAM* s, RECTANGLE_16* rectangle_16)
{
	stream_write_uint16(s, rectangle_16->left); /* left (2 bytes) */
	stream_write_uint16(s, rectangle_16->top); /* top (2 bytes) */
	stream_write_uint16(s, rectangle_16->right); /* right (2 bytes) */
	stream_write_uint16(s, rectangle_16->bottom); /* bottom (2 bytes) */
}

RECTANGLE_16* freerdp_rectangle_16_new(uint16 left, uint16 top, uint16 right, uint16 bottom)
{
	RECTANGLE_16* rectangle_16 = xnew(RECTANGLE_16);

	rectangle_16->left = left;
	rectangle_16->top = top;
	rectangle_16->right = right;
	rectangle_16->bottom = bottom;

	return rectangle_16;
}

void freerdp_rectangle_16_free(RECTANGLE_16* rectangle_16)
{
	xfree(rectangle_16);
}
