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
	PREMOTE_SERIAL_DRIVER pRemoteSerialDriver = NULL;

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

	*lpBytesReturned = 0; /* will be ajusted otherwise */

	/* remoteSerialDriver to be use ... */
	switch (pComm->remoteSerialDriverId)
	{
		case RemoteSerialDriverSerialSys:
			pRemoteSerialDriver = SerialSys();
			break;

		case RemoteSerialDriverSerCxSys:
			pRemoteSerialDriver = SerCxSys();
			break;

		case RemoteSerialDriverSerCx2Sys:
			pRemoteSerialDriver = SerCx2Sys();
			break;

		case RemoteSerialDriverUnknown:
		default:
			DEBUG_WARN("Unknown remote serial driver (%d), using SerCx2.sys", pComm->remoteSerialDriverId);
			pRemoteSerialDriver = SerCx2Sys();
			break;
	}

	assert(pRemoteSerialDriver != NULL);

	switch (dwIoControlCode)
	{
		case IOCTL_SERIAL_SET_BAUD_RATE:
		{
			PSERIAL_BAUD_RATE pBaudRate = (PSERIAL_BAUD_RATE)lpInBuffer;

			assert(nInBufferSize == sizeof(SERIAL_BAUD_RATE));
			if (nInBufferSize < sizeof(SERIAL_BAUD_RATE))
			{
				SetLastError(ERROR_INVALID_DATA);
				return FALSE;
			}
		       
			if (pRemoteSerialDriver->set_baud_rate)
			{
				return pRemoteSerialDriver->set_baud_rate(pBaudRate);
			}
			else
			{
				DEBUG_WARN(_T("unsupported IoControlCode: Ox%x (remote serial driver: %s)"), dwIoControlCode, pRemoteSerialDriver->name);
				return FALSE;
			}
		}
		default:
			DEBUG_WARN("unsupported IoControlCode: Ox%x", dwIoControlCode);
			return FALSE;
	}
	


/* IOCTL_SERIAL_GET_BAUD_RATE 0x001B0050 */
/* IOCTL_SERIAL_SET_LINE_CONTROL 0x001B000C */
/* IOCTL_SERIAL_GET_LINE_CONTROL 0x001B0054 */
/* IOCTL_SERIAL_SET_TIMEOUTS 0x001B001C */
/* IOCTL_SERIAL_GET_TIMEOUTS 0x001B0020 */
/* IOCTL_SERIAL_SET_CHARS 0x001B0058 */
/* IOCTL_SERIAL_GET_CHARS 0x001B005C */
/* IOCTL_SERIAL_SET_DTR 0x001B0024 */
/* IOCTL_SERIAL_CLR_DTR 0x001B0028 */
/* IOCTL_SERIAL_RESET_DEVICE 0x001B002C */
/* IOCTL_SERIAL_SET_RTS 0x001B0030 */
/* IOCTL_SERIAL_CLR_RTS 0x001B0034 */
/* IOCTL_SERIAL_SET_XOFF 0x001B0038 */
/* IOCTL_SERIAL_SET_XON 0x001B003C */
/* IOCTL_SERIAL_SET_BREAK_ON 0x001B0010 */
/* IOCTL_SERIAL_SET_BREAK_OFF 0x001B0014 */
/* IOCTL_SERIAL_SET_QUEUE_SIZE 0x001B0008 */
/* IOCTL_SERIAL_GET_WAIT_MASK 0x001B0040 */
/* IOCTL_SERIAL_SET_WAIT_MASK 0x001B0044 */
/* IOCTL_SERIAL_WAIT_ON_MASK 0x001B0048 */
/* IOCTL_SERIAL_IMMEDIATE_CHAR 0x001B0018 */
/* IOCTL_SERIAL_PURGE 0x001B004C */
/* IOCTL_SERIAL_GET_HANDFLOW 0x001B0060 */
/* IOCTL_SERIAL_SET_HANDFLOW 0x001B0064 */
/* IOCTL_SERIAL_GET_MODEMSTATUS 0x001B0068 */
/* IOCTL_SERIAL_GET_DTRRTS 0x001B0078 */
/* IOCTL_SERIAL_GET_COMMSTATUS 0x001B0084 */
/* IOCTL_SERIAL_GET_PROPERTIES 0x001B0074 */
/* IOCTL_SERIAL_XOFF_COUNTER 0x001B0070 */
/* IOCTL_SERIAL_LSRMST_INSERT 0x001B007C */
/* IOCTL_SERIAL_CONFIG_SIZE 0x001B0080 */
/* IOCTL_SERIAL_GET_STATS 0x001B008C */
/* IOCTL_SERIAL_CLEAR_STATS 0x001B0090 */
/* IOCTL_SERIAL_GET_MODEM_CONTROL 0x001B0094 */
/* IOCTL_SERIAL_SET_MODEM_CONTROL 0x001B0098 */
/* IOCTL_SERIAL_SET_FIFO_CONTROL 0x001B009C */

/* IOCTL_PAR_QUERY_INFORMATION 0x00160004 */
/* IOCTL_PAR_SET_INFORMATION 0x00160008 */
/* IOCTL_PAR_QUERY_DEVICE_ID 0x0016000C */
/* IOCTL_PAR_QUERY_DEVICE_ID_SIZE 0x00160010 */
/* IOCTL_IEEE1284_GET_MODE 0x00160014 */
/* IOCTL_IEEE1284_NEGOTIATE 0x00160018 */
/* IOCTL_PAR_SET_WRITE_ADDRESS 0x0016001C */
/* IOCTL_PAR_SET_READ_ADDRESS 0x00160020 */
/* IOCTL_PAR_GET_DEVICE_CAPS 0x00160024 */
/* IOCTL_PAR_GET_DEFAULT_MODES 0x00160028 */
/* IOCTL_PAR_QUERY_RAW_DEVICE_ID 0x00160030 */
/* IOCTL_PAR_IS_PORT_FREE 0x00160054 */


		return TRUE;
	}

#endif /* _WIN32 */
