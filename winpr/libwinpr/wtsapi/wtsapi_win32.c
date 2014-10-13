/**
 * WinPR: Windows Portable Runtime
 * Windows Terminal Services API
 *
 * Copyright 2013-2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/crt.h>
#include <winpr/io.h>
#include <winpr/nt.h>
#include <winpr/library.h>

#include <winpr/wtsapi.h>

#include "wtsapi_win32.h"

#include "wtsapi.h"

#define WTSAPI_CHANNEL_MAGIC	0x44484356

struct _WTSAPI_CHANNEL
{
	UINT32 magic;
	HANDLE hServer;
	DWORD SessionId;
	HANDLE hFile;
	CHAR VirtualName[8];
	BYTE unknown[12];
};
typedef struct _WTSAPI_CHANNEL WTSAPI_CHANNEL;

static BOOL g_Initialized = FALSE;
static HMODULE g_WinStaModule = NULL;

typedef HANDLE (WINAPI * fnWinStationVirtualOpen)(HANDLE hServer, DWORD SessionId, LPSTR pVirtualName);
typedef HANDLE (WINAPI * fnWinStationVirtualOpenEx)(HANDLE hServer, DWORD SessionId, LPSTR pVirtualName, DWORD flags);

static fnWinStationVirtualOpen pfnWinStationVirtualOpen = NULL;
static fnWinStationVirtualOpenEx pfnWinStationVirtualOpenEx = NULL;

HANDLE WINAPI Win32_WTSVirtualChannelOpen_Internal(HANDLE hServer, DWORD SessionId, LPSTR pVirtualName, DWORD flags)
{
	HANDLE hFile;
	WTSAPI_CHANNEL* pChannel;

	hFile = pfnWinStationVirtualOpenEx(hServer, SessionId, pVirtualName, flags);

	if (!hFile)
		return NULL;

	pChannel = (WTSAPI_CHANNEL*) LocalAlloc(LMEM_ZEROINIT, sizeof(WTSAPI_CHANNEL));

	if (!pChannel)
	{
		CloseHandle(hFile);
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		return NULL;
	}

	pChannel->magic = WTSAPI_CHANNEL_MAGIC;
	pChannel->hServer = hServer;
	pChannel->SessionId = SessionId;
	pChannel->hFile = hFile;
	CopyMemory(pChannel->VirtualName, pVirtualName, 8);

	return (HANDLE) pChannel;
}

HANDLE WINAPI Win32_WTSVirtualChannelOpen(HANDLE hServer, DWORD SessionId, LPSTR pVirtualName)
{
	return Win32_WTSVirtualChannelOpen_Internal(hServer, SessionId, pVirtualName, 0);
}

HANDLE WINAPI Win32_WTSVirtualChannelOpenEx(DWORD SessionId, LPSTR pVirtualName, DWORD flags)
{
	return Win32_WTSVirtualChannelOpen_Internal(0, SessionId, pVirtualName, flags);
}

BOOL WINAPI Win32_WTSVirtualChannelClose(HANDLE hChannel)
{
	BOOL status;
	WTSAPI_CHANNEL* pChannel = (WTSAPI_CHANNEL*) hChannel;

	if (!pChannel || (pChannel->magic != WTSAPI_CHANNEL_MAGIC))
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	status = CloseHandle(pChannel->hFile);
	pChannel->magic = 0;
	LocalFree(pChannel);

	return status;
}

BOOL WINAPI Win32_WTSVirtualChannelRead(HANDLE hChannel, DWORD dwMilliseconds, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesTransferred)
{
	OVERLAPPED overlapped;
	WTSAPI_CHANNEL* pChannel = (WTSAPI_CHANNEL*) hChannel;

	if (!pChannel || (pChannel->magic != WTSAPI_CHANNEL_MAGIC))
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	ZeroMemory(&overlapped, sizeof(OVERLAPPED));

	if (ReadFile(pChannel->hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesTransferred, &overlapped))
		return TRUE;

	if (GetLastError() != ERROR_IO_PENDING)
		return FALSE;

	if (!dwMilliseconds)
	{
		CancelIo(pChannel->hFile);
		*lpNumberOfBytesTransferred = 0;
		return TRUE;
	}

	if (WaitForSingleObject(pChannel->hFile, dwMilliseconds) != WAIT_TIMEOUT)
		return GetOverlappedResult(pChannel->hFile, &overlapped, lpNumberOfBytesTransferred, FALSE);

	CancelIo(pChannel->hFile);
	SetLastError(ERROR_IO_INCOMPLETE);

	return FALSE;
}

BOOL WINAPI Win32_WTSVirtualChannelWrite(HANDLE hChannel, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesTransferred)
{
	OVERLAPPED overlapped;
	WTSAPI_CHANNEL* pChannel = (WTSAPI_CHANNEL*) hChannel;

	if (!pChannel || (pChannel->magic != WTSAPI_CHANNEL_MAGIC))
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	ZeroMemory(&overlapped, sizeof(OVERLAPPED));

	if (WriteFile(pChannel->hFile, lpBuffer, nNumberOfBytesToWrite, lpNumberOfBytesTransferred, &overlapped))
		return TRUE;

	if (GetLastError() == ERROR_IO_PENDING)
		return GetOverlappedResult(pChannel->hFile, &overlapped, lpNumberOfBytesTransferred, TRUE);

	return FALSE;
}

#ifndef FILE_DEVICE_TERMSRV
#define FILE_DEVICE_TERMSRV		0x00000038
#endif

BOOL Win32_WTSVirtualChannelPurge_Internal(HANDLE hChannelHandle, ULONG IoControlCode)
{
	DWORD error;
	NTSTATUS ntstatus;
	IO_STATUS_BLOCK ioStatusBlock;
	WTSAPI_CHANNEL* pChannel = (WTSAPI_CHANNEL*) hChannelHandle;

	if (!pChannel || (pChannel->magic != WTSAPI_CHANNEL_MAGIC))
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	ntstatus = _NtDeviceIoControlFile(pChannel->hFile, 0, 0, 0, &ioStatusBlock, IoControlCode, 0, 0, 0, 0);

	if (ntstatus == STATUS_PENDING)
	{
		ntstatus = _NtWaitForSingleObject(pChannel->hFile, 0, 0);

		if (ntstatus >= 0)
			ntstatus = ioStatusBlock.Status;
	}

	if (ntstatus == STATUS_BUFFER_OVERFLOW)
	{
		ntstatus = STATUS_BUFFER_TOO_SMALL;
		error = _RtlNtStatusToDosError(ntstatus);
		SetLastError(error);
		return FALSE;
	}

	if (ntstatus < 0)
	{
		error = _RtlNtStatusToDosError(ntstatus);
		SetLastError(error);
		return FALSE;
	}

	return TRUE;
}

BOOL WINAPI Win32_WTSVirtualChannelPurgeInput(HANDLE hChannelHandle)
{
	return Win32_WTSVirtualChannelPurge_Internal(hChannelHandle, (FILE_DEVICE_TERMSRV << 16) | 0x0107);
}

BOOL WINAPI Win32_WTSVirtualChannelPurgeOutput(HANDLE hChannelHandle)
{
	return Win32_WTSVirtualChannelPurge_Internal(hChannelHandle, (FILE_DEVICE_TERMSRV << 16) | 0x010B);
}

BOOL WINAPI Win32_WTSVirtualChannelQuery(HANDLE hChannelHandle, WTS_VIRTUAL_CLASS WtsVirtualClass, PVOID* ppBuffer, DWORD* pBytesReturned)
{
	WTSAPI_CHANNEL* pChannel = (WTSAPI_CHANNEL*) hChannelHandle;

	if (!pChannel || (pChannel->magic != WTSAPI_CHANNEL_MAGIC))
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	if (WtsVirtualClass == WTSVirtualClientData)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}
	else if (WtsVirtualClass == WTSVirtualFileHandle)
	{
		*pBytesReturned = sizeof(HANDLE);
		*ppBuffer = LocalAlloc(LMEM_ZEROINIT, *pBytesReturned);

		if (*ppBuffer == NULL)
		{
			SetLastError(ERROR_NOT_ENOUGH_MEMORY);
			return FALSE;
		}

		CopyMemory(*ppBuffer, &(pChannel->hFile), *pBytesReturned);
	}
	else
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	return TRUE;
}

VOID WINAPI Win32_WTSFreeMemory(PVOID pMemory)
{
	LocalFree(pMemory);
	return TRUE;
}

BOOL WINAPI Win32_WTSFreeMemoryExW(WTS_TYPE_CLASS WTSTypeClass, PVOID pMemory, ULONG NumberOfEntries)
{
	return TRUE;
}

BOOL WINAPI Win32_WTSFreeMemoryExA(WTS_TYPE_CLASS WTSTypeClass, PVOID pMemory, ULONG NumberOfEntries)
{
	return WTSFreeMemoryExW(WTSTypeClass, pMemory, NumberOfEntries);
}

int Win32_InitializeWinSta(PWtsApiFunctionTable pWtsApi)
{
	g_WinStaModule = LoadLibraryA("winsta.dll");

	if (!g_WinStaModule)
		return -1;

	pfnWinStationVirtualOpen = (fnWinStationVirtualOpen) GetProcAddress(g_WinStaModule, "WinStationVirtualOpen");
	pfnWinStationVirtualOpenEx = (fnWinStationVirtualOpenEx) GetProcAddress(g_WinStaModule, "WinStationVirtualOpenEx");

	if (!pfnWinStationVirtualOpenEx)
		return -1;

	pWtsApi->pVirtualChannelOpen = Win32_WTSVirtualChannelOpen;
	pWtsApi->pVirtualChannelOpenEx = Win32_WTSVirtualChannelOpenEx;
	pWtsApi->pVirtualChannelClose = Win32_WTSVirtualChannelClose;
	pWtsApi->pVirtualChannelRead = Win32_WTSVirtualChannelRead;
	pWtsApi->pVirtualChannelWrite = Win32_WTSVirtualChannelWrite;
	pWtsApi->pVirtualChannelPurgeInput = Win32_WTSVirtualChannelPurgeInput;
	pWtsApi->pVirtualChannelPurgeOutput = Win32_WTSVirtualChannelPurgeOutput;
	pWtsApi->pVirtualChannelQuery = Win32_WTSVirtualChannelQuery;
	pWtsApi->pFreeMemory = Win32_WTSFreeMemory;
	//pWtsApi->pFreeMemoryExW = Win32_WTSFreeMemoryExW;
	//pWtsApi->pFreeMemoryExA = Win32_WTSFreeMemoryExA;

	return 1;
}
