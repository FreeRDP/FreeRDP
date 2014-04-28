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

#ifndef _WIN32

#include <termios.h>

#include <freerdp/utils/debug.h>

#include "comm_serial_sys.h"

#include <winpr/crt.h>

/*
 * Linux, Windows speeds
 * 
 */
static const speed_t _SERIAL_SYS_BAUD_TABLE[][2] = {
#ifdef B0
	{B0, 0},	/* hang up */
#endif
/* #ifdef B50 */
/* 	{B50, },	/\* undefined by serial.sys *\/  */
/* #endif */
#ifdef B75
	{B75, SERIAL_BAUD_075},
#endif
#ifdef B110
	{B110, SERIAL_BAUD_110},
#endif
/* #ifdef  B134 */
/* 	{B134, SERIAL_BAUD_134_5}, /\* TODO: might be the same? *\/ */
/* #endif */
#ifdef  B150
	{B150, SERIAL_BAUD_150},
#endif
/* #ifdef B200 */
/* 	{B200, },	/\* undefined by serial.sys *\/ */
/* #endif */
#ifdef B300
	{B300, SERIAL_BAUD_300},
#endif
#ifdef B600
	{B600, SERIAL_BAUD_600},
#endif
#ifdef B1200
	{B1200, SERIAL_BAUD_1200},
#endif
#ifdef B1800
	{B1800, SERIAL_BAUD_1800},
#endif
#ifdef B2400
	{B2400, SERIAL_BAUD_2400},
#endif
#ifdef B4800
	{B4800, SERIAL_BAUD_4800},
#endif
	/* {, SERIAL_BAUD_7200} /\* undefined on Linux *\/ */
#ifdef B9600
	{B9600, SERIAL_BAUD_9600},
#endif
	/* {, SERIAL_BAUD_14400} /\* undefined on Linux *\/ */
#ifdef B19200
	{B19200, SERIAL_BAUD_19200},
#endif
#ifdef B38400
	{B38400, SERIAL_BAUD_38400},
#endif
/* 	{, SERIAL_BAUD_56K},	/\* undefined on Linux *\/ */
#ifdef  B57600
	{B57600, SERIAL_BAUD_57600},
#endif
	/* {, SERIAL_BAUD_128K} /\* undefined on Linux *\/ */
#ifdef B115200
	{B115200, SERIAL_BAUD_115200},	/* _SERIAL_MAX_BAUD */
#endif
/* undefined by serial.sys:
#ifdef B230400
	{B230400, },
#endif
#ifdef B460800
	{B460800, },
#endif
#ifdef B500000
	{B500000, },
#endif
#ifdef  B576000
	{B576000, },
#endif
#ifdef B921600
	{B921600, },
#endif
#ifdef B1000000
	{B1000000, },
#endif
#ifdef B1152000
	{B1152000, },
#endif
#ifdef B1500000
	{B1500000, },
#endif
#ifdef B2000000
	{B2000000, },
#endif
#ifdef B2500000
	{B2500000, },
#endif
#ifdef B3000000
	{B3000000, },
#endif
#ifdef B3500000
	{B3500000, },
#endif
#ifdef B4000000
	{B4000000, },	 __MAX_BAUD
#endif
*/
};

#define _SERIAL_MAX_BAUD  B115200

static BOOL _get_properties(WINPR_COMM *pComm, COMMPROP *pProperties)
{
	int i;

	// TMP: TODO:

	// TMP: required?
	// ZeroMemory(pProperties, sizeof(COMMPROP);

	/* pProperties->PacketLength; */
	/* pProperties->PacketVersion; */
	/* pProperties->ServiceMask; */
	/* pProperties->Reserved1; */
	/* pProperties->MaxTxQueue; */
	/* pProperties->MaxRxQueue; */
	pProperties->dwMaxBaud = SERIAL_BAUD_115200; /* _SERIAL_MAX_BAUD */
	/* pProperties->ProvSubType; */
	/* pProperties->ProvCapabilities; */
	/* pProperties->SettableParams; */

	pProperties->dwSettableBaud = 0;
	for (i=0; _SERIAL_SYS_BAUD_TABLE[i][0]<=_SERIAL_MAX_BAUD; i++)
	{
		pProperties->dwSettableBaud |= _SERIAL_SYS_BAUD_TABLE[i][1];
	}

	/* pProperties->SettableData; */
	/* pProperties->SettableStopParity; */
	/* pProperties->CurrentTxQueue; */
	/* pProperties->CurrentRxQueue; */
	/* pProperties->ProvSpec1; */
	/* pProperties->ProvSpec2; */
	/* pProperties->ProvChar[1]; */

	return TRUE;
}

static BOOL _set_baud_rate(WINPR_COMM *pComm, SERIAL_BAUD_RATE *pBaudRate)
{
	int i;
	speed_t newSpeed;
	struct termios futureState;

	ZeroMemory(&futureState, sizeof(struct termios));
	if (tcgetattr(pComm->fd, &futureState) < 0) /* NB: preserves current settings not directly handled by the Communication Functions */
	{
		SetLastError(ERROR_IO_DEVICE);
		return FALSE;
	}

	for (i=0; _SERIAL_SYS_BAUD_TABLE[i][0]<=_SERIAL_MAX_BAUD; i++)
	{
		if (_SERIAL_SYS_BAUD_TABLE[i][1] == pBaudRate->BaudRate)
		{
			newSpeed = _SERIAL_SYS_BAUD_TABLE[i][0];
			if (cfsetspeed(&futureState, newSpeed) < 0)
			{
				DEBUG_WARN("failed to set speed %d (%d)", newSpeed, pBaudRate->BaudRate);
				return FALSE;
			}

			if (_comm_ioctl_tcsetattr(pComm->fd, TCSANOW, &futureState) < 0)
			{
				DEBUG_WARN("_comm_ioctl_tcsetattr failure: last-error: 0x0.8x", GetLastError());
				return FALSE;
			}

			return TRUE;
		}
	}

	DEBUG_WARN("could not find a matching speed for the baud rate %d", pBaudRate->BaudRate);
	SetLastError(ERROR_INVALID_DATA);
	return FALSE;
}


static BOOL _get_baud_rate(WINPR_COMM *pComm, SERIAL_BAUD_RATE *pBaudRate)
{
	int i;
	speed_t currentSpeed;
	struct termios currentState;

	ZeroMemory(&currentState, sizeof(struct termios));
	if (tcgetattr(pComm->fd, &currentState) < 0)
	{
		SetLastError(ERROR_IO_DEVICE);
		return FALSE;
	}

	currentSpeed = cfgetispeed(&currentState);

	for (i=0; _SERIAL_SYS_BAUD_TABLE[i][0]<=_SERIAL_MAX_BAUD; i++)
	{
		if (_SERIAL_SYS_BAUD_TABLE[i][0] == currentSpeed)
		{
			pBaudRate->BaudRate = _SERIAL_SYS_BAUD_TABLE[i][1];
			return TRUE;
		}
	}

	DEBUG_WARN("could not find a matching baud rate for the speed 0x%x", currentSpeed);
	SetLastError(ERROR_INVALID_DATA);
	return FALSE;
}


static REMOTE_SERIAL_DRIVER _SerialSys = 
{
	.id		= RemoteSerialDriverSerialSys,
	.name		= _T("Serial.sys"),
	.set_baud_rate	= _set_baud_rate,
	.get_baud_rate	= _get_baud_rate,
	.get_properties = _get_properties,
};


REMOTE_SERIAL_DRIVER* SerialSys_s()
{
	return &_SerialSys;
}

#endif /* _WIN32 */
