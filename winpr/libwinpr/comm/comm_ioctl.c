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

#include <winpr/config.h>

#include <winpr/assert.h>
#include <errno.h>

#include <winpr/wlog.h>

#include "comm.h"
#include "comm_ioctl.h"
#include "comm_serial_sys.h"
#include "comm_sercx_sys.h"
#include "comm_sercx2_sys.h"

static const char* comm_ioctl_modem_status_string(ULONG status, char* buffer, size_t size);

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
static BOOL s_CommDeviceIoControl(HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer,
                                  DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize,
                                  LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped)
{
	char buffer[128] = { 0 };
	WINPR_COMM* pComm = (WINPR_COMM*)hDevice;
	const SERIAL_DRIVER* pServerSerialDriver = NULL;

	if (!CommIsHandleValid(hDevice))
		return FALSE;

	if (lpOverlapped)
	{
		SetLastError(ERROR_NOT_SUPPORTED);
		return FALSE;
	}

	if (lpBytesReturned == NULL)
	{
		SetLastError(ERROR_INVALID_PARAMETER); /* since we doesn't support lpOverlapped != NULL */
		return FALSE;
	}

	/* clear any previous last error */
	SetLastError(ERROR_SUCCESS);

	*lpBytesReturned = 0; /* will be adjusted if required ... */

	CommLog_Print(WLOG_DEBUG, "CommDeviceIoControl: IoControlCode: %s [0x%08" PRIx32 "]",
	              _comm_serial_ioctl_name(dwIoControlCode), dwIoControlCode);

	/* remoteSerialDriver to be use ...
	 *
	 * FIXME: might prefer to use an automatic rather than static structure
	 */
	switch (pComm->serverSerialDriverId)
	{
		case SerialDriverSerialSys:
			pServerSerialDriver = SerialSys_s();
			break;

		case SerialDriverSerCxSys:
			pServerSerialDriver = SerCxSys_s();
			break;

		case SerialDriverSerCx2Sys:
			pServerSerialDriver = SerCx2Sys_s();
			break;

		case SerialDriverUnknown:
		default:
			CommLog_Print(WLOG_DEBUG, "Unknown remote serial driver (%u), using SerCx2.sys",
			              pComm->serverSerialDriverId);
			pServerSerialDriver = SerCx2Sys_s();
			break;
	}

	WINPR_ASSERT(pServerSerialDriver != NULL);

	switch (dwIoControlCode)
	{
		case IOCTL_USBPRINT_GET_1284_ID:
		{
			/* FIXME:
			 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff551803(v=vs.85).aspx */
			*lpBytesReturned = nOutBufferSize; /* an empty OutputBuffer will be returned */
			SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
			return FALSE;
		}
		case IOCTL_SERIAL_SET_BAUD_RATE:
		{
			if (pServerSerialDriver->set_baud_rate)
			{
				SERIAL_BAUD_RATE* pBaudRate = (SERIAL_BAUD_RATE*)lpInBuffer;

				WINPR_ASSERT(nInBufferSize >= sizeof(SERIAL_BAUD_RATE));
				if (nInBufferSize < sizeof(SERIAL_BAUD_RATE))
				{
					SetLastError(ERROR_INVALID_PARAMETER);
					return FALSE;
				}

				return pServerSerialDriver->set_baud_rate(pComm, pBaudRate);
			}
			break;
		}
		case IOCTL_SERIAL_GET_BAUD_RATE:
		{
			if (pServerSerialDriver->get_baud_rate)
			{
				SERIAL_BAUD_RATE* pBaudRate = (SERIAL_BAUD_RATE*)lpOutBuffer;

				WINPR_ASSERT(nOutBufferSize >= sizeof(SERIAL_BAUD_RATE));
				if (nOutBufferSize < sizeof(SERIAL_BAUD_RATE))
				{
					SetLastError(ERROR_INSUFFICIENT_BUFFER);
					return FALSE;
				}

				if (!pServerSerialDriver->get_baud_rate(pComm, pBaudRate))
					return FALSE;

				*lpBytesReturned = sizeof(SERIAL_BAUD_RATE);
				return TRUE;
			}
			break;
		}
		case IOCTL_SERIAL_GET_PROPERTIES:
		{
			if (pServerSerialDriver->get_properties)
			{
				COMMPROP* pProperties = (COMMPROP*)lpOutBuffer;

				WINPR_ASSERT(nOutBufferSize >= sizeof(COMMPROP));
				if (nOutBufferSize < sizeof(COMMPROP))
				{
					SetLastError(ERROR_INSUFFICIENT_BUFFER);
					return FALSE;
				}

				if (!pServerSerialDriver->get_properties(pComm, pProperties))
					return FALSE;

				*lpBytesReturned = sizeof(COMMPROP);
				return TRUE;
			}
			break;
		}
		case IOCTL_SERIAL_SET_CHARS:
		{
			if (pServerSerialDriver->set_serial_chars)
			{
				SERIAL_CHARS* pSerialChars = (SERIAL_CHARS*)lpInBuffer;

				WINPR_ASSERT(nInBufferSize >= sizeof(SERIAL_CHARS));
				if (nInBufferSize < sizeof(SERIAL_CHARS))
				{
					SetLastError(ERROR_INVALID_PARAMETER);
					return FALSE;
				}

				return pServerSerialDriver->set_serial_chars(pComm, pSerialChars);
			}
			break;
		}
		case IOCTL_SERIAL_GET_CHARS:
		{
			if (pServerSerialDriver->get_serial_chars)
			{
				SERIAL_CHARS* pSerialChars = (SERIAL_CHARS*)lpOutBuffer;

				WINPR_ASSERT(nOutBufferSize >= sizeof(SERIAL_CHARS));
				if (nOutBufferSize < sizeof(SERIAL_CHARS))
				{
					SetLastError(ERROR_INSUFFICIENT_BUFFER);
					return FALSE;
				}

				if (!pServerSerialDriver->get_serial_chars(pComm, pSerialChars))
					return FALSE;

				*lpBytesReturned = sizeof(SERIAL_CHARS);
				return TRUE;
			}
			break;
		}
		case IOCTL_SERIAL_SET_LINE_CONTROL:
		{
			if (pServerSerialDriver->set_line_control)
			{
				SERIAL_LINE_CONTROL* pLineControl = (SERIAL_LINE_CONTROL*)lpInBuffer;

				WINPR_ASSERT(nInBufferSize >= sizeof(SERIAL_LINE_CONTROL));
				if (nInBufferSize < sizeof(SERIAL_LINE_CONTROL))
				{
					SetLastError(ERROR_INVALID_PARAMETER);
					return FALSE;
				}

				return pServerSerialDriver->set_line_control(pComm, pLineControl);
			}
			break;
		}
		case IOCTL_SERIAL_GET_LINE_CONTROL:
		{
			if (pServerSerialDriver->get_line_control)
			{
				SERIAL_LINE_CONTROL* pLineControl = (SERIAL_LINE_CONTROL*)lpOutBuffer;

				WINPR_ASSERT(nOutBufferSize >= sizeof(SERIAL_LINE_CONTROL));
				if (nOutBufferSize < sizeof(SERIAL_LINE_CONTROL))
				{
					SetLastError(ERROR_INSUFFICIENT_BUFFER);
					return FALSE;
				}

				if (!pServerSerialDriver->get_line_control(pComm, pLineControl))
					return FALSE;

				*lpBytesReturned = sizeof(SERIAL_LINE_CONTROL);
				return TRUE;
			}
			break;
		}
		case IOCTL_SERIAL_SET_HANDFLOW:
		{
			if (pServerSerialDriver->set_handflow)
			{
				SERIAL_HANDFLOW* pHandflow = (SERIAL_HANDFLOW*)lpInBuffer;

				WINPR_ASSERT(nInBufferSize >= sizeof(SERIAL_HANDFLOW));
				if (nInBufferSize < sizeof(SERIAL_HANDFLOW))
				{
					SetLastError(ERROR_INVALID_PARAMETER);
					return FALSE;
				}

				return pServerSerialDriver->set_handflow(pComm, pHandflow);
			}
			break;
		}
		case IOCTL_SERIAL_GET_HANDFLOW:
		{
			if (pServerSerialDriver->get_handflow)
			{
				SERIAL_HANDFLOW* pHandflow = (SERIAL_HANDFLOW*)lpOutBuffer;

				WINPR_ASSERT(nOutBufferSize >= sizeof(SERIAL_HANDFLOW));
				if (nOutBufferSize < sizeof(SERIAL_HANDFLOW))
				{
					SetLastError(ERROR_INSUFFICIENT_BUFFER);
					return FALSE;
				}

				if (!pServerSerialDriver->get_handflow(pComm, pHandflow))
					return FALSE;

				*lpBytesReturned = sizeof(SERIAL_HANDFLOW);
				return TRUE;
			}
			break;
		}
		case IOCTL_SERIAL_SET_TIMEOUTS:
		{
			if (pServerSerialDriver->set_timeouts)
			{
				SERIAL_TIMEOUTS* pHandflow = (SERIAL_TIMEOUTS*)lpInBuffer;

				WINPR_ASSERT(nInBufferSize >= sizeof(SERIAL_TIMEOUTS));
				if (nInBufferSize < sizeof(SERIAL_TIMEOUTS))
				{
					SetLastError(ERROR_INVALID_PARAMETER);
					return FALSE;
				}

				return pServerSerialDriver->set_timeouts(pComm, pHandflow);
			}
			break;
		}
		case IOCTL_SERIAL_GET_TIMEOUTS:
		{
			if (pServerSerialDriver->get_timeouts)
			{
				SERIAL_TIMEOUTS* pHandflow = (SERIAL_TIMEOUTS*)lpOutBuffer;

				WINPR_ASSERT(nOutBufferSize >= sizeof(SERIAL_TIMEOUTS));
				if (nOutBufferSize < sizeof(SERIAL_TIMEOUTS))
				{
					SetLastError(ERROR_INSUFFICIENT_BUFFER);
					return FALSE;
				}

				if (!pServerSerialDriver->get_timeouts(pComm, pHandflow))
					return FALSE;

				*lpBytesReturned = sizeof(SERIAL_TIMEOUTS);
				return TRUE;
			}
			break;
		}
		case IOCTL_SERIAL_SET_DTR:
		{
			if (pServerSerialDriver->set_dtr)
			{
				return pServerSerialDriver->set_dtr(pComm);
			}
			break;
		}
		case IOCTL_SERIAL_CLR_DTR:
		{
			if (pServerSerialDriver->clear_dtr)
			{
				return pServerSerialDriver->clear_dtr(pComm);
			}
			break;
		}
		case IOCTL_SERIAL_SET_RTS:
		{
			if (pServerSerialDriver->set_rts)
			{
				return pServerSerialDriver->set_rts(pComm);
			}
			break;
		}
		case IOCTL_SERIAL_CLR_RTS:
		{
			if (pServerSerialDriver->clear_rts)
			{
				return pServerSerialDriver->clear_rts(pComm);
			}
			break;
		}
		case IOCTL_SERIAL_GET_MODEMSTATUS:
		{
			if (pServerSerialDriver->get_modemstatus)
			{
				ULONG* pRegister = (ULONG*)lpOutBuffer;

				WINPR_ASSERT(nOutBufferSize >= sizeof(ULONG));
				if (nOutBufferSize < sizeof(ULONG))
				{
					SetLastError(ERROR_INSUFFICIENT_BUFFER);
					return FALSE;
				}

				if (!pServerSerialDriver->get_modemstatus(pComm, pRegister))
					return FALSE;

				CommLog_Print(WLOG_DEBUG, "modem status %s" PRIx32,
				              comm_ioctl_modem_status_string(*pRegister, buffer, sizeof(buffer)));
				*lpBytesReturned = sizeof(ULONG);
				return TRUE;
			}
			break;
		}
		case IOCTL_SERIAL_SET_WAIT_MASK:
		{
			if (pServerSerialDriver->set_wait_mask)
			{
				ULONG* pWaitMask = (ULONG*)lpInBuffer;

				WINPR_ASSERT(nInBufferSize >= sizeof(ULONG));
				if (nInBufferSize < sizeof(ULONG))
				{
					SetLastError(ERROR_INVALID_PARAMETER);
					return FALSE;
				}

				const BOOL rc = pServerSerialDriver->set_wait_mask(pComm, pWaitMask);
				CommLog_Print(WLOG_DEBUG, "set_wait_mask %s -> %d",
				              CommSerialEvString(*pWaitMask, buffer, sizeof(buffer)), rc);
				return rc;
			}
			break;
		}
		case IOCTL_SERIAL_GET_WAIT_MASK:
		{
			if (pServerSerialDriver->get_wait_mask)
			{
				ULONG* pWaitMask = (ULONG*)lpOutBuffer;

				WINPR_ASSERT(nOutBufferSize >= sizeof(ULONG));
				if (nOutBufferSize < sizeof(ULONG))
				{
					SetLastError(ERROR_INSUFFICIENT_BUFFER);
					return FALSE;
				}

				if (!pServerSerialDriver->get_wait_mask(pComm, pWaitMask))
					return FALSE;

				CommLog_Print(WLOG_DEBUG, "get_wait_mask %s",
				              CommSerialEvString(*pWaitMask, buffer, sizeof(buffer)));
				*lpBytesReturned = sizeof(ULONG);
				return TRUE;
			}
			break;
		}
		case IOCTL_SERIAL_WAIT_ON_MASK:
		{
			if (pServerSerialDriver->wait_on_mask)
			{
				ULONG* pOutputMask = (ULONG*)lpOutBuffer;

				WINPR_ASSERT(nOutBufferSize >= sizeof(ULONG));
				if (nOutBufferSize < sizeof(ULONG))
				{
					SetLastError(ERROR_INSUFFICIENT_BUFFER);
					return FALSE;
				}

				const BOOL rc = pServerSerialDriver->wait_on_mask(pComm, pOutputMask);

				*lpBytesReturned = sizeof(ULONG);
				CommLog_Print(WLOG_DEBUG, "wait_on_mask %s -> %d",
				              CommSerialEvString(*pOutputMask, buffer, sizeof(buffer)), rc);
				return rc;
			}
			break;
		}
		case IOCTL_SERIAL_SET_QUEUE_SIZE:
		{
			if (pServerSerialDriver->set_queue_size)
			{
				SERIAL_QUEUE_SIZE* pQueueSize = (SERIAL_QUEUE_SIZE*)lpInBuffer;

				WINPR_ASSERT(nInBufferSize >= sizeof(SERIAL_QUEUE_SIZE));
				if (nInBufferSize < sizeof(SERIAL_QUEUE_SIZE))
				{
					SetLastError(ERROR_INVALID_PARAMETER);
					return FALSE;
				}

				return pServerSerialDriver->set_queue_size(pComm, pQueueSize);
			}
			break;
		}
		case IOCTL_SERIAL_PURGE:
		{
			if (pServerSerialDriver->purge)
			{
				ULONG* pPurgeMask = (ULONG*)lpInBuffer;

				WINPR_ASSERT(nInBufferSize >= sizeof(ULONG));
				if (nInBufferSize < sizeof(ULONG))
				{
					SetLastError(ERROR_INVALID_PARAMETER);
					return FALSE;
				}

				return pServerSerialDriver->purge(pComm, pPurgeMask);
			}
			break;
		}
		case IOCTL_SERIAL_GET_COMMSTATUS:
		{
			if (pServerSerialDriver->get_commstatus)
			{
				SERIAL_STATUS* pCommstatus = (SERIAL_STATUS*)lpOutBuffer;

				WINPR_ASSERT(nOutBufferSize >= sizeof(SERIAL_STATUS));
				if (nOutBufferSize < sizeof(SERIAL_STATUS))
				{
					SetLastError(ERROR_INSUFFICIENT_BUFFER);
					return FALSE;
				}

				if (!pServerSerialDriver->get_commstatus(pComm, pCommstatus))
					return FALSE;

				*lpBytesReturned = sizeof(SERIAL_STATUS);
				return TRUE;
			}
			break;
		}
		case IOCTL_SERIAL_SET_BREAK_ON:
		{
			if (pServerSerialDriver->set_break_on)
			{
				return pServerSerialDriver->set_break_on(pComm);
			}
			break;
		}
		case IOCTL_SERIAL_SET_BREAK_OFF:
		{
			if (pServerSerialDriver->set_break_off)
			{
				return pServerSerialDriver->set_break_off(pComm);
			}
			break;
		}
		case IOCTL_SERIAL_SET_XOFF:
		{
			if (pServerSerialDriver->set_xoff)
			{
				return pServerSerialDriver->set_xoff(pComm);
			}
			break;
		}
		case IOCTL_SERIAL_SET_XON:
		{
			if (pServerSerialDriver->set_xon)
			{
				return pServerSerialDriver->set_xon(pComm);
			}
			break;
		}
		case IOCTL_SERIAL_GET_DTRRTS:
		{
			if (pServerSerialDriver->get_dtrrts)
			{
				ULONG* pMask = (ULONG*)lpOutBuffer;

				WINPR_ASSERT(nOutBufferSize >= sizeof(ULONG));
				if (nOutBufferSize < sizeof(ULONG))
				{
					SetLastError(ERROR_INSUFFICIENT_BUFFER);
					return FALSE;
				}

				if (!pServerSerialDriver->get_dtrrts(pComm, pMask))
					return FALSE;

				*lpBytesReturned = sizeof(ULONG);
				return TRUE;
			}
			break;
		}
		case IOCTL_SERIAL_CONFIG_SIZE:
		{
			if (pServerSerialDriver->config_size)
			{
				ULONG* pSize = (ULONG*)lpOutBuffer;

				WINPR_ASSERT(nOutBufferSize >= sizeof(ULONG));
				if (nOutBufferSize < sizeof(ULONG))
				{
					SetLastError(ERROR_INSUFFICIENT_BUFFER);
					return FALSE;
				}

				if (!pServerSerialDriver->config_size(pComm, pSize))
					return FALSE;

				*lpBytesReturned = sizeof(ULONG);
				return TRUE;
			}
			break;
		}
		case IOCTL_SERIAL_IMMEDIATE_CHAR:
		{
			if (pServerSerialDriver->immediate_char)
			{
				UCHAR* pChar = (UCHAR*)lpInBuffer;

				WINPR_ASSERT(nInBufferSize >= sizeof(UCHAR));
				if (nInBufferSize < sizeof(UCHAR))
				{
					SetLastError(ERROR_INVALID_PARAMETER);
					return FALSE;
				}

				return pServerSerialDriver->immediate_char(pComm, pChar);
			}
			break;
		}
		case IOCTL_SERIAL_RESET_DEVICE:
		{
			if (pServerSerialDriver->reset_device)
			{
				return pServerSerialDriver->reset_device(pComm);
			}
			break;
		}
		default:
			break;
	}

	CommLog_Print(
	    WLOG_WARN, _T("unsupported IoControlCode=[0x%08" PRIX32 "] %s (remote serial driver: %s)"),
	    dwIoControlCode, _comm_serial_ioctl_name(dwIoControlCode), pServerSerialDriver->name);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED); /* => STATUS_NOT_IMPLEMENTED */
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
BOOL CommDeviceIoControl(HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer,
                         DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize,
                         LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped)
{
	WINPR_COMM* pComm = (WINPR_COMM*)hDevice;
	BOOL result = 0;

	if (hDevice == INVALID_HANDLE_VALUE)
	{
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}

	if (!CommIsHandled(hDevice))
		return FALSE;

	if (!pComm->fd)
	{
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}

	result = s_CommDeviceIoControl(hDevice, dwIoControlCode, lpInBuffer, nInBufferSize, lpOutBuffer,
	                               nOutBufferSize, lpBytesReturned, lpOverlapped);

	if (lpBytesReturned && *lpBytesReturned != nOutBufferSize)
	{
		/* This might be a hint for a bug, especially when result==TRUE */
		CommLog_Print(WLOG_WARN,
		              "IoControlCode=[0x%08" PRIX32 "] %s: lpBytesReturned=%" PRIu32
		              " and nOutBufferSize=%" PRIu32 " are different!",
		              dwIoControlCode, _comm_serial_ioctl_name(dwIoControlCode), *lpBytesReturned,
		              nOutBufferSize);
	}

	if (pComm->permissive)
	{
		if (!result)
		{
			CommLog_Print(WLOG_WARN,
			              "[permissive]: IoControlCode=[0x%08" PRIX32 "] %s failed, ignoring",
			              dwIoControlCode, _comm_serial_ioctl_name(dwIoControlCode));
		}

		return TRUE; /* always! */
	}

	return result;
}

