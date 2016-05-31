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

#include "../log.h"

#define WTSAPI_CHANNEL_MAGIC	0x44484356
#define TAG WINPR_TAG("wtsapi")

struct _WTSAPI_CHANNEL
{
	UINT32 magic;
	HANDLE hServer;
	DWORD SessionId;
	HANDLE hFile;
	HANDLE hEvent;
	char* VirtualName;

	DWORD flags;
	BYTE* chunk;
	BOOL dynamic;
	BOOL readSync;
	BOOL readAsync;
	BOOL readDone;
	UINT32 readSize;
	UINT32 readOffset;
	BYTE* readBuffer;
	BOOL showProtocol;
	BOOL waitObjectMode;
	OVERLAPPED overlapped;
	CHANNEL_PDU_HEADER* header;
};
typedef struct _WTSAPI_CHANNEL WTSAPI_CHANNEL;

static BOOL g_Initialized = FALSE;
static HMODULE g_WinStaModule = NULL;

typedef HANDLE (WINAPI * fnWinStationVirtualOpen)(HANDLE hServer, DWORD SessionId, LPSTR pVirtualName);
typedef HANDLE (WINAPI * fnWinStationVirtualOpenEx)(HANDLE hServer, DWORD SessionId, LPSTR pVirtualName, DWORD flags);

static fnWinStationVirtualOpen pfnWinStationVirtualOpen = NULL;
static fnWinStationVirtualOpenEx pfnWinStationVirtualOpenEx = NULL;

BOOL WINAPI Win32_WTSVirtualChannelClose(HANDLE hChannel);


/**
  * NOTE !!
  * An application using the WinPR wtsapi frees memory via WTSFreeMemory, which
  * might be mapped to Win32_WTSFreeMemory. Latter does not know if the passed
  * pointer was allocated by a function in wtsapi32.dll or in some internal
  * code below. The WTSFreeMemory implementation in all Windows wtsapi32.dll
  * versions up to Windows 10 uses LocalFree since all its allocating functions
  * use LocalAlloc() internally.
  * For that reason we also have to use LocalAlloc() for any memory returned by
  * our WinPR wtsapi functions.
  *
  * To be safe we only use the _wts_malloc, _wts_calloc, _wts_free wrappers
  * for memory managment the code below.
  */

static void *_wts_malloc(size_t size)
{
	return (PVOID)LocalAlloc(LMEM_FIXED, size);
}

static void *_wts_calloc(size_t nmemb, size_t size)
{
	return (PVOID)LocalAlloc(LMEM_FIXED | LMEM_ZEROINIT, nmemb * size);
}

static void _wts_free(void* ptr)
{
	LocalFree((HLOCAL)ptr);
}

BOOL Win32_WTSVirtualChannelReadAsync(WTSAPI_CHANNEL* pChannel)
{
	BOOL status = TRUE;
	DWORD numBytes = 0;

	if (pChannel->readAsync)
		return TRUE;

	ZeroMemory(&(pChannel->overlapped), sizeof(OVERLAPPED));
	pChannel->overlapped.hEvent = pChannel->hEvent;
	ResetEvent(pChannel->hEvent);

	if (pChannel->showProtocol)
	{
		ZeroMemory(pChannel->header, sizeof(CHANNEL_PDU_HEADER));

		status = ReadFile(pChannel->hFile, pChannel->header,
			sizeof(CHANNEL_PDU_HEADER), &numBytes, &(pChannel->overlapped));
	}
	else
	{
		status = ReadFile(pChannel->hFile, pChannel->chunk,
			CHANNEL_CHUNK_LENGTH, &numBytes, &(pChannel->overlapped));

		if (status)
		{
			pChannel->readOffset = 0;
			pChannel->header->length = numBytes;

			pChannel->readDone = TRUE;
			SetEvent(pChannel->hEvent);

			return TRUE;
		}
	}

	if (status)
	{
		WLog_ERR(TAG, "Unexpected ReadFile status: %d numBytes: %d", status, numBytes);
		return FALSE; /* ReadFile should return FALSE and set ERROR_IO_PENDING */
	}

	if (GetLastError() != ERROR_IO_PENDING)
	{
		WLog_ERR(TAG, "ReadFile: GetLastError() = %d", GetLastError());
		return FALSE;
	}

	pChannel->readAsync = TRUE;

	return TRUE;
}

