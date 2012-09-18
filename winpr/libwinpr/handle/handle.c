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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/handle.h>

#ifndef _WIN32

#include "../synch/synch.h"

BOOL CloseHandle(HANDLE hObject)
{
	ULONG Type;
	PVOID Object;

	if (!winpr_Handle_GetInfo(hObject, &Type, &Object))
		return FALSE;

	if (Type == HANDLE_TYPE_MUTEX)
	{
		pthread_mutex_destroy((pthread_mutex_t*) Object);
		free(Object);
		return TRUE;
	}

	return FALSE;
}

BOOL DuplicateHandle(HANDLE hSourceProcessHandle, HANDLE hSourceHandle, HANDLE hTargetProcessHandle,
	LPHANDLE lpTargetHandle, DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwOptions)
{
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
