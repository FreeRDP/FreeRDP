/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Stream Utils
 *
 * Copyright 2009-2011 Jay Sorg
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

#ifndef __STREAM_UTILS_H
#define __STREAM_UTILS_H

#include <freerdp/types/base.h>

typedef struct _STREAM STREAM;
struct _STREAM
{
	uint8 * buffer;
	uint8 * ptr;
	int size;
};

typedef uint8 * STREAM_MARK;

STREAM *
stream_new(int size);
void
stream_free(STREAM * stream);

#define stream_get_mark(_s) _s->ptr
#define stream_set_mark(_s,_m) _s->ptr = _m

#define stream_get_uint8(_s, _v) do { _v = *_s->ptr++; } while (0)
#define stream_get_uint16(_s, _v) do { _v = \
	(uint16)(*_s->ptr) + \
	(((uint16)(*(_s->ptr + 1))) << 8); } while (0)

#define stream_set_uint8(_s, _v) do { *_s->ptr++ = (uint8)(_v); } while (0)
#define stream_set_uint16(_s, _v) do { \
	*_s->ptr++ = (_v) & 0xFF; \
	*_s->ptr++ = (_v) >> 8; } while (0)

#endif /* __STREAM_UTILS_H */

