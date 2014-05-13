/**
 * WinPR: Windows Portable Runtime
 * Serial Communication API
 *
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

#include <errno.h>
#include <termios.h>
#include <unistd.h>

#include <freerdp/utils/debug.h>

#include <winpr/io.h>
#include <winpr/wtypes.h>

#include "comm.h"

/* Computes Tmax in deciseconds (m and Tcare in milliseconds) */
static UCHAR _tmax(DWORD N, ULONG m, ULONG Tc)
{
	/* Tmax = N * m  + Tc */
	
	ULONGLONG Tmax = N * m + Tc;

	/* FIXME: look for an equivalent math function otherwise let
	 * do the compiler do the optimization */
	if (Tmax == 0)
		return 0;
	else if (Tmax < 100)
		return 1;
	else if (Tmax > 25500)
		return 255; /* 0xFF */
	else
		return Tmax/100;
}


/**
 * ERRORS:
 *   ERROR_INVALID_HANDLE
 *   ERROR_NOT_SUPPORTED
 *   ERROR_INVALID_PARAMETER
 *   ERROR_TIMEOUT
 *   ERROR_IO_DEVICE
 *   ERROR_BAD_DEVICE
 */
BOOL CommReadFile(HANDLE hDevice, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
		  LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped)
{
	WINPR_COMM* pComm = (WINPR_COMM*) hDevice;
	ssize_t nbRead = 0;
	COMMTIMEOUTS *pTimeouts;
	UCHAR vmin = 0;
	UCHAR vtime = 0;
	struct termios currentTermios;

	if (hDevice == INVALID_HANDLE_VALUE)
	{
		SetLastError(ERROR_INVALID_HANDLE);
                return FALSE;
        }

	if (!pComm || pComm->Type != HANDLE_TYPE_COMM || !pComm->fd )
	{
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}

	if (lpOverlapped != NULL)
	{
		SetLastError(ERROR_NOT_SUPPORTED);
		return FALSE;
	}

	if (lpNumberOfBytesRead == NULL)
	{
		SetLastError(ERROR_INVALID_PARAMETER); /* since we doesn't suppport lpOverlapped != NULL */
		return FALSE;
	}

	*lpNumberOfBytesRead = 0; /* will be ajusted if required ... */

	if (nNumberOfBytesToRead <= 0) /* N */
	{
		return TRUE; /* FIXME: or FALSE? */
	}

	if (tcgetattr(pComm->fd, &currentTermios) < 0)
	{
		SetLastError(ERROR_IO_DEVICE);
		return FALSE;
	}

	if (currentTermios.c_lflag & ICANON)
	{
		DEBUG_WARN("Canonical mode not supported"); /* the timeout could not be set */
		SetLastError(ERROR_NOT_SUPPORTED);
		return FALSE;
	}

	/* http://msdn.microsoft.com/en-us/library/hh439614%28v=vs.85%29.aspx
	 * http://msdn.microsoft.com/en-us/library/windows/hardware/hh439614%28v=vs.85%29.aspx
	 *
	 * ReadIntervalTimeout  | ReadTotalTimeoutMultiplier | ReadTotalTimeoutConstant | VMIN | VTIME |  TMAX   |
	 *         0            |            0               |           0              |   N  |   0   |    0    | Blocks for N bytes available. FIXME: reduce N to 1?
         *   0< Ti <MAXULONG    |            0               |           0              |   N  |   Ti  |    0    | Block on first byte, then use Ti between bytes.
	 *       MAXULONG       |            0               |           0              |   0  |   0   |    0    | Returns immediately with bytes available (don't block)
	 *       MAXULONG       |         MAXULONG           |      0< Tc <MAXULONG     |   0  |   Tc  |    0    | Blocks on first byte during Tc or returns immediately whith bytes available 
	 *       MAXULONG       |            m               |        MAXULONG          |                        | Invalid
	 *         0            |            m               |      0< Tc <MAXULONG     |   0  |  Tmax |    0    | Block on first byte during Tmax or returns immediately whith bytes available 
	 *   0< Ti <MAXULONG    |            m               |      0< Tc <MAXULONG     |   N  |   Ti  | Tmax(1) | Block on first byte, then use Ti between bytes. Tmax is use for the whole system call.
	 */

	/* Tmax = N * m  + Tc */

	/* NB: 0<N; 0 < m < MAXULONG  */
	/* NB: timeout are milliseconds, VTIME are deciseconds and is an unsigned char */

	/* FIXME: Use a dedicted timer for Tmax(1). VMIN=0 and VTIME=Tmax is rather used instead of VMIN=N and WTIME=Ti.
	 * TMP: check whether open(pComm->fd, O_NONBLOCK) doesn't conflict with above use cases 
	 */

	pTimeouts = &(pComm->timeouts);

	if ((pTimeouts->ReadIntervalTimeout == MAXULONG) && (pTimeouts->ReadTotalTimeoutConstant == MAXULONG))
	{
		DEBUG_WARN("ReadIntervalTimeout and ReadTotalTimeoutConstant cannot be both set to MAXULONG");
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	/* VMIN */

	if ((pTimeouts->ReadIntervalTimeout < MAXULONG) && (pTimeouts->ReadTotalTimeoutMultiplier == 0) && (pTimeouts->ReadTotalTimeoutConstant == 0))
	{
		/* should only match the two first cases above */
		vmin = nNumberOfBytesToRead < 256 ? nNumberOfBytesToRead : 255; /* 0xFF */
	}
	

	/* VTIME */

	if ((pTimeouts->ReadIntervalTimeout > 0) && (pTimeouts->ReadTotalTimeoutMultiplier == 0) && (pTimeouts->ReadTotalTimeoutConstant == 0))
	{
		/* Ti */
		
		vtime = _tmax(0, 0, pTimeouts->ReadIntervalTimeout);
		
	}
	else if ((pTimeouts->ReadIntervalTimeout == MAXULONG) && (pTimeouts->ReadTotalTimeoutMultiplier == MAXULONG) && (pTimeouts->ReadTotalTimeoutConstant < MAXULONG))
	{
		/* Tc */

		vtime = _tmax(0, 0, pTimeouts->ReadTotalTimeoutConstant);
	}
	else if ((pTimeouts->ReadTotalTimeoutMultiplier > 0) || (pTimeouts->ReadTotalTimeoutConstant > 0)) /* <=> Tmax > 0 */
	{
		/* Tmax and Tmax(1) */

		vtime = _tmax(nNumberOfBytesToRead, pTimeouts->ReadTotalTimeoutMultiplier, pTimeouts->ReadTotalTimeoutConstant);
	}

	
	if ((currentTermios.c_cc[VMIN] != vmin) || (currentTermios.c_cc[VTIME] != vtime))
	{
		currentTermios.c_cc[VMIN]  = vmin;
		currentTermios.c_cc[VTIME] = vtime;

		// TMP:
		DEBUG_MSG("Applying timeout VMIN=%u, VTIME=%u", vmin, vtime);

		if (tcsetattr(pComm->fd, TCSANOW, &currentTermios) < 0)
		{
			DEBUG_WARN("CommReadFile failure, could not apply new timeout values: VMIN=%u, VTIME=%u", vmin, vtime);
			SetLastError(ERROR_IO_DEVICE);
			return FALSE;
		}
	}

	nbRead = read(pComm->fd, lpBuffer, nNumberOfBytesToRead);

	if (nbRead < 0)
	{
		DEBUG_WARN("CommReadFile failed, ReadIntervalTimeout=%d, ReadTotalTimeoutMultiplier=%d, ReadTotalTimeoutConstant=%d VMIN=%d, VTIME=%d",
			   pTimeouts->ReadIntervalTimeout, pTimeouts->ReadTotalTimeoutMultiplier, pTimeouts->ReadTotalTimeoutConstant, 
			   currentTermios.c_cc[VMIN], currentTermios.c_cc[VTIME]);
		DEBUG_WARN("CommReadFile failed, nNumberOfBytesToRead=%d, errno=[%d] %s", nNumberOfBytesToRead, errno, strerror(errno));

		switch (errno)
		{
			// TMP: TODO:
			case EAGAIN: /* EWOULDBLOCK */
				nbRead = 0;
				break;

			case EBADF:
				SetLastError(ERROR_BAD_DEVICE); /* STATUS_INVALID_DEVICE_REQUEST */
				return FALSE;

			default:
				return FALSE;
		}

	}

	// TODO:
	// SetLastError(ERROR_TIMEOUT)

	*lpNumberOfBytesRead = nbRead;
	return TRUE;
}


/**
 * ERRORS:
 *   ERROR_INVALID_HANDLE
 *   ERROR_NOT_SUPPORTED
 *   ERROR_INVALID_PARAMETER
 *   ERROR_BAD_DEVICE
 */
BOOL CommWriteFile(HANDLE hDevice, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
		   LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped)
{
	WINPR_COMM* pComm = (WINPR_COMM*) hDevice;
	COMMTIMEOUTS *pTimeouts;

	if (hDevice == INVALID_HANDLE_VALUE)
	{
		SetLastError(ERROR_INVALID_HANDLE);
                return FALSE;
        }

	if (!pComm || pComm->Type != HANDLE_TYPE_COMM || !pComm->fd )
	{
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}

	if (lpOverlapped != NULL)
	{
		SetLastError(ERROR_NOT_SUPPORTED);
		return FALSE;
	}

	if (lpNumberOfBytesWritten == NULL)
	{
		SetLastError(ERROR_INVALID_PARAMETER); /* since we doesn't suppport lpOverlapped != NULL */
		return FALSE;
	}

	*lpNumberOfBytesWritten = 0; /* will be ajusted if required ... */

	if (nNumberOfBytesToWrite <= 0)
	{
		return TRUE; /* FIXME: or FALSE? */
	}

	// TMP: TODO: Timeouts
	pTimeouts = &(pComm->timeouts);

	while (*lpNumberOfBytesWritten < nNumberOfBytesToWrite)
	{
		ssize_t nbWritten;

		nbWritten = write(pComm->fd, 
				  lpBuffer + (*lpNumberOfBytesWritten), 
				  nNumberOfBytesToWrite - (*lpNumberOfBytesWritten));

		if (nbWritten < 0)
		{
			DEBUG_WARN("CommWriteFile failed after %d bytes written, errno=[%d] %s\n", *lpNumberOfBytesWritten, errno, strerror(errno));

			switch (errno)
			{
				case EAGAIN:
					/* EWOULDBLOCK */
					// TMP: FIXME: fcntl(tty->fd, F_SETFL, O_NONBLOCK) and manage the timeout here?
					break;
				case EBADF:
					SetLastError(ERROR_BAD_DEVICE); /* STATUS_INVALID_DEVICE_REQUEST */
					return FALSE;
			}

			return FALSE;
		}

		*lpNumberOfBytesWritten += nbWritten;
	}
	
	return TRUE;
}

			
#endif /* _WIN32 */
