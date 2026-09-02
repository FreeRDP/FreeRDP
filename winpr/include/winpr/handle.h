/**
 * WinPR: Windows Portable Runtime
 * Handle Management
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

#ifndef WINPR_HANDLE_H
#define WINPR_HANDLE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winpr/winpr.h>
#include <winpr/wtypes.h>
#include <winpr/security.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define WINPR_FD_READ_BIT 0u
#define WINPR_FD_READ (1u << WINPR_FD_READ_BIT)

#define WINPR_FD_WRITE_BIT 1u
#define WINPR_FD_WRITE (1u << WINPR_FD_WRITE_BIT)

	/**
	 * Exports an handle to a string platform independently (so exports the HANDLE
	 * pointer as hexa under WIN32, and under UNIXes a single character identifying the
	 * handle's type followed by the file descriptor number in hexa, e.g. "P4" for a pipe)
	 * formatting it with the pseudo format string with {} for the handle (for instance
	 * `--fdIn={}`). You can use winpr_importHandleFromString to quickly get back a
	 * HANDLE from the output of this call.
	 *
	 * Only pipe HANDLEs (as returned by CreatePipe) can be exported for now under UNIXes.
	 *
	 * 	@param h the HANDLE to export
	 * 	@param format the format string with {} for the handle value
	 * 	@param outStr the output buffer
	 * 	@param outSz outStr size
	 * 	@return if the operation completed successfully
	 *
	 * 	@since version 3.32.0
	 */
	WINPR_ATTR_NODISCARD
	WINPR_API BOOL winpr_exportHandleToString(HANDLE h, const char* format, char* outStr,
	                                          size_t outSz);

	/**
	 * Imports a HANDLE that was previously exported as a string by winpr_exportHandleToString.
	 * Under WIN32 it just converts the string to a HANDLE, and under UNIXes it creates a new
	 * HANDLE.
	 *
	 * @param str the input string
	 * @param format the format string that was used by winpr_exportHandleToString with {} for the
	 * handle
	 * @returns the created HANDLE or INVALID_HANDLE if something wrong happened
	 *
	 * @since version 3.32.0
	 */
	WINPR_ATTR_NODISCARD
	WINPR_API HANDLE winpr_importHandleFromString(const char* str, const char* format);

#ifndef _WIN32

#define DUPLICATE_CLOSE_SOURCE 0x00000001
#define DUPLICATE_SAME_ACCESS 0x00000002

#define HANDLE_FLAG_INHERIT 0x00000001
#define HANDLE_FLAG_PROTECT_FROM_CLOSE 0x00000002

	WINPR_API BOOL CloseHandle(HANDLE hObject);

	WINPR_ATTR_NODISCARD
	WINPR_API BOOL DuplicateHandle(HANDLE hSourceProcessHandle, HANDLE hSourceHandle,
	                               HANDLE hTargetProcessHandle, LPHANDLE lpTargetHandle,
	                               DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwOptions);

	WINPR_ATTR_NODISCARD
	WINPR_API BOOL GetHandleInformation(HANDLE hObject, LPDWORD lpdwFlags);

	WINPR_ATTR_NODISCARD
	WINPR_API BOOL SetHandleInformation(HANDLE hObject, DWORD dwMask, DWORD dwFlags);

#endif

#ifdef __cplusplus
}
#endif

#endif /* WINPR_HANDLE_H */
