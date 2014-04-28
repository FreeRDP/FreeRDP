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

#include <assert.h>
#include <termios.h>

#include <freerdp/utils/debug.h>

#include "comm_serial_sys.h"


/* 0: B* (Linux termios)
 * 1: CBR_* or actual baud rate
 * 2: BAUD_* (similar to SERIAL_BAUD_*)
 */
static const speed_t _SERCX_SYS_BAUD_TABLE[][3] = {
#ifdef B0
	{B0, 0, 0},	/* hang up */
#endif
#ifdef B50
	{B50, 50, 0},
#endif
#ifdef B75
	{B75, 75, BAUD_075},
#endif
#ifdef B110
	{B110, CBR_110, BAUD_110},
#endif
#ifdef  B134
	{B134, 134, 0 /*BAUD_134_5*/},
#endif
#ifdef  B150
	{B150, 150, BAUD_150},
#endif
#ifdef B200
	{B200, 200, 0},
#endif
#ifdef B300
	{B300, CBR_300, BAUD_300},
#endif
#ifdef B600
	{B600, CBR_600, BAUD_600},
#endif
#ifdef B1200
	{B1200, CBR_1200, BAUD_1200},
#endif
#ifdef B1800
	{B1800, 1800, BAUD_1800},
#endif
#ifdef B2400
	{B2400, CBR_2400, BAUD_2400},
#endif
#ifdef B4800
	{B4800, CBR_4800, BAUD_4800},
#endif
	/* {, ,BAUD_7200} */
#ifdef B9600
	{B9600, CBR_9600, BAUD_9600},
#endif
	/* {, CBR_14400, BAUD_14400},	/\* unsupported on Linux *\/ */
#ifdef B19200
	{B19200, CBR_19200, BAUD_19200},
#endif
#ifdef B38400
	{B38400, CBR_38400, BAUD_38400},
#endif
	/* {, CBR_56000, BAUD_56K},	/\* unsupported on Linux *\/ */
#ifdef  B57600
	{B57600, CBR_57600, BAUD_57600},
#endif
#ifdef B115200
	{B115200, CBR_115200, BAUD_115200},
#endif
	/* {, CBR_128000, BAUD_128K},	/\* unsupported on Linux *\/ */
	/* {, CBR_256000, BAUD_USER},	/\* unsupported on Linux *\/ */
#ifdef B230400
	{B230400, 230400, BAUD_USER},
#endif
#ifdef B460800
	{B460800, 460800, BAUD_USER},
#endif
#ifdef B500000
	{B500000, 500000, BAUD_USER},
#endif
#ifdef  B576000
	{B576000, 576000, BAUD_USER},
#endif
#ifdef B921600
	{B921600, 921600, BAUD_USER},
#endif
#ifdef B1000000
	{B1000000, 1000000, BAUD_USER},
#endif
#ifdef B1152000
	{B1152000, 1152000, BAUD_USER},
#endif
#ifdef B1500000
	{B1500000, 1500000, BAUD_USER},
#endif
#ifdef B2000000
	{B2000000, 2000000, BAUD_USER},
#endif
#ifdef B2500000
	{B2500000, 2500000, BAUD_USER},
#endif
#ifdef B3000000
	{B3000000, 3000000, BAUD_USER},
#endif
#ifdef B3500000
	{B3500000, 3500000, BAUD_USER},
#endif
#ifdef B4000000
	{B4000000, 4000000, BAUD_USER},	/* __MAX_BAUD */
#endif
};


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
	pProperties->dwMaxBaud = BAUD_USER;
	/* pProperties->ProvSubType; */
	/* pProperties->ProvCapabilities; */
	/* pProperties->SettableParams; */

	pProperties->dwSettableBaud = 0;
	for (i=0; _SERCX_SYS_BAUD_TABLE[i][1]<=__MAX_BAUD; i++)
	{
		pProperties->dwSettableBaud |= _SERCX_SYS_BAUD_TABLE[i][2];
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

	for (i=0; _SERCX_SYS_BAUD_TABLE[i][0]<=__MAX_BAUD; i++)
	{
		if (_SERCX_SYS_BAUD_TABLE[i][1] == pBaudRate->BaudRate)
		{
			newSpeed = _SERCX_SYS_BAUD_TABLE[i][0];	
			if (cfsetspeed(&futureState, newSpeed) < 0)
			{
				DEBUG_WARN("failed to set speed 0x%x (%d)", newSpeed, pBaudRate->BaudRate);
				return FALSE;
			}

			assert(cfgetispeed(&futureState) == newSpeed);

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

	for (i=0; _SERCX_SYS_BAUD_TABLE[i][0]<=__MAX_BAUD; i++)
	{
		if (_SERCX_SYS_BAUD_TABLE[i][0] == currentSpeed)
		{
			pBaudRate->BaudRate = _SERCX_SYS_BAUD_TABLE[i][1];
			return TRUE;
		}
	}

	DEBUG_WARN("could not find a matching baud rate for the speed 0x%x", currentSpeed);
	SetLastError(ERROR_INVALID_DATA);
	return FALSE;
}


/* specific functions only */
static REMOTE_SERIAL_DRIVER _SerCxSys = 
{
	.id		= RemoteSerialDriverSerCxSys,
	.name		= _T("SerCx.sys"),
	.set_baud_rate	= _set_baud_rate,
	.get_baud_rate  = _get_baud_rate,
	.get_properties = _get_properties,
};



REMOTE_SERIAL_DRIVER* SerCxSys_s()
{
	/* _SerCxSys completed with default SerialSys functions */
	//REMOTE_SERIAL_DRIVER* pSerialSys = SerialSys();

	return &_SerCxSys;
}

#endif /* _WIN32 */
