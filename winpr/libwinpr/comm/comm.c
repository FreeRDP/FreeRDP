/**
 * WinPR: Windows Portable Runtime
 * Serial Communication API
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2014 Hewlett-Packard Development Company, L.P.
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
#include <winpr/comm.h>
#include <winpr/collections.h>
#include <winpr/tchar.h>

/**
 * Communication Resources:
 * http://msdn.microsoft.com/en-us/library/windows/desktop/aa363196/
 */

#ifndef _WIN32

#include "comm.h"

BOOL BuildCommDCBA(LPCSTR lpDef, LPDCB lpDCB)
{
	return TRUE;
}

BOOL BuildCommDCBW(LPCWSTR lpDef, LPDCB lpDCB)
{
	return TRUE;
}

BOOL BuildCommDCBAndTimeoutsA(LPCSTR lpDef, LPDCB lpDCB, LPCOMMTIMEOUTS lpCommTimeouts)
{
	return TRUE;
}

BOOL BuildCommDCBAndTimeoutsW(LPCWSTR lpDef, LPDCB lpDCB, LPCOMMTIMEOUTS lpCommTimeouts)
{
	return TRUE;
}

BOOL CommConfigDialogA(LPCSTR lpszName, HWND hWnd, LPCOMMCONFIG lpCC)
{
	return TRUE;
}

BOOL CommConfigDialogW(LPCWSTR lpszName, HWND hWnd, LPCOMMCONFIG lpCC)
{
	return TRUE;
}

BOOL GetCommConfig(HANDLE hCommDev, LPCOMMCONFIG lpCC, LPDWORD lpdwSize)
{
	WINPR_COMM* pComm = (WINPR_COMM*) hCommDev;

	if (!pComm)
		return FALSE;

	return TRUE;
}

BOOL SetCommConfig(HANDLE hCommDev, LPCOMMCONFIG lpCC, DWORD dwSize)
{
	WINPR_COMM* pComm = (WINPR_COMM*) hCommDev;

	if (!pComm)
		return FALSE;

	return TRUE;
}

BOOL GetCommMask(HANDLE hFile, PDWORD lpEvtMask)
{
	WINPR_COMM* pComm = (WINPR_COMM*) hFile;

	if (!pComm)
		return FALSE;

	return TRUE;
}

BOOL SetCommMask(HANDLE hFile, DWORD dwEvtMask)
{
	WINPR_COMM* pComm = (WINPR_COMM*) hFile;

	if (!pComm)
		return FALSE;

	return TRUE;
}

BOOL GetCommModemStatus(HANDLE hFile, PDWORD lpModemStat)
{
	WINPR_COMM* pComm = (WINPR_COMM*) hFile;

	if (!pComm)
		return FALSE;

	return TRUE;
}

BOOL GetCommProperties(HANDLE hFile, LPCOMMPROP lpCommProp)
{
	WINPR_COMM* pComm = (WINPR_COMM*) hFile;

	if (!pComm)
		return FALSE;

	return TRUE;
}

BOOL GetCommState(HANDLE hFile, LPDCB lpDCB)
{
	WINPR_COMM* pComm = (WINPR_COMM*) hFile;

	if (!pComm)
		return FALSE;

	return TRUE;
}

BOOL SetCommState(HANDLE hFile, LPDCB lpDCB)
{
	WINPR_COMM* pComm = (WINPR_COMM*) hFile;

	if (!pComm)
		return FALSE;

	return TRUE;
}

BOOL GetCommTimeouts(HANDLE hFile, LPCOMMTIMEOUTS lpCommTimeouts)
{
	WINPR_COMM* pComm = (WINPR_COMM*) hFile;

	if (!pComm)
		return FALSE;

	return TRUE;
}

BOOL SetCommTimeouts(HANDLE hFile, LPCOMMTIMEOUTS lpCommTimeouts)
{
	WINPR_COMM* pComm = (WINPR_COMM*) hFile;

	if (!pComm)
		return FALSE;

	return TRUE;
}

BOOL GetDefaultCommConfigA(LPCSTR lpszName, LPCOMMCONFIG lpCC, LPDWORD lpdwSize)
{
	return TRUE;
}

BOOL GetDefaultCommConfigW(LPCWSTR lpszName, LPCOMMCONFIG lpCC, LPDWORD lpdwSize)
{
	return TRUE;
}

BOOL SetDefaultCommConfigA(LPCSTR lpszName, LPCOMMCONFIG lpCC, DWORD dwSize)
{
	return TRUE;
}

