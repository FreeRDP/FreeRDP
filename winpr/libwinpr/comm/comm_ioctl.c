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


const char* _comm_serial_ioctl_name(ULONG number)
{
	int i;

	for (i=0; _SERIAL_IOCTL_NAMES[i].number != 0; i++)
	{
		if (_SERIAL_IOCTL_NAMES[i].number == number)
		{
			return _SERIAL_IOCTL_NAMES[i].name;
		}
	}

	return "(unknown ioctl name)";
}


static BOOL _CommDeviceIoControl(HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize,
				LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped)
{
	WINPR_COMM* pComm = (WINPR_COMM*) hDevice;
	REMOTE_SERIAL_DRIVER* pRemoteSerialDriver = NULL;

	/* clear any previous last error */
	SetLastError(ERROR_SUCCESS);

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

	if (lpBytesReturned == NULL)
	{
		SetLastError(ERROR_INVALID_PARAMETER); /* since we doesn't suppport lpOverlapped != NULL */
		return FALSE;
	}

	*lpBytesReturned = 0; /* will be ajusted if required ... */

	DEBUG_MSG("CommDeviceIoControl: IoControlCode: 0x%0.8x", dwIoControlCode);

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
			DEBUG_MSG("Unknown remote serial driver (%d), using SerCx2.sys", pComm->remoteSerialDriverId);
			pRemoteSerialDriver = SerCx2Sys_s();
			break;
	}

	assert(pRemoteSerialDriver != NULL);

	switch (dwIoControlCode)
	{
		case IOCTL_USBPRINT_GET_1284_ID:
		{
			/* FIXME: http://msdn.microsoft.com/en-us/library/windows/hardware/ff551803(v=vs.85).aspx */
			*lpBytesReturned = nOutBufferSize; /* an empty OutputBuffer will be returned */
			SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
			return FALSE;
		}
		case IOCTL_SERIAL_SET_BAUD_RATE:
		{
			if (pRemoteSerialDriver->set_baud_rate)
			{
				SERIAL_BAUD_RATE *pBaudRate = (SERIAL_BAUD_RATE*)lpInBuffer;

				assert(nInBufferSize >= sizeof(SERIAL_BAUD_RATE));
				if (nInBufferSize < sizeof(SERIAL_BAUD_RATE))
				{
					SetLastError(ERROR_INVALID_PARAMETER);
					return FALSE;
				}

				return pRemoteSerialDriver->set_baud_rate(pComm, pBaudRate);
			}
			break;
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
			break;
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
			break;
		}
		case IOCTL_SERIAL_SET_CHARS:
		{
			if (pRemoteSerialDriver->set_serial_chars)
			{
				SERIAL_CHARS *pSerialChars = (SERIAL_CHARS*)lpInBuffer;

				assert(nInBufferSize >= sizeof(SERIAL_CHARS));
				if (nInBufferSize < sizeof(SERIAL_CHARS))
				{
					SetLastError(ERROR_INVALID_PARAMETER);
					return FALSE;
				}

				return pRemoteSerialDriver->set_serial_chars(pComm, pSerialChars);
			}
			break;
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
			break;
		}
		case IOCTL_SERIAL_SET_LINE_CONTROL:
		{
			if (pRemoteSerialDriver->set_line_control)
			{
				SERIAL_LINE_CONTROL *pLineControl = (SERIAL_LINE_CONTROL*)lpInBuffer;

				assert(nInBufferSize >= sizeof(SERIAL_LINE_CONTROL));
				if (nInBufferSize < sizeof(SERIAL_LINE_CONTROL))
				{
					SetLastError(ERROR_INVALID_PARAMETER);
					return FALSE;
				}

				return pRemoteSerialDriver->set_line_control(pComm, pLineControl);
			}
			break;
		}
		case IOCTL_SERIAL_GET_LINE_CONTROL:
		{
			if (pRemoteSerialDriver->get_line_control)
			{
				SERIAL_LINE_CONTROL *pLineControl = (SERIAL_LINE_CONTROL*)lpOutBuffer;

				assert(nOutBufferSize >= sizeof(SERIAL_LINE_CONTROL));
				if (nOutBufferSize < sizeof(SERIAL_LINE_CONTROL))
				{
					SetLastError(ERROR_INSUFFICIENT_BUFFER);
					return FALSE;
				}

				if (!pRemoteSerialDriver->get_line_control(pComm, pLineControl))
					return FALSE;

				*lpBytesReturned = sizeof(SERIAL_LINE_CONTROL);
				return TRUE;
			}
			break;
		}
		case IOCTL_SERIAL_SET_HANDFLOW:
		{
			if (pRemoteSerialDriver->set_handflow)
			{
				SERIAL_HANDFLOW *pHandflow = (SERIAL_HANDFLOW*)lpInBuffer;

				assert(nInBufferSize >= sizeof(SERIAL_HANDFLOW));
				if (nInBufferSize < sizeof(SERIAL_HANDFLOW))
				{
					SetLastError(ERROR_INVALID_PARAMETER);
					return FALSE;
				}

				return pRemoteSerialDriver->set_handflow(pComm, pHandflow);
			}
			break;
		}
		case IOCTL_SERIAL_GET_HANDFLOW:
		{
			if (pRemoteSerialDriver->get_handflow)
			{
				SERIAL_HANDFLOW *pHandflow = (SERIAL_HANDFLOW*)lpOutBuffer;

				assert(nOutBufferSize >= sizeof(SERIAL_HANDFLOW));
				if (nOutBufferSize < sizeof(SERIAL_HANDFLOW))
				{
					SetLastError(ERROR_INSUFFICIENT_BUFFER);
					return FALSE;
				}

				if (!pRemoteSerialDriver->get_handflow(pComm, pHandflow))
					return FALSE;

				*lpBytesReturned = sizeof(SERIAL_HANDFLOW);
				return TRUE;
			}
			break;
		}
		case IOCTL_SERIAL_SET_TIMEOUTS:
		{
			if (pRemoteSerialDriver->set_timeouts)
			{
				SERIAL_TIMEOUTS *pHandflow = (SERIAL_TIMEOUTS*)lpInBuffer;

				assert(nInBufferSize >= sizeof(SERIAL_TIMEOUTS));
				if (nInBufferSize < sizeof(SERIAL_TIMEOUTS))
				{
					SetLastError(ERROR_INVALID_PARAMETER);
					return FALSE;
				}

				return pRemoteSerialDriver->set_timeouts(pComm, pHandflow);
			}
			break;
		}
		case IOCTL_SERIAL_GET_TIMEOUTS:
		{
			if (pRemoteSerialDriver->get_timeouts)
			{
				SERIAL_TIMEOUTS *pHandflow = (SERIAL_TIMEOUTS*)lpOutBuffer;

				assert(nOutBufferSize >= sizeof(SERIAL_TIMEOUTS));
				if (nOutBufferSize < sizeof(SERIAL_TIMEOUTS))
				{
					SetLastError(ERROR_INSUFFICIENT_BUFFER);
					return FALSE;
				}

				if (!pRemoteSerialDriver->get_timeouts(pComm, pHandflow))
					return FALSE;

				*lpBytesReturned = sizeof(SERIAL_TIMEOUTS);
				return TRUE;
			}
			break;
		}
		case IOCTL_SERIAL_SET_DTR:
		{
			if (pRemoteSerialDriver->set_dtr)
			{
				return pRemoteSerialDriver->set_dtr(pComm);
			}
			break;
		}
		case IOCTL_SERIAL_CLR_DTR:
		{
			if (pRemoteSerialDriver->clear_dtr)
			{
				return pRemoteSerialDriver->clear_dtr(pComm);
			}
			break;
		}
		case IOCTL_SERIAL_SET_RTS:
		{
			if (pRemoteSerialDriver->set_rts)
			{
				return pRemoteSerialDriver->set_rts(pComm);
			}
			break;
		}
		case IOCTL_SERIAL_CLR_RTS:
		{
			if (pRemoteSerialDriver->clear_rts)
			{
				return pRemoteSerialDriver->clear_rts(pComm);
			}
			break;
		}
		case IOCTL_SERIAL_GET_MODEMSTATUS:
		{
			if (pRemoteSerialDriver->get_modemstatus)
			{
				ULONG *pRegister = (ULONG*)lpOutBuffer;

				assert(nOutBufferSize >= sizeof(ULONG));
				if (nOutBufferSize < sizeof(ULONG))
				{
					SetLastError(ERROR_INSUFFICIENT_BUFFER);
					return FALSE;
				}

				if (!pRemoteSerialDriver->get_modemstatus(pComm, pRegister))
					return FALSE;

				*lpBytesReturned = sizeof(ULONG);
				return TRUE;
			}
			break;
		}
		case IOCTL_SERIAL_SET_WAIT_MASK:
		{
			if (pRemoteSerialDriver->set_wait_mask)
			{
				ULONG *pWaitMask = (ULONG*)lpInBuffer;

				assert(nInBufferSize >= sizeof(ULONG));
				if (nInBufferSize < sizeof(ULONG))
				{
					SetLastError(ERROR_INVALID_PARAMETER);
					return FALSE;
				}

				return pRemoteSerialDriver->set_wait_mask(pComm, pWaitMask);
			}
			break;
		}
		case IOCTL_SERIAL_GET_WAIT_MASK:
		{
			if (pRemoteSerialDriver->get_wait_mask)
			{
				ULONG *pWaitMask = (ULONG*)lpOutBuffer;

				assert(nOutBufferSize >= sizeof(ULONG));
				if (nOutBufferSize < sizeof(ULONG))
				{
					SetLastError(ERROR_INSUFFICIENT_BUFFER);
					return FALSE;
				}

				if (!pRemoteSerialDriver->get_wait_mask(pComm, pWaitMask))
					return FALSE;

				*lpBytesReturned = sizeof(ULONG);
				return TRUE;
			}
			break;
		}
		case IOCTL_SERIAL_WAIT_ON_MASK:
		{
			if (pRemoteSerialDriver->wait_on_mask)
			{
				ULONG *pOutputMask = (ULONG*)lpOutBuffer;

				assert(nOutBufferSize >= sizeof(ULONG));
				if (nOutBufferSize < sizeof(ULONG))
				{
					SetLastError(ERROR_INSUFFICIENT_BUFFER);
					return FALSE;
				}

				if (!pRemoteSerialDriver->wait_on_mask(pComm, pOutputMask))
				{
					*lpBytesReturned = sizeof(ULONG); /* TMP: TODO: all lpBytesReturned values to be reviewed on error */
					return FALSE;
				}

				*lpBytesReturned = sizeof(ULONG);
				return TRUE;
			}
			break;
		}
		case IOCTL_SERIAL_SET_QUEUE_SIZE:
		{
			if (pRemoteSerialDriver->set_queue_size)
			{
				SERIAL_QUEUE_SIZE *pQueueSize = (SERIAL_QUEUE_SIZE*)lpInBuffer;

				assert(nInBufferSize >= sizeof(SERIAL_QUEUE_SIZE));
				if (nInBufferSize < sizeof(SERIAL_QUEUE_SIZE))
				{
					SetLastError(ERROR_INVALID_PARAMETER);
					return FALSE;
				}

				return pRemoteSerialDriver->set_queue_size(pComm, pQueueSize);
			}
			break;
		}
		case IOCTL_SERIAL_PURGE:
		{
			if (pRemoteSerialDriver->purge)
			{
				ULONG *pPurgeMask = (ULONG*)lpInBuffer;

				assert(nInBufferSize >= sizeof(ULONG));
				if (nInBufferSize < sizeof(ULONG))
				{
					SetLastError(ERROR_INVALID_PARAMETER);
					return FALSE;
				}

				return pRemoteSerialDriver->purge(pComm, pPurgeMask);
			}
			break;
		}
		case IOCTL_SERIAL_GET_COMMSTATUS:
		{
			if (pRemoteSerialDriver->get_commstatus)
			{
				SERIAL_STATUS *pCommstatus = (SERIAL_STATUS*)lpOutBuffer;

				assert(nOutBufferSize >= sizeof(SERIAL_STATUS));
				if (nOutBufferSize < sizeof(SERIAL_STATUS))
				{
					SetLastError(ERROR_INSUFFICIENT_BUFFER);
					return FALSE;
				}

				if (!pRemoteSerialDriver->get_commstatus(pComm, pCommstatus))
					return FALSE;

				*lpBytesReturned = sizeof(SERIAL_STATUS);
				return TRUE;
			}
			break;
		}
		case IOCTL_SERIAL_SET_BREAK_ON:
		{
			if (pRemoteSerialDriver->set_break_on)
			{
				return pRemoteSerialDriver->set_break_on(pComm);
			}
			break;
		}
		case IOCTL_SERIAL_SET_BREAK_OFF:
		{
			if (pRemoteSerialDriver->set_break_off)
			{
				return pRemoteSerialDriver->set_break_off(pComm);
			}
			break;
		}
		case IOCTL_SERIAL_SET_XOFF:
		{
			if (pRemoteSerialDriver->set_xoff)
			{
				return pRemoteSerialDriver->set_xoff(pComm);
			}
			break;
		}
		case IOCTL_SERIAL_SET_XON:
		{
			if (pRemoteSerialDriver->set_xon)
			{
				return pRemoteSerialDriver->set_xon(pComm);
			}
			break;
		}
		case IOCTL_SERIAL_GET_DTRRTS:
		{
			if (pRemoteSerialDriver->get_dtrrts)
			{
				ULONG *pMask = (ULONG*)lpOutBuffer;

				assert(nOutBufferSize >= sizeof(ULONG));
				if (nOutBufferSize < sizeof(ULONG))
				{
					SetLastError(ERROR_INSUFFICIENT_BUFFER);
					return FALSE;
				}

				if (!pRemoteSerialDriver->get_dtrrts(pComm, pMask))
					return FALSE;

				*lpBytesReturned = sizeof(ULONG);
				return TRUE;
			}
			break;

		}
		case IOCTL_SERIAL_CONFIG_SIZE:
		{
			if (pRemoteSerialDriver->config_size)
			{
				ULONG *pSize = (ULONG*)lpOutBuffer;

				assert(nOutBufferSize >= sizeof(ULONG));
				if (nOutBufferSize < sizeof(ULONG))
				{
					SetLastError(ERROR_INSUFFICIENT_BUFFER);
					return FALSE;
				}

				if (!pRemoteSerialDriver->config_size(pComm, pSize))
					return FALSE;

				*lpBytesReturned = sizeof(ULONG);
				return TRUE;
			}
			break;

		}
		case IOCTL_SERIAL_IMMEDIATE_CHAR:
		{
			if (pRemoteSerialDriver->immediate_char)
			{
				UCHAR *pChar = (UCHAR*)lpInBuffer;

				assert(nInBufferSize >= sizeof(UCHAR));
				if (nInBufferSize < sizeof(UCHAR))
				{
					SetLastError(ERROR_INVALID_PARAMETER);
					return FALSE;
				}

				return pRemoteSerialDriver->immediate_char(pComm, pChar);
			}
			break;
		}
	}

	DEBUG_WARN(_T("unsupported IoControlCode=[0x%lX] %s (remote serial driver: %s)"),
		dwIoControlCode, _comm_serial_ioctl_name(dwIoControlCode), pRemoteSerialDriver->name);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;

}


