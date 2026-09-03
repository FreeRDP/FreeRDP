/**
 * WinPR: Windows Portable Runtime
 * Handle Management
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2014 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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

#include <winpr/config.h>

#include <winpr/handle.h>
#include <winpr/assert.h>

#include "../log.h"
#define TAG WINPR_TAG("handle")

#ifndef _WIN32

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>

#include "../synch/synch.h"
#include "../thread/thread.h"
#include "../pipe/pipe.h"
#include "../comm/comm.h"
#include "../security/security.h"

#ifdef WINPR_HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "../handle/handle.h"

#endif

/* finds the "{}" placeholder in `format`, splitting it into the literal text before and after it.
 * Returns FALSE (and leaves *prefixLen and *suffix untouched) if `format` has none. */
WINPR_ATTR_NODISCARD
static BOOL winpr_handle_format_split(const char* format, size_t* prefixLen, const char** suffix)
{
	const char* placeholder = strstr(format, "{}");
	if (!placeholder)
	{
		WLog_ERR(TAG, "format string '%s' has no {} placeholder for the handle value", format);
		return FALSE;
	}

	*prefixLen = (size_t)(placeholder - format);
	*suffix = placeholder + 2;
	return TRUE;
}

#ifndef _WIN32
/* single-character tag identifying a handle's type in the exported value (see
 * winpr_exportHandleToString()/winpr_importHandleFromString()), so a POSIX-side import knows how
 * to rebuild the right kind of HANDLE around the fd. Only pipes (as returned by CreatePipe()) are
 * supported for now; add a case here (and a matching one in winpr_importHandleFromString()) when
 * another handle type needs to cross a fork()+exec(). */
WINPR_ATTR_NODISCARD
static char winpr_handle_export_type_tag(ULONG type)
{
	switch (type)
	{
		case HANDLE_TYPE_ANONYMOUS_PIPE:
			return 'P';
		default:
			return '\0';
	}
}
#endif

BOOL winpr_exportHandleToString(HANDLE h, const char* format, char* outStr, size_t outSz)
{
	if (!format || !outStr || (outSz == 0))
		return FALSE;

	size_t prefixLen = 0;
	const char* suffix = nullptr;
	if (!winpr_handle_format_split(format, &prefixLen, &suffix))
		return FALSE;

	if (!h || (h == INVALID_HANDLE_VALUE))
	{
		WLog_ERR(TAG, "refusing to export an invalid handle");
		return FALSE;
	}
	size_t remaining = outSz - 1;
	if (remaining <= prefixLen)
		return FALSE;
	remaining -= prefixLen;

	size_t suffixLen = strlen(suffix);
	if (remaining <= suffixLen)
		return FALSE;
	remaining -= suffixLen;

	char value[32] = { 0 };
#ifdef _WIN32
	const int valueLenInt = snprintf(value, sizeof(value), "%llx", (unsigned long long)(UINT_PTR)h);
#else
	ULONG type = 0;
	WINPR_HANDLE* hdl = nullptr;
	if (!winpr_Handle_GetInfo(h, &type, &hdl))
	{
		WLog_ERR(TAG, "unable to resolve handle info");
		return FALSE;
	}

	const char typeTag = winpr_handle_export_type_tag(type);
	if (typeTag == '\0')
	{
		WLog_ERR(TAG,
		         "exporting handle type %" PRIu32 " is not supported, only pipe handles can be "
		         "exported for now",
		         type);
		return FALSE;
	}

	const int fd = winpr_Handle_getFd(h);
	if (fd < 0)
	{
		WLog_ERR(TAG, "unable to resolve a file descriptor for this handle");
		return FALSE;
	}
	const int valueLenInt = snprintf(value, sizeof(value), "%c%x", typeTag, (unsigned)fd);
#endif
	if (valueLenInt <= 0)
		return FALSE;
	const size_t valueLen = (size_t)valueLenInt;

	if (valueLen > remaining)
		return FALSE;

	memcpy(outStr, format, prefixLen);
	memcpy(outStr + prefixLen, value, valueLen);
	memcpy(outStr + prefixLen + valueLen, suffix, suffixLen + 1);
	return TRUE;
}

HANDLE winpr_importHandleFromString(const char* str, const char* format)
{
	if (!str || !format)
		return INVALID_HANDLE_VALUE;

	size_t prefixLen = 0;
	const char* suffix = nullptr;
	if (!winpr_handle_format_split(format, &prefixLen, &suffix))
		return INVALID_HANDLE_VALUE;

	const size_t suffixLen = strlen(suffix);
	const size_t strLen = strlen(str);
	if ((strLen < prefixLen + suffixLen) || (strncmp(str, format, prefixLen) != 0) ||
	    (strcmp(str + strLen - suffixLen, suffix) != 0))
	{
		WLog_ERR(TAG, "input string '%s' does not match format '%s'", str, format);
		return INVALID_HANDLE_VALUE;
	}

	char value[32] = WINPR_C_ARRAY_INIT;
	size_t valueLen = strLen - prefixLen - suffixLen;
	const char* valueStart = str + prefixLen;
	if ((valueLen == 0) || (valueLen >= sizeof(value)))
	{
		WLog_ERR(TAG, "input string '%s' has no usable handle value", str);
		return INVALID_HANDLE_VALUE;
	}

#ifndef _WIN32
	/* first character is the type tag written by winpr_exportHandleToString() (see
	 * winpr_handle_export_type_tag()) - only pipes are supported for now. */
	const char typeTag = valueStart[0];
	if (typeTag != 'P')
	{
		WLog_ERR(TAG,
		         "unsupported handle type tag '%c' in '%s', only pipe handles ('P') can be "
		         "imported for now",
		         typeTag, str);
		return INVALID_HANDLE_VALUE;
	}
	valueStart++;
	valueLen--;
	if (valueLen == 0)
	{
		WLog_ERR(TAG, "input string '%s' has no usable handle value after the type tag", str);
		return INVALID_HANDLE_VALUE;
	}
#endif

	memcpy(value, valueStart, valueLen);
	value[valueLen] = '\0';

	char* end = nullptr;
	const unsigned long long numeric = strtoull(value, &end, 16);
	if (!end || (*end != '\0'))
	{
		WLog_ERR(TAG, "invalid handle value '%s'", value);
		return INVALID_HANDLE_VALUE;
	}

#ifdef _WIN32
	return (HANDLE)(UINT_PTR)numeric;
#else
	if (numeric > INT32_MAX)
	{
		WLog_ERR(TAG, "handle value '%s' out of range for a file descriptor", value);
		return INVALID_HANDLE_VALUE;
	}
	return winpr_Pipe_FromFd((int)numeric);
#endif
}