HANDLE WINAPI Win32_WTSVirtualChannelOpen_Internal(HANDLE hServer, DWORD SessionId, LPSTR pVirtualName, DWORD flags)
{
	HANDLE hFile;
	HANDLE hChannel;
	WTSAPI_CHANNEL* pChannel;
	size_t virtualNameLen;

	virtualNameLen = pVirtualName ? strlen(pVirtualName) : 0;

	if (!virtualNameLen)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return NULL;
	}

	if (!pfnWinStationVirtualOpenEx)
	{
		SetLastError(ERROR_INVALID_FUNCTION);
		return NULL;
	}

	hFile = pfnWinStationVirtualOpenEx(hServer, SessionId, pVirtualName, flags);

	if (!hFile)
		return NULL;

	pChannel = (WTSAPI_CHANNEL*) _wts_calloc(1, sizeof(WTSAPI_CHANNEL));

	if (!pChannel)
	{
		CloseHandle(hFile);
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		return NULL;
	}

	hChannel = (HANDLE) pChannel;
	pChannel->magic = WTSAPI_CHANNEL_MAGIC;
	pChannel->hServer = hServer;
	pChannel->SessionId = SessionId;
	pChannel->hFile = hFile;
	pChannel->VirtualName = _wts_calloc(1, virtualNameLen + 1);
	if (!pChannel->VirtualName)
	{
		CloseHandle(hFile);
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		_wts_free(pChannel);
		return NULL;
	}
	memcpy(pChannel->VirtualName, pVirtualName, virtualNameLen);

	pChannel->flags = flags;
	pChannel->dynamic = (flags & WTS_CHANNEL_OPTION_DYNAMIC) ? TRUE : FALSE;

	pChannel->showProtocol = pChannel->dynamic;

	pChannel->readSize = CHANNEL_PDU_LENGTH;
	pChannel->readBuffer = (BYTE*) _wts_malloc(pChannel->readSize);

	pChannel->header = (CHANNEL_PDU_HEADER*) pChannel->readBuffer;
	pChannel->chunk = &(pChannel->readBuffer[sizeof(CHANNEL_PDU_HEADER)]);

	pChannel->hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	pChannel->overlapped.hEvent = pChannel->hEvent;

	if (!pChannel->hEvent || !pChannel->VirtualName || !pChannel->readBuffer)
	{
		Win32_WTSVirtualChannelClose(hChannel);
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		return NULL;
	}

	return hChannel;
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
	BOOL status = TRUE;
	WTSAPI_CHANNEL* pChannel = (WTSAPI_CHANNEL*) hChannel;

	if (!pChannel || (pChannel->magic != WTSAPI_CHANNEL_MAGIC))
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	if (pChannel->hFile)
	{
		if (pChannel->readAsync)
		{
			CancelIo(pChannel->hFile);
			pChannel->readAsync = FALSE;
		}

		status = CloseHandle(pChannel->hFile);
		pChannel->hFile = NULL;
	}

	if (pChannel->hEvent)
	{
		CloseHandle(pChannel->hEvent);
		pChannel->hEvent = NULL;
	}

	if (pChannel->VirtualName)
	{
		_wts_free(pChannel->VirtualName);
		pChannel->VirtualName = NULL;
	}

	if (pChannel->readBuffer)
	{
		_wts_free(pChannel->readBuffer);
		pChannel->readBuffer = NULL;
	}

	pChannel->magic = 0;
	_wts_free(pChannel);

	return status;
}

