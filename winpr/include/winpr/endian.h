/*
 * WinPR: Windows Portable Runtime
 * Endianness Macros
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2024 Armin Novak <anovak@thincast.com>
 * Copyright 2024 Thincast Technologies GmbH
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

#ifndef WINPR_ENDIAN_H
#define WINPR_ENDIAN_H

#include <winpr/winpr.h>
#include <winpr/wtypes.h>
#include <winpr/platform.h>
#include <winpr/assert.h>

#ifdef __cplusplus
extern "C"
{
#endif

	static INLINE UINT8 winpr_Data_Get_UINT8(const void* d)
	{
		WINPR_ASSERT(d);
		return *(const UINT8*)d;
	}

	static INLINE INT8 winpr_Data_Get_INT8(const void* d)
	{
		WINPR_ASSERT(d);
		return *(const INT8*)d;
	}

	static INLINE UINT16 winpr_Data_Get_UINT16_NE(const void* d)
	{
		return *(const UINT16*)d;
	}

	static INLINE UINT16 winpr_Data_Get_UINT16(const void* d)
	{
		const size_t typesize = sizeof(UINT16);
		UINT16 v = 0;
		for (size_t x = 0; x < typesize; x++)
		{
			v <<= 8;
			v |= ((const UINT8*)d)[typesize - x - 1];
		}
		return v;
	}

	static INLINE UINT16 winpr_Data_Get_UINT16_BE(const void* d)
	{
		const size_t typesize = sizeof(UINT16);
		UINT16 v = 0;
		for (size_t x = 0; x < typesize; x++)
		{
			v <<= 8;
			v |= ((const UINT8*)d)[x];
		}
		return v;
	}

	static INLINE INT16 winpr_Data_Get_INT16_NE(const void* d)
	{
		return *(const INT16*)d;
	}

	static INLINE INT16 winpr_Data_Get_INT16(const void* d)
	{
		const UINT16 u16 = winpr_Data_Get_UINT16(d);
		return (INT16)u16;
	}

	static INLINE INT16 winpr_Data_Get_INT16_BE(const void* d)
	{
		const UINT16 u16 = winpr_Data_Get_UINT16_BE(d);
		return (INT16)u16;
	}

	static INLINE UINT32 winpr_Data_Get_UINT32_NE(const void* d)
	{
		return *(const UINT32*)d;
	}

	static INLINE UINT32 winpr_Data_Get_UINT32(const void* d)
	{
		const size_t typesize = sizeof(UINT32);
		UINT32 v = 0;
		for (size_t x = 0; x < typesize; x++)
		{
			v <<= 8;
			v |= ((const UINT8*)d)[typesize - x - 1];
		}
		return v;
	}

	static INLINE UINT32 winpr_Data_Get_UINT32_BE(const void* d)
	{
		const size_t typesize = sizeof(UINT32);
		UINT32 v = 0;
		for (size_t x = 0; x < typesize; x++)
		{
			v <<= 8;
			v |= ((const UINT8*)d)[x];
		}
		return v;
	}

	static INLINE INT32 winpr_Data_Get_INT32_NE(const void* d)
	{
		return *(const INT32*)d;
	}

	static INLINE INT32 winpr_Data_Get_INT32(const void* d)
	{
		const UINT32 u32 = winpr_Data_Get_UINT32(d);
		return (INT32)u32;
	}

	static INLINE INT32 winpr_Data_Get_INT32_BE(const void* d)
	{
		const UINT32 u32 = winpr_Data_Get_UINT32_BE(d);
		return (INT32)u32;
	}

	static INLINE UINT64 winpr_Data_Get_UINT64_NE(const void* d)
	{
		return *(const UINT64*)d;
	}

	static INLINE UINT64 winpr_Data_Get_UINT64(const void* d)
	{
		const size_t typesize = sizeof(UINT64);
		UINT64 v = 0;
		for (size_t x = 0; x < typesize; x++)
		{
			v <<= 8;
			v |= ((const UINT8*)d)[typesize - x - 1];
		}
		return v;
	}

	static INLINE UINT64 winpr_Data_Get_UINT64_BE(const void* d)
	{
		const size_t typesize = sizeof(UINT64);
		UINT64 v = 0;
		for (size_t x = 0; x < typesize; x++)
		{
			v <<= 8;
			v |= ((const UINT8*)d)[x];
		}
		return v;
	}

	static INLINE INT64 winpr_Data_Get_INT64_NE(const void* d)
	{
		return *(const INT64*)d;
	}

	static INLINE INT64 winpr_Data_Get_INT64(const void* d)
	{
		const UINT64 u64 = winpr_Data_Get_UINT64(d);
		return (INT64)u64;
	}

	static INLINE INT64 winpr_Data_Get_INT64_BE(const void* d)
	{
		const UINT64 u64 = winpr_Data_Get_UINT64_BE(d);
		return (INT64)u64;
	}

	static INLINE void winpr_Data_Write_UINT8_NE(void* d, UINT8 v)
	{
		WINPR_ASSERT(d);
		*((UINT8*)d) = v;
	}

	static INLINE void winpr_Data_Write_UINT8(void* d, UINT8 v)
	{
		WINPR_ASSERT(d);
		*((UINT8*)d) = v;
	}

	static INLINE void winpr_Data_Write_UINT16_NE(void* d, UINT16 v)
	{
		WINPR_ASSERT(d);
		*((UINT16*)d) = v;
	}

	static INLINE void winpr_Data_Write_UINT16(void* d, UINT16 v)
	{
		WINPR_ASSERT(d);
		((UINT8*)d)[0] = v & 0xFF;
		((UINT8*)d)[1] = (v >> 8) & 0xFF;
	}

	static INLINE void winpr_Data_Write_UINT16_BE(void* d, UINT16 v)
	{
		WINPR_ASSERT(d);
		((UINT8*)d)[1] = v & 0xFF;
		((UINT8*)d)[0] = (v >> 8) & 0xFF;
	}

	static INLINE void winpr_Data_Write_UINT32_NE(void* d, UINT32 v)
	{
		WINPR_ASSERT(d);
		*((UINT32*)d) = v;
	}

	static INLINE void winpr_Data_Write_UINT32(void* d, UINT32 v)
	{
		WINPR_ASSERT(d);
		BYTE* b = (BYTE*)d;
		winpr_Data_Write_UINT16(b, v & 0xFFFF);
		winpr_Data_Write_UINT16(b + 2, (v >> 16) & 0xFFFF);
	}

	static INLINE void winpr_Data_Write_UINT32_BE(void* d, UINT32 v)
	{
		WINPR_ASSERT(d);
		BYTE* b = (BYTE*)d;
		winpr_Data_Write_UINT16_BE(b, (v >> 16) & 0xFFFF);
		winpr_Data_Write_UINT16_BE(b + 2, v & 0xFFFF);
	}

	static INLINE void winpr_Data_Write_UINT64_NE(void* d, UINT64 v)
	{
		WINPR_ASSERT(d);
		*((UINT64*)d) = v;
	}

	static INLINE void winpr_Data_Write_UINT64(void* d, UINT64 v)
	{
		WINPR_ASSERT(d);
		BYTE* b = (BYTE*)d;
		winpr_Data_Write_UINT32(b, v & 0xFFFFFFFF);
		winpr_Data_Write_UINT32(b + 4, (v >> 32) & 0xFFFFFFFF);
	}

	static INLINE void winpr_Data_Write_UINT64_BE(void* d, UINT64 v)
	{
		WINPR_ASSERT(d);
		BYTE* b = (BYTE*)d;
		winpr_Data_Write_UINT32_BE(b, (v >> 32) & 0xFFFFFFFF);
		winpr_Data_Write_UINT32_BE(b + 4, v & 0xFFFFFFFF);
	}

	static INLINE void winpr_Data_Write_INT8_NE(void* d, INT8 v)
	{
		WINPR_ASSERT(d);
		*((INT8*)d) = v;
	}

	static INLINE void winpr_Data_Write_INT8(void* d, INT8 v)
	{
		WINPR_ASSERT(d);
		*((INT8*)d) = v;
	}

	static INLINE void winpr_Data_Write_INT16_NE(void* d, INT16 v)
	{
		WINPR_ASSERT(d);
		*((INT16*)d) = v;
	}

	static INLINE void winpr_Data_Write_INT16(void* d, INT16 v)
	{
		WINPR_ASSERT(d);
		((UINT8*)d)[0] = v & 0xFF;
		((UINT8*)d)[1] = (v >> 8) & 0xFF;
	}

	static INLINE void winpr_Data_Write_INT16_BE(void* d, INT16 v)
	{
		WINPR_ASSERT(d);
		((UINT8*)d)[1] = v & 0xFF;
		((UINT8*)d)[0] = (v >> 8) & 0xFF;
	}

	static INLINE void winpr_Data_Write_INT32_NE(void* d, INT32 v)
	{
		WINPR_ASSERT(d);
		*((UINT32*)d) = v;
	}

	static INLINE void winpr_Data_Write_INT32(void* d, INT32 v)
	{
		WINPR_ASSERT(d);
		BYTE* b = (BYTE*)d;
		winpr_Data_Write_UINT16(b, v & 0xFFFF);
		winpr_Data_Write_UINT16(b + 2, (v >> 16) & 0xFFFF);
	}

	static INLINE void winpr_Data_Write_INT32_BE(void* d, INT32 v)
	{
		WINPR_ASSERT(d);
		BYTE* b = (BYTE*)d;
		winpr_Data_Write_UINT16_BE(b, (v >> 16) & 0xFFFF);
		winpr_Data_Write_UINT16_BE(b + 2, v & 0xFFFF);
	}

	static INLINE void winpr_Data_Write_INT64_NE(void* d, INT64 v)
	{
		WINPR_ASSERT(d);
		*((UINT64*)d) = v;
	}

	static INLINE void winpr_Data_Write_INT64(void* d, INT64 v)
	{
		WINPR_ASSERT(d);
		BYTE* b = (BYTE*)d;
		winpr_Data_Write_UINT32(b, v & 0xFFFFFFFF);
		winpr_Data_Write_UINT32(b + 4, (v >> 32) & 0xFFFFFFFF);
	}

	static INLINE void winpr_Data_Write_INT64_BE(void* d, INT64 v)
	{
		WINPR_ASSERT(d);
		BYTE* b = (BYTE*)d;
		winpr_Data_Write_UINT32_BE(b, (v >> 32) & 0xFFFFFFFF);
		winpr_Data_Write_UINT32_BE(b + 4, v & 0xFFFFFFFF);
	}

#if defined(WINPR_DEPRECATED)
#define Data_Read_UINT8_NE(_d, _v) _v = winpr_Data_Get_UINT8(_d)

#define Data_Read_UINT8(_d, _v) _v = winpr_Data_Get_UINT8(_d)

#define Data_Read_UINT16_NE(_d, _v) _v = winpr_Data_Get_UINT16_NE(_d)

#define Data_Read_UINT16(_d, _v) _v = winpr_Data_Get_UINT16(_d)

#define Data_Read_UINT16_BE(_d, _v) _v = winpr_Data_Get_UINT16_BE(_d)

#define Data_Read_UINT32_NE(_d, _v) _v = winpr_Data_Get_UINT32_NE(_d)

#define Data_Read_UINT32(_d, _v) _v = winpr_Data_Get_UINT32(_d)

#define Data_Read_UINT32_BE(_d, _v) _v = winpr_Data_Get_UINT32_BE(_d)

#define Data_Read_UINT64_NE(_d, _v) _v = winpr_Data_Get_UINT64_NE(_d)

#define Data_Read_UINT64(_d, _v) _v = winpr_Data_Get_UINT64(_d)

#define Data_Read_UINT64_BE(_d, _v) _v = winpr_Data_Get_UINT64_BE(_d)

#define Data_Write_UINT8_NE(_d, _v) winpr_Data_Write_UINT8_NE(_d, _v)

#define Data_Write_UINT8(_d, _v) winpr_Data_Write_UINT8(_d, _v)

#define Data_Write_UINT16_NE(_d, _v) winpr_Data_Write_UINT16_NE(_d, _v)
#define Data_Write_UINT16(_d, _v) winpr_Data_Write_UINT16(_d, _v)

#define Data_Write_UINT16_BE(_d, _v) winpr_Data_Write_UINT16_BE(_d, _v)

#define Data_Write_UINT32_NE(_d, _v) winpr_Data_Write_UINT32_NE(_d, _v)

#define Data_Write_UINT32(_d, _v) winpr_Data_Write_UINT32(_d, _v)

#define Data_Write_UINT32_BE(_d, _v) winpr_Data_Write_UINT32_BE(_d, _v)

#define Data_Write_UINT64_NE(_d, _v) winpr_Data_Write_UINT64_NE(_d, _v)

#define Data_Write_UINT64(_d, _v) winpr_Data_Write_UINT64(_d, _v)

#define Data_Write_UINT64_BE(_d, _v) winpr_Data_Write_UINT64_BE(_d, _v)
#endif

#ifdef __cplusplus
}
#endif

#endif /* WINPR_ENDIAN_H */