BOOL SetDefaultCommConfigW(LPCWSTR lpszName, LPCOMMCONFIG lpCC, DWORD dwSize)
{
	return TRUE;
}

BOOL SetCommBreak(HANDLE hFile)
{
	WINPR_COMM* pComm = (WINPR_COMM*) hFile;

	if (!pComm)
		return FALSE;

	return TRUE;
}

BOOL ClearCommBreak(HANDLE hFile)
{
	WINPR_COMM* pComm = (WINPR_COMM*) hFile;

	if (!pComm)
		return FALSE;

	return TRUE;
}

BOOL ClearCommError(HANDLE hFile, PDWORD lpErrors, LPCOMSTAT lpStat)
{
	WINPR_COMM* pComm = (WINPR_COMM*) hFile;

	if (!pComm)
		return FALSE;

	return TRUE;
}

BOOL PurgeComm(HANDLE hFile, DWORD dwFlags)
{
	WINPR_COMM* pComm = (WINPR_COMM*) hFile;

	if (!pComm)
		return FALSE;

	return TRUE;
}

BOOL SetupComm(HANDLE hFile, DWORD dwInQueue, DWORD dwOutQueue)
{
	WINPR_COMM* pComm = (WINPR_COMM*) hFile;

	if (!pComm)
		return FALSE;

	return TRUE;
}

BOOL EscapeCommFunction(HANDLE hFile, DWORD dwFunc)
{
	WINPR_COMM* pComm = (WINPR_COMM*) hFile;

	if (!pComm)
		return FALSE;

	return TRUE;
}

BOOL TransmitCommChar(HANDLE hFile, char cChar)
{
	WINPR_COMM* pComm = (WINPR_COMM*) hFile;

	if (!pComm)
		return FALSE;

	return TRUE;
}

BOOL WaitCommEvent(HANDLE hFile, PDWORD lpEvtMask, LPOVERLAPPED lpOverlapped)
{
	WINPR_COMM* pComm = (WINPR_COMM*) hFile;

	if (!pComm)
		return FALSE;

	return TRUE;
}


/* Extended API */

/* FIXME: DefineCommDevice / QueryCommDevice look over complicated for
 * just a couple of strings, should be simplified.
 * 
 * TODO: what about libwinpr-io.so?
 */
static wHashTable *_CommDevices = NULL;

static int deviceNameCmp(void* pointer1, void* pointer2)
{
	return _tcscmp(pointer1, pointer2);
}


static int devicePathCmp(void* pointer1, void* pointer2)
{
	return _tcscmp(pointer1, pointer2);
}

/* copied from HashTable.c */
static unsigned long HashTable_StringHashFunctionA(void* key)
{
	int c;
	unsigned long hash = 5381;
	unsigned char* str = (unsigned char*) key;

	/* djb2 algorithm */
	while ((c = *str++) != '\0')
		hash = (hash * 33) + c;

	return hash;
}


static void _CommDevicesInit()
{
	/* 
	 * TMP: FIXME: What kind of mutex should be used here?
	 * better have to let DefineCommDevice() and QueryCommDevice() thread unsafe ?
	 * use of a module_init() ?
	 */ 

	if (_CommDevices == NULL)
	{
		_CommDevices = HashTable_New(TRUE);
		_CommDevices->keycmp = deviceNameCmp;
		_CommDevices->valuecmp = devicePathCmp;
		_CommDevices->hashFunction = HashTable_StringHashFunctionA; /* TMP: FIXME: need of a HashTable_StringHashFunctionW */
		_CommDevices->keyDeallocator = free;
		_CommDevices->valueDeallocator = free;
	}
}


static BOOL _IsReservedCommDeviceName(LPCTSTR lpName)
{
	int i;

	/* Serial ports, COM1-9 */
	for (i=1; i<10; i++)
	{
		TCHAR genericName[5];
		if (_stprintf_s(genericName, 5, "COM%d", i) < 0)
		{
			return FALSE;
		}

		if (_tcscmp(genericName, lpName) == 0)
			return TRUE;
	}

	/* Parallel ports, LPT1-9 */
	for (i=1; i<10; i++)
	{
		TCHAR genericName[5];
		if (_stprintf_s(genericName, 5, "LPT%d", i) < 0)
		{
			return FALSE;
		}

		if (_tcscmp(genericName, lpName) == 0)
			return TRUE;
	}

	/* TMP: TODO: PRN ? */

	return FALSE;
}


