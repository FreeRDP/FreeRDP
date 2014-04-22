/**
 * WinPR: Windows Portable Runtime
 * Serial Communication API
 *
 * Copyright 2011 O.S. Systems Software Ltda.
 * Copyright 2011 Eduardo Fiss Beloni <beloni@ossystems.com.br>
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

#ifndef _WIN32

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>

#include <freerdp/utils/debug.h>

#include <winpr/crt.h>
#include <winpr/comm.h>
#include <winpr/collections.h>
#include <winpr/tchar.h>

/**
 * Communication Resources:
 * http://msdn.microsoft.com/en-us/library/windows/desktop/aa363196/
 */

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


/*
 * Linux, Windows speeds
 * 
 */
static const speed_t _SPEED_TABLE[][2] = {
#ifdef B0
	{B0, 0},	/* hang up */
#endif
#ifdef B50
	{B50, 50},
#endif
#ifdef B75
	{B75, 75},
#endif
#ifdef B110
	{B110, CBR_110},
#endif
#ifdef  B134
	{B134, 134},
#endif
#ifdef  B150
	{B150, 150},
#endif
#ifdef B200
	{B200, 200},
#endif
#ifdef B300
	{B300, CBR_300},
#endif
#ifdef B600
	{B600, CBR_600},
#endif
#ifdef B1200
	{B1200, CBR_1200},
#endif
#ifdef B1800
	{B1800, 1800},
#endif
#ifdef B2400
	{B2400, CBR_2400},
#endif
#ifdef B4800
	{B4800, CBR_4800},
#endif
#ifdef B9600
	{B9600, CBR_9600},
#endif
	/* {, CBR_14400},	/\* unsupported on Linux *\/ */
#ifdef B19200
	{B19200, CBR_19200},
#endif
#ifdef B38400
	{B38400, CBR_38400},
#endif
	/* {, CBR_56000},	/\* unsupported on Linux *\/ */
#ifdef  B57600
	{B57600, CBR_57600},
#endif
#ifdef B115200
	{B115200, CBR_115200},
#endif
	/* {, CBR_128000},	/\* unsupported on Linux *\/ */
	/* {, CBR_256000},	/\* unsupported on Linux *\/ */
#ifdef B230400
	{B230400, 230400},
#endif
#ifdef B460800
	{B460800, 460800},
#endif
#ifdef B500000
	{B500000, 500000},
#endif
#ifdef  B576000
	{B576000, 576000},
#endif
#ifdef B921600
	{B921600, 921600},
#endif
#ifdef B1000000
	{B1000000, 1000000},
#endif
#ifdef B1152000
	{B1152000, 1152000},
#endif
#ifdef B1500000
	{B1500000, 1500000},
#endif
#ifdef B2000000
	{B2000000, 2000000},
#endif
#ifdef B2500000
	{B2500000, 2500000},
#endif
#ifdef B3000000
	{B3000000, 3000000},
#endif
#ifdef B3500000
	{B3500000, 3500000},
#endif
#ifdef B4000000
	{B4000000, 4000000},	/* __MAX_BAUD */
#endif
};


/* Set lpDcb->BaudRate with the current baud rate.
 */
static BOOL _GetBaudRate(LPDCB lpDcb, struct termios *lpCurrentState)
{
	int i;
	speed_t currentSpeed;

	currentSpeed = cfgetispeed(lpCurrentState);

	for (i=0; _SPEED_TABLE[i][0]<=__MAX_BAUD; i++)
	{
		if (_SPEED_TABLE[i][0] == currentSpeed)
		{
			lpDcb->BaudRate = _SPEED_TABLE[i][1];
			return TRUE;
		}
	}

	DEBUG_WARN("could not find a matching baud rate for the speed 0x%x", currentSpeed);
	return FALSE;
}


/* Set lpFutureState's speed to lpDcb->BaudRate.
 */
static BOOL _SetBaudRate(struct termios *lpFutureState, LPDCB lpDcb)
{
	int i;
	speed_t newSpeed;

	for (i=0; _SPEED_TABLE[i][0]<=__MAX_BAUD; i++)
	{
		if (_SPEED_TABLE[i][1] == lpDcb->BaudRate)
		{
			newSpeed = _SPEED_TABLE[i][0];
			if (cfsetspeed(lpFutureState, newSpeed) < 0)
			{
				DEBUG_WARN("failed to set speed 0x%x (%d)", newSpeed, lpDcb->BaudRate);
				return FALSE;
			}

			return TRUE;
		}
	}

	DEBUG_WARN("could not find a matching speed for the baud rate %d", lpDcb->BaudRate);
	return FALSE;
}

/**
 * 
 *
 * ERRORS:
 *   ERROR_INVALID_HANDLE
 *   ERROR_INVALID_DATA
 *   ERROR_IO_DEVICE
 *   ERROR_OUTOFMEMORY
 */
