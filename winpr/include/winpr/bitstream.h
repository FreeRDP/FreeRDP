/*
 * WinPR: Windows Portable Runtime
 * BitStream Utils
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef WINPR_UTILS_BITSTREAM_H
#define WINPR_UTILS_BITSTREAM_H

#include <winpr/winpr.h>
#include <winpr/wtypes.h>

#include <winpr/crt.h>

struct _wBitStream
{
	BYTE* buffer;
	BYTE* pointer;
	DWORD position;
	DWORD length;
	DWORD capacity;
	UINT32 mask;
	UINT32 offset;
	UINT32 prefetch;
	UINT32 accumulator;
};
typedef struct _wBitStream wBitStream;

#define BITDUMP_MSB_FIRST	0x00000001
#define BITDUMP_STDERR		0x00000002

#ifdef __cplusplus
extern "C" {
#endif

#define BitStream_Prefetch(_bs) do { \
	(_bs->prefetch) = 0; \
	if (((UINT32) (_bs->pointer - _bs->buffer)) < (_bs->capacity + 4)) \
		(_bs->prefetch) |= (*(_bs->pointer + 4) << 24); \
	if (((UINT32) (_bs->pointer - _bs->buffer)) < (_bs->capacity + 5)) \
		(_bs->prefetch) |= (*(_bs->pointer + 5) << 16); \
	if (((UINT32) (_bs->pointer - _bs->buffer)) < (_bs->capacity + 6)) \
		(_bs->prefetch) |= (*(_bs->pointer + 6) << 8); \
	if (((UINT32) (_bs->pointer - _bs->buffer)) < (_bs->capacity + 7)) \
		(_bs->prefetch) |= (*(_bs->pointer + 7) << 0); \
} while(0)

#define BitStream_Fetch(_bs) do { \
	(_bs->accumulator) = 0; \
	if (((UINT32) (_bs->pointer - _bs->buffer)) < (_bs->capacity + 0)) \
		(_bs->accumulator) |= (*(_bs->pointer + 0) << 24); \
	if (((UINT32) (_bs->pointer - _bs->buffer)) < (_bs->capacity + 1)) \
		(_bs->accumulator) |= (*(_bs->pointer + 1) << 16); \
	if (((UINT32) (_bs->pointer - _bs->buffer)) < (_bs->capacity + 2)) \
		(_bs->accumulator) |= (*(_bs->pointer + 2) << 8); \
	if (((UINT32) (_bs->pointer - _bs->buffer)) <(_bs->capacity + 3)) \
		(_bs->accumulator) |= (*(_bs->pointer + 3) << 0); \
	BitStream_Prefetch(_bs); \
} while(0)

#define BitStream_Flush(_bs) do { \
	if (((UINT32) (_bs->pointer - _bs->buffer)) < (_bs->capacity + 0)) \
		*(_bs->pointer + 0) = (_bs->accumulator >> 24); \
	if (((UINT32) (_bs->pointer - _bs->buffer)) < (_bs->capacity + 1)) \
		*(_bs->pointer + 1) = (_bs->accumulator >> 16); \
	if (((UINT32) (_bs->pointer - _bs->buffer)) < (_bs->capacity + 2)) \
		*(_bs->pointer + 2) = (_bs->accumulator >> 8); \
	if (((UINT32) (_bs->pointer - _bs->buffer)) < (_bs->capacity + 3)) \
		*(_bs->pointer + 3) = (_bs->accumulator >> 0); \
} while(0)

#define BitStream_Shift(_bs, _nbits) do { \
	_bs->accumulator <<= _nbits; \
	_bs->position += _nbits; \
	_bs->offset += _nbits; \
	if (_bs->offset < 32) { \
		_bs->mask = ((1 << _nbits) - 1); \
		_bs->accumulator |= ((_bs->prefetch >> (32 - _nbits)) & _bs->mask); \
		_bs->prefetch <<= _nbits; \
	} else { \
		_bs->mask = ((1 << _nbits) - 1); \
		_bs->accumulator |= ((_bs->prefetch >> (32 - _nbits)) & _bs->mask); \
		_bs->prefetch <<= _nbits; \
		_bs->offset -= 32; \
		_bs->pointer += 4; \
		BitStream_Prefetch(_bs); \
		if (_bs->offset) { \
			_bs->mask = ((1 << _bs->offset) - 1); \
			_bs->accumulator |= ((_bs->prefetch >> (32 - _bs->offset)) & _bs->mask); \
			_bs->prefetch <<= _bs->offset; \
		} \
	} \
} while(0)

#define BitStream_Write_Bits(_bs, _bits, _nbits) do { \
	_bs->position += _nbits; \
	_bs->offset += _nbits; \
	if (_bs->offset < 32) { \
		_bs->accumulator |= (_bits << (32 - _bs->offset)); \
	} else { \
		_bs->offset -= 32; \
		_bs->mask = ((1 << (_nbits - _bs->offset)) - 1); \
		_bs->accumulator |= ((_bits >> _bs->offset) & _bs->mask); \
		BitStream_Flush(bs); \
		_bs->accumulator = 0; \
		_bs->pointer += 4; \
		if (_bs->offset) { \
			_bs->mask = ((1 << _bs->offset) - 1); \
			_bs->accumulator |= ((_bits & _bs->mask) << (32 - _bs->offset)); \
		} \
	} \
} while(0)

WINPR_API void BitDump(const BYTE* buffer, UINT32 length, UINT32 flags);
WINPR_API UINT32 ReverseBits32(UINT32 bits, UINT32 nbits);

WINPR_API void BitStream_Attach(wBitStream* bs, BYTE* buffer, UINT32 capacity);

WINPR_API wBitStream* BitStream_New();
WINPR_API void BitStream_Free(wBitStream* bs);

#ifdef __cplusplus
}
#endif

#endif /* WINPR_UTILS_BITSTREAM_H */
