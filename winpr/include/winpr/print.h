/**
 * WinPR: Windows Portable Runtime
 * Print Utils
 *
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

#ifndef WINPR_UTILS_PRINT_H
#define WINPR_UTILS_PRINT_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <winpr/winpr.h>
#include <winpr/wtypes.h>
#include <winpr/wlog.h>

#define WINPR_HEXDUMP_LINE_LENGTH 16

#ifdef __cplusplus
extern "C"
{
#endif

	WINPR_API void winpr_HexDump(const char* tag, UINT32 lvl, const BYTE* data, size_t length);
	WINPR_API void winpr_HexLogDump(wLog* log, UINT32 lvl, const BYTE* data, size_t length);
	WINPR_API void winpr_CArrayDump(const char* tag, UINT32 lvl, const BYTE* data, size_t length,
	                                size_t width);

	WINPR_API char* winpr_BinToHexString(const BYTE* data, size_t length, BOOL space);
	WINPR_API size_t winpr_BinToHexStringBuffer(const BYTE* data, size_t length, char* dstStr,
	                                            size_t dstSize, BOOL space);

	WINPR_API size_t winpr_HexStringToBinBuffer(const char* str, size_t strLength, BYTE* data,
	                                            size_t dataLength);

#ifdef __cplusplus
}
#endif

#endif /* WINPR_UTILS_PRINT_H */
