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
	BYTE* p;
	BYTE* end;
	BYTE* data;
	size_t size;
};
typedef struct stream Stream;
typedef struct stream* PStream;

WINPR_API PStream PStreamAlloc(size_t size);
WINPR_API void StreamAlloc(PStream s, size_t size);
WINPR_API void StreamReAlloc(PStream s, size_t size);
WINPR_API PStream PStreamAllocAttach(BYTE* data, size_t size);
WINPR_API void StreamAllocAttach(PStream s, BYTE* data, size_t size);
WINPR_API void PStreamFree(PStream s);
WINPR_API void StreamFree(PStream s);
WINPR_API void PStreamFreeDetach(PStream s);
WINPR_API void StreamFreeDetach(PStream s);
WINPR_API void StreamAttach(PStream s, BYTE* data, size_t size);
WINPR_API void StreamDetach(PStream s);

#define StreamRead_UINT8(_s, _v) do { _v = \
	*_s->p++; } while (0)

#define StreamRead_UINT16(_s, _v) do { _v = \
	(UINT16)(*_s->p) + \
	(((UINT16)(*(_s->p + 1))) << 8); \
	_s->p += 2; } while (0)

#define StreamRead_UINT16_BE(_s, _v) do { _v = \
	(((UINT16)(*_s->p)) << 8) + \
	(UINT16)(*(_s->p + 1)); \
	_s->p += 2; } while (0)

#define StreamRead_UINT32(_s, _v) do { _v = \
	(UINT32)(*_s->p) + \
	(((UINT32)(*(_s->p + 1))) << 8) + \
	(((UINT32)(*(_s->p + 2))) << 16) + \
	(((UINT32)(*(_s->p + 3))) << 24); \
	_s->p += 4; } while (0)

#define StreamRead_UINT32_BE(_s, _v) do { _v = \
	(((uint32)(*(_s->p))) << 24) + \
	(((uint32)(*(_s->p + 1))) << 16) + \
	(((uint32)(*(_s->p + 2))) << 8) + \
	(((uint32)(*(_s->p + 3)))); \
	_s->p += 4; } while (0)

#define StreamRead_UINT64(_s, _v) do { _v = \
	(UINT64)(*_s->p) + \
	(((UINT64)(*(_s->p + 1))) << 8) + \
	(((UINT64)(*(_s->p + 2))) << 16) + \
	(((UINT64)(*(_s->p + 3))) << 24) + \
	(((UINT64)(*(_s->p + 4))) << 32) + \
	(((UINT64)(*(_s->p + 5))) << 40) + \
	(((UINT64)(*(_s->p + 6))) << 48) + \
	(((UINT64)(*(_s->p + 7))) << 56); \
	_s->p += 8; } while (0)

#define StreamRead(_s, _b, _n) do { \
	memcpy(_b, (_s->p), (_n)); \
	_s->p += (_n); \
	} while (0)

#define StreamWrite_UINT8(_s, _v) do { \
	*_s->p++ = (UINT8)(_v); } while (0)

#define StreamWrite_UINT16(_s, _v) do { \
	*_s->p++ = (_v) & 0xFF; \
	*_s->p++ = ((_v) >> 8) & 0xFF; } while (0)

#define StreamWrite_UINT16_BE(_s, _v) do { \
	*_s->p++ = ((_v) >> 8) & 0xFF; \
	*_s->p++ = (_v) & 0xFF; } while (0)

#define StreamWrite_UINT32(_s, _v) do { \
	*_s->p++ = (_v) & 0xFF; \
	*_s->p++ = ((_v) >> 8) & 0xFF; \
	*_s->p++ = ((_v) >> 16) & 0xFF; \
	*_s->p++ = ((_v) >> 24) & 0xFF; } while (0)

#define StreamWrite_UINT32_BE(_s, _v) do { \
	StreamWrite_UINT16_BE(_s, ((_v) >> 16 & 0xFFFF)); \
	StreamWrite_UINT16_BE(_s, ((_v) & 0xFFFF)); \
	} while (0)