/**
 * FIXME: to be used through winpr-io's DeviceIoControl
 *
 * Any previous error as returned by GetLastError is cleared.
 *
 * ERRORS:
 *   ERROR_INVALID_HANDLE
 *   ERROR_INVALID_PARAMETER
 *   ERROR_NOT_SUPPORTED lpOverlapped is not supported
 *   ERROR_INSUFFICIENT_BUFFER
 *   ERROR_CALL_NOT_IMPLEMENTED unimplemented ioctl
 */
BOOL CommDeviceIoControl(HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize,
			LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped)
{
	WINPR_COMM* pComm = (WINPR_COMM*) hDevice;
	BOOL result;

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

	result = _CommDeviceIoControl(hDevice, dwIoControlCode, lpInBuffer, nInBufferSize,
				lpOutBuffer, nOutBufferSize, lpBytesReturned, lpOverlapped);

	if (pComm->permissive)
	{
		if (!result)
		{
			DEBUG_WARN("[permissive]: whereas it failed, made to succeed IoControlCode=[0x%lX] %s, last-error: 0x%lX",
				dwIoControlCode, _comm_serial_ioctl_name(dwIoControlCode), GetLastError());
		}

		return TRUE; /* always! */
	}

	return result;
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
		DEBUG_MSG("all termios parameters are not set yet, doing a second attempt...");
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
			DEBUG_WARN("Failure: all termios parameters are still not set on a second attempt");
			return -1; /* TMP: double-check whether some parameters can differ anyway */
		}
	}

	return 0;
}


#endif /* _WIN32 */