#ifndef _WIN32

BOOL CloseHandle(HANDLE hObject)
{
	ULONG type = 0;
	WINPR_HANDLE* hdl = nullptr;

	if (!winpr_Handle_GetInfo(hObject, &type, &hdl) || !hdl || !hdl->ops)
		return FALSE;

	BOOL ok = TRUE;
	if (hdl->ops->CloseHandle)
		ok = hdl->ops->CloseHandle(hObject);

	/* a type's CloseHandle op can legitimately refuse (e.g. file.c won't close the pStdHandleFile
	 * singleton unless forced) - if it did, this handle wasn't actually closed, so leave its
	 * refcount/memory alone entirely. */
	if (!ok)
		return FALSE;

	/* WINPR_THREAD manages its own close/free lifecycle: closing the handle of a still-running
	 * thread detaches it instead of freeing it, and the free only happens later, from the
	 * thread's own pthread routine once it actually returns (see ThreadCloseHandle() /
	 * cleanup_handle() in thread.c). Applying the generic release-then-maybe-free sequence below
	 * on top of that would free the struct while the thread may still be running against it. */
	if (type == HANDLE_TYPE_THREAD)
		return TRUE;

	if (!winpr_Handle_Release(hObject))
		winpr_Handle_ConvertToNone(hObject);

	return TRUE;
}

BOOL DuplicateHandle(WINPR_ATTR_UNUSED HANDLE hSourceProcessHandle,
                     WINPR_ATTR_UNUSED HANDLE hSourceHandle,
                     WINPR_ATTR_UNUSED HANDLE hTargetProcessHandle,
                     WINPR_ATTR_UNUSED LPHANDLE lpTargetHandle,
                     WINPR_ATTR_UNUSED DWORD dwDesiredAccess, WINPR_ATTR_UNUSED BOOL bInheritHandle,
                     WINPR_ATTR_UNUSED DWORD dwOptions)
{
	WLog_ERR(TAG, "DuplicateHandle not implemented");
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL GetHandleInformation(HANDLE hObject, LPDWORD lpdwFlags)
{
	if (!lpdwFlags)
		return FALSE;

	const int fd = winpr_Handle_getFd(hObject);
	if (fd < 0)
	{
		WLog_ERR(TAG, "unable to resolve a file descriptor for this handle type");
		return FALSE;
	}

	const int flags = fcntl(fd, F_GETFD);
	if (flags < 0)
	{
		char buffer[64] = WINPR_C_ARRAY_INIT;
		WLog_ERR(TAG, "fcntl(F_GETFD) failed: %s", winpr_strerror(errno, buffer, sizeof(buffer)));
		return FALSE;
	}

	*lpdwFlags = (flags & FD_CLOEXEC) ? 0 : HANDLE_FLAG_INHERIT;
	return TRUE;
}

BOOL SetHandleInformation(HANDLE hObject, DWORD dwMask, DWORD dwFlags)
{
	const int fd = winpr_Handle_getFd(hObject);
	if (fd < 0)
	{
		WLog_ERR(TAG, "unable to resolve a file descriptor for this handle type");
		return FALSE;
	}

	/* only HANDLE_FLAG_INHERIT is meaningful on POSIX; ignore any other bit in the mask rather
	 * than failing, matching how Windows treats masks it doesn't recognize on other handle
	 * types */
	if (!(dwMask & HANDLE_FLAG_INHERIT))
		return TRUE;

	int flags = fcntl(fd, F_GETFD);
	if (flags < 0)
	{
		char buffer[64] = WINPR_C_ARRAY_INIT;
		WLog_ERR(TAG, "fcntl(F_GETFD) failed: %s", winpr_strerror(errno, buffer, sizeof(buffer)));
		return FALSE;
	}

	flags = (dwFlags & HANDLE_FLAG_INHERIT) ? (flags & ~FD_CLOEXEC) : (flags | FD_CLOEXEC);
	if (fcntl(fd, F_SETFD, flags) < 0)
	{
		char buffer[64] = WINPR_C_ARRAY_INIT;
		WLog_ERR(TAG, "fcntl(F_SETFD) failed: %s", winpr_strerror(errno, buffer, sizeof(buffer)));
		return FALSE;
	}

	return TRUE;
}

BOOL winpr_set_cloexec(int fd, BOOL cloexec)
{
	const int flags = fcntl(fd, F_GETFD);
	if (flags < 0)
		return FALSE;

	const int newFlags = cloexec ? (flags | FD_CLOEXEC) : (flags & ~FD_CLOEXEC);
	return fcntl(fd, F_SETFD, newFlags) >= 0;
}

#endif
