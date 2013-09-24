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

#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/environment.h>

#include <errno.h>
#include <spawn.h>
#include <sys/wait.h>

#include <pthread.h>

#include "thread.h"

#include "../handle/handle.h"

char** EnvironmentBlockToEnvpA(LPCH lpszEnvironmentBlock)
{
	char* p;
	int index;
	int count;
	int length;
	char** envp = NULL;

	count = 0;
	p = (char*) lpszEnvironmentBlock;

	while (p[0] && p[1])
	{
		length = strlen(p);
		p += (length + 1);
		count++;
	}

	index = 0;
	p = (char*) lpszEnvironmentBlock;

	envp = (char**) malloc(sizeof(char*) * (count + 1));
	envp[count] = NULL;

	while (p[0] && p[1])
	{
		length = strlen(p);
		envp[index] = _strdup(p);
		p += (length + 1);
		index++;
	}

	return envp;
}

BOOL _CreateProcessExA(HANDLE hToken, DWORD dwLogonFlags,
		LPCSTR lpApplicationName, LPSTR lpCommandLine, LPSECURITY_ATTRIBUTES lpProcessAttributes,
		LPSECURITY_ATTRIBUTES lpThreadAttributes, BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment,
		LPCSTR lpCurrentDirectory, LPSTARTUPINFOA lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation)
{
	pid_t pid;
	int status;
	int numArgs;
	LPSTR* pArgs;
	char** envp;
	WINPR_THREAD* thread;
	WINPR_PROCESS* process;
	LPTCH lpszEnvironmentBlock;

	pid = 0;
	envp = NULL;
	numArgs = 0;
	lpszEnvironmentBlock = NULL;

	pArgs = CommandLineToArgvA(lpCommandLine, &numArgs);

	if (lpEnvironment)
	{
		envp = EnvironmentBlockToEnvpA(lpEnvironment);
	}
	else
	{
		lpszEnvironmentBlock = GetEnvironmentStrings();
		envp = EnvironmentBlockToEnvpA(lpszEnvironmentBlock);
	}

	status = posix_spawnp(&pid, pArgs[0], NULL, NULL, pArgs, envp);

	if (status != 0)
		return FALSE;

	process = (WINPR_PROCESS*) malloc(sizeof(WINPR_PROCESS));

	if (!process)
		return FALSE;

	ZeroMemory(process, sizeof(WINPR_PROCESS));

	WINPR_HANDLE_SET_TYPE(process, HANDLE_TYPE_PROCESS);

	process->pid = pid;
	process->status = 0;
	process->dwExitCode = 0;

	thread = (WINPR_THREAD*) malloc(sizeof(WINPR_THREAD));

	ZeroMemory(thread, sizeof(WINPR_THREAD));

	if (!thread)
		return FALSE;

	WINPR_HANDLE_SET_TYPE(thread, HANDLE_TYPE_THREAD);

	thread->mainProcess = TRUE;

	lpProcessInformation->hProcess = (HANDLE) process;
	lpProcessInformation->hThread = (HANDLE) thread;
	lpProcessInformation->dwProcessId = (DWORD) pid;
	lpProcessInformation->dwThreadId = (DWORD) pid;

	return TRUE;
}

BOOL CreateProcessA(LPCSTR lpApplicationName, LPSTR lpCommandLine, LPSECURITY_ATTRIBUTES lpProcessAttributes,
		LPSECURITY_ATTRIBUTES lpThreadAttributes, BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment,
		LPCSTR lpCurrentDirectory, LPSTARTUPINFOA lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation)
{
	return _CreateProcessExA(NULL, 0,
			lpApplicationName, lpCommandLine, lpProcessAttributes,
			lpThreadAttributes, bInheritHandles, dwCreationFlags, lpEnvironment,
			lpCurrentDirectory, lpStartupInfo, lpProcessInformation);
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
	return _CreateProcessExA(hToken, 0,
			lpApplicationName, lpCommandLine, lpProcessAttributes,
			lpThreadAttributes, bInheritHandles, dwCreationFlags, lpEnvironment,
			lpCurrentDirectory, lpStartupInfo, lpProcessInformation);
}

BOOL CreateProcessAsUserW(HANDLE hToken, LPCWSTR lpApplicationName, LPWSTR lpCommandLine, LPSECURITY_ATTRIBUTES lpProcessAttributes,
		LPSECURITY_ATTRIBUTES lpThreadAttributes, BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment,
		LPCWSTR lpCurrentDirectory, LPSTARTUPINFOW lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation)
{
	return TRUE;
}

BOOL CreateProcessWithLogonA(LPCSTR lpUsername, LPCSTR lpDomain, LPCSTR lpPassword, DWORD dwLogonFlags,
		LPCSTR lpApplicationName, LPSTR lpCommandLine, DWORD dwCreationFlags, LPVOID lpEnvironment,
		LPCSTR lpCurrentDirectory, LPSTARTUPINFOA lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation)
{
	return TRUE;
}

BOOL CreateProcessWithLogonW(LPCWSTR lpUsername, LPCWSTR lpDomain, LPCWSTR lpPassword, DWORD dwLogonFlags,
		LPCWSTR lpApplicationName, LPWSTR lpCommandLine, DWORD dwCreationFlags, LPVOID lpEnvironment,
		LPCWSTR lpCurrentDirectory, LPSTARTUPINFOW lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation)
{
	return TRUE;
}

BOOL CreateProcessWithTokenA(HANDLE hToken, DWORD dwLogonFlags,
		LPCSTR lpApplicationName, LPSTR lpCommandLine, DWORD dwCreationFlags, LPVOID lpEnvironment,
		LPCSTR lpCurrentDirectory, LPSTARTUPINFOA lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation)
{
	return _CreateProcessExA(NULL, 0,
			lpApplicationName, lpCommandLine, NULL,
			NULL, FALSE, dwCreationFlags, lpEnvironment,
			lpCurrentDirectory, lpStartupInfo, lpProcessInformation);
}

BOOL CreateProcessWithTokenW(HANDLE hToken, DWORD dwLogonFlags,
		LPCWSTR lpApplicationName, LPWSTR lpCommandLine, DWORD dwCreationFlags, LPVOID lpEnvironment,
		LPCWSTR lpCurrentDirectory, LPSTARTUPINFOW lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation)
{
	return TRUE;
}

DECLSPEC_NORETURN VOID ExitProcess(UINT uExitCode)
{

}

BOOL GetExitCodeProcess(HANDLE hProcess, LPDWORD lpExitCode)
{
	WINPR_PROCESS* process;

	if (!hProcess)
		return FALSE;

	if (!lpExitCode)
		return FALSE;

	process = (WINPR_PROCESS*) hProcess;

	*lpExitCode = process->dwExitCode;

	return TRUE;
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

