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

#if defined __linux__ && !defined ANDROID

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include <winpr/crt.h>
#include <winpr/comm.h>
#include <winpr/tchar.h>
#include <winpr/wlog.h>
#include <winpr/handle.h>

#include "comm_ioctl.h"


/**
 * Communication Resources:
 * http://msdn.microsoft.com/en-us/library/windows/desktop/aa363196/
 */

#include "comm.h"


static wLog *_Log = NULL;

struct comm_device
{
	LPTSTR name;
	LPTSTR path;
};

typedef struct comm_device COMM_DEVICE;


/* FIXME: get a clever data structure, see also io.h functions */
/* _CommDevices is a NULL-terminated array with a maximun of COMM_DEVICE_MAX COMM_DEVICE */
#define COMM_DEVICE_MAX	128
static COMM_DEVICE **_CommDevices = NULL;
static CRITICAL_SECTION _CommDevicesLock;

static HANDLE_CREATOR _CommHandleCreator;

static pthread_once_t _CommInitialized = PTHREAD_ONCE_INIT;

static int CommGetFd(HANDLE handle)
{
	WINPR_COMM *comm = (WINPR_COMM *)handle;

	if (!CommIsHandled(handle))
		return -1;

	return comm->fd;
}

HANDLE_CREATOR *GetCommHandleCreator(void)
{
	_CommHandleCreator.IsHandled = IsCommDevice;
	_CommHandleCreator.CreateFileA = CommCreateFileA;
	return &_CommHandleCreator;
}

static void _CommInit(void)
{
	/* NB: error management to be done outside of this function */

	assert(_Log == NULL);
	assert(_CommDevices == NULL);

	_CommDevices = (COMM_DEVICE**)calloc(COMM_DEVICE_MAX+1, sizeof(COMM_DEVICE*));
	if (!_CommDevices)
		return;

	if (!InitializeCriticalSectionEx(&_CommDevicesLock, 0, 0))
	{
		free(_CommDevices);
		_CommDevices = NULL;
		return;
	}

	_Log = WLog_Get("com.winpr.comm");
	assert(_Log != NULL);
}

/**
 * Returns TRUE when the comm module is correctly intialized, FALSE otherwise
 * with ERROR_DLL_INIT_FAILED set as the last error.
 */
static BOOL CommInitialized()
{
	if (pthread_once(&_CommInitialized, _CommInit) != 0)
	{
		SetLastError(ERROR_DLL_INIT_FAILED);
		return FALSE;
	}

	return TRUE;
}


void CommLog_Print(int level, char *fmt, ...)
{
	if (!CommInitialized())
		return;

	va_list ap;
	va_start(ap, fmt);
	WLog_PrintVA(_Log, level, fmt, ap);
	va_end(ap);
}