BOOL GetCommState(HANDLE hFile, LPDCB lpDCB)
{
	DCB *lpLocalDcb;
	struct termios currentState;
	WINPR_COMM* pComm = (WINPR_COMM*) hFile;

	if (!pComm || pComm->Type != HANDLE_TYPE_COMM || !pComm->fd )
	{
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}

	if (!lpDCB)
	{
		SetLastError(ERROR_INVALID_DATA);
		return FALSE;
	}

	if (lpDCB->DCBlength < sizeof(DCB))
	{
		SetLastError(ERROR_INVALID_DATA);
		return FALSE;
	}

	if (tcgetattr(pComm->fd, &currentState) < 0)
	{
		SetLastError(ERROR_IO_DEVICE);
		return FALSE;
	}

	lpLocalDcb = (DCB*)calloc(1, lpDCB->DCBlength);
	if (lpLocalDcb == NULL)
	{
		SetLastError(ERROR_OUTOFMEMORY);
		return FALSE;
	}

	/* error_handle */
	
	lpLocalDcb->DCBlength = lpDCB->DCBlength;

	if (!_GetBaudRate(lpLocalDcb, &currentState))
	{
		SetLastError(ERROR_NOT_SUPPORTED);
		goto error_handle;
	}

	lpLocalDcb->fBinary = TRUE; /* TMP: should the raw mode be tested? */

	lpLocalDcb->fParity =  (currentState.c_iflag & INPCK) != 0;


	memcpy(lpDCB, lpLocalDcb, lpDCB->DCBlength);
	return TRUE;


  error_handle:
	if (lpLocalDcb)
		free(lpLocalDcb);

	return FALSE;
}


/**
 * ERRORS:
 *   ERROR_INVALID_HANDLE
 *   ERROR_IO_DEVICE
 */
