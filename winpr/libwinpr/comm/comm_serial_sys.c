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

#if defined __linux__ && !defined ANDROID

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include "comm_serial_sys.h"
#ifdef __UCLIBC__
#include "comm.h"
#endif

#include <winpr/crt.h>
#include <winpr/wlog.h>

/* hard-coded in N_TTY */
#define TTY_THRESHOLD_THROTTLE		128 /* now based on remaining room */
#define TTY_THRESHOLD_UNTHROTTLE 	128
#define N_TTY_BUF_SIZE			4096


#define _BAUD_TABLE_END	0010020	/* __MAX_BAUD + 1 */

/* 0: B* (Linux termios)
 * 1: CBR_* or actual baud rate
 * 2: BAUD_* (identical to SERIAL_BAUD_*)
 */
static const speed_t _BAUD_TABLE[][3] = {
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
	{_BAUD_TABLE_END, 0, 0}
};


static BOOL _get_properties(WINPR_COMM *pComm, COMMPROP *pProperties)
{
	int i;

	/* http://msdn.microsoft.com/en-us/library/windows/hardware/jj680684%28v=vs.85%29.aspx
	 * http://msdn.microsoft.com/en-us/library/windows/desktop/aa363189%28v=vs.85%29.aspx
	 */

	/* FIXME: properties should be better probed. The current
	 * implementation just relies on the Linux' implementation.
	 */

	if (pProperties->dwProvSpec1 != COMMPROP_INITIALIZED)
	{
		ZeroMemory(pProperties, sizeof(COMMPROP));
		pProperties->wPacketLength = sizeof(COMMPROP);
	}

	pProperties->wPacketVersion = 2;

	pProperties->dwServiceMask = SERIAL_SP_SERIALCOMM;

	/* pProperties->Reserved1; not used */

	/* FIXME: could be implemented on top of N_TTY */
	pProperties->dwMaxTxQueue = N_TTY_BUF_SIZE;
	pProperties->dwMaxRxQueue = N_TTY_BUF_SIZE;

	/* FIXME: to be probe on the device? */
	pProperties->dwMaxBaud = BAUD_USER; 

	/* FIXME: what about PST_RS232? see also: serial_struct */
	pProperties->dwProvSubType = PST_UNSPECIFIED;

	/* TODO: to be finalized */
	pProperties->dwProvCapabilities =
		/*PCF_16BITMODE |*/ PCF_DTRDSR | PCF_INTTIMEOUTS | PCF_PARITY_CHECK | /*PCF_RLSD |*/
		PCF_RTSCTS | PCF_SETXCHAR | /*PCF_SPECIALCHARS |*/ PCF_TOTALTIMEOUTS | PCF_XONXOFF;

	/* TODO: double check SP_RLSD */
	pProperties->dwSettableParams = SP_BAUD | SP_DATABITS | SP_HANDSHAKING | SP_PARITY | SP_PARITY_CHECK | /*SP_RLSD |*/ SP_STOPBITS;

	pProperties->dwSettableBaud = 0;
	for (i=0; _BAUD_TABLE[i][0]<_BAUD_TABLE_END; i++)
	{
		pProperties->dwSettableBaud |= _BAUD_TABLE[i][2];
	}

	pProperties->wSettableData = DATABITS_5 | DATABITS_6 | DATABITS_7 | DATABITS_8 /*| DATABITS_16 | DATABITS_16X*/;

	pProperties->wSettableStopParity = STOPBITS_10 | /*STOPBITS_15 |*/ STOPBITS_20 | PARITY_NONE | PARITY_ODD | PARITY_EVEN | PARITY_MARK | PARITY_SPACE;

	/* FIXME: additional input and output buffers could be implemented on top of N_TTY */
	pProperties->dwCurrentTxQueue = N_TTY_BUF_SIZE;
	pProperties->dwCurrentRxQueue = N_TTY_BUF_SIZE;

	/* pProperties->ProvSpec1; see above */
	/* pProperties->ProvSpec2; ignored */
	/* pProperties->ProvChar[1]; ignored */

	return TRUE;
}

static BOOL _set_baud_rate(WINPR_COMM *pComm, const SERIAL_BAUD_RATE *pBaudRate)
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

	for (i=0; _BAUD_TABLE[i][0]<_BAUD_TABLE_END; i++)
	{
		if (_BAUD_TABLE[i][1] == pBaudRate->BaudRate)
		{
			newSpeed = _BAUD_TABLE[i][0];
			if (cfsetspeed(&futureState, newSpeed) < 0)
			{
				CommLog_Print(WLOG_WARN, "failed to set speed 0x%x (%lu)", newSpeed, pBaudRate->BaudRate);
				return FALSE;
			}

			assert(cfgetispeed(&futureState) == newSpeed);

			if (_comm_ioctl_tcsetattr(pComm->fd, TCSANOW, &futureState) < 0)
			{
				CommLog_Print(WLOG_WARN, "_comm_ioctl_tcsetattr failure: last-error: 0x%lX", GetLastError());
				return FALSE;
			}

			return TRUE;
		}
	}

	CommLog_Print(WLOG_WARN, "could not find a matching speed for the baud rate %lu", pBaudRate->BaudRate);
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

	for (i=0; _BAUD_TABLE[i][0]<_BAUD_TABLE_END; i++)
	{
		if (_BAUD_TABLE[i][0] == currentSpeed)
		{
			pBaudRate->BaudRate = _BAUD_TABLE[i][1];
			return TRUE;
		}
	}

	CommLog_Print(WLOG_WARN, "could not find a matching baud rate for the speed 0x%x", currentSpeed);
	SetLastError(ERROR_INVALID_DATA);
	return FALSE;
}


/**
 * NOTE: Only XonChar and XoffChar are plenty supported with the Linux
 *       N_TTY line discipline.
 *
 * ERRORS:
 *   ERROR_IO_DEVICE
 *   ERROR_INVALID_PARAMETER when Xon and Xoff chars are the same;
 *   ERROR_NOT_SUPPORTED
 */