BOOL WINAPI Win32_WTSVirtualChannelRead_Static(WTSAPI_CHANNEL* pChannel, DWORD dwMilliseconds, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesTransferred)
{
	if (pChannel->readDone)
	{
		DWORD numBytesRead = 0;
		DWORD numBytesToRead = 0;
	
		*lpNumberOfBytesTransferred = 0;

		numBytesToRead = nNumberOfBytesToRead;

		if (numBytesToRead > (pChannel->header->length - pChannel->readOffset))
			numBytesToRead = (pChannel->header->length - pChannel->readOffset);

		CopyMemory(lpBuffer, &(pChannel->chunk[pChannel->readOffset]), numBytesToRead);
		*lpNumberOfBytesTransferred += numBytesToRead;
		pChannel->readOffset += numBytesToRead;

		if (pChannel->readOffset != pChannel->header->length)
		{
			SetLastError(ERROR_MORE_DATA);
			return FALSE;
		}
		else
		{
			pChannel->readDone = FALSE;
			Win32_WTSVirtualChannelReadAsync(pChannel);
		}

		return TRUE;
	}
	else if (pChannel->readSync)
	{
		BOOL bSuccess;
		OVERLAPPED overlapped;
		DWORD numBytesRead = 0;
		DWORD numBytesToRead = 0;
	
		*lpNumberOfBytesTransferred = 0;

		ZeroMemory(&overlapped, sizeof(OVERLAPPED));

		numBytesToRead = nNumberOfBytesToRead;

		if (numBytesToRead > (pChannel->header->length - pChannel->readOffset))
			numBytesToRead = (pChannel->header->length - pChannel->readOffset);

		if (ReadFile(pChannel->hFile, lpBuffer, numBytesToRead, &numBytesRead, &overlapped))
		{
			*lpNumberOfBytesTransferred += numBytesRead;
			pChannel->readOffset += numBytesRead;

			if (pChannel->readOffset != pChannel->header->length)
			{
				SetLastError(ERROR_MORE_DATA);
				return FALSE;
			}

			pChannel->readSync = FALSE;
			Win32_WTSVirtualChannelReadAsync(pChannel);

			return TRUE;
		}

		if (GetLastError() != ERROR_IO_PENDING)
			return FALSE;

		bSuccess = GetOverlappedResult(pChannel->hFile, &overlapped, &numBytesRead, TRUE);

		if (!bSuccess)
			return FALSE;

		*lpNumberOfBytesTransferred += numBytesRead;
		pChannel->readOffset += numBytesRead;

		if (pChannel->readOffset != pChannel->header->length)
		{
			SetLastError(ERROR_MORE_DATA);
			return FALSE;
		}

		pChannel->readSync = FALSE;
		Win32_WTSVirtualChannelReadAsync(pChannel);

		return TRUE;
	}
	else if (pChannel->readAsync)
	{
		BOOL bSuccess;
		DWORD numBytesRead = 0;
		DWORD numBytesToRead = 0;

		*lpNumberOfBytesTransferred = 0;

		if (WaitForSingleObject(pChannel->hEvent, dwMilliseconds) != WAIT_TIMEOUT)
		{
			bSuccess = GetOverlappedResult(pChannel->hFile,
					&(pChannel->overlapped), &numBytesRead, TRUE);

			pChannel->readOffset = 0;
			pChannel->header->length = numBytesRead;

			if (!bSuccess && (GetLastError() != ERROR_MORE_DATA))
				return FALSE;

			numBytesToRead = nNumberOfBytesToRead;

			if (numBytesRead < numBytesToRead)
			{
				numBytesToRead = numBytesRead;
				nNumberOfBytesToRead = numBytesRead;
			}

			CopyMemory(lpBuffer, pChannel->chunk, numBytesToRead);
			*lpNumberOfBytesTransferred += numBytesToRead;
			((BYTE*) lpBuffer) += numBytesToRead;
			nNumberOfBytesToRead -= numBytesToRead;
			pChannel->readOffset += numBytesToRead;

			pChannel->readAsync = FALSE;

			if (!nNumberOfBytesToRead)
			{
				Win32_WTSVirtualChannelReadAsync(pChannel);
				return TRUE;
			}

			pChannel->readSync = TRUE;

			numBytesRead = 0;

			bSuccess = Win32_WTSVirtualChannelRead_Static(pChannel, dwMilliseconds,
						lpBuffer, nNumberOfBytesToRead, &numBytesRead);

			*lpNumberOfBytesTransferred += numBytesRead;
			return bSuccess;
		}
		else
		{
			SetLastError(ERROR_IO_INCOMPLETE);
			return FALSE;
		}
	}

	return FALSE;
}

