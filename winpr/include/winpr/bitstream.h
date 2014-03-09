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

WINPR_API void BitStream_Attach(wBitStream* bs, BYTE* buffer, UINT32 capacity);
WINPR_API void BitStream_Write_Bits(wBitStream* bs, UINT32 bits, UINT32 nbits);
WINPR_API void BitStream_Flush(wBitStream* bs);
WINPR_API void BitStream_Prefetch(wBitStream* bs);
WINPR_API void BitStream_Fetch(wBitStream* bs);
WINPR_API void BitStream_Shift(wBitStream* bs, UINT32 nbits);

WINPR_API void BitDump(const BYTE* buffer, UINT32 length, UINT32 flags);

WINPR_API wBitStream* BitStream_New();
WINPR_API void BitStream_Free(wBitStream* bs);

#ifdef __cplusplus
}
#endif

#endif /* WINPR_UTILS_BITSTREAM_H */