BOOL SetCommState(HANDLE hFile, LPDCB lpDCB)
{
	struct termios futureState;
	struct termios currentState;
	WINPR_COMM* pComm = (WINPR_COMM*) hFile;

	if (!pComm || pComm->Type != HANDLE_TYPE_COMM || !pComm->fd )
	{
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}

	if (!lpDCB)
	{
		SetLastError(ERROR_INVALID_DATA);
		return FALSE;
	}

	ZeroMemory(&futureState, sizeof(struct termios));
	if (tcgetattr(pComm->fd, &futureState) < 0) /* NB: preserves current settings not directly handled by the Communication Functions */
	{
		SetLastError(ERROR_IO_DEVICE);
		return FALSE;
	}

	if (!_SetBaudRate(&futureState, lpDCB))
	{
		SetLastError(ERROR_NOT_SUPPORTED);
		return FALSE;
	}

	if (!lpDCB->fBinary)
	{
		DEBUG_WARN("unexpected nonbinary mode transfers");
		SetLastError(ERROR_NOT_SUPPORTED);
		return FALSE;
	}
	// TMP: set the raw mode here?
	
	if (lpDCB->fParity)
	{
		futureState.c_iflag |= INPCK;
	}
	else
	{
		futureState.c_iflag &= ~INPCK;
	}

	// TMP: FIXME: validate changes according GetCommProperties


	/* http://msdn.microsoft.com/en-us/library/windows/desktop/aa363423%28v=vs.85%29.aspx
	 *
	 * The SetCommState function reconfigures the communications
	 * resource, but it does not affect the internal output and
	 * input buffers of the specified driver. The buffers are not
	 * flushed, and pending read and write operations are not
	 * terminated prematurely.
	 *
	 * TCSANOW matches the best this definition
	 */
	
	if (tcsetattr(pComm->fd, TCSANOW, &futureState) < 0)
	{
		DEBUG_WARN("could not apply parameters, errno: %d", errno);
		return FALSE;
	}
	
	/* NB: tcsetattr() can succeed even if not all changes have been applied. */
	ZeroMemory(&currentState, sizeof(struct termios));
	if (tcgetattr(pComm->fd, &currentState) < 0)
	{
		SetLastError(ERROR_IO_DEVICE);
		return FALSE;
	}

	if (memcmp(&currentState, &futureState, sizeof(struct termios)) != 0)
	{
		DEBUG_WARN("all parameters were not set, doing a second attempt...");
		if (tcsetattr(pComm->fd, TCSAFLUSH, &futureState) < 0)
		{
			DEBUG_WARN("could not apply parameters, errno: %d", errno);
			return FALSE;
		}

		ZeroMemory(&currentState, sizeof(struct termios));
		if (tcgetattr(pComm->fd, &currentState) < 0)
		{
			SetLastError(ERROR_IO_DEVICE);
			return FALSE;
		}

		if (memcmp(&currentState, &futureState, sizeof(struct termios)) != 0)
		{
			DEBUG_WARN("Failure: all parameters were not set on a second attempt.");
			SetLastError(ERROR_IO_DEVICE);
			return FALSE; /* TMP: double-check whether some parameters can differ anyway */
		}
	}

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

/* http://msdn.microsoft.com/en-us/library/windows/desktop/aa363423%28v=vs.85%29.aspx */
/* A process reinitializes a communications resource by using the SetupComm function, which performs the following tasks: */

/*     Terminates pending read and write operations, even if they have not been completed. */
/*     Discards unread characters and frees the internal output and input buffers of the driver associated with the specified resource. */
/*     Reallocates the internal output and input buffers. */

/* A process is not required to call SetupComm. If it does not, the resource's driver initializes the device with the default settings the first time that the communications resource handle is used. */

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

static HANDLE_CREATOR *_CommHandleCreator = NULL;

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

		_CommHandleCreator = (HANDLE_CREATOR*)malloc(sizeof(HANDLE_CREATOR));
		_CommHandleCreator->IsHandled = IsCommDevice;
		_CommHandleCreator->CreateFileA = CommCreateFileA;

		RegisterHandleCreator(_CommHandleCreator);
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


/**
 * http://msdn.microsoft.com/en-us/library/windows/desktop/aa363198%28v=vs.85%29.aspx
 *
 * @param lpDeviceName e.g. COM1, \\.\COM1, ...
 *
 * @param dwDesiredAccess expects GENERIC_READ | GENERIC_WRITE, a
 * warning message is printed otherwise. TODO: better support.
 *
 * @param dwShareMode must be zero, INVALID_HANDLE_VALUE is returned
 * otherwise and GetLastError() should return ERROR_SHARING_VIOLATION.
 * 
 * @param lpSecurityAttributes NULL expected, a warning message is printed
 * otherwise. TODO: better support.
 * 
 * @param dwCreationDisposition must be OPEN_EXISTING. If the
 * communication device doesn't exist INVALID_HANDLE_VALUE is returned
 * and GetLastError() returns ERROR_FILE_NOT_FOUND.
 *
 * @param dwFlagsAndAttributes zero expected, a warning message is
 * printed otherwise.
 *
 * @param hTemplateFile must be NULL.
 *
 * @return INVALID_HANDLE_VALUE on error. 
 */
HANDLE CommCreateFileA(LPCSTR lpDeviceName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes,
		       DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
	CHAR devicePath[MAX_PATH];
	struct stat deviceStat;
	WINPR_COMM* pComm = NULL;

	
	if (dwDesiredAccess != (GENERIC_READ | GENERIC_WRITE))
	{
		DEBUG_WARN("unexpected access to the device: 0x%x", dwDesiredAccess);
	}

	if (dwShareMode != 0)
	{
		SetLastError(ERROR_SHARING_VIOLATION);
		return INVALID_HANDLE_VALUE;
	}

	/* TODO: Prevents other processes from opening a file or
         * device if they request delete, read, or write access. */

	if (lpSecurityAttributes != NULL)
	{
		DEBUG_WARN("unexpected security attributes: 0x%x", lpSecurityAttributes);
	}

	if (dwCreationDisposition != OPEN_EXISTING)
	{
		SetLastError(ERROR_FILE_NOT_FOUND); /* FIXME: ERROR_NOT_SUPPORTED better? */
		return INVALID_HANDLE_VALUE;
	}
	
	if (QueryCommDevice(lpDeviceName, devicePath, MAX_PATH) <= 0)
	{
		/* SetLastError(GetLastError()); */
		return INVALID_HANDLE_VALUE;
	}

	if (stat(devicePath, &deviceStat) < 0)
	{
		DEBUG_WARN("device not found %s", devicePath);
		SetLastError(ERROR_FILE_NOT_FOUND);
		return INVALID_HANDLE_VALUE;
	}

	if (!S_ISCHR(deviceStat.st_mode))
	{
		DEBUG_WARN("bad device %d", devicePath);
		SetLastError(ERROR_BAD_DEVICE);
		return INVALID_HANDLE_VALUE;
	}

	if (dwFlagsAndAttributes != 0)
	{
		DEBUG_WARN("unexpected flags and attributes: 0x%x", dwFlagsAndAttributes);
	}

	if (hTemplateFile != NULL)
	{
		SetLastError(ERROR_NOT_SUPPORTED); /* FIXME: other proper error? */
		return INVALID_HANDLE_VALUE;
	}

	pComm = (WINPR_COMM*) calloc(1, sizeof(WINPR_COMM));
	if (pComm == NULL)
	{
		SetLastError(ERROR_OUTOFMEMORY);
		return INVALID_HANDLE_VALUE;
	}

	WINPR_HANDLE_SET_TYPE(pComm, HANDLE_TYPE_COMM);

	/* error_handle */

	pComm->fd = open(devicePath, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (pComm->fd < 0)
	{
		DEBUG_WARN("failed to open device %s", devicePath);
		SetLastError(ERROR_BAD_DEVICE);
		goto error_handle;
	}


	return (HANDLE)pComm;


  error_handle:
	if (pComm != NULL)
	{
		CloseHandle(pComm);
	}


	return INVALID_HANDLE_VALUE;
}

#endif /* _WIN32 */
