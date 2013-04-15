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

#ifndef WINPR_THREAD_H
#define WINPR_THREAD_H

#include <winpr/winpr.h>
#include <winpr/wtypes.h>

#include <winpr/spec.h>
#include <winpr/handle.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _WIN32

typedef struct _STARTUPINFOA
{
	DWORD cb;
	LPSTR lpReserved;
	LPSTR lpDesktop;
	LPSTR lpTitle;
	DWORD dwX;
	DWORD dwY;
	DWORD dwXSize;
	DWORD dwYSize;
	DWORD dwXCountChars;
	DWORD dwYCountChars;
	DWORD dwFillAttribute;
	DWORD dwFlags;
	WORD wShowWindow;
	WORD cbReserved2;
	LPBYTE lpReserved2;
	HANDLE hStdInput;
	HANDLE hStdOutput;
	HANDLE hStdError;
} STARTUPINFOA, *LPSTARTUPINFOA;

typedef struct _STARTUPINFOW
{
	DWORD cb;
	LPWSTR lpReserved;
	LPWSTR lpDesktop;
	LPWSTR lpTitle;
	DWORD dwX;
	DWORD dwY;
	DWORD dwXSize;
	DWORD dwYSize;
	DWORD dwXCountChars;
	DWORD dwYCountChars;
	DWORD dwFillAttribute;
	DWORD dwFlags;
	WORD wShowWindow;
	WORD cbReserved2;
	LPBYTE lpReserved2;
	HANDLE hStdInput;
	HANDLE hStdOutput;
	HANDLE hStdError;
} STARTUPINFOW, *LPSTARTUPINFOW;

#ifdef UNICODE
typedef STARTUPINFOW	STARTUPINFO;
typedef LPSTARTUPINFOW	LPSTARTUPINFO;
#else
typedef STARTUPINFOA	STARTUPINFO;
typedef LPSTARTUPINFOA	LPSTARTUPINFO;
#endif

/* Process */

WINPR_API BOOL CreateProcessA(LPCSTR lpApplicationName, LPSTR lpCommandLine, LPSECURITY_ATTRIBUTES lpProcessAttributes,
		LPSECURITY_ATTRIBUTES lpThreadAttributes, BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment,
		LPCSTR lpCurrentDirectory, LPSTARTUPINFOA lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation);

WINPR_API BOOL CreateProcessW(LPCWSTR lpApplicationName, LPWSTR lpCommandLine, LPSECURITY_ATTRIBUTES lpProcessAttributes,
		LPSECURITY_ATTRIBUTES lpThreadAttributes, BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment,
		LPCWSTR lpCurrentDirectory, LPSTARTUPINFOW lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation);

WINPR_API BOOL CreateProcessAsUserA(HANDLE hToken, LPCSTR lpApplicationName, LPSTR lpCommandLine, LPSECURITY_ATTRIBUTES lpProcessAttributes,
		LPSECURITY_ATTRIBUTES lpThreadAttributes, BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment,
		LPCSTR lpCurrentDirectory, LPSTARTUPINFOA lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation);

WINPR_API BOOL CreateProcessAsUserW(HANDLE hToken, LPCWSTR lpApplicationName, LPWSTR lpCommandLine, LPSECURITY_ATTRIBUTES lpProcessAttributes,
		LPSECURITY_ATTRIBUTES lpThreadAttributes, BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment,
		LPCWSTR lpCurrentDirectory, LPSTARTUPINFOW lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation);

#ifdef UNICODE
#define CreateProcess		CreateProcessW
#define CreateProcessAsUser	CreateProcessAsUserW
#else
#define CreateProcess		CreateProcessA
#define CreateProcessAsUser	CreateProcessAsUserA
#endif

DECLSPEC_NORETURN WINPR_API VOID ExitProcess(UINT uExitCode);

WINPR_API HANDLE _GetCurrentProcess(void);
WINPR_API DWORD GetCurrentProcessId(void);

WINPR_API BOOL TerminateProcess(HANDLE hProcess, UINT uExitCode);

/* Thread */

#define CREATE_SUSPENDED				0x00000004
#define STACK_SIZE_PARAM_IS_A_RESERVATION		0x00010000

WINPR_API HANDLE CreateThread(LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize,
	LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter, DWORD dwCreationFlags, LPDWORD lpThreadId);

WINPR_API HANDLE CreateRemoteThread(HANDLE hProcess, LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize,
		LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter, DWORD dwCreationFlags, LPDWORD lpThreadId);

DECLSPEC_NORETURN WINPR_API VOID ExitThread(DWORD dwExitCode);
WINPR_API BOOL GetExitCodeThread(HANDLE hThread, LPDWORD lpExitCode);

WINPR_API HANDLE _GetCurrentThread(void);
WINPR_API DWORD GetCurrentThreadId(void);

WINPR_API DWORD ResumeThread(HANDLE hThread);
WINPR_API DWORD SuspendThread(HANDLE hThread);
WINPR_API BOOL SwitchToThread(void);

WINPR_API BOOL TerminateThread(HANDLE hThread, DWORD dwExitCode);

/* Processor */

WINPR_API DWORD GetCurrentProcessorNumber(void);

/* Thread-Local Storage */

#define TLS_OUT_OF_INDEXES	((DWORD) 0xFFFFFFFF)

WINPR_API DWORD TlsAlloc(void);
WINPR_API LPVOID TlsGetValue(DWORD dwTlsIndex);
WINPR_API BOOL TlsSetValue(DWORD dwTlsIndex, LPVOID lpTlsValue);
WINPR_API BOOL TlsFree(DWORD dwTlsIndex);

#else

/*
 * GetCurrentProcess / GetCurrentThread cause a conflict on Mac OS X
 */
#define _GetCurrentProcess	GetCurrentProcess
#define _GetCurrentThread	GetCurrentThread

#endif

#ifdef __cplusplus
}
#endif

#endif /* WINPR_THREAD_H */

