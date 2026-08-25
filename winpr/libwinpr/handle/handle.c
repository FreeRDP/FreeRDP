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

#include <winpr/assert.h>

#include "../handle/handle.h"
#include "../log.h"
#define TAG WINPR_TAG("handle")

BOOL CloseHandle(HANDLE hObject)
{
	ULONG Type = 0;
	WINPR_HANDLE* Object = nullptr;

	if (!winpr_Handle_GetInfo(hObject, &Type, &Object))
		return FALSE;

	if (!Object)
		return FALSE;

	if (!Object->ops)
		return FALSE;

	if (Object->ops->CloseHandle)
		return Object->ops->CloseHandle(hObject);

	return FALSE;
}

BOOL DuplicateHandle(WINPR_ATTR_UNUSED HANDLE hSourceProcessHandle,
                     WINPR_ATTR_UNUSED HANDLE hSourceHandle,
                     WINPR_ATTR_UNUSED HANDLE hTargetProcessHandle,
                     WINPR_ATTR_UNUSED LPHANDLE lpTargetHandle,
                     WINPR_ATTR_UNUSED DWORD dwDesiredAccess, WINPR_ATTR_UNUSED BOOL bInheritHandle,
                     WINPR_ATTR_UNUSED DWORD dwOptions)
{
	*((ULONG_PTR*)lpTargetHandle) = (ULONG_PTR)hSourceHandle;
	return TRUE;
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
