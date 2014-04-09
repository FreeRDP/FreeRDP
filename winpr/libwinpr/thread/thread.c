/**
 * WinPR: Windows Portable Runtime
 * Process Thread Functions
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

#include <winpr/thread.h>

/**
 * api-ms-win-core-processthreads-l1-1-1.dll
 * 
 * CreateRemoteThread
 * CreateRemoteThreadEx
 * CreateThread
 * DeleteProcThreadAttributeList
 * ExitThread
 * FlushInstructionCache
 * FlushProcessWriteBuffers
 * GetCurrentThread
 * GetCurrentThreadId
 * GetCurrentThreadStackLimits
 * GetExitCodeThread
 * GetPriorityClass
 * GetStartupInfoW
 * GetThreadContext
 * GetThreadId
 * GetThreadIdealProcessorEx
 * GetThreadPriority
 * GetThreadPriorityBoost
 * GetThreadTimes
 * InitializeProcThreadAttributeList
 * OpenThread
 * OpenThreadToken
 * QueryProcessAffinityUpdateMode
 * QueueUserAPC
 * ResumeThread
 * SetPriorityClass
 * SetThreadContext
 * SetThreadPriority
 * SetThreadPriorityBoost
 * SetThreadStackGuarantee
 * SetThreadToken
 * SuspendThread
 * SwitchToThread
 * TerminateThread
 * UpdateProcThreadAttribute
 */

#ifndef _WIN32

#include <winpr/crt.h>

#include "thread.h"

#include "../handle/handle.h"

/**
 * TODO: implement thread suspend/resume using pthreads
 * http://stackoverflow.com/questions/3140867/suspend-pthreads-without-using-condition
 */

void winpr_StartThread(WINPR_THREAD* thread)
{
	pthread_attr_t attr;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	if (thread->dwStackSize > 0)
		pthread_attr_setstacksize(&attr, (size_t) thread->dwStackSize);

	thread->started = TRUE;
	pthread_create(&thread->thread, &attr, (pthread_start_routine) thread->lpStartAddress, thread->lpParameter);

	pthread_attr_destroy(&attr);
}

HANDLE CreateThread(LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize,
	LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter, DWORD dwCreationFlags, LPDWORD lpThreadId)
{
	HANDLE handle;
	WINPR_THREAD* thread;

	thread = (WINPR_THREAD*) calloc(1, sizeof(WINPR_THREAD));
	if (!thread)
		return NULL;

	thread->started = FALSE;
	thread->dwStackSize = dwStackSize;
	thread->lpParameter = lpParameter;
	thread->lpStartAddress = lpStartAddress;
	thread->lpThreadAttributes = lpThreadAttributes;

	pthread_mutex_init(&thread->mutex, 0);

	WINPR_HANDLE_SET_TYPE(thread, HANDLE_TYPE_THREAD);
	handle = (HANDLE) thread;

	if (!(dwCreationFlags & CREATE_SUSPENDED))
		winpr_StartThread(thread);

	return handle;
}

HANDLE CreateRemoteThread(HANDLE hProcess, LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize,
		LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter, DWORD dwCreationFlags, LPDWORD lpThreadId)
{
	return NULL;
}

VOID ExitThread(DWORD dwExitCode)
{
	pthread_exit((void*) (size_t) dwExitCode);
}

BOOL GetExitCodeThread(HANDLE hThread, LPDWORD lpExitCode)
{
	ULONG Type;
	PVOID Object;
	WINPR_THREAD* thread;

	if (!winpr_Handle_GetInfo(hThread, &Type, &Object))
		return FALSE;

	thread = (WINPR_THREAD*) Object;

	*lpExitCode = thread->dwExitCode;

	return TRUE;
}

HANDLE _GetCurrentThread(VOID)
{
	return NULL;
}

DWORD GetCurrentThreadId(VOID)
{
	pthread_t tid;
	tid = pthread_self();
	return (DWORD) tid;
}

DWORD ResumeThread(HANDLE hThread)
{
	ULONG Type;
	PVOID Object;
	WINPR_THREAD* thread;

	if (!winpr_Handle_GetInfo(hThread, &Type, &Object))
		return 0;

	thread = (WINPR_THREAD*) Object;

	pthread_mutex_lock(&thread->mutex);

	if (!thread->started)
		winpr_StartThread(thread);

	pthread_mutex_unlock(&thread->mutex);

	return 0;
}

DWORD SuspendThread(HANDLE hThread)
{
	return 0;
}

BOOL SwitchToThread(VOID)
{
	return TRUE;
}

BOOL TerminateThread(HANDLE hThread, DWORD dwExitCode)
{
	ULONG Type;
	PVOID Object;
	WINPR_THREAD* thread;

	if (!winpr_Handle_GetInfo(hThread, &Type, &Object))
		return 0;

	thread = (WINPR_THREAD*) Object;

	pthread_mutex_lock(&thread->mutex);

#ifndef ANDROID
	pthread_cancel(thread->thread);
#endif

	pthread_mutex_unlock(&thread->mutex);

	return TRUE;
}

#endif

