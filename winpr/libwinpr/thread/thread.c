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

#include <winpr/thread.h>

/**
 * api-ms-win-core-processthreads-l1-1-1.dll
 * 
 * CreateProcessA
 * CreateProcessAsUserW
 * CreateProcessW
 * CreateRemoteThread
 * CreateRemoteThreadEx
 * CreateThread
 * DeleteProcThreadAttributeList
 * ExitProcess
 * ExitThread
 * FlushInstructionCache
 * FlushProcessWriteBuffers
 * GetCurrentProcess
 * GetCurrentProcessId
 * GetCurrentProcessorNumber
 * GetCurrentProcessorNumberEx
 * GetCurrentThread
 * GetCurrentThreadId
 * GetCurrentThreadStackLimits
 * GetExitCodeProcess
 * GetExitCodeThread
 * GetPriorityClass
 * GetProcessHandleCount
 * GetProcessId
 * GetProcessIdOfThread
 * GetProcessMitigationPolicy
 * GetProcessTimes
 * GetProcessVersion
 * GetStartupInfoW
 * GetThreadContext
 * GetThreadId
 * GetThreadIdealProcessorEx
 * GetThreadPriority
 * GetThreadPriorityBoost
 * GetThreadTimes
 * InitializeProcThreadAttributeList
 * IsProcessorFeaturePresent
 * OpenProcess
 * OpenProcessToken
 * OpenThread
 * OpenThreadToken
 * ProcessIdToSessionId
 * QueryProcessAffinityUpdateMode
 * QueueUserAPC
 * ResumeThread
 * SetPriorityClass
 * SetProcessAffinityUpdateMode
 * SetProcessMitigationPolicy
 * SetProcessShutdownParameters
 * SetThreadContext
 * SetThreadIdealProcessorEx
 * SetThreadPriority
 * SetThreadPriorityBoost
 * SetThreadStackGuarantee
 * SetThreadToken
 * SuspendThread
 * SwitchToThread
 * TerminateProcess
 * TerminateThread
 * TlsAlloc
 * TlsFree
 * TlsGetValue
 * TlsSetValue
 * UpdateProcThreadAttribute
 */

#ifndef _WIN32

#include <pthread.h>

typedef void *(*pthread_start_routine)(void*);

BOOL CreateProcessA(LPCSTR lpApplicationName, LPSTR lpCommandLine, LPSECURITY_ATTRIBUTES lpProcessAttributes,
		LPSECURITY_ATTRIBUTES lpThreadAttributes, BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment,
		LPCSTR lpCurrentDirectory, LPSTARTUPINFOA lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation)
{
	return TRUE;
}

BOOL CreateProcessW(LPCWSTR lpApplicationName, LPWSTR lpCommandLine, LPSECURITY_ATTRIBUTES lpProcessAttributes,
		LPSECURITY_ATTRIBUTES lpThreadAttributes, BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment,
		LPCWSTR lpCurrentDirectory, LPSTARTUPINFOW lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation)
{
	return TRUE;
}

BOOL CreateProcessAsUserA(HANDLE hToken, LPCSTR lpApplicationName, LPSTR lpCommandLine, LPSECURITY_ATTRIBUTES lpProcessAttributes,
		LPSECURITY_ATTRIBUTES lpThreadAttributes, BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment,
		LPCSTR lpCurrentDirectory, LPSTARTUPINFOA lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation)
{
	return TRUE;
}

BOOL CreateProcessAsUserW(HANDLE hToken, LPCWSTR lpApplicationName, LPWSTR lpCommandLine, LPSECURITY_ATTRIBUTES lpProcessAttributes,
		LPSECURITY_ATTRIBUTES lpThreadAttributes, BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment,
		LPCWSTR lpCurrentDirectory, LPSTARTUPINFOW lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation)
{
	return TRUE;
}

HANDLE CreateRemoteThread(HANDLE hProcess, LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize,
		LPTHREAD_START_ROUTINE lpStartAddress,LPVOID lpParameter,DWORD dwCreationFlags,LPDWORD lpThreadId)
{
	return NULL;
}

HANDLE CreateThread(LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize,
	LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter, DWORD dwCreationFlags, LPDWORD lpThreadId)
{
	pthread_t thread;
	pthread_create(&thread, 0, (pthread_start_routine) lpStartAddress, lpParameter);
	pthread_detach(thread);

	return NULL;
}

VOID ExitProcess(UINT uExitCode)
{

}

VOID ExitThread(DWORD dwExitCode)
{

}

HANDLE GetCurrentProcess(VOID)
{
	return NULL;
}

DWORD GetCurrentProcessId(VOID)
{
	return 0;
}

DWORD GetCurrentProcessorNumber(VOID)
{
	return 0;
}

HANDLE GetCurrentThread(VOID)
{
	return NULL;
}

DWORD GetCurrentThreadId(VOID)
{
	return 0;
}

DWORD GetProcessId(HANDLE Process)
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

BOOL TerminateProcess(HANDLE hProcess, UINT uExitCode)
{
	return TRUE;
}

BOOL TerminateThread(HANDLE hThread, DWORD dwExitCode)
{
	return TRUE;
}

DWORD TlsAlloc(VOID)
{
	return 0;
}

LPVOID TlsGetValue(DWORD dwTlsIndex)
{
	return NULL;
}

BOOL TlsSetValue(DWORD dwTlsIndex, LPVOID lpTlsValue)
{
	return TRUE;
}

BOOL TlsFree(DWORD dwTlsIndex)
{
	return TRUE;
}

#endif

