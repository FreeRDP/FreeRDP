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
 * CreateProcessA
 * CreateProcessW
 * CreateProcessAsUserA
 * CreateProcessAsUserW
 * ExitProcess
 * GetCurrentProcess
 * GetCurrentProcessId
 * GetExitCodeProcess
 * GetProcessHandleCount
 * GetProcessId
 * GetProcessIdOfThread
 * GetProcessMitigationPolicy
 * GetProcessTimes
 * GetProcessVersion
 * OpenProcess
 * OpenProcessToken
 * ProcessIdToSessionId
 * SetProcessAffinityUpdateMode
 * SetProcessMitigationPolicy
 * SetProcessShutdownParameters
 * TerminateProcess
 */

#ifndef _WIN32

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

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

DECLSPEC_NORETURN VOID ExitProcess(UINT uExitCode)
{

}

HANDLE _GetCurrentProcess(VOID)
{
	return NULL;
}

DWORD GetCurrentProcessId(VOID)
{
	return ((DWORD) getpid());
}

DWORD GetProcessId(HANDLE Process)
{
	return 0;
}

BOOL TerminateProcess(HANDLE hProcess, UINT uExitCode)
{
	return TRUE;
}

#endif

