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


#include <assert.h>
#include <errno.h>

#include <freerdp/utils/debug.h>

#include "comm.h"
#include "comm_ioctl.h"
#include "comm_serial_sys.h"
#include "comm_sercx_sys.h"
#include "comm_sercx2_sys.h"


/* NB: MS-RDPESP's recommendation:
 *
 * <2> Section 3.2.5.1.6: Windows Implementations use IOCTL constants
 * for IoControlCode values.  The content and values of the IOCTLs are
 * opaque to the protocol. On the server side, the data contained in
 * an IOCTL is simply packaged and sent to the client side. For
 * maximum compatibility between the different versions of the Windows
 * operating system, the client implementation only singles out
 * critical IOCTLs and invokes the applicable Win32 port API. The
 * other IOCTLS are passed directly to the client-side driver, and the
 * processing of this value depends on the drivers installed on the
 * client side. The values and parameters for these IOCTLS can be
 * found in [MSFT-W2KDDK] Volume 2, Part 2—Serial and Parallel
 * Drivers, and in [MSDN-PORTS].
 */


/**
 * FIXME: to be used through winpr-io's DeviceIoControl
 *
 * ERRORS:
 *   ERROR_INVALID_HANDLE
 *   ERROR_NOT_SUPPORTED lpOverlapped is not supported
 *   ERROR_INSUFFICIENT_BUFFER
 */
BOOL CommDeviceIoControl(HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize,
			 LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped)
{

	WINPR_COMM* pComm = (WINPR_COMM*) hDevice;
	REMOTE_SERIAL_DRIVER* pRemoteSerialDriver = NULL;

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

	if (lpBytesReturned == NULL)
	{
		SetLastError(ERROR_INVALID_DATA); /* since we doesn't suppport lpOverlapped != NULL */
		return FALSE;
	}

	*lpBytesReturned = 0; /* will be ajusted if required ... */

	/* remoteSerialDriver to be use ... 
	 *
	 * FIXME: might prefer to use an automatic rather than static structure
	 */
	switch (pComm->remoteSerialDriverId)
	{
		case RemoteSerialDriverSerialSys:
			pRemoteSerialDriver = SerialSys_s();
			break;

		case RemoteSerialDriverSerCxSys:
			pRemoteSerialDriver = SerCxSys_s();
			break;

		case RemoteSerialDriverSerCx2Sys:
			pRemoteSerialDriver = SerCx2Sys_s();
			break;

		case RemoteSerialDriverUnknown:
		default:
			DEBUG_WARN("Unknown remote serial driver (%d), using SerCx2.sys", pComm->remoteSerialDriverId);
			pRemoteSerialDriver = SerCx2Sys_s();
			break;
	}

	assert(pRemoteSerialDriver != NULL);

	switch (dwIoControlCode)
	{
		case IOCTL_SERIAL_SET_BAUD_RATE:
		{
			if (pRemoteSerialDriver->set_baud_rate)
			{
				SERIAL_BAUD_RATE *pBaudRate = (SERIAL_BAUD_RATE*)lpInBuffer;

				assert(nInBufferSize >= sizeof(SERIAL_BAUD_RATE));
				if (nInBufferSize < sizeof(SERIAL_BAUD_RATE))
				{
					SetLastError(ERROR_INVALID_DATA);
					return FALSE;
				}

				return pRemoteSerialDriver->set_baud_rate(pComm, pBaudRate);
			}
		}
		case IOCTL_SERIAL_GET_BAUD_RATE:
		{
			if (pRemoteSerialDriver->get_baud_rate)
			{
				SERIAL_BAUD_RATE *pBaudRate = (SERIAL_BAUD_RATE*)lpOutBuffer;

				assert(nOutBufferSize >= sizeof(SERIAL_BAUD_RATE));
				if (nOutBufferSize < sizeof(SERIAL_BAUD_RATE))
				{
					SetLastError(ERROR_INSUFFICIENT_BUFFER);
					return FALSE;
				}

				if (!pRemoteSerialDriver->get_baud_rate(pComm, pBaudRate))
					return FALSE;

				*lpBytesReturned = sizeof(SERIAL_BAUD_RATE);
				return TRUE;
			}
		}
		case IOCTL_SERIAL_GET_PROPERTIES:
		{
			if (pRemoteSerialDriver->get_properties)
			{
				COMMPROP *pProperties = (COMMPROP*)lpOutBuffer;
				
				assert(nOutBufferSize >= sizeof(COMMPROP));
				if (nOutBufferSize < sizeof(COMMPROP))
				{
					SetLastError(ERROR_INSUFFICIENT_BUFFER);
					return FALSE;					
				}

				if (!pRemoteSerialDriver->get_properties(pComm, pProperties))
					return FALSE;

				*lpBytesReturned = sizeof(COMMPROP);
				return TRUE;
			}
		}
		case IOCTL_SERIAL_SET_CHARS:
		{
			if (pRemoteSerialDriver->set_serial_chars)
			{
				SERIAL_CHARS *pSerialChars = (SERIAL_CHARS*)lpInBuffer;

				assert(nInBufferSize >= sizeof(SERIAL_CHARS));
				if (nInBufferSize < sizeof(SERIAL_CHARS))
				{
					SetLastError(ERROR_INVALID_DATA);
					return FALSE;
				}

				return pRemoteSerialDriver->set_serial_chars(pComm, pSerialChars);
			}
		}
		case IOCTL_SERIAL_GET_CHARS:
		{
			if (pRemoteSerialDriver->get_serial_chars)
			{
				SERIAL_CHARS *pSerialChars = (SERIAL_CHARS*)lpOutBuffer;
				
				assert(nOutBufferSize >= sizeof(SERIAL_CHARS));
				if (nOutBufferSize < sizeof(SERIAL_CHARS))
				{
					SetLastError(ERROR_INSUFFICIENT_BUFFER);
					return FALSE;					
				}

				if (!pRemoteSerialDriver->get_serial_chars(pComm, pSerialChars))
					return FALSE;

				*lpBytesReturned = sizeof(SERIAL_CHARS);
				return TRUE;
			}
		}
	}
	
	DEBUG_WARN(_T("unsupported IoControlCode: Ox%x (remote serial driver: %s)"), dwIoControlCode, pRemoteSerialDriver->name);
	return FALSE;
	
}