int comm_ioctl_tcsetattr(int fd, int optional_actions, const struct termios* termios_p)
{
	struct termios currentState = { 0 };
	size_t count = 0;
	do
	{
		const int src = tcsetattr(fd, optional_actions, termios_p);
		if (src < 0)
		{
			char buffer[64] = { 0 };
			CommLog_Print(WLOG_WARN, "[%" PRIuz "] tcsetattr failure, errno: %s [%d]", count,
			              winpr_strerror(errno, buffer, sizeof(buffer)), errno);
			return src;
		}

		/* NB: tcsetattr() can succeed even if not all changes have been applied. */
		const int rrc = tcgetattr(fd, &currentState);
		if (rrc < 0)
		{
			char buffer[64] = { 0 };
			CommLog_Print(WLOG_WARN, "[%" PRIuz "] tcgetattr failure, errno: %s [%d]", count,
			              winpr_strerror(errno, buffer, sizeof(buffer)), errno);
			return rrc;
		}
		// NOLINTNEXTLINE(bugprone-suspicious-memory-comparison,cert-exp42-c,cert-flp37-c)
	} while ((memcmp(&currentState, termios_p, sizeof(struct termios)) != 0) && (count++ < 2));

	return 0;
}

static const char* comm_ioctl_modem_flag_str(ULONG flag)
{
	switch (flag)
	{
		case SERIAL_MSR_DCTS:
			return "SERIAL_MSR_DCTS";
		case SERIAL_MSR_DDSR:
			return "SERIAL_MSR_DDSR";
		case SERIAL_MSR_TERI:
			return "SERIAL_MSR_TERI";
		case SERIAL_MSR_DDCD:
			return "SERIAL_MSR_DDCD";
		case SERIAL_MSR_CTS:
			return "SERIAL_MSR_CTS";
		case SERIAL_MSR_DSR:
			return "SERIAL_MSR_DSR";
		case SERIAL_MSR_RI:
			return "SERIAL_MSR_RI";
		case SERIAL_MSR_DCD:
			return "SERIAL_MSR_DCD";
		default:
			return "SERIAL_MSR_UNKNOWN";
	}
}

const char* comm_ioctl_modem_status_string(ULONG status, char* buffer, size_t size)
{
	const ULONG flags[] = { SERIAL_MSR_DCTS, SERIAL_MSR_DDSR, SERIAL_MSR_TERI, SERIAL_MSR_DDCD,
		                    SERIAL_MSR_CTS,  SERIAL_MSR_DSR,  SERIAL_MSR_RI,   SERIAL_MSR_DCD

	};
	winpr_str_append("{", buffer, size, "");

	const char* sep = "";
	for (size_t x = 0; x < ARRAYSIZE(flags); x++)
	{
		const ULONG flag = flags[x];
		if (status & flag)
		{
			winpr_str_append(comm_ioctl_modem_flag_str(flag), buffer, size, sep);
			sep = "|";
		}
	}

	char number[32] = { 0 };
	(void)_snprintf(number, sizeof(number), "}[0x%08" PRIx32 "]", status);
	winpr_str_append(number, buffer, size, "");
	return buffer;
}
