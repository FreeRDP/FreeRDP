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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/handle.h>

#ifndef _WIN32

#include <assert.h>
#include <pthread.h>

#include "../synch/synch.h"
#include "../thread/thread.h"
#include "../pipe/pipe.h"
#include "../comm/comm.h"
#include "../security/security.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <assert.h>

#include "../handle/handle.h"

/* _HandleCreators is a NULL-terminated array with a maximun of HANDLE_CREATOR_MAX HANDLE_CREATOR */
#define HANDLE_CLOSE_CB_MAX 128
static HANDLE_CLOSE_CB** _HandleCloseCbs = NULL;
static CRITICAL_SECTION _HandleCloseCbsLock;

static pthread_once_t _HandleCloseCbsInitialized = PTHREAD_ONCE_INIT;
static void _HandleCloseCbsInit()
{
	/* NB: error management to be done outside of this function */
	assert(_HandleCloseCbs == NULL);
	_HandleCloseCbs = (HANDLE_CLOSE_CB**)calloc(HANDLE_CLOSE_CB_MAX+1, sizeof(HANDLE_CLOSE_CB*));
	InitializeCriticalSection(&_HandleCloseCbsLock);
	assert(_HandleCloseCbs != NULL);
}

/**
 * Returns TRUE on success, FALSE otherwise.
 */
BOOL RegisterHandleCloseCb(HANDLE_CLOSE_CB* pHandleCloseCb)
{
	int i;

	if (pthread_once(&_HandleCloseCbsInitialized, _HandleCloseCbsInit) != 0)
	{
		return FALSE;
	}

	if (_HandleCloseCbs == NULL)
	{
		return FALSE;
	}

	EnterCriticalSection(&_HandleCloseCbsLock);

	for (i=0; i<HANDLE_CLOSE_CB_MAX; i++)
	{
		if (_HandleCloseCbs[i] == NULL)
		{
			_HandleCloseCbs[i] = pHandleCloseCb;
			LeaveCriticalSection(&_HandleCloseCbsLock);
			return TRUE;
		}
	}

	LeaveCriticalSection(&_HandleCloseCbsLock);
	return FALSE;
}



BOOL CloseHandle(HANDLE hObject)
{
	int i;
	ULONG Type;
	PVOID Object;

	if (!winpr_Handle_GetInfo(hObject, &Type, &Object))
		return FALSE;

	if (pthread_once(&_HandleCloseCbsInitialized, _HandleCloseCbsInit) != 0)
	{
		return FALSE;
	}

	if (_HandleCloseCbs == NULL)
	{
		return FALSE;
	}

	EnterCriticalSection(&_HandleCloseCbsLock);

	for (i=0; _HandleCloseCbs[i] != NULL; i++)
	{
		HANDLE_CLOSE_CB* close_cb = (HANDLE_CLOSE_CB*)_HandleCloseCbs[i];

		if (close_cb && close_cb->IsHandled(hObject))
		{
			BOOL result = close_cb->CloseHandle(hObject);
			LeaveCriticalSection(&_HandleCloseCbsLock);
			return result;
		}
	}

	LeaveCriticalSection(&_HandleCloseCbsLock);

	return FALSE;
}

BOOL DuplicateHandle(HANDLE hSourceProcessHandle, HANDLE hSourceHandle, HANDLE hTargetProcessHandle,
		LPHANDLE lpTargetHandle, DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwOptions)
{
	*((ULONG_PTR*) lpTargetHandle) = (ULONG_PTR) hSourceHandle;
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