int _comm_ioctl_tcsetattr(int fd, int optional_actions, const struct termios *termios_p)
{
	int result;
	struct termios currentState;

	if ((result = tcsetattr(fd, optional_actions, termios_p)) < 0)
	{
		DEBUG_WARN("tcsetattr failure, errno: %d", errno);
		return result;
	}

	/* NB: tcsetattr() can succeed even if not all changes have been applied. */
	ZeroMemory(&currentState, sizeof(struct termios));
	if ((result = tcgetattr(fd, &currentState)) < 0)
	{
		DEBUG_WARN("tcgetattr failure, errno: %d", errno);
		return result;
	}

	if (memcmp(&currentState, &termios_p, sizeof(struct termios)) != 0)
	{
		DEBUG_WARN("all termios parameters were not set, doing a second attempt...");
		if ((result = tcsetattr(fd, optional_actions, termios_p)) < 0)
		{
			DEBUG_WARN("2nd tcsetattr failure, errno: %d", errno);
			return result;
		}

		ZeroMemory(&currentState, sizeof(struct termios));
		if ((result = tcgetattr(fd, &currentState)) < 0)
		{
			DEBUG_WARN("tcgetattr failure, errno: %d", errno);
			return result;
		}

		if (memcmp(&currentState, termios_p, sizeof(struct termios)) != 0)
		{
			DEBUG_WARN("Failure: all parameters were not set on a second attempt.");
			return -1; /* TMP: double-check whether some parameters can differ anyway */
		}
	}

	return 0;
}


#endif /* _WIN32 */