#define StreamWrite_UINT64(_s, _v) do { \
	*_s->p++ = (UINT64)(_v) & 0xFF; \
	*_s->p++ = ((UINT64)(_v) >> 8) & 0xFF; \
	*_s->p++ = ((UINT64)(_v) >> 16) & 0xFF; \
	*_s->p++ = ((UINT64)(_v) >> 24) & 0xFF; \
	*_s->p++ = ((UINT64)(_v) >> 32) & 0xFF; \
	*_s->p++ = ((UINT64)(_v) >> 40) & 0xFF; \
	*_s->p++ = ((UINT64)(_v) >> 48) & 0xFF; \
	*_s->p++ = ((UINT64)(_v) >> 56) & 0xFF; } while (0)

#define StreamWrite(_s, _b, _n) do { \
	memcpy(_s->p, (_b), (_n)); \
	_s->p += (_n); \
	} while (0)

#define StreamPeek_UINT8(_s, _v) do { _v = \
	*_s->p; } while (0)

#define StreamPeek_UINT16(_s, _v) do { _v = \
	(UINT16)(*_s->p) + \
	(((UINT16)(*(_s->p + 1))) << 8); \
	} while (0)

#define StreamPeek_UINT32(_s, _v) do { _v = \
	(UINT32)(*_s->p) + \
	(((UINT32)(*(_s->p + 1))) << 8) + \
	(((UINT32)(*(_s->p + 2))) << 16) + \
	(((UINT32)(*(_s->p + 3))) << 24); \
	} while (0)

#define StreamPeek_UINT64(_s, _v) do { _v = \
	(UINT64)(*_s->p) + \
	(((UINT64)(*(_s->p + 1))) << 8) + \
	(((UINT64)(*(_s->p + 2))) << 16) + \
	(((UINT64)(*(_s->p + 3))) << 24) + \
	(((UINT64)(*(_s->p + 4))) << 32) + \
	(((UINT64)(*(_s->p + 5))) << 40) + \
	(((UINT64)(*(_s->p + 6))) << 48) + \
	(((UINT64)(*(_s->p + 7))) << 56); \
	} while (0)

#define StreamPeek(_s, _b, _n) do { \
	memcpy(_b, (_s->p), (_n)); \
	} while (0)

#define StreamSeek(_s,_offset)		_s->p += (_offset)
#define StreamRewind(_s,_offset)	_s->p -= (_offset)

#define StreamSeek_UINT8(_s)		StreamSeek(_s, 1)
#define StreamSeek_UINT16(_s)		StreamSeek(_s, 2)
#define StreamSeek_UINT32(_s)		StreamSeek(_s, 4)
#define StreamSeek_UINT64(_s)		StreamSeek(_s, 8)

#define StreamRewind_UINT8(_s)		StreamRewind(_s, 1)
#define StreamRewind_UINT16(_s)		StreamRewind(_s, 2)
#define StreamRewind_UINT32(_s)		StreamRewind(_s, 4)
#define StreamRewind_UINT64(_s)		StreamRewind(_s, 8)

#define StreamZero(_s, _n) do { \
	memset(_s->p, '\0', (_n)); \
	_s->p += (_n); \
	} while (0)

#define StreamFill(_s, _v, _n) do { \
	memset(_s->p, _v, (_n)); \
	_s->p += (_n); \
	} while (0)

#define StreamCopy(_dst, _src, _n) do { \
	memcpy(_dst->p, _src->p, _n); \
	_dst->p += _n; \
	_src->p += _n; \
	} while (0)

#define StreamGetOffset(_s)		(_s->p - _s->data)
#define StreamSetOffset(_s, _m)		_s->p = _s->data + (_m)

#define StreamSeal(_s)			_s->end = (_s->p - _s->data)

#define StreamGetMark(_s, _m)		_m = _s->p
#define StreamSetMark(_s, _m)		_s->p = _m

#define StreamGetData(_s)		_s->data
#define StreamGetPointer(_s)		_s->p
#define StreamSetPointer(_s, _m)	_s->p = _m
#define StreamGetEnd(_s)		_s->end
#define StreamGetSize(_s)		_s->size

#define StreamSize(_s)			(_s->p - _s->data)
#define StreamSealedSize(_s)		(_s->end - _s->data)
#define StreamRemainingSize(_s)		(_s->size - (_s->p - _s->data))

#endif /* WINPR_UTILS_STREAM_H */
