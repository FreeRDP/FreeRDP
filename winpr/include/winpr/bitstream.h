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

#include <winpr/assert.h>
#include <winpr/winpr.h>
#include <winpr/wtypes.h>

#include <winpr/crt.h>
#include <winpr/wlog.h>

typedef struct
{
	const BYTE* buffer;
	BYTE* pointer;
	UINT32 position;
	UINT32 length;
	UINT32 capacity;
	UINT32 mask;
	UINT32 offset;
	UINT32 prefetch;
	UINT32 accumulator;
} wBitStream;

#define BITDUMP_MSB_FIRST 0x00000001
#define BITDUMP_STDERR 0x00000002

#ifdef __cplusplus
extern "C"
{
#endif

	static INLINE void BitStream_Prefetch(wBitStream* _bs)
	{
		WINPR_ASSERT(_bs);

		(_bs->prefetch) = 0;
		if (((UINT32)(_bs->pointer - _bs->buffer) + 4) < (_bs->capacity))
			(_bs->prefetch) |= ((UINT32) * (_bs->pointer + 4) << 24);
		if (((UINT32)(_bs->pointer - _bs->buffer) + 5) < (_bs->capacity))
			(_bs->prefetch) |= ((UINT32) * (_bs->pointer + 5) << 16);
		if (((UINT32)(_bs->pointer - _bs->buffer) + 6) < (_bs->capacity))
			(_bs->prefetch) |= ((UINT32) * (_bs->pointer + 6) << 8);
		if (((UINT32)(_bs->pointer - _bs->buffer) + 7) < (_bs->capacity))
			(_bs->prefetch) |= ((UINT32) * (_bs->pointer + 7) << 0);
	}

	static INLINE void BitStream_Fetch(wBitStream* _bs)
	{
		WINPR_ASSERT(_bs);
		(_bs->accumulator) = 0;
		if (((UINT32)(_bs->pointer - _bs->buffer) + 0) < (_bs->capacity))
			(_bs->accumulator) |= ((UINT32) * (_bs->pointer + 0) << 24);
		if (((UINT32)(_bs->pointer - _bs->buffer) + 1) < (_bs->capacity))
			(_bs->accumulator) |= ((UINT32) * (_bs->pointer + 1) << 16);
		if (((UINT32)(_bs->pointer - _bs->buffer) + 2) < (_bs->capacity))
			(_bs->accumulator) |= ((UINT32) * (_bs->pointer + 2) << 8);
		if (((UINT32)(_bs->pointer - _bs->buffer) + 3) < (_bs->capacity))
			(_bs->accumulator) |= ((UINT32) * (_bs->pointer + 3) << 0);
		BitStream_Prefetch(_bs);
	}

	static INLINE void BitStream_Flush(wBitStream* _bs)
	{
		WINPR_ASSERT(_bs);
		if (((UINT32)(_bs->pointer - _bs->buffer) + 0) < (_bs->capacity))
			*(_bs->pointer + 0) = (BYTE)((UINT32)_bs->accumulator >> 24);
		if (((UINT32)(_bs->pointer - _bs->buffer) + 1) < (_bs->capacity))
			*(_bs->pointer + 1) = (BYTE)((UINT32)_bs->accumulator >> 16);
		if (((UINT32)(_bs->pointer - _bs->buffer) + 2) < (_bs->capacity))
			*(_bs->pointer + 2) = (BYTE)((UINT32)_bs->accumulator >> 8);
		if (((UINT32)(_bs->pointer - _bs->buffer) + 3) < (_bs->capacity))
			*(_bs->pointer + 3) = (BYTE)((UINT32)_bs->accumulator >> 0);
	}

	static INLINE void BitStream_Shift(wBitStream* _bs, UINT32 _nbits)
	{
		WINPR_ASSERT(_bs);
		if (_nbits == 0)
		{
		}
		else if ((_nbits > 0) && (_nbits < 32))
		{
			_bs->accumulator <<= _nbits;
			_bs->position += _nbits;
			_bs->offset += _nbits;
			if (_bs->offset < 32)
			{
				_bs->mask = (UINT32)((1UL << _nbits) - 1UL);
				_bs->accumulator |= ((_bs->prefetch >> (32 - _nbits)) & _bs->mask);
				_bs->prefetch <<= _nbits;
			}
			else
			{
				_bs->mask = (UINT32)((1UL << _nbits) - 1UL);
				_bs->accumulator |= ((_bs->prefetch >> (32 - _nbits)) & _bs->mask);
				_bs->prefetch <<= _nbits;
				_bs->offset -= 32;
				_bs->pointer += 4;
				BitStream_Prefetch(_bs);
				if (_bs->offset)
				{
					_bs->mask = (UINT32)((1UL << _bs->offset) - 1UL);
					_bs->accumulator |= ((_bs->prefetch >> (32 - _bs->offset)) & _bs->mask);
					_bs->prefetch <<= _bs->offset;
				}
			}
		}
		else
		{
			WLog_WARN("com.winpr.bitstream", "warning: BitStream_Shift(%u)", (unsigned)_nbits);
		}
	}

	static INLINE void BitStream_Shift32(wBitStream* _bs)
	{
		WINPR_ASSERT(_bs);
		BitStream_Shift(_bs, 16);
		BitStream_Shift(_bs, 16);
	}

	static INLINE void BitStream_Write_Bits(wBitStream* _bs, UINT32 _bits, UINT32 _nbits)
	{
		WINPR_ASSERT(_bs);
		_bs->position += _nbits;
		_bs->offset += _nbits;
		if (_bs->offset < 32)
		{
			_bs->accumulator |= (_bits << (32 - _bs->offset));
		}
		else
		{
			_bs->offset -= 32;
			_bs->mask = ((1 << (_nbits - _bs->offset)) - 1);
			_bs->accumulator |= ((_bits >> _bs->offset) & _bs->mask);
			BitStream_Flush(_bs);
			_bs->accumulator = 0;
			_bs->pointer += 4;
			if (_bs->offset)
			{
				_bs->mask = (UINT32)((1UL << _bs->offset) - 1);
				_bs->accumulator |= ((_bits & _bs->mask) << (32 - _bs->offset));
			}
		}
	}

	static INLINE size_t BitStream_GetRemainingLength(wBitStream* _bs)
	{
		WINPR_ASSERT(_bs);
		return (_bs->length - _bs->position);
	}

	WINPR_API void BitDump(const char* tag, UINT32 level, const BYTE* buffer, UINT32 length,
	                       UINT32 flags);
	WINPR_API UINT32 ReverseBits32(UINT32 bits, UINT32 nbits);

	WINPR_API void BitStream_Attach(wBitStream* bs, const BYTE* buffer, UINT32 capacity);

	WINPR_API wBitStream* BitStream_New(void);
	WINPR_API void BitStream_Free(wBitStream* bs);

#ifdef __cplusplus
}
#endif

#endif /* WINPR_UTILS_BITSTREAM_H */
