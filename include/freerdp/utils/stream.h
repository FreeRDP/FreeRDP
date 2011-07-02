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
	int capacity;
};

STREAM *
stream_new(int capacity);
void
stream_free(STREAM * stream);

void
stream_extend(STREAM * stream);
#define stream_check_capacity(_s,_n) \
	while (_s->ptr - _s->buffer + (_n) > _s->capacity) \
		stream_extend(_s)

#define stream_get_pos(_s) (_s->ptr - _s->buffer)
#define stream_set_pos(_s,_m) _s->ptr = _s->buffer + (_m)
#define stream_seek(_s,_offset) _s->ptr += (_offset)

#define stream_read_uint8(_s, _v) do { _v = *_s->ptr++; } while (0)
#define stream_read_uint16(_s, _v) do { _v = \
	(uint16)(*_s->ptr) + \
	(((uint16)(*(_s->ptr + 1))) << 8); \
	_s->ptr += 2; } while (0)
#define stream_read_uint32(_s, _v) do { _v = \
	(uint32)(*_s->ptr) + \
	(((uint32)(*(_s->ptr + 1))) << 8) + \
	(((uint32)(*(_s->ptr + 2))) << 16) + \
	(((uint32)(*(_s->ptr + 3))) << 24); \
	_s->ptr += 4; } while (0)
#define stream_read_uint64(_s, _v) do { _v = \
	(uint64)(*_s->ptr) + \
	(((uint64)(*(_s->ptr + 1))) << 8) + \
	(((uint64)(*(_s->ptr + 2))) << 16) + \
	(((uint64)(*(_s->ptr + 3))) << 24) + \
	(((uint64)(*(_s->ptr + 4))) << 32) + \
	(((uint64)(*(_s->ptr + 5))) << 40) + \
	(((uint64)(*(_s->ptr + 6))) << 48) + \
	(((uint64)(*(_s->ptr + 7))) << 56); \
	_s->ptr += 8; } while (0)

#define stream_write_uint8(_s, _v) do { \
	*_s->ptr++ = (uint8)(_v); } while (0)
#define stream_write_uint16(_s, _v) do { \
	*_s->ptr++ = (_v) & 0xFF; \
	*_s->ptr++ = ((_v) >> 8) & 0xFF; } while (0)
#define stream_write_uint32(_s, _v) do { \
	*_s->ptr++ = (_v) & 0xFF; \
	*_s->ptr++ = ((_v) >> 8) & 0xFF; \
	*_s->ptr++ = ((_v) >> 16) & 0xFF; \
	*_s->ptr++ = ((_v) >> 24) & 0xFF; } while (0)
#define stream_write_uint64(_s, _v) do { \
	*_s->ptr++ = (_v) & 0xFF; \
	*_s->ptr++ = ((_v) >> 8) & 0xFF; \
	*_s->ptr++ = ((_v) >> 16) & 0xFF; \
	*_s->ptr++ = ((_v) >> 24) & 0xFF; \
	*_s->ptr++ = ((_v) >> 32) & 0xFF; \
	*_s->ptr++ = ((_v) >> 40) & 0xFF; \
	*_s->ptr++ = ((_v) >> 48) & 0xFF; \
	*_s->ptr++ = ((_v) >> 56) & 0xFF; } while (0)

#endif /* __STREAM_UTILS_H */