BOOL BuildCommDCBA(LPCSTR lpDef, LPDCB lpDCB)
{
	if (!CommInitialized())
		return FALSE;

	/* TODO: not implemented */

	CommLog_Print(WLOG_ERROR, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL BuildCommDCBW(LPCWSTR lpDef, LPDCB lpDCB)
{
	if (!CommInitialized())
		return FALSE;

	/* TODO: not implemented */

	CommLog_Print(WLOG_ERROR, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL BuildCommDCBAndTimeoutsA(LPCSTR lpDef, LPDCB lpDCB, LPCOMMTIMEOUTS lpCommTimeouts)
{
	if (!CommInitialized())
		return FALSE;

	/* TODO: not implemented */

	CommLog_Print(WLOG_ERROR, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL BuildCommDCBAndTimeoutsW(LPCWSTR lpDef, LPDCB lpDCB, LPCOMMTIMEOUTS lpCommTimeouts)
{
	if (!CommInitialized())
		return FALSE;

	/* TODO: not implemented */

	CommLog_Print(WLOG_ERROR, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL CommConfigDialogA(LPCSTR lpszName, HWND hWnd, LPCOMMCONFIG lpCC)
{
	if (!CommInitialized())
		return FALSE;

	/* TODO: not implemented */

	CommLog_Print(WLOG_ERROR, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL CommConfigDialogW(LPCWSTR lpszName, HWND hWnd, LPCOMMCONFIG lpCC)
{
	if (!CommInitialized())
		return FALSE;

	/* TODO: not implemented */

	CommLog_Print(WLOG_ERROR, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL GetCommConfig(HANDLE hCommDev, LPCOMMCONFIG lpCC, LPDWORD lpdwSize)
{
	WINPR_COMM* pComm = (WINPR_COMM*) hCommDev;

	if (!CommInitialized())
		return FALSE;

	/* TODO: not implemented */

	if (!pComm)
		return FALSE;

	CommLog_Print(WLOG_ERROR, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL SetCommConfig(HANDLE hCommDev, LPCOMMCONFIG lpCC, DWORD dwSize)
{
	WINPR_COMM* pComm = (WINPR_COMM*) hCommDev;

	if (!CommInitialized())
		return FALSE;

	/* TODO: not implemented */

	if (!pComm)
		return FALSE;

	CommLog_Print(WLOG_ERROR, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL GetCommMask(HANDLE hFile, PDWORD lpEvtMask)
{
	WINPR_COMM* pComm = (WINPR_COMM*) hFile;

	if (!CommInitialized())
		return FALSE;

	/* TODO: not implemented */

	if (!pComm)
		return FALSE;

	CommLog_Print(WLOG_ERROR, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL SetCommMask(HANDLE hFile, DWORD dwEvtMask)
{
	WINPR_COMM* pComm = (WINPR_COMM*) hFile;

	if (!CommInitialized())
		return FALSE;

	/* TODO: not implemented */

	if (!pComm)
		return FALSE;

	CommLog_Print(WLOG_ERROR, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL GetCommModemStatus(HANDLE hFile, PDWORD lpModemStat)
{
	WINPR_COMM* pComm = (WINPR_COMM*) hFile;

	if (!CommInitialized())
		return FALSE;

	/* TODO: not implemented */

	if (!pComm)
		return FALSE;

	CommLog_Print(WLOG_ERROR, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

/**
 * ERRORS:
 *   ERROR_DLL_INIT_FAILED
 *   ERROR_INVALID_HANDLE
 */
BOOL GetCommProperties(HANDLE hFile, LPCOMMPROP lpCommProp)
{
	WINPR_COMM* pComm = (WINPR_COMM*) hFile;
	DWORD bytesReturned;

	if (!CommInitialized())
		return FALSE;

	if (!pComm || pComm->Type != HANDLE_TYPE_COMM || !pComm->fd )
	{
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}

	if (!CommDeviceIoControl(pComm, IOCTL_SERIAL_GET_PROPERTIES, NULL, 0, lpCommProp, sizeof(COMMPROP), &bytesReturned, NULL))
	{
		CommLog_Print(WLOG_WARN, "GetCommProperties failure.");
		return FALSE;
	}

	return TRUE;
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
	DWORD bytesReturned;

	if (!CommInitialized())
		return FALSE;

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

	SERIAL_BAUD_RATE baudRate;
	if (!CommDeviceIoControl(pComm, IOCTL_SERIAL_GET_BAUD_RATE, NULL, 0, &baudRate, sizeof(SERIAL_BAUD_RATE), &bytesReturned, NULL))
	{
		CommLog_Print(WLOG_WARN, "GetCommState failure: could not get the baud rate.");
		goto error_handle;
	}
	lpLocalDcb->BaudRate = baudRate.BaudRate;

	lpLocalDcb->fBinary = (currentState.c_cflag & ICANON) == 0;
	if (!lpLocalDcb->fBinary)
	{
		CommLog_Print(WLOG_WARN, "Unexpected nonbinary mode, consider to unset the ICANON flag.");
	}

	lpLocalDcb->fParity =  (currentState.c_iflag & INPCK) != 0;

	SERIAL_HANDFLOW handflow;
	if (!CommDeviceIoControl(pComm, IOCTL_SERIAL_GET_HANDFLOW, NULL, 0, &handflow, sizeof(SERIAL_HANDFLOW), &bytesReturned, NULL))
	{
		CommLog_Print(WLOG_WARN, "GetCommState failure: could not get the handflow settings.");
		goto error_handle;
	}

	lpLocalDcb->fOutxCtsFlow = (handflow.ControlHandShake & SERIAL_CTS_HANDSHAKE) != 0;

	lpLocalDcb->fOutxDsrFlow = (handflow.ControlHandShake & SERIAL_DSR_HANDSHAKE) != 0;

	if (handflow.ControlHandShake & SERIAL_DTR_HANDSHAKE)
	{
		lpLocalDcb->fDtrControl = DTR_CONTROL_HANDSHAKE;
	}
	else if (handflow.ControlHandShake & SERIAL_DTR_CONTROL)
	{
		lpLocalDcb->fDtrControl = DTR_CONTROL_ENABLE;
	}
	else
	{
		lpLocalDcb->fDtrControl = DTR_CONTROL_DISABLE;
	}

	lpLocalDcb->fDsrSensitivity = (handflow.ControlHandShake & SERIAL_DSR_SENSITIVITY) != 0;

	lpLocalDcb->fTXContinueOnXoff = (handflow.FlowReplace & SERIAL_XOFF_CONTINUE) != 0;

	lpLocalDcb->fOutX = (handflow.FlowReplace & SERIAL_AUTO_TRANSMIT) != 0;

	lpLocalDcb->fInX = (handflow.FlowReplace & SERIAL_AUTO_RECEIVE) != 0;

	lpLocalDcb->fErrorChar = (handflow.FlowReplace & SERIAL_ERROR_CHAR) != 0;

	lpLocalDcb->fNull = (handflow.FlowReplace & SERIAL_NULL_STRIPPING) != 0;

	if (handflow.FlowReplace & SERIAL_RTS_HANDSHAKE)
	{
		lpLocalDcb->fRtsControl = RTS_CONTROL_HANDSHAKE;
	}
	else if (handflow.FlowReplace & SERIAL_RTS_CONTROL)
	{
		lpLocalDcb->fRtsControl = RTS_CONTROL_ENABLE;
	}
	else
	{
		lpLocalDcb->fRtsControl = RTS_CONTROL_DISABLE;
	}

	// FIXME: how to get the RTS_CONTROL_TOGGLE state? Does it match the UART 16750's Autoflow Control Enabled bit in its Modem Control Register (MCR)


	lpLocalDcb->fAbortOnError = (handflow.ControlHandShake & SERIAL_ERROR_ABORT) != 0;

	/* lpLocalDcb->fDummy2 not used */

	lpLocalDcb->wReserved = 0; /* must be zero */

	lpLocalDcb->XonLim = handflow.XonLimit;

	lpLocalDcb->XoffLim = handflow.XoffLimit;

	SERIAL_LINE_CONTROL lineControl;
	if (!CommDeviceIoControl(pComm, IOCTL_SERIAL_GET_LINE_CONTROL, NULL, 0, &lineControl, sizeof(SERIAL_LINE_CONTROL), &bytesReturned, NULL))
	{
		CommLog_Print(WLOG_WARN, "GetCommState failure: could not get the control settings.");
		goto error_handle;
	}

	lpLocalDcb->ByteSize = lineControl.WordLength;

	lpLocalDcb->Parity = lineControl.Parity;

	lpLocalDcb->StopBits = lineControl.StopBits;


	SERIAL_CHARS serialChars;
	if (!CommDeviceIoControl(pComm, IOCTL_SERIAL_GET_CHARS, NULL, 0, &serialChars, sizeof(SERIAL_CHARS), &bytesReturned, NULL))
	{
		CommLog_Print(WLOG_WARN, "GetCommState failure: could not get the serial chars.");
		goto error_handle;
	}

	lpLocalDcb->XonChar = serialChars.XonChar;

	lpLocalDcb->XoffChar = serialChars.XoffChar;

	lpLocalDcb->ErrorChar = serialChars.ErrorChar;

	lpLocalDcb->EofChar = serialChars.EofChar;

	lpLocalDcb->EvtChar = serialChars.EventChar;


	memcpy(lpDCB, lpLocalDcb, lpDCB->DCBlength);
	free(lpLocalDcb);
	return TRUE;

error_handle:
	free(lpLocalDcb);

	return FALSE;
}


/**
 * @return TRUE on success, FALSE otherwise.
 *
 * As of today, SetCommState() can fail half-way with some settings
 * applied and some others not. SetCommState() returns on the first
 * failure met. FIXME: or is it correct?
 *
 * ERRORS:
 *   ERROR_INVALID_HANDLE
 *   ERROR_IO_DEVICE
 */
BOOL SetCommState(HANDLE hFile, LPDCB lpDCB)
{
	struct termios upcomingTermios;
	WINPR_COMM* pComm = (WINPR_COMM*) hFile;
	DWORD bytesReturned;

	/* FIXME: validate changes according GetCommProperties? */

	if (!CommInitialized())
		return FALSE;

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

	/* NB: did the choice to call ioctls first when available and
	   then to setup upcomingTermios. Don't mix both stages. */

	/** ioctl calls stage **/

	SERIAL_BAUD_RATE baudRate;
	baudRate.BaudRate = lpDCB->BaudRate;
	if (!CommDeviceIoControl(pComm, IOCTL_SERIAL_SET_BAUD_RATE, &baudRate, sizeof(SERIAL_BAUD_RATE), NULL, 0, &bytesReturned, NULL))
	{
		CommLog_Print(WLOG_WARN, "SetCommState failure: could not set the baud rate.");
		return FALSE;
	}

	SERIAL_CHARS serialChars;
	if (!CommDeviceIoControl(pComm, IOCTL_SERIAL_GET_CHARS, NULL, 0, &serialChars, sizeof(SERIAL_CHARS), &bytesReturned, NULL)) /* as of today, required for BreakChar */
	{
		CommLog_Print(WLOG_WARN, "SetCommState failure: could not get the initial serial chars.");
		return FALSE;
	}
	serialChars.XonChar = lpDCB->XonChar;
	serialChars.XoffChar = lpDCB->XoffChar;
	serialChars.ErrorChar = lpDCB->ErrorChar;
	serialChars.EofChar = lpDCB->EofChar;
	serialChars.EventChar = lpDCB->EvtChar;
	if (!CommDeviceIoControl(pComm, IOCTL_SERIAL_SET_CHARS, &serialChars, sizeof(SERIAL_CHARS), NULL, 0, &bytesReturned, NULL))
	{
		CommLog_Print(WLOG_WARN, "SetCommState failure: could not set the serial chars.");
		return FALSE;
	}

	SERIAL_LINE_CONTROL lineControl;
	lineControl.StopBits = lpDCB->StopBits;
	lineControl.Parity = lpDCB->Parity;
	lineControl.WordLength = lpDCB->ByteSize;
	if (!CommDeviceIoControl(pComm, IOCTL_SERIAL_SET_LINE_CONTROL, &lineControl, sizeof(SERIAL_LINE_CONTROL), NULL, 0, &bytesReturned, NULL))
	{
		CommLog_Print(WLOG_WARN, "SetCommState failure: could not set the control settings.");
		return FALSE;
	}


	SERIAL_HANDFLOW handflow;
	ZeroMemory(&handflow, sizeof(SERIAL_HANDFLOW));

	if (lpDCB->fOutxCtsFlow)
	{
		handflow.ControlHandShake |= SERIAL_CTS_HANDSHAKE;
	}


	if (lpDCB->fOutxDsrFlow)
	{
		handflow.ControlHandShake |= SERIAL_DSR_HANDSHAKE;
	}

	switch (lpDCB->fDtrControl)
	{
		case SERIAL_DTR_HANDSHAKE:
			handflow.ControlHandShake |= DTR_CONTROL_HANDSHAKE;
			break;

		case SERIAL_DTR_CONTROL:
			handflow.ControlHandShake |= DTR_CONTROL_ENABLE;
			break;

		case DTR_CONTROL_DISABLE:
			/* do nothing since handflow is init-zeroed */
			break;

		default:
			CommLog_Print(WLOG_WARN, "Unexpected fDtrControl value: %d\n", lpDCB->fDtrControl);
			return FALSE;
	}

	if (lpDCB->fDsrSensitivity)
	{
		handflow.ControlHandShake |= SERIAL_DSR_SENSITIVITY;
	}

	if (lpDCB->fTXContinueOnXoff)
	{
		handflow.FlowReplace |= SERIAL_XOFF_CONTINUE;
	}

	if (lpDCB->fOutX)
	{
		handflow.FlowReplace |= SERIAL_AUTO_TRANSMIT;
	}

	if (lpDCB->fInX)
	{
		handflow.FlowReplace |= SERIAL_AUTO_RECEIVE;
	}

	if (lpDCB->fErrorChar)
	{
		handflow.FlowReplace |= SERIAL_ERROR_CHAR;
	}

	if (lpDCB->fNull)
	{
		handflow.FlowReplace |= SERIAL_NULL_STRIPPING;
	}

	switch (lpDCB->fRtsControl)
	{
		case RTS_CONTROL_TOGGLE:
			CommLog_Print(WLOG_WARN, "Unsupported RTS_CONTROL_TOGGLE feature");
			// FIXME: see also GetCommState()
			return FALSE;

		case RTS_CONTROL_HANDSHAKE:
			handflow.FlowReplace |= SERIAL_RTS_HANDSHAKE;
			break;

		case RTS_CONTROL_ENABLE:
			handflow.FlowReplace |= SERIAL_RTS_CONTROL;
			break;

		case RTS_CONTROL_DISABLE:
			/* do nothing since handflow is init-zeroed */
			break;

		default:
			CommLog_Print(WLOG_WARN, "Unexpected fRtsControl value: %d\n", lpDCB->fRtsControl);
			return FALSE;
	}

	if (lpDCB->fAbortOnError)
	{
		handflow.ControlHandShake |= SERIAL_ERROR_ABORT;
	}

	/* lpDCB->fDummy2 not used */

	/* lpLocalDcb->wReserved  ignored */

	handflow.XonLimit = lpDCB->XonLim;

	handflow.XoffLimit = lpDCB->XoffLim;

	if (!CommDeviceIoControl(pComm, IOCTL_SERIAL_SET_HANDFLOW, &handflow, sizeof(SERIAL_HANDFLOW), NULL, 0, &bytesReturned, NULL))
	{
		CommLog_Print(WLOG_WARN, "SetCommState failure: could not set the handflow settings.");
		return FALSE;
	}


	/** upcomingTermios stage **/


	ZeroMemory(&upcomingTermios, sizeof(struct termios));
	if (tcgetattr(pComm->fd, &upcomingTermios) < 0) /* NB: preserves current settings not directly handled by the Communication Functions */
	{
		SetLastError(ERROR_IO_DEVICE);
		return FALSE;
	}

	if (lpDCB->fBinary)
	{
		upcomingTermios.c_lflag &= ~ICANON;
	}
	else
	{
		upcomingTermios.c_lflag |= ICANON;
		CommLog_Print(WLOG_WARN, "Unexpected nonbinary mode, consider to unset the ICANON flag.");
	}

	if (lpDCB->fParity)
	{
		upcomingTermios.c_iflag |= INPCK;
	}
	else
	{
		upcomingTermios.c_iflag &= ~INPCK;
	}


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

	if (_comm_ioctl_tcsetattr(pComm->fd, TCSANOW, &upcomingTermios) < 0)
	{
		SetLastError(ERROR_IO_DEVICE);
		return FALSE;
	}


	return TRUE;
}

/**
 * ERRORS:
 *   ERROR_INVALID_HANDLE
 */
BOOL GetCommTimeouts(HANDLE hFile, LPCOMMTIMEOUTS lpCommTimeouts)
{
	WINPR_COMM* pComm = (WINPR_COMM*) hFile;
	DWORD bytesReturned;

	if (!CommInitialized())
		return FALSE;

	if (!pComm || pComm->Type != HANDLE_TYPE_COMM || !pComm->fd )
	{
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}

	/* as of today, SERIAL_TIMEOUTS and COMMTIMEOUTS structures are identical */

	if (!CommDeviceIoControl(pComm, IOCTL_SERIAL_GET_TIMEOUTS, NULL, 0, lpCommTimeouts, sizeof(COMMTIMEOUTS), &bytesReturned, NULL))
	{
		CommLog_Print(WLOG_WARN, "GetCommTimeouts failure.");
		return FALSE;
	}

	return TRUE;
}


/**
 * ERRORS:
 *   ERROR_INVALID_HANDLE
 */
BOOL SetCommTimeouts(HANDLE hFile, LPCOMMTIMEOUTS lpCommTimeouts)
{
	WINPR_COMM* pComm = (WINPR_COMM*) hFile;
	DWORD bytesReturned;

	if (!CommInitialized())
		return FALSE;

	if (!pComm || pComm->Type != HANDLE_TYPE_COMM || !pComm->fd )
	{
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}

	/* as of today, SERIAL_TIMEOUTS and COMMTIMEOUTS structures are identical */

	if (!CommDeviceIoControl(pComm, IOCTL_SERIAL_SET_TIMEOUTS, lpCommTimeouts, sizeof(COMMTIMEOUTS), NULL, 0, &bytesReturned, NULL))
	{
		CommLog_Print(WLOG_WARN, "SetCommTimeouts failure.");
		return FALSE;
	}

	return TRUE;
}

BOOL GetDefaultCommConfigA(LPCSTR lpszName, LPCOMMCONFIG lpCC, LPDWORD lpdwSize)
{
	if (!CommInitialized())
		return FALSE;

	/* TODO: not implemented */

	CommLog_Print(WLOG_ERROR, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL GetDefaultCommConfigW(LPCWSTR lpszName, LPCOMMCONFIG lpCC, LPDWORD lpdwSize)
{
	if (!CommInitialized())
		return FALSE;

	/* TODO: not implemented */

	CommLog_Print(WLOG_ERROR, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL SetDefaultCommConfigA(LPCSTR lpszName, LPCOMMCONFIG lpCC, DWORD dwSize)
{
	if (!CommInitialized())
		return FALSE;

	/* TODO: not implemented */

	CommLog_Print(WLOG_ERROR, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL SetDefaultCommConfigW(LPCWSTR lpszName, LPCOMMCONFIG lpCC, DWORD dwSize)
{
	if (!CommInitialized())
		return FALSE;

	/* TODO: not implemented */

	CommLog_Print(WLOG_ERROR, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL SetCommBreak(HANDLE hFile)
{
	WINPR_COMM* pComm = (WINPR_COMM*) hFile;

	if (!CommInitialized())
		return FALSE;

	/* TODO: not implemented */

	if (!pComm)
		return FALSE;

	CommLog_Print(WLOG_ERROR, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL ClearCommBreak(HANDLE hFile)
{
	WINPR_COMM* pComm = (WINPR_COMM*) hFile;

	if (!CommInitialized())
		return FALSE;

	/* TODO: not implemented */

	if (!pComm)
		return FALSE;

	CommLog_Print(WLOG_ERROR, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL ClearCommError(HANDLE hFile, PDWORD lpErrors, LPCOMSTAT lpStat)
{
	WINPR_COMM* pComm = (WINPR_COMM*) hFile;

	if (!CommInitialized())
		return FALSE;

	/* TODO: not implemented */

	if (!pComm)
		return FALSE;

	CommLog_Print(WLOG_ERROR, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}


BOOL PurgeComm(HANDLE hFile, DWORD dwFlags)
{
	WINPR_COMM* pComm = (WINPR_COMM*) hFile;
	DWORD bytesReturned = 0;

	if (!CommInitialized())
		return FALSE;

	if (!pComm || pComm->Type != HANDLE_TYPE_COMM || !pComm->fd )
	{
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}

	if (!CommDeviceIoControl(pComm, IOCTL_SERIAL_PURGE, &dwFlags, sizeof(DWORD), NULL, 0, &bytesReturned, NULL))
	{
		CommLog_Print(WLOG_WARN, "PurgeComm failure.");
		return FALSE;
	}

	return TRUE;
}


BOOL SetupComm(HANDLE hFile, DWORD dwInQueue, DWORD dwOutQueue)
{
	WINPR_COMM* pComm = (WINPR_COMM*) hFile;
	SERIAL_QUEUE_SIZE queueSize;
	DWORD bytesReturned = 0;

	if (!CommInitialized())
		return FALSE;

	if (!pComm || pComm->Type != HANDLE_TYPE_COMM || !pComm->fd )
	{
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}

	queueSize.InSize = dwInQueue;
	queueSize.OutSize = dwOutQueue;

	if (!CommDeviceIoControl(pComm, IOCTL_SERIAL_SET_QUEUE_SIZE, &queueSize, sizeof(SERIAL_QUEUE_SIZE), NULL, 0, &bytesReturned, NULL))
	{
		CommLog_Print(WLOG_WARN, "SetCommTimeouts failure.");
		return FALSE;
	}

	return TRUE;
}

BOOL EscapeCommFunction(HANDLE hFile, DWORD dwFunc)
{
	WINPR_COMM* pComm = (WINPR_COMM*) hFile;

	if (!CommInitialized())
		return FALSE;

	/* TODO: not implemented */

	if (!pComm)
		return FALSE;

	CommLog_Print(WLOG_ERROR, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL TransmitCommChar(HANDLE hFile, char cChar)
{
	WINPR_COMM* pComm = (WINPR_COMM*) hFile;

	if (!CommInitialized())
		return FALSE;

	/* TODO: not implemented */

	if (!pComm)
		return FALSE;

	CommLog_Print(WLOG_ERROR, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL WaitCommEvent(HANDLE hFile, PDWORD lpEvtMask, LPOVERLAPPED lpOverlapped)
{
	WINPR_COMM* pComm = (WINPR_COMM*) hFile;

	if (!CommInitialized())
		return FALSE;

	/* TODO: not implemented */

	if (!pComm)
		return FALSE;

	CommLog_Print(WLOG_ERROR, "%s: Not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}


/* Extended API */

static BOOL _IsReservedCommDeviceName(LPCTSTR lpName)
{
	int i;

	if (!CommInitialized())
		return FALSE;

	/* Serial ports, COM1-9 */
	for (i=1; i<10; i++)
	{
		TCHAR genericName[5];
		if (_stprintf_s(genericName, 5, _T("COM%d"), i) < 0)
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
		if (_stprintf_s(genericName, 5, _T("LPT%d"), i) < 0)
		{
			return FALSE;
		}

		if (_tcscmp(genericName, lpName) == 0)
			return TRUE;
	}

	/* FIXME: what about PRN ? */

	return FALSE;
}


/**
 * Returns TRUE on success, FALSE otherwise. To get extended error
 * information, call GetLastError.
 *
 * ERRORS:
 *   ERROR_DLL_INIT_FAILED
 *   ERROR_OUTOFMEMORY was not possible to get mappings.
 *   ERROR_INVALID_DATA was not possible to add the device.
 */
BOOL DefineCommDevice(/* DWORD dwFlags,*/ LPCTSTR lpDeviceName, LPCTSTR lpTargetPath)
{
	int i = 0;
	LPTSTR storedDeviceName = NULL;
	LPTSTR storedTargetPath = NULL;

	if (!CommInitialized())
		return FALSE;

	EnterCriticalSection(&_CommDevicesLock);

	if (_CommDevices == NULL)
	{
		SetLastError(ERROR_DLL_INIT_FAILED);
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

	for (i=0; i<COMM_DEVICE_MAX; i++)
	{
		if (_CommDevices[i] != NULL)
		{
			if (_tcscmp(_CommDevices[i]->name, storedDeviceName) == 0)
			{
				/* take over the emplacement */
				free(_CommDevices[i]->name);
				free(_CommDevices[i]->path);
				_CommDevices[i]->name = storedDeviceName;
				_CommDevices[i]->path = storedTargetPath;

				break;
			}
		}
		else
		{
			/* new emplacement */
			_CommDevices[i] = (COMM_DEVICE*)calloc(1, sizeof(COMM_DEVICE));
			if (_CommDevices[i] == NULL)
			{
				SetLastError(ERROR_OUTOFMEMORY);
				goto error_handle;
			}

			_CommDevices[i]->name = storedDeviceName;
			_CommDevices[i]->path = storedTargetPath;
			break;
		}
	}

	if (i == COMM_DEVICE_MAX)
	{
		SetLastError(ERROR_OUTOFMEMORY);
		goto error_handle;
	}

	LeaveCriticalSection(&_CommDevicesLock);
	return TRUE;


error_handle:
	free(storedDeviceName);
	free(storedTargetPath);

	LeaveCriticalSection(&_CommDevicesLock);
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
 *   ERROR_DLL_INIT_FAILED
 *   ERROR_OUTOFMEMORY was not possible to get mappings.
 *   ERROR_NOT_SUPPORTED equivalent QueryDosDevice feature not supported.
 *   ERROR_INVALID_DATA was not possible to retrieve any device information.
 *   ERROR_INSUFFICIENT_BUFFER too small lpTargetPath
 */
DWORD QueryCommDevice(LPCTSTR lpDeviceName, LPTSTR lpTargetPath, DWORD ucchMax)
{
	int i;
	LPTSTR storedTargetPath;

	SetLastError(ERROR_SUCCESS);

	if (!CommInitialized())
		return 0;

	if (_CommDevices == NULL)
	{
		SetLastError(ERROR_DLL_INIT_FAILED);
		return 0;
	}

	if (lpDeviceName == NULL || lpTargetPath == NULL)
	{
		SetLastError(ERROR_NOT_SUPPORTED);
		return 0;
	}

	EnterCriticalSection(&_CommDevicesLock);

	storedTargetPath = NULL;
	for (i=0; i<COMM_DEVICE_MAX; i++)
	{
		if (_CommDevices[i] != NULL)
		{
			if (_tcscmp(_CommDevices[i]->name, lpDeviceName) == 0)
			{
				storedTargetPath = _CommDevices[i]->path;
				break;
			}

			continue;
		}

		break;
	}

	LeaveCriticalSection(&_CommDevicesLock);

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
	lpTargetPath[_tcslen(storedTargetPath) + 1] = '\0'; /* 2nd final '\0' */

	return _tcslen(lpTargetPath) + 2;
}

/**
 * Checks whether lpDeviceName is a valid and registered Communication device.
 */
BOOL IsCommDevice(LPCTSTR lpDeviceName)
{
	TCHAR lpTargetPath[MAX_PATH];

	if (!CommInitialized())
		return FALSE;

	if (QueryCommDevice(lpDeviceName, lpTargetPath, MAX_PATH) > 0)
	{
		return TRUE;
	}

	return FALSE;
}


/**
 * Sets
 */
void _comm_setServerSerialDriver(HANDLE hComm, SERIAL_DRIVER_ID driverId)
{
	ULONG Type;
	WINPR_HANDLE* Object;
	WINPR_COMM* pComm;

	if (!CommInitialized())
		return;

	if (!winpr_Handle_GetInfo(hComm, &Type, &Object))
	{
		CommLog_Print(WLOG_WARN, "_comm_setServerSerialDriver failure");
		return;
	}

	pComm = (WINPR_COMM*)Object;
	pComm->serverSerialDriverId = driverId;
}

static HANDLE_OPS ops = {
		CommIsHandled,
		CommCloseHandle,
		CommGetFd,
		NULL /* CleanupHandle */
};


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
	struct termios upcomingTermios;

	if (!CommInitialized())
		return INVALID_HANDLE_VALUE;
	
	if (dwDesiredAccess != (GENERIC_READ | GENERIC_WRITE))
	{
		CommLog_Print(WLOG_WARN, "unexpected access to the device: 0x%lX", dwDesiredAccess);
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
		CommLog_Print(WLOG_WARN, "unexpected security attributes, nLength=%lu", lpSecurityAttributes->nLength);
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
		CommLog_Print(WLOG_WARN, "device not found %s", devicePath);
		SetLastError(ERROR_FILE_NOT_FOUND);
		return INVALID_HANDLE_VALUE;
	}

	if (!S_ISCHR(deviceStat.st_mode))
	{
		CommLog_Print(WLOG_WARN, "bad device %s", devicePath);
		SetLastError(ERROR_BAD_DEVICE);
		return INVALID_HANDLE_VALUE;
	}

	if (dwFlagsAndAttributes != 0)
	{
		CommLog_Print(WLOG_WARN, "unexpected flags and attributes: 0x%lX", dwFlagsAndAttributes);
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

	WINPR_HANDLE_SET_TYPE_AND_MODE(pComm, HANDLE_TYPE_COMM, WINPR_FD_READ);

	pComm->ops = &ops;

	/* error_handle */

	pComm->fd = open(devicePath, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (pComm->fd < 0)
	{
		CommLog_Print(WLOG_WARN, "failed to open device %s", devicePath);
		SetLastError(ERROR_BAD_DEVICE);
		goto error_handle;
	}

	pComm->fd_read = open(devicePath, O_RDONLY | O_NOCTTY | O_NONBLOCK);
	if (pComm->fd_read < 0)
	{
		CommLog_Print(WLOG_WARN, "failed to open fd_read, device: %s", devicePath);
		SetLastError(ERROR_BAD_DEVICE);
		goto error_handle;
	}

	pComm->fd_read_event = eventfd(0, EFD_NONBLOCK); /* EFD_NONBLOCK required because a read() is not always expected */
	if (pComm->fd_read_event < 0)
	{
		CommLog_Print(WLOG_WARN, "failed to open fd_read_event, device: %s", devicePath);
		SetLastError(ERROR_BAD_DEVICE);
		goto error_handle;
	}

	InitializeCriticalSection(&pComm->ReadLock);

	pComm->fd_write = open(devicePath, O_WRONLY | O_NOCTTY | O_NONBLOCK);
	if (pComm->fd_write < 0)
	{
		CommLog_Print(WLOG_WARN, "failed to open fd_write, device: %s", devicePath);
		SetLastError(ERROR_BAD_DEVICE);
		goto error_handle;
	}

	pComm->fd_write_event = eventfd(0, EFD_NONBLOCK); /* EFD_NONBLOCK required because a read() is not always expected */
	if (pComm->fd_write_event < 0)
	{
		CommLog_Print(WLOG_WARN, "failed to open fd_write_event, device: %s", devicePath);
		SetLastError(ERROR_BAD_DEVICE);
		goto error_handle;
	}

	InitializeCriticalSection(&pComm->WriteLock);

	/* can also be setup later on with _comm_setServerSerialDriver() */
	pComm->serverSerialDriverId = SerialDriverUnknown;

	InitializeCriticalSection(&pComm->EventsLock);

	if (ioctl(pComm->fd, TIOCGICOUNT, &(pComm->counters)) < 0)
	{
		CommLog_Print(WLOG_WARN, "TIOCGICOUNT ioctl failed, errno=[%d] %s.", errno, strerror(errno));
		CommLog_Print(WLOG_WARN, "could not read counters.");

		/* could not initialize counters but keep on. 
		 *
		 * Not all drivers, especially for USB to serial
		 * adapters (e.g. those based on pl2303), does support
		 * this call.
		 */

		ZeroMemory(&(pComm->counters), sizeof(struct serial_icounter_struct));
	}


	/* The binary/raw mode is required for the redirection but
	 * only flags that are not handle somewhere-else, except
	 * ICANON, are forced here. */

	ZeroMemory(&upcomingTermios, sizeof(struct termios));
	if (tcgetattr(pComm->fd, &upcomingTermios) < 0)
	{
		SetLastError(ERROR_IO_DEVICE);
		goto error_handle;
	}

	upcomingTermios.c_iflag &= ~(/*IGNBRK |*/ BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL /*| IXON*/);
	upcomingTermios.c_oflag = 0; /* <=> &= ~OPOST */
	upcomingTermios.c_lflag = 0; /* <=> &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN); */
	/* upcomingTermios.c_cflag &= ~(CSIZE | PARENB); */
	/* upcomingTermios.c_cflag |= CS8; */

	/* About missing flags recommended by termios(3):
	 *
	 *   IGNBRK and IXON, see: IOCTL_SERIAL_SET_HANDFLOW
	 *   CSIZE, PARENB and CS8, see: IOCTL_SERIAL_SET_LINE_CONTROL
	 */

	/* a few more settings required for the redirection */
	upcomingTermios.c_cflag |= CLOCAL | CREAD;

	if (_comm_ioctl_tcsetattr(pComm->fd, TCSANOW, &upcomingTermios) < 0)
	{
		SetLastError(ERROR_IO_DEVICE);
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


BOOL CommIsHandled(HANDLE handle)
{
	WINPR_COMM *pComm;

	if (!CommInitialized())
		return FALSE;

	pComm = (WINPR_COMM*)handle;

	if (!pComm || (pComm->Type != HANDLE_TYPE_COMM) || (pComm == INVALID_HANDLE_VALUE))
	{
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}

	return TRUE;
}

BOOL CommCloseHandle(HANDLE handle)
{
	WINPR_COMM *pComm;

	if (!CommInitialized())
		return FALSE;

	pComm = (WINPR_COMM*)handle;

	if (!pComm || pComm->Type != HANDLE_TYPE_COMM)
	{
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}

	if (pComm->PendingEvents & SERIAL_EV_FREERDP_WAITING)
	{
		ULONG WaitMask = 0;
		DWORD BytesReturned = 0;

		/* ensures to gracefully stop the WAIT_ON_MASK's loop */
		if (!CommDeviceIoControl(handle, IOCTL_SERIAL_SET_WAIT_MASK, &WaitMask, sizeof(ULONG), NULL, 0, &BytesReturned, NULL))
		{
			CommLog_Print(WLOG_WARN, "failure to WAIT_ON_MASK's loop!");
		}
	}

	DeleteCriticalSection(&pComm->ReadLock);
	DeleteCriticalSection(&pComm->WriteLock);
	DeleteCriticalSection(&pComm->EventsLock);

	if (pComm->fd > 0)
		close(pComm->fd);

	if (pComm->fd_write > 0)
		close(pComm->fd_write);

	if (pComm->fd_write_event > 0)
		close(pComm->fd_write_event);

	if (pComm->fd_read > 0)
		close(pComm->fd_read);

	if (pComm->fd_read_event > 0)
		close(pComm->fd_read_event);

	free(pComm);

	return TRUE;
}

#ifndef WITH_EVENTFD_READ_WRITE
int eventfd_read(int fd, eventfd_t* value)
{
	return (read(fd, value, sizeof(*value)) == sizeof(*value)) ? 0 : -1;
}

int eventfd_write(int fd, eventfd_t value)
{
	return (write(fd, &value, sizeof(value)) == sizeof(value)) ? 0 : -1;
}
#endif

#endif /* __linux__ */
