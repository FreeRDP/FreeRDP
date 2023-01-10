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

#include <pthread.h>

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

BOOL CloseHandle(HANDLE hObject)
{
	ULONG Type;
	WINPR_HANDLE* Object;

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

BOOL DuplicateHandle(HANDLE hSourceProcessHandle, HANDLE hSourceHandle, HANDLE hTargetProcessHandle,
                     LPHANDLE lpTargetHandle, DWORD dwDesiredAccess, BOOL bInheritHandle,
                     DWORD dwOptions)
{
	*((ULONG_PTR*)lpTargetHandle) = (ULONG_PTR)hSourceHandle;
	return TRUE;
}

BOOL GetHandleInformation(HANDLE hObject, LPDWORD lpdwFlags)
{
	return TRUE;
}

BOOL SetHandleInformation(HANDLE hObject, DWORD dwMask, DWORD dwFlags)
{
	return TRUE;
}

#endif