static BOOL _set_serial_chars(WINPR_COMM *pComm, const SERIAL_CHARS *pSerialChars)
{
	BOOL result = TRUE;
	struct termios upcomingTermios;

	ZeroMemory(&upcomingTermios, sizeof(struct termios));
	if (tcgetattr(pComm->fd, &upcomingTermios) < 0)
	{
		SetLastError(ERROR_IO_DEVICE);
		return FALSE;
	}

	if (pSerialChars->XonChar == pSerialChars->XoffChar)
	{
		/* http://msdn.microsoft.com/en-us/library/windows/hardware/ff546688%28v=vs.85%29.aspx */
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	/* termios(3): (..) above symbolic subscript values are all
         * different, except that VTIME, VMIN may have the same value
         * as VEOL, VEOF, respectively. In noncanonical mode the
         * special character meaning is replaced by the timeout
         * meaning.
	 *
	 * EofChar and c_cc[VEOF] are not quite the same, prefer to
	 * don't use c_cc[VEOF] at all.
	 *
	 * FIXME: might be implemented during read/write I/O
	 */
	if (pSerialChars->EofChar != '\0')
	{
		CommLog_Print(WLOG_WARN, "EofChar='%c' cannot be set\n", pSerialChars->EofChar);
		SetLastError(ERROR_NOT_SUPPORTED);
		result = FALSE; /* but keep on */
	}

	/* According the Linux's n_tty discipline, charaters with a
	 * parity error can only be let unchanged, replaced by \0 or
	 * get the prefix the prefix \377 \0
	 */

	/* FIXME: see also: _set_handflow() */
	if (pSerialChars->ErrorChar != '\0')
	{
		CommLog_Print(WLOG_WARN, "ErrorChar='%c' (0x%x) cannot be set (unsupported).\n", pSerialChars->ErrorChar, pSerialChars->ErrorChar);
		SetLastError(ERROR_NOT_SUPPORTED);
		result = FALSE; /* but keep on */
	}

	/* FIXME: see also: _set_handflow() */
	if (pSerialChars->BreakChar != '\0')
	{
		CommLog_Print(WLOG_WARN, "BreakChar='%c' (0x%x) cannot be set (unsupported).\n", pSerialChars->BreakChar, pSerialChars->BreakChar);
		SetLastError(ERROR_NOT_SUPPORTED);
		result = FALSE; /* but keep on */
	}

	/* FIXME: could be implemented during read/write I/O. What about ISIG? */
	if (pSerialChars->EventChar != '\0')
	{
		CommLog_Print(WLOG_WARN, "EventChar='%c' (0x%x) cannot be set\n", pSerialChars->EventChar, pSerialChars->EventChar);
		SetLastError(ERROR_NOT_SUPPORTED);
		result = FALSE; /* but keep on */
	}

	upcomingTermios.c_cc[VSTART] = pSerialChars->XonChar;

	upcomingTermios.c_cc[VSTOP] = pSerialChars->XoffChar;


	if (_comm_ioctl_tcsetattr(pComm->fd, TCSANOW, &upcomingTermios) < 0)
	{
		CommLog_Print(WLOG_WARN, "_comm_ioctl_tcsetattr failure: last-error: 0x%lX", GetLastError());
		return FALSE;
	}

	return result;
}


static BOOL _get_serial_chars(WINPR_COMM *pComm, SERIAL_CHARS *pSerialChars)
{
	struct termios currentTermios;

	ZeroMemory(&currentTermios, sizeof(struct termios));
	if (tcgetattr(pComm->fd, &currentTermios) < 0)
	{
		SetLastError(ERROR_IO_DEVICE);
		return FALSE;
	}

	ZeroMemory(pSerialChars, sizeof(SERIAL_CHARS));

	/* EofChar unsupported */

	/* ErrorChar unsupported */

	/* BreakChar unsupported */

	/* FIXME: see also: _set_serial_chars() */
	/* EventChar */

	pSerialChars->XonChar = currentTermios.c_cc[VSTART];

	pSerialChars->XoffChar = currentTermios.c_cc[VSTOP];

	return TRUE;
}


static BOOL _set_line_control(WINPR_COMM *pComm, const SERIAL_LINE_CONTROL *pLineControl)
{
	BOOL result = TRUE;
	struct termios upcomingTermios;


	/* http://msdn.microsoft.com/en-us/library/windows/desktop/aa363214%28v=vs.85%29.aspx
	 *
	 * The use of 5 data bits with 2 stop bits is an invalid
	 * combination, as is 6, 7, or 8 data bits with 1.5 stop bits.
	 *
	 * FIXME: prefered to let the underlying driver to deal with
	 * this issue. At least produce a warning message?
	 */


	ZeroMemory(&upcomingTermios, sizeof(struct termios));
	if (tcgetattr(pComm->fd, &upcomingTermios) < 0)
	{
		SetLastError(ERROR_IO_DEVICE);
		return FALSE;
	}

	/* FIXME: use of a COMMPROP to validate new settings? */

	switch (pLineControl->StopBits)
	{
		case STOP_BIT_1:
			upcomingTermios.c_cflag &= ~CSTOPB;
			break;

		case STOP_BITS_1_5:
			CommLog_Print(WLOG_WARN, "Unsupported one and a half stop bits.");
			break;

		case STOP_BITS_2:
			upcomingTermios.c_cflag |= CSTOPB;
			break;

		default:
			CommLog_Print(WLOG_WARN, "unexpected number of stop bits: %d\n", pLineControl->StopBits);
			result = FALSE; /* but keep on */
			break;
	}


	switch (pLineControl->Parity)
	{
		case NO_PARITY:
			upcomingTermios.c_cflag &= ~(PARENB | PARODD | CMSPAR);
			break;

		case ODD_PARITY:
			upcomingTermios.c_cflag &= ~CMSPAR;
			upcomingTermios.c_cflag |= PARENB | PARODD;
			break;

		case EVEN_PARITY:
			upcomingTermios.c_cflag &= ~(PARODD | CMSPAR);
			upcomingTermios.c_cflag |= PARENB;
			break;

		case MARK_PARITY:
			upcomingTermios.c_cflag |= PARENB | PARODD | CMSPAR;
			break;

		case SPACE_PARITY:
			upcomingTermios.c_cflag &= ~PARODD;
			upcomingTermios.c_cflag |= PARENB | CMSPAR;
			break;

		default:
			CommLog_Print(WLOG_WARN, "unexpected type of parity: %d\n", pLineControl->Parity);
			result = FALSE; /* but keep on */
			break;
	}

	switch (pLineControl->WordLength)
	{
		case 5:
			upcomingTermios.c_cflag &= ~CSIZE;
			upcomingTermios.c_cflag |= CS5;
			break;

		case 6:
			upcomingTermios.c_cflag &= ~CSIZE;
			upcomingTermios.c_cflag |= CS6;
			break;

		case 7:
			upcomingTermios.c_cflag &= ~CSIZE;
			upcomingTermios.c_cflag |= CS7;
			break;

		case 8:
			upcomingTermios.c_cflag &= ~CSIZE;
			upcomingTermios.c_cflag |= CS8;
			break;

		default:
			CommLog_Print(WLOG_WARN, "unexpected number od data bits per character: %d\n", pLineControl->WordLength);
			result = FALSE; /* but keep on */
			break;
	}

	if (_comm_ioctl_tcsetattr(pComm->fd, TCSANOW, &upcomingTermios) < 0)
	{
		CommLog_Print(WLOG_WARN, "_comm_ioctl_tcsetattr failure: last-error: 0x%lX", GetLastError());
		return FALSE;
	}

	return result;
}


static BOOL _get_line_control(WINPR_COMM *pComm, SERIAL_LINE_CONTROL *pLineControl)
{
	struct termios currentTermios;

	ZeroMemory(&currentTermios, sizeof(struct termios));
	if (tcgetattr(pComm->fd, &currentTermios) < 0)
	{
		SetLastError(ERROR_IO_DEVICE);
		return FALSE;
	}

	pLineControl->StopBits = (currentTermios.c_cflag & CSTOPB) ? STOP_BITS_2 : STOP_BIT_1;

	if (!(currentTermios.c_cflag & PARENB))
	{
		pLineControl->Parity = NO_PARITY;
	}
	else if (currentTermios.c_cflag & CMSPAR)
	{
		pLineControl->Parity = (currentTermios.c_cflag & PARODD) ? MARK_PARITY : SPACE_PARITY;
	}
	else
	{
		/* PARENB is set */
		pLineControl->Parity = (currentTermios.c_cflag & PARODD) ?  ODD_PARITY : EVEN_PARITY;
	}

	switch (currentTermios.c_cflag & CSIZE)
	{
		case CS5:
			pLineControl->WordLength = 5;
			break;
		case CS6:
			pLineControl->WordLength = 6;
			break;
		case CS7:
			pLineControl->WordLength = 7;
			break;
		default:
			pLineControl->WordLength = 8;
			break;
	}

	return TRUE;
}


static BOOL _set_handflow(WINPR_COMM *pComm, const SERIAL_HANDFLOW *pHandflow)
{
	BOOL result = TRUE;
	struct termios upcomingTermios;

	ZeroMemory(&upcomingTermios, sizeof(struct termios));
	if (tcgetattr(pComm->fd, &upcomingTermios) < 0)
	{
		SetLastError(ERROR_IO_DEVICE);
		return FALSE;
	}

	/* HUPCL */

	/* logical XOR */
	if ((!(pHandflow->ControlHandShake & SERIAL_DTR_CONTROL) && (pHandflow->FlowReplace & SERIAL_RTS_CONTROL)) ||
		((pHandflow->ControlHandShake & SERIAL_DTR_CONTROL) && !(pHandflow->FlowReplace & SERIAL_RTS_CONTROL)))
	{
		CommLog_Print(WLOG_WARN, "SERIAL_DTR_CONTROL:%s and SERIAL_RTS_CONTROL:%s cannot be different, HUPCL will be set since it is claimed for one of the both lines.",
			(pHandflow->ControlHandShake & SERIAL_DTR_CONTROL) ? "ON" : "OFF",
			(pHandflow->FlowReplace & SERIAL_RTS_CONTROL) ? "ON" : "OFF");
	}

	if ((pHandflow->ControlHandShake & SERIAL_DTR_CONTROL) || (pHandflow->FlowReplace & SERIAL_RTS_CONTROL))
	{
		upcomingTermios.c_cflag |= HUPCL;
	}
	else
	{
		upcomingTermios.c_cflag &= ~HUPCL;

		/* FIXME: is the DTR line also needs to be forced to a disable state according SERIAL_DTR_CONTROL? */
		/* FIXME: is the RTS line also needs to be forced to a disable state according SERIAL_RTS_CONTROL? */
	}


	/* CRTSCTS */

	/* logical XOR */
	if ((!(pHandflow->ControlHandShake & SERIAL_CTS_HANDSHAKE) && (pHandflow->FlowReplace & SERIAL_RTS_HANDSHAKE)) ||
		((pHandflow->ControlHandShake & SERIAL_CTS_HANDSHAKE) && !(pHandflow->FlowReplace & SERIAL_RTS_HANDSHAKE)))
	{
		CommLog_Print(WLOG_WARN, "SERIAL_CTS_HANDSHAKE:%s and SERIAL_RTS_HANDSHAKE:%s cannot be different, CRTSCTS will be set since it is claimed for one of the both lines.",
			(pHandflow->ControlHandShake & SERIAL_CTS_HANDSHAKE) ? "ON" : "OFF",
			(pHandflow->FlowReplace & SERIAL_RTS_HANDSHAKE) ? "ON" : "OFF");
	}

	if ((pHandflow->ControlHandShake & SERIAL_CTS_HANDSHAKE) || (pHandflow->FlowReplace & SERIAL_RTS_HANDSHAKE))
	{
		upcomingTermios.c_cflag |= CRTSCTS;
	}
	else
	{
		upcomingTermios.c_cflag &= ~CRTSCTS;
	}


	/* ControlHandShake */

	if (pHandflow->ControlHandShake & SERIAL_DTR_HANDSHAKE)
	{
		/* DTR/DSR flow control not supported on Linux */
		CommLog_Print(WLOG_WARN, "Attempt to use the unsupported SERIAL_DTR_HANDSHAKE feature.");
		SetLastError(ERROR_NOT_SUPPORTED);
		result = FALSE; /* but keep on */
	}


	if (pHandflow->ControlHandShake & SERIAL_DSR_HANDSHAKE)
	{
		/* DTR/DSR flow control not supported on Linux */
		CommLog_Print(WLOG_WARN, "Attempt to use the unsupported SERIAL_DSR_HANDSHAKE feature.");
		SetLastError(ERROR_NOT_SUPPORTED);
		result = FALSE; /* but keep on */
	}

	if (pHandflow->ControlHandShake & SERIAL_DCD_HANDSHAKE)
	{
		/* DCD flow control not supported on Linux */
		CommLog_Print(WLOG_WARN, "Attempt to use the unsupported SERIAL_DCD_HANDSHAKE feature.");
		SetLastError(ERROR_NOT_SUPPORTED);
		result = FALSE; /* but keep on */
	}

	// FIXME: could be implemented during read/write I/O
	if (pHandflow->ControlHandShake & SERIAL_DSR_SENSITIVITY)
	{
		/* DSR line control not supported on Linux */
		CommLog_Print(WLOG_WARN, "Attempt to use the unsupported SERIAL_DSR_SENSITIVITY feature.");
		SetLastError(ERROR_NOT_SUPPORTED);
		result = FALSE; /* but keep on */
	}

	// FIXME: could be implemented during read/write I/O
	if (pHandflow->ControlHandShake & SERIAL_ERROR_ABORT)
	{
		/* Aborting operations on error not supported on Linux */
		CommLog_Print(WLOG_WARN, "Attempt to use the unsupported SERIAL_ERROR_ABORT feature.");
		SetLastError(ERROR_NOT_SUPPORTED);
		result = FALSE; /* but keep on */
	}

	/* FlowReplace */

	if (pHandflow->FlowReplace & SERIAL_AUTO_TRANSMIT)
	{
		upcomingTermios.c_iflag |= IXON;
	}
	else
	{
		upcomingTermios.c_iflag &= ~IXON;
	}

	if (pHandflow->FlowReplace & SERIAL_AUTO_RECEIVE)
	{
		upcomingTermios.c_iflag |= IXOFF;
	}
	else
	{
		upcomingTermios.c_iflag &= ~IXOFF;
	}

	// FIXME: could be implemented during read/write I/O, as of today ErrorChar is necessary '\0'
	if (pHandflow->FlowReplace & SERIAL_ERROR_CHAR)
	{
		/* errors will be replaced by the character '\0'. */
		upcomingTermios.c_iflag &= ~IGNPAR;
	}
	else
	{
		upcomingTermios.c_iflag |= IGNPAR;
	}

	if (pHandflow->FlowReplace & SERIAL_NULL_STRIPPING)
	{
		upcomingTermios.c_iflag |= IGNBRK;
	}
	else
	{
		upcomingTermios.c_iflag &= ~IGNBRK;
	}

	// FIXME: could be implemented during read/write I/O
	if (pHandflow->FlowReplace & SERIAL_BREAK_CHAR)
	{
		CommLog_Print(WLOG_WARN, "Attempt to use the unsupported SERIAL_BREAK_CHAR feature.");
		SetLastError(ERROR_NOT_SUPPORTED);
		result = FALSE; /* but keep on */
	}

	// FIXME: could be implemented during read/write I/O
	if (pHandflow->FlowReplace & SERIAL_XOFF_CONTINUE)
	{
		/* not supported on Linux */
		CommLog_Print(WLOG_WARN, "Attempt to use the unsupported SERIAL_XOFF_CONTINUE feature.");
		SetLastError(ERROR_NOT_SUPPORTED);
		result = FALSE; /* but keep on */
	}

	/* XonLimit */

	// FIXME: could be implemented during read/write I/O
	if (pHandflow->XonLimit != TTY_THRESHOLD_UNTHROTTLE)
	{
		CommLog_Print(WLOG_WARN, "Attempt to set XonLimit with an unsupported value: %lu", pHandflow->XonLimit);
		SetLastError(ERROR_NOT_SUPPORTED);
		result = FALSE; /* but keep on */
	}

	/* XoffChar */

	// FIXME: could be implemented during read/write I/O
	if (pHandflow->XoffLimit != TTY_THRESHOLD_THROTTLE)
	{
		CommLog_Print(WLOG_WARN, "Attempt to set XoffLimit with an unsupported value: %lu", pHandflow->XoffLimit);
		SetLastError(ERROR_NOT_SUPPORTED);
		result = FALSE; /* but keep on */
	}


	if (_comm_ioctl_tcsetattr(pComm->fd, TCSANOW, &upcomingTermios) < 0)
	{
		CommLog_Print(WLOG_WARN, "_comm_ioctl_tcsetattr failure: last-error: 0x%lX", GetLastError());
		return FALSE;
	}

	return result;
}


static BOOL _get_handflow(WINPR_COMM *pComm, SERIAL_HANDFLOW *pHandflow)
{
	struct termios currentTermios;

	ZeroMemory(&currentTermios, sizeof(struct termios));
	if (tcgetattr(pComm->fd, &currentTermios) < 0)
	{
		SetLastError(ERROR_IO_DEVICE);
		return FALSE;
	}


	/* ControlHandShake */

	pHandflow->ControlHandShake = 0;

	if (currentTermios.c_cflag & HUPCL)
		pHandflow->ControlHandShake |= SERIAL_DTR_CONTROL;

	/* SERIAL_DTR_HANDSHAKE unsupported */

	if (currentTermios.c_cflag & CRTSCTS)
		pHandflow->ControlHandShake |= SERIAL_CTS_HANDSHAKE;

	/* SERIAL_DSR_HANDSHAKE unsupported */

	/* SERIAL_DCD_HANDSHAKE unsupported */

	/* SERIAL_DSR_SENSITIVITY unsupported */

	/* SERIAL_ERROR_ABORT unsupported */


	/* FlowReplace */

	pHandflow->FlowReplace = 0;

	if (currentTermios.c_iflag & IXON)
		pHandflow->FlowReplace |= SERIAL_AUTO_TRANSMIT;

	if (currentTermios.c_iflag & IXOFF)
		pHandflow->FlowReplace |= SERIAL_AUTO_RECEIVE;

	if (!(currentTermios.c_iflag & IGNPAR))
		pHandflow->FlowReplace |= SERIAL_ERROR_CHAR;

	if (currentTermios.c_iflag & IGNBRK)
		pHandflow->FlowReplace |= SERIAL_NULL_STRIPPING;

	/* SERIAL_BREAK_CHAR unsupported */

	if (currentTermios.c_cflag & HUPCL)
		pHandflow->FlowReplace |= SERIAL_RTS_CONTROL;

	if (currentTermios.c_cflag & CRTSCTS)
		pHandflow->FlowReplace |= SERIAL_RTS_HANDSHAKE;

	/* SERIAL_XOFF_CONTINUE unsupported */


	/* XonLimit */

	pHandflow->XonLimit = TTY_THRESHOLD_UNTHROTTLE;


	/* XoffLimit */

	pHandflow->XoffLimit = TTY_THRESHOLD_THROTTLE;

	return TRUE;
}

static BOOL _set_timeouts(WINPR_COMM *pComm, const SERIAL_TIMEOUTS *pTimeouts)
{
	/* NB: timeouts are applied on system during read/write I/O */

	/* http://msdn.microsoft.com/en-us/library/windows/hardware/hh439614%28v=vs.85%29.aspx */
	if ((pTimeouts->ReadIntervalTimeout == MAXULONG) && (pTimeouts->ReadTotalTimeoutConstant == MAXULONG))
	{
		CommLog_Print(WLOG_WARN, "ReadIntervalTimeout and ReadTotalTimeoutConstant cannot be both set to MAXULONG");
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	pComm->timeouts.ReadIntervalTimeout         = pTimeouts->ReadIntervalTimeout;
	pComm->timeouts.ReadTotalTimeoutMultiplier  = pTimeouts->ReadTotalTimeoutMultiplier;
	pComm->timeouts.ReadTotalTimeoutConstant    = pTimeouts->ReadTotalTimeoutConstant;
	pComm->timeouts.WriteTotalTimeoutMultiplier = pTimeouts->WriteTotalTimeoutMultiplier;
	pComm->timeouts.WriteTotalTimeoutConstant   = pTimeouts->WriteTotalTimeoutConstant;

	CommLog_Print(WLOG_DEBUG, "ReadIntervalTimeout %d", pComm->timeouts.ReadIntervalTimeout);
	CommLog_Print(WLOG_DEBUG, "ReadTotalTimeoutMultiplier %d", pComm->timeouts.ReadTotalTimeoutMultiplier);
	CommLog_Print(WLOG_DEBUG, "ReadTotalTimeoutConstant %d", pComm->timeouts.ReadTotalTimeoutConstant);
	CommLog_Print(WLOG_DEBUG, "WriteTotalTimeoutMultiplier %d", pComm->timeouts.WriteTotalTimeoutMultiplier);
	CommLog_Print(WLOG_DEBUG, "WriteTotalTimeoutConstant %d", pComm->timeouts.WriteTotalTimeoutConstant);

	return TRUE;
}

static BOOL _get_timeouts(WINPR_COMM *pComm, SERIAL_TIMEOUTS *pTimeouts)
{
	pTimeouts->ReadIntervalTimeout         = pComm->timeouts.ReadIntervalTimeout;
	pTimeouts->ReadTotalTimeoutMultiplier  = pComm->timeouts.ReadTotalTimeoutMultiplier;
	pTimeouts->ReadTotalTimeoutConstant    = pComm->timeouts.ReadTotalTimeoutConstant;
	pTimeouts->WriteTotalTimeoutMultiplier = pComm->timeouts.WriteTotalTimeoutMultiplier;
	pTimeouts->WriteTotalTimeoutConstant   = pComm->timeouts.WriteTotalTimeoutConstant;

	return TRUE;
}


static BOOL _set_lines(WINPR_COMM *pComm, UINT32 lines)
{
	if (ioctl(pComm->fd, TIOCMBIS, &lines) < 0)
	{
		CommLog_Print(WLOG_WARN, "TIOCMBIS ioctl failed, lines=0x%X, errno=[%d] %s", lines, errno, strerror(errno));
		SetLastError(ERROR_IO_DEVICE);
		return FALSE;
	}

	return TRUE;
}


static BOOL _clear_lines(WINPR_COMM *pComm, UINT32 lines)
{
	if (ioctl(pComm->fd, TIOCMBIC, &lines) < 0)
	{
		CommLog_Print(WLOG_WARN, "TIOCMBIC ioctl failed, lines=0x%X, errno=[%d] %s", lines, errno, strerror(errno));
		SetLastError(ERROR_IO_DEVICE);
		return FALSE;
	}

	return TRUE;
}


static BOOL _set_dtr(WINPR_COMM *pComm)
{
	SERIAL_HANDFLOW handflow;
	if (!_get_handflow(pComm, &handflow))
		return FALSE;

	/* SERIAL_DTR_HANDSHAKE not supported as of today */
	assert((handflow.ControlHandShake & SERIAL_DTR_HANDSHAKE) == 0);

	if (handflow.ControlHandShake & SERIAL_DTR_HANDSHAKE)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	return _set_lines(pComm, TIOCM_DTR);
}

static BOOL _clear_dtr(WINPR_COMM *pComm)
{
	SERIAL_HANDFLOW handflow;
	if (!_get_handflow(pComm, &handflow))
		return FALSE;

	/* SERIAL_DTR_HANDSHAKE not supported as of today */
	assert((handflow.ControlHandShake & SERIAL_DTR_HANDSHAKE) == 0);

	if (handflow.ControlHandShake & SERIAL_DTR_HANDSHAKE)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	return _clear_lines(pComm, TIOCM_DTR);
}

static BOOL _set_rts(WINPR_COMM *pComm)
{
	SERIAL_HANDFLOW handflow;
	if (!_get_handflow(pComm, &handflow))
		return FALSE;

	if (handflow.FlowReplace & SERIAL_RTS_HANDSHAKE)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	return _set_lines(pComm, TIOCM_RTS);
}

static BOOL _clear_rts(WINPR_COMM *pComm)
{
	SERIAL_HANDFLOW handflow;
	if (!_get_handflow(pComm, &handflow))
		return FALSE;

	if (handflow.FlowReplace & SERIAL_RTS_HANDSHAKE)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	return _clear_lines(pComm, TIOCM_RTS);
}



static BOOL _get_modemstatus(WINPR_COMM *pComm, ULONG *pRegister)
{
	UINT32 lines=0;
	if (ioctl(pComm->fd, TIOCMGET, &lines) < 0)
	{
		CommLog_Print(WLOG_WARN, "TIOCMGET ioctl failed, errno=[%d] %s", errno, strerror(errno));
		SetLastError(ERROR_IO_DEVICE);
		return FALSE;
	}

	ZeroMemory(pRegister, sizeof(ULONG));

	/* FIXME: Is the last read of the MSR register available or
	 * cached somewhere? Not quite sure we need to return the 4
	 * LSBits anyway. A direct access to the register -- which
	 * would reset the register -- is likely not expected from
	 * this function.
	 */

	/* #define SERIAL_MSR_DCTS     0x01 */
	/* #define SERIAL_MSR_DDSR     0x02 */
	/* #define SERIAL_MSR_TERI     0x04 */
	/* #define SERIAL_MSR_DDCD     0x08 */

	if (lines & TIOCM_CTS)
		*pRegister |= SERIAL_MSR_CTS;
	if (lines & TIOCM_DSR)
		*pRegister |= SERIAL_MSR_DSR;
	if (lines & TIOCM_RI)
		*pRegister |= SERIAL_MSR_RI;
	if (lines & TIOCM_CD)
		*pRegister |= SERIAL_MSR_DCD;

	return TRUE;
}

/* http://msdn.microsoft.com/en-us/library/windows/hardware/hh439605%28v=vs.85%29.aspx */
static const ULONG _SERIAL_SYS_SUPPORTED_EV_MASK =
	SERIAL_EV_RXCHAR   |
	SERIAL_EV_RXFLAG   |
	SERIAL_EV_TXEMPTY  |
	SERIAL_EV_CTS      |
	SERIAL_EV_DSR      |
	SERIAL_EV_RLSD     |
	SERIAL_EV_BREAK    |
	SERIAL_EV_ERR      |
	SERIAL_EV_RING     |
	/* SERIAL_EV_PERR     | */
	SERIAL_EV_RX80FULL /*|
	SERIAL_EV_EVENT1   |
	SERIAL_EV_EVENT2*/;


static BOOL _set_wait_mask(WINPR_COMM *pComm, const ULONG *pWaitMask)
{
	ULONG possibleMask;


	/* Stops pending IOCTL_SERIAL_WAIT_ON_MASK
	 * http://msdn.microsoft.com/en-us/library/ff546805%28v=vs.85%29.aspx
	 */

	if (pComm->PendingEvents & SERIAL_EV_FREERDP_WAITING)
	{
		/* FIXME: any doubt on reading PendingEvents out of a critical section? */

		EnterCriticalSection(&pComm->EventsLock);
		pComm->PendingEvents |= SERIAL_EV_FREERDP_STOP;
		LeaveCriticalSection(&pComm->EventsLock);

		/* waiting the end of the pending _wait_on_mask() */
		while (pComm->PendingEvents & SERIAL_EV_FREERDP_WAITING)
			Sleep(10); /* 10ms */
	}

	/* NB: ensure to leave the critical section before to return */
	EnterCriticalSection(&pComm->EventsLock);

	if (*pWaitMask == 0)
	{
		/* clearing pending events */

		if (ioctl(pComm->fd, TIOCGICOUNT, &(pComm->counters)) < 0)
		{
			CommLog_Print(WLOG_WARN, "TIOCGICOUNT ioctl failed, errno=[%d] %s.", errno, strerror(errno));
			
			if (pComm->permissive)
			{
				/* counters could not be reset but keep on */
				ZeroMemory(&(pComm->counters), sizeof(struct serial_icounter_struct));
			}
			else
			{
				SetLastError(ERROR_IO_DEVICE);
				LeaveCriticalSection(&pComm->EventsLock);
				return FALSE;
			}
		}

		pComm->PendingEvents = 0;
	}

	possibleMask = *pWaitMask & _SERIAL_SYS_SUPPORTED_EV_MASK;

	if (possibleMask != *pWaitMask)
	{
		CommLog_Print(WLOG_WARN, "Not all wait events supported (Serial.sys), requested events= 0X%lX, possible events= 0X%lX", *pWaitMask, possibleMask);

		/* FIXME: shall we really set the possibleMask and return FALSE? */
		pComm->WaitEventMask = possibleMask;

		LeaveCriticalSection(&pComm->EventsLock);
		return FALSE;
	}

	pComm->WaitEventMask = possibleMask;

	LeaveCriticalSection(&pComm->EventsLock);
	return TRUE;
}


static BOOL _get_wait_mask(WINPR_COMM *pComm, ULONG *pWaitMask)
{
	*pWaitMask = pComm->WaitEventMask;
	return TRUE;
}



static BOOL _set_queue_size(WINPR_COMM *pComm, const SERIAL_QUEUE_SIZE *pQueueSize)
{
	if ((pQueueSize->InSize <= N_TTY_BUF_SIZE) && (pQueueSize->OutSize <= N_TTY_BUF_SIZE))
		return TRUE; /* nothing to do */

	/* FIXME: could be implemented on top of N_TTY */

	if (pQueueSize->InSize > N_TTY_BUF_SIZE)
		CommLog_Print(WLOG_WARN, "Requested an incompatible input buffer size: %lu, keeping on with a %d bytes buffer.", pQueueSize->InSize, N_TTY_BUF_SIZE);

	if (pQueueSize->OutSize > N_TTY_BUF_SIZE)
		CommLog_Print(WLOG_WARN, "Requested an incompatible output buffer size: %lu, keeping on with a %d bytes buffer.", pQueueSize->OutSize, N_TTY_BUF_SIZE);

	SetLastError(ERROR_CANCELLED);
	return FALSE;
}


static BOOL _purge(WINPR_COMM *pComm, const ULONG *pPurgeMask)
{
	if ((*pPurgeMask & ~(SERIAL_PURGE_TXABORT | SERIAL_PURGE_RXABORT | SERIAL_PURGE_TXCLEAR | SERIAL_PURGE_RXCLEAR)) > 0)
	{
		CommLog_Print(WLOG_WARN, "Invalid purge mask: 0x%lX\n", *pPurgeMask);
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	/* FIXME: currently relying too much on the fact the server
	 * sends a single IRP_MJ_WRITE or IRP_MJ_READ at a time
	 * (taking care though that one IRP_MJ_WRITE and one
	 * IRP_MJ_READ can be sent simultaneously) */

	if (*pPurgeMask & SERIAL_PURGE_TXABORT)
	{
		/* Purges all write (IRP_MJ_WRITE) requests. */

		if (eventfd_write(pComm->fd_write_event, FREERDP_PURGE_TXABORT) < 0)
		{
			if (errno != EAGAIN)
			{
				CommLog_Print(WLOG_WARN, "eventfd_write failed, errno=[%d] %s", errno, strerror(errno));
			}

			assert(errno == EAGAIN); /* no reader <=> no pending IRP_MJ_WRITE */
		}
	}

	if (*pPurgeMask & SERIAL_PURGE_RXABORT)
	{
		/* Purges all read (IRP_MJ_READ) requests. */

		if (eventfd_write(pComm->fd_read_event, FREERDP_PURGE_RXABORT) < 0)
		{
			if (errno != EAGAIN)
			{
				CommLog_Print(WLOG_WARN, "eventfd_write failed, errno=[%d] %s", errno, strerror(errno));
			}

			assert(errno == EAGAIN); /* no reader <=> no pending IRP_MJ_READ */
		}
	}

	if (*pPurgeMask & SERIAL_PURGE_TXCLEAR)
	{
		/* Purges the transmit buffer, if one exists. */

		if (tcflush(pComm->fd, TCOFLUSH) < 0)
		{
			CommLog_Print(WLOG_WARN, "tcflush(TCOFLUSH) failure, errno=[%d] %s", errno, strerror(errno));
			SetLastError(ERROR_CANCELLED);
			return FALSE;
		}
	}


	if (*pPurgeMask & SERIAL_PURGE_RXCLEAR)
	{
		/* Purges the receive buffer, if one exists. */

		if (tcflush(pComm->fd, TCIFLUSH) < 0)
		{
			CommLog_Print(WLOG_WARN, "tcflush(TCIFLUSH) failure, errno=[%d] %s", errno, strerror(errno));
			SetLastError(ERROR_CANCELLED);
			return FALSE;
		}
	}

	return TRUE;
}

/* NB: _get_commstatus also produces most of the events consumed by _wait_on_mask(). Exceptions:
 *  - SERIAL_EV_RXFLAG: FIXME: once EventChar supported
 *
 */
static BOOL _get_commstatus(WINPR_COMM *pComm, SERIAL_STATUS *pCommstatus)
{
	/* http://msdn.microsoft.com/en-us/library/jj673022%28v=vs.85%29.aspx */

	struct serial_icounter_struct currentCounters;

	/* NB: ensure to leave the critical section before to return */
	EnterCriticalSection(&pComm->EventsLock);

	ZeroMemory(pCommstatus, sizeof(SERIAL_STATUS));

	ZeroMemory(&currentCounters, sizeof(struct serial_icounter_struct));
	if (ioctl(pComm->fd, TIOCGICOUNT, &currentCounters) < 0)
	{
		CommLog_Print(WLOG_WARN, "TIOCGICOUNT ioctl failed, errno=[%d] %s.", errno, strerror(errno));
		CommLog_Print(WLOG_WARN, "  coult not read counters.");
		
		if (pComm->permissive)
		{
			/* Errors and events based on counters could not be
			 * detected but keep on.
			 */
			ZeroMemory(&currentCounters, sizeof(struct serial_icounter_struct));
		}
		else
		{
			SetLastError(ERROR_IO_DEVICE);
			LeaveCriticalSection(&pComm->EventsLock);
			return FALSE;
		}
	}

	/* NB: preferred below (currentCounters.* != pComm->counters.*) over (currentCounters.* > pComm->counters.*) thinking the counters can loop */

	/* Errors */

	if (currentCounters.buf_overrun != pComm->counters.buf_overrun)
	{
		pCommstatus->Errors |= SERIAL_ERROR_QUEUEOVERRUN;
	}

	if (currentCounters.overrun != pComm->counters.overrun)
	{
		pCommstatus->Errors |= SERIAL_ERROR_OVERRUN;
		pComm->PendingEvents |= SERIAL_EV_ERR;
	}

	if (currentCounters.brk != pComm->counters.brk)
	{
		pCommstatus->Errors |= SERIAL_ERROR_BREAK;
		pComm->PendingEvents |= SERIAL_EV_BREAK;
	}

	if (currentCounters.parity != pComm->counters.parity)
	{
		pCommstatus->Errors |= SERIAL_ERROR_PARITY;
		pComm->PendingEvents |= SERIAL_EV_ERR;
	}

	if (currentCounters.frame != pComm->counters.frame)
	{
		pCommstatus->Errors |= SERIAL_ERROR_FRAMING;
		pComm->PendingEvents |= SERIAL_EV_ERR;
	}


	/* HoldReasons */


	/* TODO: SERIAL_TX_WAITING_FOR_CTS */

	/* TODO: SERIAL_TX_WAITING_FOR_DSR */

	/* TODO: SERIAL_TX_WAITING_FOR_DCD */

	/* TODO: SERIAL_TX_WAITING_FOR_XON */

	/* TODO: SERIAL_TX_WAITING_ON_BREAK, see LCR's bit 6 */

	/* TODO: SERIAL_TX_WAITING_XOFF_SENT */



	/* AmountInInQueue */

	if (ioctl(pComm->fd, TIOCINQ, &(pCommstatus->AmountInInQueue)) < 0)
	{
		CommLog_Print(WLOG_WARN, "TIOCINQ ioctl failed, errno=[%d] %s", errno, strerror(errno));
		SetLastError(ERROR_IO_DEVICE);

		LeaveCriticalSection(&pComm->EventsLock);
		return FALSE;
	}


	/*  AmountInOutQueue */

	if (ioctl(pComm->fd, TIOCOUTQ, &(pCommstatus->AmountInOutQueue)) < 0)
	{
		CommLog_Print(WLOG_WARN, "TIOCOUTQ ioctl failed, errno=[%d] %s", errno, strerror(errno));
		SetLastError(ERROR_IO_DEVICE);

		LeaveCriticalSection(&pComm->EventsLock);
		return FALSE;
	}

	/*  BOOLEAN EofReceived; FIXME: once EofChar supported */


	/*  BOOLEAN WaitForImmediate; TODO: once IOCTL_SERIAL_IMMEDIATE_CHAR fully supported */


	/* other events based on counters */

	if (currentCounters.rx != pComm->counters.rx)
	{
		pComm->PendingEvents |= SERIAL_EV_RXCHAR;
	}

	if ((currentCounters.tx != pComm->counters.tx) && /* at least a transmission occurred AND ...*/
		(pCommstatus->AmountInOutQueue == 0)) /* output bufer is now empty */
	{
		pComm->PendingEvents |= SERIAL_EV_TXEMPTY;
	}
	else
	{
		/* FIXME: "now empty" from the specs is ambiguous, need to track previous completed transmission? */
		pComm->PendingEvents &= ~SERIAL_EV_TXEMPTY;
	}

	if (currentCounters.cts != pComm->counters.cts)
	{
		pComm->PendingEvents |= SERIAL_EV_CTS;
	}

	if (currentCounters.dsr != pComm->counters.dsr)
	{
		pComm->PendingEvents |= SERIAL_EV_DSR;
	}

	if (currentCounters.dcd != pComm->counters.dcd)
	{
		pComm->PendingEvents |= SERIAL_EV_RLSD;
	}

	if (currentCounters.rng != pComm->counters.rng)
	{
		pComm->PendingEvents |= SERIAL_EV_RING;
	}

	if (pCommstatus->AmountInInQueue > (0.8 * N_TTY_BUF_SIZE))
	{
		pComm->PendingEvents |= SERIAL_EV_RX80FULL;
	}
	else
	{
		/* FIXME: "is 80 percent full" from the specs is ambiguous, need to track when it previously * occurred? */
		pComm->PendingEvents &= ~SERIAL_EV_RX80FULL;
	}


	pComm->counters = currentCounters;

	LeaveCriticalSection(&pComm->EventsLock);
	return TRUE;
}

static BOOL _refresh_PendingEvents(WINPR_COMM *pComm)
{
	SERIAL_STATUS serialStatus;

	/* NB: also ensures PendingEvents to be up to date */
	ZeroMemory(&serialStatus, sizeof(SERIAL_STATUS));
	if (!_get_commstatus(pComm, &serialStatus))
	{
		return FALSE;
	}

	return TRUE;
}


static void _consume_event(WINPR_COMM *pComm, ULONG *pOutputMask, ULONG event)
{
	if ((pComm->WaitEventMask & event) && (pComm->PendingEvents & event))
	{
		pComm->PendingEvents &= ~event; /* consumed */
		*pOutputMask |= event;
	}
}

/*
 * NB: see also: _set_wait_mask()
 */
static BOOL _wait_on_mask(WINPR_COMM *pComm, ULONG *pOutputMask)
{
	assert(*pOutputMask == 0);

	EnterCriticalSection(&pComm->EventsLock);
	pComm->PendingEvents |= SERIAL_EV_FREERDP_WAITING;
	LeaveCriticalSection(&pComm->EventsLock);


	while (TRUE)
	{
		/* NB: EventsLock also used by _refresh_PendingEvents() */
		if (!_refresh_PendingEvents(pComm))
		{
			EnterCriticalSection(&pComm->EventsLock);
			pComm->PendingEvents &= ~SERIAL_EV_FREERDP_WAITING;
			LeaveCriticalSection(&pComm->EventsLock);
			return FALSE;
		}

		/* NB: ensure to leave the critical section before to return */
		EnterCriticalSection(&pComm->EventsLock);

		if (pComm->PendingEvents & SERIAL_EV_FREERDP_STOP)
		{
			pComm->PendingEvents &= ~SERIAL_EV_FREERDP_STOP;

			/* pOutputMask must remain empty but should
			 * not have been modified.
			 *
			 * http://msdn.microsoft.com/en-us/library/ff546805%28v=vs.85%29.aspx
			 */
			assert(*pOutputMask == 0);

			pComm->PendingEvents &= ~SERIAL_EV_FREERDP_WAITING;
			LeaveCriticalSection(&pComm->EventsLock);
			return TRUE;
		}

		_consume_event(pComm, pOutputMask, SERIAL_EV_RXCHAR);
		_consume_event(pComm, pOutputMask, SERIAL_EV_RXFLAG);
		_consume_event(pComm, pOutputMask, SERIAL_EV_TXEMPTY);
		_consume_event(pComm, pOutputMask, SERIAL_EV_CTS);
		_consume_event(pComm, pOutputMask, SERIAL_EV_DSR);
		_consume_event(pComm, pOutputMask, SERIAL_EV_RLSD);
		_consume_event(pComm, pOutputMask, SERIAL_EV_BREAK);
		_consume_event(pComm, pOutputMask, SERIAL_EV_ERR);
		_consume_event(pComm, pOutputMask, SERIAL_EV_RING );
		_consume_event(pComm, pOutputMask, SERIAL_EV_RX80FULL);

		LeaveCriticalSection(&pComm->EventsLock);

		/* NOTE: PendingEvents can be modified from now on but
		 * not pOutputMask */

		if (*pOutputMask != 0)
		{
			/* at least an event occurred */

			EnterCriticalSection(&pComm->EventsLock);
			pComm->PendingEvents &= ~SERIAL_EV_FREERDP_WAITING;
			LeaveCriticalSection(&pComm->EventsLock);
			return TRUE;
		}


		/* waiting for a modification of PendingEvents.
		 *
		 * NOTE: previously used a semaphore but used
		 * sem_timedwait() anyway. Finally preferred a simpler
		 * solution with Sleep() whithout the burden of the
		 * semaphore initialization and destroying.
		 */

		Sleep(100); /* 100 ms */
	}

	CommLog_Print(WLOG_WARN, "_wait_on_mask, unexpected return, WaitEventMask=0X%lX", pComm->WaitEventMask);
	EnterCriticalSection(&pComm->EventsLock);
	pComm->PendingEvents &= ~SERIAL_EV_FREERDP_WAITING;
	LeaveCriticalSection(&pComm->EventsLock);
	assert(FALSE);
	return FALSE;
}

static BOOL _set_break_on(WINPR_COMM *pComm)
{
	if (ioctl(pComm->fd, TIOCSBRK, NULL) < 0)
	{
		CommLog_Print(WLOG_WARN, "TIOCSBRK ioctl failed, errno=[%d] %s", errno, strerror(errno));
		SetLastError(ERROR_IO_DEVICE);
		return FALSE;
	}

	return TRUE;
}


static BOOL _set_break_off(WINPR_COMM *pComm)
{
	if (ioctl(pComm->fd, TIOCCBRK, NULL) < 0)
	{
		CommLog_Print(WLOG_WARN, "TIOCSBRK ioctl failed, errno=[%d] %s", errno, strerror(errno));
		SetLastError(ERROR_IO_DEVICE);
		return FALSE;
	}

	return TRUE;
}


static BOOL _set_xoff(WINPR_COMM *pComm)
{
	if (tcflow(pComm->fd, TCIOFF) < 0)
	{
		CommLog_Print(WLOG_WARN, "TCIOFF failure, errno=[%d] %s", errno, strerror(errno));
		SetLastError(ERROR_IO_DEVICE);
		return FALSE;
	}

	return TRUE;
}


static BOOL _set_xon(WINPR_COMM *pComm)
{
	if (tcflow(pComm->fd, TCION) < 0)
	{
		CommLog_Print(WLOG_WARN, "TCION failure, errno=[%d] %s", errno, strerror(errno));
		SetLastError(ERROR_IO_DEVICE);
		return FALSE;
	}

	return TRUE;
}


BOOL _get_dtrrts(WINPR_COMM *pComm, ULONG *pMask)
{
	UINT32 lines=0;
	if (ioctl(pComm->fd, TIOCMGET, &lines) < 0)
	{
		CommLog_Print(WLOG_WARN, "TIOCMGET ioctl failed, errno=[%d] %s", errno, strerror(errno));
		SetLastError(ERROR_IO_DEVICE);
		return FALSE;
	}

	*pMask = 0;

	if (!(lines & TIOCM_DTR))
		*pMask |= SERIAL_DTR_STATE;
	if (!(lines & TIOCM_RTS))
		*pMask |= SERIAL_RTS_STATE;

	return TRUE;
}


BOOL _config_size(WINPR_COMM *pComm, ULONG *pSize)
{
	/* http://msdn.microsoft.com/en-us/library/ff546548%28v=vs.85%29.aspx */
	pSize = 0;
	return TRUE;
}


BOOL _immediate_char(WINPR_COMM *pComm, const UCHAR *pChar)
{
	BOOL result;
	DWORD nbBytesWritten = -1;

	/* FIXME: CommWriteFile uses a critical section, shall it be
	 * interrupted?
	 *
	 * FIXME: see also _get_commstatus()'s WaitForImmediate boolean
	 */

	result = CommWriteFile(pComm, pChar, 1, &nbBytesWritten, NULL);

	assert(nbBytesWritten == 1);

	return result;
}


BOOL _reset_device(WINPR_COMM *pComm)
{
	/* http://msdn.microsoft.com/en-us/library/dn265347%28v=vs.85%29.aspx */
	return TRUE;
}

static SERIAL_DRIVER _SerialSys =
{
	.id		  = SerialDriverSerialSys,
	.name		  = _T("Serial.sys"),
	.set_baud_rate	  = _set_baud_rate,
	.get_baud_rate	  = _get_baud_rate,
	.get_properties   = _get_properties,
	.set_serial_chars = _set_serial_chars,
	.get_serial_chars = _get_serial_chars,
	.set_line_control = _set_line_control,
	.get_line_control = _get_line_control,
	.set_handflow     = _set_handflow,
	.get_handflow     = _get_handflow,
	.set_timeouts     = _set_timeouts,
	.get_timeouts     = _get_timeouts,
	.set_dtr          = _set_dtr,
	.clear_dtr        = _clear_dtr,
	.set_rts          = _set_rts,
	.clear_rts        = _clear_rts,
	.get_modemstatus  = _get_modemstatus,
	.set_wait_mask    = _set_wait_mask,
	.get_wait_mask    = _get_wait_mask,
	.wait_on_mask     = _wait_on_mask,
	.set_queue_size   = _set_queue_size,
	.purge            = _purge,
	.get_commstatus   = _get_commstatus,
	.set_break_on     = _set_break_on,
	.set_break_off    = _set_break_off,
	.set_xoff         = _set_xoff,
	.set_xon          = _set_xon,
	.get_dtrrts       = _get_dtrrts,
	.config_size      = _config_size,
	.immediate_char   = _immediate_char,
	.reset_device     = _reset_device,
};


SERIAL_DRIVER* SerialSys_s()
{
	return &_SerialSys;
}

#endif /* __linux__ */
