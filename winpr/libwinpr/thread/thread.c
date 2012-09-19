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

#include <pthread.h>

typedef void *(*pthread_start_routine)(void*);

HANDLE CreateRemoteThread(HANDLE hProcess, LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize,
		LPTHREAD_START_ROUTINE lpStartAddress,LPVOID lpParameter,DWORD dwCreationFlags,LPDWORD lpThreadId)
{
	return NULL;
}

HANDLE CreateThread(LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize,
	LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter, DWORD dwCreationFlags, LPDWORD lpThreadId)
{
	HANDLE handle;
	pthread_t thread;

	pthread_create(&thread, 0, (pthread_start_routine) lpStartAddress, lpParameter);
	pthread_detach(thread);

	handle = winpr_Handle_Insert(HANDLE_TYPE_THREAD, (void*) thread);

	return handle;
}

VOID ExitThread(DWORD dwExitCode)
{

}

HANDLE GetCurrentThread(VOID)
{
	return NULL;
}

DWORD GetCurrentThreadId(VOID)
{
	return 0;
}

DWORD ResumeThread(HANDLE hThread)
{
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
	return TRUE;
}

#endif