/**
 * Returns TRUE on success, FALSE otherwise. To get extended error
 * information, call GetLastError.
 * 
 * ERRORS:
 *   ERROR_OUTOFMEMORY was not possible to get mappings.
 *   ERROR_INVALID_DATA was not possible to add the device.
 */
BOOL DefineCommDevice(/* DWORD dwFlags,*/ LPCTSTR lpDeviceName, LPCTSTR lpTargetPath)
{
	LPTSTR storedDeviceName = NULL;
	LPTSTR storedTargetPath = NULL;

	_CommDevicesInit();
	if (_CommDevices == NULL)
	{
		SetLastError(ERROR_OUTOFMEMORY);
		goto error_handle;
	}

	if (_tcsncmp(lpDeviceName, _T("\\\\.\\"), 4) != 0)
	{
		if (!_IsReservedCommDeviceName(lpDeviceName))
		{
			SetLastError(ERROR_INVALID_DATA);
			goto error_handle;
		}
	}

	storedDeviceName = _tcsdup(lpDeviceName);
	if (storedDeviceName == NULL)
	{
		SetLastError(ERROR_OUTOFMEMORY);
		goto error_handle;
	}

	storedTargetPath = _tcsdup(lpTargetPath);
	if (storedTargetPath == NULL)
	{
		SetLastError(ERROR_OUTOFMEMORY);
		goto error_handle;
	}

	if (HashTable_Add(_CommDevices, storedDeviceName, storedTargetPath) < 0)
	{
		SetLastError(ERROR_INVALID_DATA);
		goto error_handle;
	}

	return TRUE;


  error_handle:
	if (storedDeviceName != NULL)
		free(storedDeviceName);

	if (storedTargetPath != NULL)
		free(storedTargetPath);

	return FALSE;
}


/**
 * Returns the number of target paths in the buffer pointed to by
 * lpTargetPath. 
 *
 * The current implementation returns in any case 0 and 1 target
 * path. A NULL lpDeviceName is not supported yet to get all the
 * paths.
 *  
 * ERRORS:
 *   ERROR_SUCCESS
 *   ERROR_OUTOFMEMORY was not possible to get mappings.
 *   ERROR_NOT_SUPPORTED equivalent QueryDosDevice feature not supported.
 *   ERROR_INVALID_DATA was not possible to retrieve any device information.
 *   ERROR_INSUFFICIENT_BUFFER too small lpTargetPath
 */
DWORD QueryCommDevice(LPCTSTR lpDeviceName, LPTSTR lpTargetPath, DWORD ucchMax)
{
	LPTSTR storedTargetPath;

	SetLastError(ERROR_SUCCESS);

	_CommDevicesInit();
	if (_CommDevices == NULL)
	{
		SetLastError(ERROR_OUTOFMEMORY);
		return 0;
	}

	if (lpDeviceName == NULL || lpTargetPath == NULL)
	{
		SetLastError(ERROR_NOT_SUPPORTED);
		return 0;
	}

	storedTargetPath = HashTable_GetItemValue(_CommDevices, (void*)lpDeviceName);
	if (storedTargetPath == NULL)
	{
		SetLastError(ERROR_INVALID_DATA);
		return 0;
	}

	if (_tcslen(storedTargetPath) + 2 > ucchMax)
	{
		SetLastError(ERROR_INSUFFICIENT_BUFFER);
		return 0;
	}

	_tcscpy(lpTargetPath, storedTargetPath);
	_tcscat(lpTargetPath, ""); /* 2nd final '\0' */

	return _tcslen(lpTargetPath) + 2;
}

/**
 * Checks whether lpDeviceName is a valid and registered Communication device. 
 */
BOOL IsCommDevice(LPCTSTR lpDeviceName)
{
	TCHAR lpTargetPath[MAX_PATH];

	if (QueryCommDevice(lpDeviceName, lpTargetPath, MAX_PATH) > 0)
	{
		return TRUE;
	}

	return FALSE;
}


HANDLE _CommCreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes,
			DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
	HANDLE hComm;
	WINPR_COMM* pComm;

	//SetLastError(ERROR_BAD_PATHNAME);

	pComm = (WINPR_COMM*) calloc(1, sizeof(WINPR_COMM));
	hComm = (HANDLE) pComm;

	WINPR_HANDLE_SET_TYPE(pComm, HANDLE_TYPE_COMM);

	//return INVALID_HANDLE_VALUE;
	return hComm;
}

#endif /* _WIN32 */
