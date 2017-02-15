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

#define STARTF_USESHOWWINDOW        0x00000001
#define STARTF_USESIZE              0x00000002
#define STARTF_USEPOSITION          0x00000004
#define STARTF_USECOUNTCHARS        0x00000008
#define STARTF_USEFILLATTRIBUTE     0x00000010
#define STARTF_RUNFULLSCREEN        0x00000020
#define STARTF_FORCEONFEEDBACK      0x00000040
#define STARTF_FORCEOFFFEEDBACK     0x00000080
#define STARTF_USESTDHANDLES        0x00000100
#define STARTF_USEHOTKEY            0x00000200
#define STARTF_TITLEISLINKNAME      0x00000800
#define STARTF_TITLEISAPPID         0x00001000
#define STARTF_PREVENTPINNING       0x00002000

/* Process */

#define LOGON_WITH_PROFILE			0x00000001
#define LOGON_NETCREDENTIALS_ONLY		0x00000002
#define LOGON_ZERO_PASSWORD_BUFFER		0x80000000

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

WINPR_API BOOL CreateProcessWithLogonA(LPCSTR lpUsername, LPCSTR lpDomain, LPCSTR lpPassword, DWORD dwLogonFlags,
		LPCSTR lpApplicationName, LPSTR lpCommandLine, DWORD dwCreationFlags, LPVOID lpEnvironment,
		LPCSTR lpCurrentDirectory, LPSTARTUPINFOA lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation);

WINPR_API BOOL CreateProcessWithLogonW(LPCWSTR lpUsername, LPCWSTR lpDomain, LPCWSTR lpPassword, DWORD dwLogonFlags,
		LPCWSTR lpApplicationName, LPWSTR lpCommandLine, DWORD dwCreationFlags, LPVOID lpEnvironment,
		LPCWSTR lpCurrentDirectory, LPSTARTUPINFOW lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation);

WINPR_API BOOL CreateProcessWithTokenA(HANDLE hToken, DWORD dwLogonFlags,
		LPCSTR lpApplicationName, LPSTR lpCommandLine, DWORD dwCreationFlags, LPVOID lpEnvironment,
		LPCSTR lpCurrentDirectory, LPSTARTUPINFOA lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation);

WINPR_API BOOL CreateProcessWithTokenW(HANDLE hToken, DWORD dwLogonFlags,
		LPCWSTR lpApplicationName, LPWSTR lpCommandLine, DWORD dwCreationFlags, LPVOID lpEnvironment,
		LPCWSTR lpCurrentDirectory, LPSTARTUPINFOW lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation);

#ifdef UNICODE
#define CreateProcess		CreateProcessW
#define CreateProcessAsUser	CreateProcessAsUserW
#define CreateProcessWithLogon	CreateProcessWithLogonW
#define CreateProcessWithToken	CreateProcessWithTokenW
#else
#define CreateProcess		CreateProcessA
#define CreateProcessAsUser	CreateProcessAsUserA
#define CreateProcessWithLogon	CreateProcessWithLogonA
#define CreateProcessWithToken	CreateProcessWithTokenA
#endif

DECLSPEC_NORETURN WINPR_API VOID ExitProcess(UINT uExitCode);
WINPR_API BOOL GetExitCodeProcess(HANDLE hProcess, LPDWORD lpExitCode);

WINPR_API HANDLE _GetCurrentProcess(void);
WINPR_API DWORD GetCurrentProcessId(void);

WINPR_API BOOL TerminateProcess(HANDLE hProcess, UINT uExitCode);

/* Process Argument Vector Parsing */

WINPR_API LPWSTR* CommandLineToArgvW(LPCWSTR lpCmdLine, int* pNumArgs);

#ifdef UNICODE
#define CommandLineToArgv	CommandLineToArgvW
#else
#define CommandLineToArgv	CommandLineToArgvA
#endif

/* Thread */

#define CREATE_SUSPENDED				0x00000004
#define STACK_SIZE_PARAM_IS_A_RESERVATION		0x00010000

WINPR_API HANDLE CreateThread(LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize,
	LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter, DWORD dwCreationFlags, LPDWORD lpThreadId);

WINPR_API HANDLE CreateRemoteThread(HANDLE hProcess, LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize,
		LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter, DWORD dwCreationFlags, LPDWORD lpThreadId);

WINPR_API DECLSPEC_NORETURN VOID ExitThread(DWORD dwExitCode);
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

/* CommandLineToArgvA is not present in the original Windows API, WinPR always exports it */

WINPR_API LPSTR *CommandLineToArgvA(LPCSTR lpCmdLine, int *pNumArgs);

#if defined(WITH_DEBUG_THREADS)
WINPR_API VOID DumpThreadHandles(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* WINPR_THREAD_H */

