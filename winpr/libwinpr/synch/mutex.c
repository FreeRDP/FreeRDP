/**
 * WinPR: Windows Portable Runtime
 * Synchronization Functions
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

#include <winpr/config.h>

#include <winpr/synch.h>
#include <winpr/debug.h>
#include <winpr/wlog.h>
#include <winpr/string.h>

#include "synch.h"

#ifndef _WIN32

#include <errno.h>

#include "../handle/handle.h"

#include "../log.h"
#define TAG WINPR_TAG("sync.mutex")

static BOOL MutexCloseHandle(HANDLE handle);

static BOOL MutexIsHandled(HANDLE handle)
{
	return WINPR_HANDLE_IS_HANDLED(handle, HANDLE_TYPE_MUTEX, FALSE);
}

static int MutexGetFd(HANDLE handle)
{
	WINPR_MUTEX* mux = (WINPR_MUTEX*)handle;

	if (!MutexIsHandled(handle))
		return -1;

	/* TODO: Mutex does not support file handles... */
	(void)mux;
	return -1;
}

BOOL MutexCloseHandle(HANDLE handle)
{
	WINPR_MUTEX* mutex = (WINPR_MUTEX*)handle;
	int rc;

	if (!MutexIsHandled(handle))
		return FALSE;

	if ((rc = pthread_mutex_destroy(&mutex->mutex)))
	{
		WLog_ERR(TAG, "pthread_mutex_destroy failed with %s [%d]", strerror(rc), rc);
#if defined(WITH_DEBUG_MUTEX)
		{
			size_t used = 0, i;
			void* stack = winpr_backtrace(20);
			char** msg = NULL;

			if (stack)
				msg = winpr_backtrace_symbols(stack, &used);

			if (msg)
			{
				for (i = 0; i < used; i++)
					WLog_ERR(TAG, "%2" PRIdz ": %s", i, msg[i]);
			}

			free(msg);
			winpr_backtrace_free(stack);
		}
#endif
		/**
		 * Note: unfortunately we may not return FALSE here since CloseHandle(hmutex) on
		 * Windows always seems to succeed independently of the mutex object locking state
		 */
	}

	free(mutex->name);
	free(handle);
	return TRUE;
}

static HANDLE_OPS ops = { MutexIsHandled,
	                      MutexCloseHandle,
	                      MutexGetFd,
	                      NULL, /* CleanupHandle */
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL };

HANDLE CreateMutexW(LPSECURITY_ATTRIBUTES lpMutexAttributes, BOOL bInitialOwner, LPCWSTR lpName)
{
	HANDLE handle;
	char* name = NULL;

	if (lpName)
	{
		name = ConvertWCharToUtf8Alloc(lpName, NULL);
		if (!name)
			return NULL;
	}

	handle = CreateMutexA(lpMutexAttributes, bInitialOwner, name);
	free(name);
	return handle;
}

HANDLE CreateMutexA(LPSECURITY_ATTRIBUTES lpMutexAttributes, BOOL bInitialOwner, LPCSTR lpName)
{
	HANDLE handle = NULL;
	WINPR_MUTEX* mutex;
	mutex = (WINPR_MUTEX*)calloc(1, sizeof(WINPR_MUTEX));

	if (lpMutexAttributes)
		WLog_WARN(TAG, "%s [%s] does not support lpMutexAttributes", __FUNCTION__, lpName);

	if (mutex)
	{
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
		pthread_mutex_init(&mutex->mutex, &attr);
		WINPR_HANDLE_SET_TYPE_AND_MODE(mutex, HANDLE_TYPE_MUTEX, WINPR_FD_READ);
		mutex->common.ops = &ops;
		handle = (HANDLE)mutex;

		if (bInitialOwner)
			pthread_mutex_lock(&mutex->mutex);

		if (lpName)
			mutex->name = strdup(lpName); /* Non runtime relevant information, skip NULL check */
	}

	return handle;
}

HANDLE CreateMutexExA(LPSECURITY_ATTRIBUTES lpMutexAttributes, LPCSTR lpName, DWORD dwFlags,
                      DWORD dwDesiredAccess)
{
	BOOL initial = FALSE;
	/* TODO: support access modes */

	if (dwDesiredAccess != 0)
		WLog_WARN(TAG, "%s [%s] does not support dwDesiredAccess 0x%08" PRIx32, __FUNCTION__,
		          lpName, dwDesiredAccess);

	if (dwFlags & CREATE_MUTEX_INITIAL_OWNER)
		initial = TRUE;

	return CreateMutexA(lpMutexAttributes, initial, lpName);
}

HANDLE CreateMutexExW(LPSECURITY_ATTRIBUTES lpMutexAttributes, LPCWSTR lpName, DWORD dwFlags,
                      DWORD dwDesiredAccess)
{
	BOOL initial = FALSE;

	/* TODO: support access modes */
	if (dwDesiredAccess != 0)
		WLog_WARN(TAG, "%s [%s] does not support dwDesiredAccess 0x%08" PRIx32, __FUNCTION__,
		          lpName, dwDesiredAccess);

	if (dwFlags & CREATE_MUTEX_INITIAL_OWNER)
		initial = TRUE;

	return CreateMutexW(lpMutexAttributes, initial, lpName);
}

HANDLE OpenMutexA(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCSTR lpName)
{
	/* TODO: Implement */
	WINPR_UNUSED(dwDesiredAccess);
	WINPR_UNUSED(bInheritHandle);
	WINPR_UNUSED(lpName);
	WLog_ERR(TAG, "%s not implemented", __FUNCTION__);
	return NULL;
}

HANDLE OpenMutexW(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCWSTR lpName)
{
	/* TODO: Implement */
	WINPR_UNUSED(dwDesiredAccess);
	WINPR_UNUSED(bInheritHandle);
	WINPR_UNUSED(lpName);
	WLog_ERR(TAG, "%s not implemented", __FUNCTION__);
	return NULL;
}

BOOL ReleaseMutex(HANDLE hMutex)
{
	ULONG Type;
	WINPR_HANDLE* Object;

	if (!winpr_Handle_GetInfo(hMutex, &Type, &Object))
		return FALSE;

	if (Type == HANDLE_TYPE_MUTEX)
	{
		WINPR_MUTEX* mutex = (WINPR_MUTEX*)Object;
		int rc = pthread_mutex_unlock(&mutex->mutex);

		if (rc)
		{
			WLog_ERR(TAG, "pthread_mutex_unlock failed with %s [%d]", strerror(rc), rc);
			return FALSE;
		}

		return TRUE;
	}

	return FALSE;
}

#endif
