/**
 * WinPR: Windows Portable Runtime
 * Pipe Functions
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

#ifndef WINPR_PIPE_H
#define WINPR_PIPE_H

#include <winpr/file.h>
#include <winpr/winpr.h>
#include <winpr/error.h>
#include <winpr/handle.h>
#include <winpr/wtypes.h>

#ifndef _WIN32

#define PIPE_UNLIMITED_INSTANCES	0xFF

#define PIPE_ACCESS_INBOUND		0x00000001
#define PIPE_ACCESS_OUTBOUND		0x00000002
#define PIPE_ACCESS_DUPLEX		0x00000003

#define FILE_FLAG_FIRST_PIPE_INSTANCE	0x00080000
#define FILE_FLAG_WRITE_THROUGH		0x80000000
#define FILE_FLAG_OVERLAPPED		0x40000000

#define PIPE_CLIENT_END			0x00000000
#define PIPE_SERVER_END			0x00000001

#define PIPE_TYPE_BYTE			0x00000000
#define PIPE_TYPE_MESSAGE		0x00000004

#define PIPE_READMODE_BYTE		0x00000000
#define PIPE_READMODE_MESSAGE		0x00000002

#define PIPE_WAIT			0x00000000
#define PIPE_NOWAIT			0x00000001

#define PIPE_ACCEPT_REMOTE_CLIENTS	0x00000000
#define PIPE_REJECT_REMOTE_CLIENTS	0x00000008

#define NMPWAIT_USE_DEFAULT_WAIT	0x00000000
#define NMPWAIT_NOWAIT			0x00000001
#define NMPWAIT_WAIT_FOREVER		0xFFFFFFFF

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Unnamed pipe
 */

WINPR_API BOOL CreatePipe(PHANDLE hReadPipe, PHANDLE hWritePipe, LPSECURITY_ATTRIBUTES lpPipeAttributes, DWORD nSize);

/**
 * Named pipe
 */

WINPR_API HANDLE CreateNamedPipeA(LPCSTR lpName, DWORD dwOpenMode, DWORD dwPipeMode, DWORD nMaxInstances,
		DWORD nOutBufferSize, DWORD nInBufferSize, DWORD nDefaultTimeOut, LPSECURITY_ATTRIBUTES lpSecurityAttributes);
WINPR_API HANDLE CreateNamedPipeW(LPCWSTR lpName, DWORD dwOpenMode, DWORD dwPipeMode, DWORD nMaxInstances,
		DWORD nOutBufferSize, DWORD nInBufferSize, DWORD nDefaultTimeOut, LPSECURITY_ATTRIBUTES lpSecurityAttributes);

WINPR_API BOOL ConnectNamedPipe(HANDLE hNamedPipe, LPOVERLAPPED lpOverlapped);

WINPR_API BOOL DisconnectNamedPipe(HANDLE hNamedPipe);

WINPR_API BOOL PeekNamedPipe(HANDLE hNamedPipe, LPVOID lpBuffer, DWORD nBufferSize,
		LPDWORD lpBytesRead, LPDWORD lpTotalBytesAvail, LPDWORD lpBytesLeftThisMessage);

WINPR_API BOOL TransactNamedPipe(HANDLE hNamedPipe, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer,
		DWORD nOutBufferSize, LPDWORD lpBytesRead, LPOVERLAPPED lpOverlapped);

WINPR_API BOOL WaitNamedPipeA(LPCSTR lpNamedPipeName, DWORD nTimeOut);
WINPR_API BOOL WaitNamedPipeW(LPCWSTR lpNamedPipeName, DWORD nTimeOut);

WINPR_API BOOL SetNamedPipeHandleState(HANDLE hNamedPipe, LPDWORD lpMode, LPDWORD lpMaxCollectionCount, LPDWORD lpCollectDataTimeout);

WINPR_API BOOL ImpersonateNamedPipeClient(HANDLE hNamedPipe);

WINPR_API BOOL GetNamedPipeClientComputerNameA(HANDLE Pipe, LPCSTR ClientComputerName, ULONG ClientComputerNameLength);
WINPR_API BOOL GetNamedPipeClientComputerNameW(HANDLE Pipe, LPCWSTR ClientComputerName, ULONG ClientComputerNameLength);

#ifdef UNICODE
#define CreateNamedPipe				CreateNamedPipeW
#define WaitNamedPipe				WaitNamedPipeW
#define GetNamedPipeClientComputerName		GetNamedPipeClientComputerNameW
#else
#define CreateNamedPipe				CreateNamedPipeA
#define WaitNamedPipe				WaitNamedPipeA
#define GetNamedPipeClientComputerName		GetNamedPipeClientComputerNameA
#endif

#ifdef __cplusplus
}
#endif

#endif

#endif /* WINPR_PIPE_H */
