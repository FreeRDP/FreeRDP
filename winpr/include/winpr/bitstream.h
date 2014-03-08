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

#define BitStream_Attach(_bs, _buffer, _capacity) { \
	_bs->position = 0; \
	_bs->buffer = _buffer; \
	_bs->offset = 0; \
	_bs->accumulator = 0; \
	_bs->pointer = _bs->buffer; \
	_bs->capacity = _capacity; \
	_bs->length = _bs->capacity; \
}

#define BitStream_Write_Bits(_bs, _bits, _nbits) { \
	_bs->accumulator |= (_bits << _bs->offset); \
	_bs->position += _nbits; \
	_bs->offset += _nbits; \
	if (_bs->offset >= 32) { \
		*((UINT32*) _bs->pointer) = (_bs->accumulator); \
		_bs->offset = _bs->offset - 32; \
		_bs->accumulator = _bits >> (_nbits - _bs->offset); \
		_bs->pointer += 4; \
	} \
}

#define BitStream_Flush(_bs) { \
	if ((_bs->pointer - _bs->buffer) < (_bs->capacity + 3)) { \
	*((UINT32*) _bs->pointer) = (_bs->accumulator); \
	} else { \
	if ((_bs->pointer - _bs->buffer) < (_bs->capacity + 0)) \
		*(_bs->pointer + 0) = (_bs->accumulator >> 0); \
	if ((_bs->pointer - _bs->buffer) < (_bs->capacity + 1)) \
		*(_bs->pointer + 1) = (_bs->accumulator >> 8); \
	if ((_bs->pointer - _bs->buffer) < (_bs->capacity + 2)) \
		*(_bs->pointer + 2) = (_bs->accumulator >> 16); \
	if ((_bs->pointer - _bs->buffer) < (_bs->capacity + 3)) \
		*(_bs->pointer + 3) = (_bs->accumulator >> 24); \
	} \
}

#define BITDUMP_MSB_FIRST	0x00000001
#define BITDUMP_STDERR		0x00000002

#ifdef __cplusplus
extern "C" {
#endif

WINPR_API void BitDump(const BYTE* buffer, UINT32 length, UINT32 flags);

WINPR_API wBitStream* BitStream_New();
WINPR_API void BitStream_Free(wBitStream* bs);

#ifdef __cplusplus
}
#endif

#endif /* WINPR_UTILS_BITSTREAM_H */
