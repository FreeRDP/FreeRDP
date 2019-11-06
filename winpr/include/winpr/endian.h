/*
 * WinPR: Windows Portable Runtime
 * Endianness Macros
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifdef __cplusplus
extern "C"
{
#endif

#define Data_Read_UINT8_NE(_d, _v) \
	do                             \
	{                              \
		_v = *((const BYTE*)_d);   \
	} while (0)

#define Data_Read_UINT8(_d, _v)  \
	do                           \
	{                            \
		_v = *((const BYTE*)_d); \
	} while (0)

#define Data_Read_UINT16_NE(_d, _v) \
	do                              \
	{                               \
		_v = *((const UINT16*)_d);  \
	} while (0)

#define Data_Read_UINT16(_d, _v)                                                               \
	do                                                                                         \
	{                                                                                          \
		_v = (UINT16)(*((const BYTE*)_d)) + (UINT16)(((UINT16)(*((const BYTE*)_d + 1))) << 8); \
	} while (0)

#define Data_Read_UINT16_BE(_d, _v)                                                  \
	do                                                                               \
	{                                                                                \
		_v = (((UINT16)(*(const BYTE*)_d)) << 8) + (UINT16)(*((const BYTE*)_d + 1)); \
	} while (0)

#define Data_Read_UINT32_NE(_d, _v) \
	do                              \
	{                               \
		_v = *((UINT32*)_d);        \
	} while (0)

#define Data_Read_UINT32(_d, _v)                                                        \
	do                                                                                  \
	{                                                                                   \
		_v = (UINT32)(*((const BYTE*)_d)) + (((UINT32)(*((const BYTE*)_d + 1))) << 8) + \
		     (((UINT32)(*((const BYTE*)_d + 2))) << 16) +                               \
		     (((UINT32)(*((const BYTE*)_d + 3))) << 24);                                \
	} while (0)

#define Data_Read_UINT32_BE(_d, _v)                                                                \
	do                                                                                             \
	{                                                                                              \
		_v = (((UINT32)(*((const BYTE*)_d))) << 24) + (((UINT32)(*((const BYTE*)_d + 1))) << 16) + \
		     (((UINT32)(*((const BYTE*)_d + 2))) << 8) + (((UINT32)(*((const BYTE*)_d + 3))));     \
	} while (0)

#define Data_Read_UINT64_NE(_d, _v) \
	do                              \
	{                               \
		_v = *((UINT64*)_d);        \
	} while (0)

#define Data_Read_UINT64(_d, _v)                                                        \
	do                                                                                  \
	{                                                                                   \
		_v = (UINT64)(*((const BYTE*)_d)) + (((UINT64)(*((const BYTE*)_d + 1))) << 8) + \
		     (((UINT64)(*((const BYTE*)_d + 2))) << 16) +                               \
		     (((UINT64)(*((const BYTE*)_d + 3))) << 24) +                               \
		     (((UINT64)(*((const BYTE*)_d + 4))) << 32) +                               \
		     (((UINT64)(*((const BYTE*)_d + 5))) << 40) +                               \
		     (((UINT64)(*((const BYTE*)_d + 6))) << 48) +                               \
		     (((UINT64)(*((const BYTE*)_d + 7))) << 56);                                \
	} while (0)

#define Data_Write_UINT8_NE(_d, _v) \
	do                              \
	{                               \
		*((UINT8*)_d) = v;          \
	} while (0)

#define Data_Write_UINT8(_d, _v) \
	do                           \
	{                            \
		*_d = (UINT8)(_v);       \
	} while (0)

#define Data_Write_UINT16_NE(_d, _v) \
	do                               \
	{                                \
		*((UINT16*)_d) = _v;         \
	} while (0)

#define Data_Write_UINT16(_d, _v)              \
	do                                         \
	{                                          \
		*((BYTE*)_d) = (_v)&0xFF;              \
		*((BYTE*)_d + 1) = ((_v) >> 8) & 0xFF; \
	} while (0)

#define Data_Write_UINT16_BE(_d, _v)       \
	do                                     \
	{                                      \
		*((BYTE*)_d) = ((_v) >> 8) & 0xFF; \
		*((BYTE*)_d + 1) = (_v)&0xFF;      \
	} while (0)

#define Data_Write_UINT32_NE(_d, _v) \
	do                               \
	{                                \
		*((UINT32*)_d) = _v;         \
	} while (0)

#define Data_Write_UINT32(_d, _v)               \
	do                                          \
	{                                           \
		*((BYTE*)_d) = (_v)&0xFF;               \
		*((BYTE*)_d + 1) = ((_v) >> 8) & 0xFF;  \
		*((BYTE*)_d + 2) = ((_v) >> 16) & 0xFF; \
		*((BYTE*)_d + 3) = ((_v) >> 24) & 0xFF; \
	} while (0)

#define Data_Write_UINT32_BE(_d, _v)                            \
	do                                                          \
	{                                                           \
		Data_Write_UINT16_BE((BYTE*)_d, ((_v) >> 16 & 0xFFFF)); \
		Data_Write_UINT16_BE((BYTE*)_d + 2, ((_v)&0xFFFF));     \
	} while (0)

#define Data_Write_UINT64_NE(_d, _v) \
	do                               \
	{                                \
		*((UINT64*)_d) = _v;         \
	} while (0)

#define Data_Write_UINT64(_d, _v)                       \
	do                                                  \
	{                                                   \
		*((BYTE*)_d) = (UINT64)(_v)&0xFF;               \
		*((BYTE*)_d + 1) = ((UINT64)(_v) >> 8) & 0xFF;  \
		*((BYTE*)_d + 2) = ((UINT64)(_v) >> 16) & 0xFF; \
		*((BYTE*)_d + 3) = ((UINT64)(_v) >> 24) & 0xFF; \
		*((BYTE*)_d + 4) = ((UINT64)(_v) >> 32) & 0xFF; \
		*((BYTE*)_d + 5) = ((UINT64)(_v) >> 40) & 0xFF; \
		*((BYTE*)_d + 6) = ((UINT64)(_v) >> 48) & 0xFF; \
		*((BYTE*)_d + 7) = ((UINT64)(_v) >> 56) & 0xFF; \
	} while (0)

#ifdef __cplusplus
}
#endif

#endif /* WINPR_ENDIAN_H */
