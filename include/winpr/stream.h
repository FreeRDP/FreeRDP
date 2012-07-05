/*
 * WinPR: Windows Portable Runtime
 * Stream Utils
 *
 * Copyright 2011 Vic Lee
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef WINPR_UTILS_STREAM_H
#define WINPR_UTILS_STREAM_H

#include <winpr/winpr.h>
#include <winpr/wtypes.h>

struct stream
{
	BYTE* pointer;
	BYTE* buffer;
	size_t size;
	size_t length;
};
typedef struct stream Stream;
typedef struct stream* PStream;

WINPR_API PStream PStreamAlloc(size_t size);
WINPR_API void StreamAlloc(PStream s, size_t size);
WINPR_API void StreamReAlloc(PStream s, size_t size);
WINPR_API PStream PStreamAllocAttach(BYTE* buffer, size_t size);
WINPR_API void StreamAllocAttach(PStream s, BYTE* buffer, size_t size);
WINPR_API void PStreamFree(PStream s);
WINPR_API void StreamFree(PStream s);
WINPR_API void PStreamFreeDetach(PStream s);
WINPR_API void StreamFreeDetach(PStream s);
WINPR_API void StreamAttach(PStream s, BYTE* buffer, size_t size);
WINPR_API void StreamDetach(PStream s);

#define StreamRead_UINT8(_s, _v) do { _v = \
	*_s->pointer++; } while (0)

#define StreamRead_UINT16(_s, _v) do { _v = \
	(UINT16)(*_s->pointer) + \
	(((UINT16)(*(_s->pointer + 1))) << 8); \
	_s->pointer += 2; } while (0)

#define StreamRead_UINT16_BE(_s, _v) do { _v = \
	(((UINT16)(*_s->pointer)) << 8) + \
	(UINT16)(*(_s->pointer + 1)); \
	_s->pointer += 2; } while (0)

#define StreamRead_UINT32(_s, _v) do { _v = \
	(UINT32)(*_s->pointer) + \
	(((UINT32)(*(_s->pointer + 1))) << 8) + \
	(((UINT32)(*(_s->pointer + 2))) << 16) + \
	(((UINT32)(*(_s->pointer + 3))) << 24); \
	_s->pointer += 4; } while (0)

#define StreamRead_UINT32_BE(_s, _v) do { _v = \
	(((uint32)(*(_s->pointer))) << 24) + \
	(((uint32)(*(_s->pointer + 1))) << 16) + \
	(((uint32)(*(_s->pointer + 2))) << 8) + \
	(((uint32)(*(_s->pointer + 3)))); \
	_s->pointer += 4; } while (0)

#define StreamRead_UINT64(_s, _v) do { _v = \
	(UINT64)(*_s->pointer) + \
	(((UINT64)(*(_s->pointer + 1))) << 8) + \
	(((UINT64)(*(_s->pointer + 2))) << 16) + \
	(((UINT64)(*(_s->pointer + 3))) << 24) + \
	(((UINT64)(*(_s->pointer + 4))) << 32) + \
	(((UINT64)(*(_s->pointer + 5))) << 40) + \
	(((UINT64)(*(_s->pointer + 6))) << 48) + \
	(((UINT64)(*(_s->pointer + 7))) << 56); \
	_s->pointer += 8; } while (0)

#define StreamRead(_s, _b, _n) do { \
	memcpy(_b, (_s->pointer), (_n)); \
	_s->pointer += (_n); \
	} while (0)

#define StreamWrite_UINT8(_s, _v) do { \
	*_s->pointer++ = (UINT8)(_v); } while (0)

#define StreamWrite_UINT16(_s, _v) do { \
	*_s->pointer++ = (_v) & 0xFF; \
	*_s->pointer++ = ((_v) >> 8) & 0xFF; } while (0)

#define StreamWrite_UINT16_BE(_s, _v) do { \
	*_s->pointer++ = ((_v) >> 8) & 0xFF; \
	*_s->pointer++ = (_v) & 0xFF; } while (0)

#define StreamWrite_UINT32(_s, _v) do { \
	*_s->pointer++ = (_v) & 0xFF; \
	*_s->pointer++ = ((_v) >> 8) & 0xFF; \
	*_s->pointer++ = ((_v) >> 16) & 0xFF; \
	*_s->pointer++ = ((_v) >> 24) & 0xFF; } while (0)

#define StreamWrite_UINT32_BE(_s, _v) do { \
	StreamWrite_UINT16_BE(_s, ((_v) >> 16 & 0xFFFF)); \
	StreamWrite_UINT16_BE(_s, ((_v) & 0xFFFF)); \
	} while (0)

#define StreamWrite_UINT64(_s, _v) do { \
	*_s->pointer++ = (UINT64)(_v) & 0xFF; \
	*_s->pointer++ = ((UINT64)(_v) >> 8) & 0xFF; \
	*_s->pointer++ = ((UINT64)(_v) >> 16) & 0xFF; \
	*_s->pointer++ = ((UINT64)(_v) >> 24) & 0xFF; \
	*_s->pointer++ = ((UINT64)(_v) >> 32) & 0xFF; \
	*_s->pointer++ = ((UINT64)(_v) >> 40) & 0xFF; \
	*_s->pointer++ = ((UINT64)(_v) >> 48) & 0xFF; \
	*_s->pointer++ = ((UINT64)(_v) >> 56) & 0xFF; } while (0)

#define StreamWrite(_s, _b, _n) do { \
	memcpy(_s->pointer, (_b), (_n)); \
	_s->pointer += (_n); \
	} while (0)

#define StreamPeek_UINT8(_s, _v) do { _v = \
	*_s->pointer; } while (0)

#define StreamPeek_UINT16(_s, _v) do { _v = \
	(UINT16)(*_s->pointer) + \
	(((UINT16)(*(_s->pointer + 1))) << 8); \
	} while (0)

#define StreamPeek_UINT32(_s, _v) do { _v = \
	(UINT32)(*_s->pointer) + \
	(((UINT32)(*(_s->pointer + 1))) << 8) + \
	(((UINT32)(*(_s->pointer + 2))) << 16) + \
	(((UINT32)(*(_s->pointer + 3))) << 24); \
	} while (0)

#define StreamPeek_UINT64(_s, _v) do { _v = \
	(UINT64)(*_s->pointer) + \
	(((UINT64)(*(_s->pointer + 1))) << 8) + \
	(((UINT64)(*(_s->pointer + 2))) << 16) + \
	(((UINT64)(*(_s->pointer + 3))) << 24) + \
	(((UINT64)(*(_s->pointer + 4))) << 32) + \
	(((UINT64)(*(_s->pointer + 5))) << 40) + \
	(((UINT64)(*(_s->pointer + 6))) << 48) + \
	(((UINT64)(*(_s->pointer + 7))) << 56); \
	} while (0)

#define StreamPeek(_s, _b, _n) do { \
	memcpy(_b, (_s->pointer), (_n)); \
	} while (0)

#define StreamSeek(_s,_offset)		_s->pointer += (_offset)
#define StreamRewind(_s,_offset)	_s->pointer -= (_offset)

#define StreamSeek_UINT8(_s)		StreamSeek(_s, 1)
#define StreamSeek_UINT16(_s)		StreamSeek(_s, 2)
#define StreamSeek_UINT32(_s)		StreamSeek(_s, 4)
#define StreamSeek_UINT64(_s)		StreamSeek(_s, 8)

#define StreamRewind_UINT8(_s)		StreamRewind(_s, 1)
#define StreamRewind_UINT16(_s)		StreamRewind(_s, 2)
#define StreamRewind_UINT32(_s)		StreamRewind(_s, 4)
#define StreamRewind_UINT64(_s)		StreamRewind(_s, 8)

#define StreamZero(_s, _n) do { \
	memset(_s->pointer, '\0', (_n)); \
	_s->pointer += (_n); \
	} while (0)

#define StreamFill(_s, _v, _n) do { \
	memset(_s->pointer, _v, (_n)); \
	_s->pointer += (_n); \
	} while (0)

#define StreamCopy(_dst, _src, _n) do { \
	memcpy(_dst->pointer, _src->pointer, _n); \
	_dst->pointer += _n; \
	_src->pointer += _n; \
	} while (0)

#define StreamGetPointer(_s, _p)	_p = _s->pointer
#define StreamSetPointer(_s, _p)	_s->pointer = _p

#define StreamGetPosition(_s)		(_s->pointer - _s->buffer)
#define StreamSetPosition(_s, _p)	_s->pointer = _s->buffer + (_p)

#define StreamPointer(_s)		_s->pointer
#define StreamBuffer(_s)		_s->buffer
#define StreamSize(_s)			_s->size
#define StreamLength(_s)		_s->length

#define StreamRemainingSize(_s)		(_s->size - (_s->pointer - _s->buffer))
#define StreamRemainingLength(_s)	(_s->length - (_s->pointer - _s->buffer))

#endif /* WINPR_UTILS_STREAM_H */
/* Modeline for vim. Don't delete */
/* vim: set cindent:noet:sw=8:ts=8 */