BOOL WINAPI Win32_WTSVirtualChannelRead_Dynamic(WTSAPI_CHANNEL* pChannel, DWORD dwMilliseconds, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesTransferred)
{
	if (pChannel->readSync)
	{
		BOOL bSuccess;
		OVERLAPPED overlapped;
		DWORD numBytesRead = 0;
		DWORD numBytesToRead = 0;
	
		*lpNumberOfBytesTransferred = 0;

		ZeroMemory(&overlapped, sizeof(OVERLAPPED));

		numBytesToRead = nNumberOfBytesToRead;

		if (numBytesToRead > (pChannel->header->length - pChannel->readOffset))
			numBytesToRead = (pChannel->header->length - pChannel->readOffset);

		if (ReadFile(pChannel->hFile, lpBuffer, numBytesToRead, &numBytesRead, &overlapped))
		{
			*lpNumberOfBytesTransferred += numBytesRead;
			pChannel->readOffset += numBytesRead;

			if (pChannel->readOffset != pChannel->header->length)
			{
				SetLastError(ERROR_MORE_DATA);
				return FALSE;
			}

			pChannel->readSync = FALSE;
			Win32_WTSVirtualChannelReadAsync(pChannel);

			return TRUE;
		}

		if (GetLastError() != ERROR_IO_PENDING)
			return FALSE;

		bSuccess = GetOverlappedResult(pChannel->hFile, &overlapped, &numBytesRead, TRUE);

		if (!bSuccess)
			return FALSE;

		*lpNumberOfBytesTransferred += numBytesRead;
		pChannel->readOffset += numBytesRead;

		if (pChannel->readOffset != pChannel->header->length)
		{
			SetLastError(ERROR_MORE_DATA);
			return FALSE;
		}

		pChannel->readSync = FALSE;
		Win32_WTSVirtualChannelReadAsync(pChannel);

		return TRUE;
	}
	else if (pChannel->readAsync)
	{
		BOOL bSuccess;
		DWORD numBytesRead = 0;

		*lpNumberOfBytesTransferred = 0;

		if (WaitForSingleObject(pChannel->hEvent, dwMilliseconds) != WAIT_TIMEOUT)
		{
			bSuccess = GetOverlappedResult(pChannel->hFile,
					&(pChannel->overlapped), &numBytesRead, TRUE);

			if (pChannel->showProtocol)
			{
				if (numBytesRead != sizeof(CHANNEL_PDU_HEADER))
					return FALSE;

				if (!bSuccess && (GetLastError() != ERROR_MORE_DATA))
					return FALSE;

				CopyMemory(lpBuffer, pChannel->header, numBytesRead);
				*lpNumberOfBytesTransferred += numBytesRead;
				((BYTE*) lpBuffer) += numBytesRead;
				nNumberOfBytesToRead -= numBytesRead;
			}

			pChannel->readAsync = FALSE;

			if (!pChannel->header->length)
			{
				Win32_WTSVirtualChannelReadAsync(pChannel);
				return TRUE;
			}

			pChannel->readSync = TRUE;
			pChannel->readOffset = 0;

			if (!nNumberOfBytesToRead)
			{
				SetLastError(ERROR_MORE_DATA);
				return FALSE;
			}

			numBytesRead = 0;

			bSuccess = Win32_WTSVirtualChannelRead_Dynamic(pChannel, dwMilliseconds,
						lpBuffer, nNumberOfBytesToRead, &numBytesRead);

			*lpNumberOfBytesTransferred += numBytesRead;
			return bSuccess;
		}
		else
		{
			SetLastError(ERROR_IO_INCOMPLETE);
			return FALSE;
		}
	}

	return FALSE;
}

BOOL WINAPI Win32_WTSVirtualChannelRead(HANDLE hChannel, DWORD dwMilliseconds, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesTransferred)
{
	WTSAPI_CHANNEL* pChannel = (WTSAPI_CHANNEL*) hChannel;

	if (!pChannel || (pChannel->magic != WTSAPI_CHANNEL_MAGIC))
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	if (!pChannel->waitObjectMode)
	{
		OVERLAPPED overlapped;
	
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
	else
	{
		if (pChannel->dynamic)
		{
			return Win32_WTSVirtualChannelRead_Dynamic(pChannel, dwMilliseconds,
				lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesTransferred);
		}
		else
		{
			return Win32_WTSVirtualChannelRead_Static(pChannel, dwMilliseconds,
				lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesTransferred);
		}
	}

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
		*ppBuffer = _wts_calloc(1, *pBytesReturned);

		if (*ppBuffer == NULL)
		{
			SetLastError(ERROR_NOT_ENOUGH_MEMORY);
			return FALSE;
		}

		CopyMemory(*ppBuffer, &(pChannel->hFile), *pBytesReturned);
	}
	else if (WtsVirtualClass == WTSVirtualEventHandle)
	{
		*pBytesReturned = sizeof(HANDLE);
		*ppBuffer = _wts_calloc(1, *pBytesReturned);

		if (*ppBuffer == NULL)
		{
			SetLastError(ERROR_NOT_ENOUGH_MEMORY);
			return FALSE;
		}

		CopyMemory(*ppBuffer, &(pChannel->hEvent), *pBytesReturned);

		Win32_WTSVirtualChannelReadAsync(pChannel);
		pChannel->waitObjectMode = TRUE;
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
	_wts_free(pMemory);
}

BOOL WINAPI Win32_WTSFreeMemoryExW(WTS_TYPE_CLASS WTSTypeClass, PVOID pMemory, ULONG NumberOfEntries)
{
	return FALSE;
}

BOOL WINAPI Win32_WTSFreeMemoryExA(WTS_TYPE_CLASS WTSTypeClass, PVOID pMemory, ULONG NumberOfEntries)
{
	return WTSFreeMemoryExW(WTSTypeClass, pMemory, NumberOfEntries);
}

BOOL Win32_InitializeWinSta(PWtsApiFunctionTable pWtsApi)
{
	g_WinStaModule = LoadLibraryA("winsta.dll");

	if (!g_WinStaModule)
		return FALSE;

	pfnWinStationVirtualOpen = (fnWinStationVirtualOpen) GetProcAddress(g_WinStaModule, "WinStationVirtualOpen");
	pfnWinStationVirtualOpenEx = (fnWinStationVirtualOpenEx) GetProcAddress(g_WinStaModule, "WinStationVirtualOpenEx");

	if (!pfnWinStationVirtualOpen | !pfnWinStationVirtualOpenEx)
		return FALSE;

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

	return TRUE;
}
