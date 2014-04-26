/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Serial Port Device Service Virtual Channel
 *
 * Copyright 2011 O.S. Systems Software Ltda.
 * Copyright 2011 Eduardo Fiss Beloni <beloni@ossystems.com.br>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/stream.h>

#include <freerdp/utils/svc_plugin.h>
#include <freerdp/channels/rdpdr.h>

#ifndef _WIN32
#include <unistd.h>
#include <termios.h>
#include <dirent.h>
#include <sys/ioctl.h>
#endif

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include "serial_tty.h"
#include "serial_constants.h"

#ifdef HAVE_SYS_MODEM_H
#include <sys/modem.h>
#endif
#ifdef HAVE_SYS_FILIO_H
#include <sys/filio.h>
#endif
#ifdef HAVE_SYS_STRTIO_H
#include <sys/strtio.h>
#endif

#ifndef CRTSCTS
#define CRTSCTS 0
#endif

/* FIONREAD should really do the same thing as TIOCINQ, where it is not available */

#if !defined(TIOCINQ) && defined(FIONREAD)
#define TIOCINQ FIONREAD
#endif

#if !defined(TIOCOUTQ) && defined(FIONWRITE)
#define TIOCOUTQ FIONWRITE
#endif

/**
 * Refer to ReactOS's ntddser.h (public domain) for constant definitions
 */

static UINT32 tty_write_data(SERIAL_TTY* tty, BYTE* data, int len);
static void tty_set_termios(SERIAL_TTY* tty);
static BOOL tty_get_termios(SERIAL_TTY* tty);
static int tty_get_error_status();

UINT32 serial_tty_control(SERIAL_TTY* tty, UINT32 IoControlCode, wStream* input, wStream* output, UINT32* abortIo)
{
	UINT32 result;
	BYTE immediate;
	int purge_mask;
	UINT32 modemstate;
	UINT32 begPos, endPos;
	UINT32 OutputBufferLength;
	UINT32 status = STATUS_SUCCESS;
	UINT32 IoCtlDeviceType;
	UINT32 IoCtlFunction;
	UINT32 IoCtlMethod;
	UINT32 IoCtlAccess;

	IoCtlMethod = (IoControlCode & 0x3);
	IoCtlFunction = ((IoControlCode >> 2) & 0xFFF);
	IoCtlAccess = ((IoControlCode >> 14) & 0x3);
	IoCtlDeviceType = ((IoControlCode >> 16) & 0xFFFF);

	/**
	 * FILE_DEVICE_SERIAL_PORT		0x0000001B
	 * FILE_DEVICE_UNKNOWN			0x00000022
	 */

	if (IoCtlDeviceType == 0x00000022)
	{
		IoControlCode &= 0xFFFF;
		IoControlCode |= (0x0000001B << 16);
	}

	Stream_Seek_UINT32(output); /* OutputBufferLength (4 bytes) */
	begPos = (UINT32) Stream_GetPosition(output);

	switch (IoControlCode)
	{
		case IOCTL_SERIAL_SET_BAUD_RATE:
			Stream_Read_UINT32(input, tty->baud_rate);
			tty_set_termios(tty);
			DEBUG_SVC("SERIAL_SET_BAUD_RATE %d", tty->baud_rate);
			break;

		case IOCTL_SERIAL_GET_BAUD_RATE:
			OutputBufferLength = 4;
			Stream_Write_UINT32(output, tty->baud_rate);
			DEBUG_SVC("SERIAL_GET_BAUD_RATE %d", tty->baud_rate);
			break;

		case IOCTL_SERIAL_SET_QUEUE_SIZE:
			Stream_Read_UINT32(input, tty->queue_in_size);
			Stream_Read_UINT32(input, tty->queue_out_size);
			DEBUG_SVC("SERIAL_SET_QUEUE_SIZE in %d out %d", tty->queue_in_size, tty->queue_out_size);
			break;

		case IOCTL_SERIAL_SET_LINE_CONTROL:
			Stream_Read_UINT8(input, tty->stop_bits);
			Stream_Read_UINT8(input, tty->parity);
			Stream_Read_UINT8(input, tty->word_length);
			tty_set_termios(tty);
			DEBUG_SVC("SERIAL_SET_LINE_CONTROL stop %d parity %d word %d",
				tty->stop_bits, tty->parity, tty->word_length);
			break;

		case IOCTL_SERIAL_GET_LINE_CONTROL:
			DEBUG_SVC("SERIAL_GET_LINE_CONTROL");
			OutputBufferLength = 3;
			Stream_Write_UINT8(output, tty->stop_bits);
			Stream_Write_UINT8(output, tty->parity);
			Stream_Write_UINT8(output, tty->word_length);
			break;

		case IOCTL_SERIAL_IMMEDIATE_CHAR:
			DEBUG_SVC("SERIAL_IMMEDIATE_CHAR");
			Stream_Read_UINT8(input, immediate);
			tty_write_data(tty, &immediate, 1);
			break;

		case IOCTL_SERIAL_CONFIG_SIZE:
			DEBUG_SVC("SERIAL_CONFIG_SIZE");
			OutputBufferLength = 4;
			Stream_Write_UINT32(output, 0);
			break;

		case IOCTL_SERIAL_GET_CHARS:
			DEBUG_SVC("SERIAL_GET_CHARS");
			OutputBufferLength = 6;
			Stream_Write(output, tty->chars, 6);
			break;

		case IOCTL_SERIAL_SET_CHARS:
			DEBUG_SVC("SERIAL_SET_CHARS");
			Stream_Read(input, tty->chars, 6);
			tty_set_termios(tty);
			break;

		case IOCTL_SERIAL_GET_HANDFLOW:
			OutputBufferLength = 16;
			tty_get_termios(tty);
			Stream_Write_UINT32(output, tty->control);
			Stream_Write_UINT32(output, tty->xonoff);
			Stream_Write_UINT32(output, tty->onlimit);
			Stream_Write_UINT32(output, tty->offlimit);
			DEBUG_SVC("IOCTL_SERIAL_GET_HANDFLOW %X %X %X %X",
				tty->control, tty->xonoff, tty->onlimit, tty->offlimit);
			break;

		case IOCTL_SERIAL_SET_HANDFLOW:
			Stream_Read_UINT32(input, tty->control);
			Stream_Read_UINT32(input, tty->xonoff);
			Stream_Read_UINT32(input, tty->onlimit);
			Stream_Read_UINT32(input, tty->offlimit);
			DEBUG_SVC("IOCTL_SERIAL_SET_HANDFLOW %X %X %X %X",
				tty->control, tty->xonoff, tty->onlimit, tty->offlimit);
			tty_set_termios(tty);
			break;

		case IOCTL_SERIAL_SET_TIMEOUTS:
			Stream_Read_UINT32(input, tty->read_interval_timeout);
			Stream_Read_UINT32(input, tty->read_total_timeout_multiplier);
			Stream_Read_UINT32(input, tty->read_total_timeout_constant);
			Stream_Read_UINT32(input, tty->write_total_timeout_multiplier);
			Stream_Read_UINT32(input, tty->write_total_timeout_constant);

			/* http://www.codeproject.com/KB/system/chaiyasit_t.aspx, see 'ReadIntervalTimeout' section
				http://msdn.microsoft.com/en-us/library/ms885171.aspx */
			if (tty->read_interval_timeout == SERIAL_TIMEOUT_MAX)
			{
				tty->read_interval_timeout = 0;
				tty->read_total_timeout_multiplier = 0;
			}

			DEBUG_SVC("SERIAL_SET_TIMEOUTS read timeout %d %d %d",
				tty->read_interval_timeout,
				tty->read_total_timeout_multiplier,
				tty->read_total_timeout_constant);
			break;

		case IOCTL_SERIAL_GET_TIMEOUTS:
			DEBUG_SVC("SERIAL_GET_TIMEOUTS read timeout %d %d %d",
				tty->read_interval_timeout,
				tty->read_total_timeout_multiplier,
				tty->read_total_timeout_constant);
			OutputBufferLength = 20;
			Stream_Write_UINT32(output, tty->read_interval_timeout);
			Stream_Write_UINT32(output, tty->read_total_timeout_multiplier);
			Stream_Write_UINT32(output, tty->read_total_timeout_constant);
			Stream_Write_UINT32(output, tty->write_total_timeout_multiplier);
			Stream_Write_UINT32(output, tty->write_total_timeout_constant);
			break;

		case IOCTL_SERIAL_GET_WAIT_MASK:
			DEBUG_SVC("SERIAL_GET_WAIT_MASK %X", tty->wait_mask);
			OutputBufferLength = 4;
			Stream_Write_UINT32(output, tty->wait_mask);
			break;

		case IOCTL_SERIAL_SET_WAIT_MASK:
			Stream_Read_UINT32(input, tty->wait_mask);
			DEBUG_SVC("SERIAL_SET_WAIT_MASK %X", tty->wait_mask);
			break;

		case IOCTL_SERIAL_SET_DTR:
			DEBUG_SVC("SERIAL_SET_DTR");
			ioctl(tty->fd, TIOCMGET, &result);
			result |= TIOCM_DTR;
			ioctl(tty->fd, TIOCMSET, &result);
			tty->dtr = 1;
			break;

		case IOCTL_SERIAL_CLR_DTR:
			DEBUG_SVC("SERIAL_CLR_DTR");
			ioctl(tty->fd, TIOCMGET, &result);
			result &= ~TIOCM_DTR;
			ioctl(tty->fd, TIOCMSET, &result);
			tty->dtr = 0;
			break;

		case IOCTL_SERIAL_SET_RTS:
			DEBUG_SVC("SERIAL_SET_RTS");
			ioctl(tty->fd, TIOCMGET, &result);
			result |= TIOCM_RTS;
			ioctl(tty->fd, TIOCMSET, &result);
			tty->rts = 1;
			break;

		case IOCTL_SERIAL_CLR_RTS:
			DEBUG_SVC("SERIAL_CLR_RTS");
			ioctl(tty->fd, TIOCMGET, &result);
			result &= ~TIOCM_RTS;
			ioctl(tty->fd, TIOCMSET, &result);
			tty->rts = 0;
			break;

		case IOCTL_SERIAL_GET_MODEMSTATUS:
			modemstate = 0;
#ifdef TIOCMGET
			ioctl(tty->fd, TIOCMGET, &result);
			if (result & TIOCM_CTS)
				modemstate |= SERIAL_MS_CTS;
			if (result & TIOCM_DSR)
				modemstate |= SERIAL_MS_DSR;
			if (result & TIOCM_RNG)
				modemstate |= SERIAL_MS_RNG;
			if (result & TIOCM_CAR)
				modemstate |= SERIAL_MS_CAR;
			if (result & TIOCM_DTR)
				modemstate |= SERIAL_MS_DTR;
			if (result & TIOCM_RTS)
				modemstate |= SERIAL_MS_RTS;
#endif
			DEBUG_SVC("SERIAL_GET_MODEMSTATUS %X", modemstate);
			OutputBufferLength = 4;
			Stream_Write_UINT32(output, modemstate);
			break;

		case IOCTL_SERIAL_GET_COMMSTATUS:
			OutputBufferLength = 18;
			Stream_Write_UINT32(output, 0);	/* Errors */
			Stream_Write_UINT32(output, 0);	/* Hold reasons */

			result = 0;
#ifdef TIOCINQ
			ioctl(tty->fd, TIOCINQ, &result);
#endif
			Stream_Write_UINT32(output, result); /* Amount in in queue */
			if (result)
				DEBUG_SVC("SERIAL_GET_COMMSTATUS in queue %d", result);

			result = 0;
#ifdef TIOCOUTQ
			ioctl(tty->fd, TIOCOUTQ, &result);
#endif
			Stream_Write_UINT32(output, result);	/* Amount in out queue */
			DEBUG_SVC("SERIAL_GET_COMMSTATUS out queue %d", result);

			Stream_Write_UINT8(output, 0); /* EofReceived */
			Stream_Write_UINT8(output, 0); /* WaitForImmediate */
			break;

		case IOCTL_SERIAL_PURGE:
			Stream_Read_UINT32(input, purge_mask);
			DEBUG_SVC("SERIAL_PURGE purge_mask %X", purge_mask);

		/*	See http://msdn.microsoft.com/en-us/library/ms901431.aspx
			PURGE_TXCLEAR 	Clears the output buffer, if the driver has one.
			PURGE_RXCLEAR 	Clears the input buffer, if the driver has one.

			It clearly states to clear the *driver* buffer, not the port buffer
		*/

#ifdef DEBUG_SVC
			if (purge_mask & SERIAL_PURGE_TXCLEAR)
				DEBUG_SVC("Ignoring SERIAL_PURGE_TXCLEAR");
			if (purge_mask & SERIAL_PURGE_RXCLEAR)
				DEBUG_SVC("Ignoring SERIAL_PURGE_RXCLEAR");
#endif

			if (purge_mask & SERIAL_PURGE_TXABORT)
				*abortIo |= SERIAL_ABORT_IO_WRITE;
			if (purge_mask & SERIAL_PURGE_RXABORT)
				*abortIo |= SERIAL_ABORT_IO_READ;
			break;
		case IOCTL_SERIAL_WAIT_ON_MASK:
			DEBUG_SVC("SERIAL_WAIT_ON_MASK %X", tty->wait_mask);
			tty->event_pending = 1;
			OutputBufferLength = 4;
			if (serial_tty_get_event(tty, &result))
			{
				DEBUG_SVC("WAIT end  event = %X", result);
				Stream_Write_UINT32(output, result);
				break;
			}
			status = STATUS_PENDING;
			break;

		case IOCTL_SERIAL_SET_BREAK_ON:
			DEBUG_SVC("SERIAL_SET_BREAK_ON");
			tcsendbreak(tty->fd, 0);
			break;

		case IOCTL_SERIAL_RESET_DEVICE:
			DEBUG_SVC("SERIAL_RESET_DEVICE");
			break;

		case IOCTL_SERIAL_SET_BREAK_OFF:
			DEBUG_SVC("SERIAL_SET_BREAK_OFF");
			break;

		case IOCTL_SERIAL_SET_XOFF:
			DEBUG_SVC("SERIAL_SET_XOFF");
			break;

		case IOCTL_SERIAL_SET_XON:
			DEBUG_SVC("SERIAL_SET_XON");
			tcflow(tty->fd, TCION);
			break;

		default:
			DEBUG_SVC("NOT FOUND IoControlCode SERIAL IOCTL 0x%08X", IoControlCode);
			return STATUS_INVALID_PARAMETER;
	}

	endPos = (UINT32) Stream_GetPosition(output);
	OutputBufferLength = endPos - begPos;

	if (OutputBufferLength < 1)
	{
		Stream_Write_UINT8(output, 0); /* Padding (1 byte) */
		endPos = (UINT32) Stream_GetPosition(output);
		OutputBufferLength = endPos - begPos;
	}

	Stream_SealLength(output);

	Stream_SetPosition(output, 16);
	Stream_Write_UINT32(output, OutputBufferLength); /* OutputBufferLength (4 bytes) */
	Stream_SetPosition(output, endPos);

	return status;
}

BOOL serial_tty_read(SERIAL_TTY* tty, BYTE* buffer, UINT32* Length)
{
	ssize_t status;
	long timeout = 90;

	/* Set timeouts kind of like the windows serial timeout parameters. Multiply timeout
	   with requested read size */
	if (tty->read_total_timeout_multiplier | tty->read_total_timeout_constant)
	{
		timeout =
			(tty->read_total_timeout_multiplier * (*Length) +
			 tty->read_total_timeout_constant + 99) / 100;
	}
	else if (tty->read_interval_timeout)
	{
		timeout = (tty->read_interval_timeout * (*Length) + 99) / 100;
	}

        if (tty->timeout != timeout)
        {
		struct termios* ptermios;

		ptermios = (struct termios*) calloc(1, sizeof(struct termios));

		if (tcgetattr(tty->fd, ptermios) < 0) {
			free(ptermios);
			return FALSE;
		}

		/**
		 * If a timeout is set, do a blocking read, which times out after some time.
		 * It will make FreeRDP less responsive, but it will improve serial performance,
		 * by not reading one character at a time.
		 */

		if (timeout == 0)
		{
			ptermios->c_cc[VTIME] = 0;
			ptermios->c_cc[VMIN] = 0;
		}
		else
		{
			ptermios->c_cc[VTIME] = timeout;
			ptermios->c_cc[VMIN] = 1;
		}

		tcsetattr(tty->fd, TCSANOW, ptermios);
		tty->timeout = timeout;
		free(ptermios);
	}

	ZeroMemory(buffer, *Length);

	status = read(tty->fd, buffer, *Length);

	if (status < 0)
	{
		DEBUG_WARN("failed with %zd, errno=[%d] %s\n", 
				status, errno, strerror(errno));
		return FALSE;
	}

	tty->event_txempty = status;
	*Length = status;

	return TRUE;
}

int serial_tty_write(SERIAL_TTY* tty, BYTE* buffer, UINT32 Length)
{
	ssize_t status = 0;
	UINT32 event_txempty = Length;

	while (Length > 0)
	{
		status = write(tty->fd, buffer, Length);

		if (status < 0)
		{
			if (errno == EAGAIN)
				status = 0;
			else
				return status;
		}

		Length -= status;
		buffer += status;
	}

	tty->event_txempty = event_txempty;

	return status;
}

/**
 * This function is used to deallocated a SERIAL_TTY structure.
 *
 * @param tty [in]	- pointer to the SERIAL_TTY structure to deallocate.
 * 					  This will typically be allocated by a call to serial_tty_new().
 *					  On return, this pointer is invalid.
 */
void serial_tty_free(SERIAL_TTY* tty)
{
	if (!tty)
		return;

	if (tty->fd >= 0)
	{
		if (tty->pold_termios)
			tcsetattr(tty->fd, TCSANOW, tty->pold_termios);

		close(tty->fd);
	}

	free(tty->ptermios);
	free(tty->pold_termios);
	free(tty);
}

SERIAL_TTY* serial_tty_new(const char* path, UINT32 id)
{
	SERIAL_TTY* tty;

	tty = (SERIAL_TTY*) calloc(1, sizeof(SERIAL_TTY));

	if (!tty)
		return NULL;

	tty->id = id;
	tty->fd = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK);

	if (tty->fd < 0)
	{
		perror("open");
		DEBUG_WARN("failed to open device %s", path);
		serial_tty_free(tty);
		return NULL;
	}
	else
	{
		DEBUG_SVC("tty fd %d successfully opened", tty->fd);
	}

	tty->ptermios = (struct termios*) calloc(1, sizeof(struct termios));

	if (!tty->ptermios)
	{
		serial_tty_free(tty);
		return NULL;
	}

	tty->pold_termios = (struct termios*) calloc(1, sizeof(struct termios));

	if (!tty->pold_termios)
	{
		serial_tty_free(tty);
		return NULL;
	}
	tcgetattr(tty->fd, tty->pold_termios);

	if (!tty_get_termios(tty))
	{
		DEBUG_WARN("%s access denied", path);
		fflush(stdout);
		serial_tty_free(tty);
		return NULL;
	}

	tty->ptermios->c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
	tty->ptermios->c_oflag &= ~OPOST;
	tty->ptermios->c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	tty->ptermios->c_cflag &= ~(CSIZE | PARENB);
	tty->ptermios->c_cflag |= CS8;

	tty->ptermios->c_iflag = IGNPAR;
	tty->ptermios->c_cflag |= CLOCAL | CREAD;

	tcsetattr(tty->fd, TCSANOW, tty->ptermios);

	tty->event_txempty = 0;
	tty->event_cts = 0;
	tty->event_dsr = 0;
	tty->event_rlsd = 0;
	tty->event_pending = 0;

	/* all read and writes should be non-blocking */
	if (fcntl(tty->fd, F_SETFL, O_NONBLOCK) == -1)
	{
		DEBUG_WARN("%s fcntl", path);
		perror("fcntl");
		serial_tty_free(tty) ;
		return NULL;
	}

	tty->read_total_timeout_constant = 5;

	return tty;
}

BOOL serial_tty_get_event(SERIAL_TTY* tty, UINT32* result)
{
	int bytes;
	BOOL status = FALSE;

	*result = 0;

#ifdef TIOCINQ
	/* When wait_mask is set to zero we ought to cancel it all
	   For reference: http://msdn.microsoft.com/en-us/library/aa910487.aspx */
	if (tty->wait_mask == 0)
	{
		tty->event_pending = 0;
		return TRUE;
	}

	ioctl(tty->fd, TIOCINQ, &bytes);

	if (bytes > 0)
	{
		DEBUG_SVC("bytes %d", bytes);

		if (bytes > tty->event_rlsd)
		{
			tty->event_rlsd = bytes;

			if (tty->wait_mask & SERIAL_EV_RLSD)
			{
				DEBUG_SVC("SERIAL_EV_RLSD");
				*result |= SERIAL_EV_RLSD;
				status = TRUE;
			}
		}

		if ((bytes > 1) && (tty->wait_mask & SERIAL_EV_RXFLAG))
		{
			DEBUG_SVC("SERIAL_EV_RXFLAG bytes %d", bytes);
			*result |= SERIAL_EV_RXFLAG;
			status = TRUE;
		}

		if ((tty->wait_mask & SERIAL_EV_RXCHAR))
		{
			DEBUG_SVC("SERIAL_EV_RXCHAR bytes %d", bytes);
			*result |= SERIAL_EV_RXCHAR;
			status = TRUE;
		}
	}
	else
	{
		tty->event_rlsd = 0;
	}
#endif

#ifdef TIOCOUTQ
	ioctl(tty->fd, TIOCOUTQ, &bytes);
	if ((bytes == 0) && (tty->event_txempty > 0) && (tty->wait_mask & SERIAL_EV_TXEMPTY))
	{
		DEBUG_SVC("SERIAL_EV_TXEMPTY");
		*result |= SERIAL_EV_TXEMPTY;
		status = TRUE;
	}
	tty->event_txempty = bytes;
#endif

	ioctl(tty->fd, TIOCMGET, &bytes);
	if ((bytes & TIOCM_DSR) != tty->event_dsr)
	{
		tty->event_dsr = bytes & TIOCM_DSR;
		if (tty->wait_mask & SERIAL_EV_DSR)
		{
			DEBUG_SVC("SERIAL_EV_DSR %s", (bytes & TIOCM_DSR) ? "ON" : "OFF");
			*result |= SERIAL_EV_DSR;
			status = TRUE;
		}
	}

	if ((bytes & TIOCM_CTS) != tty->event_cts)
	{
		tty->event_cts = bytes & TIOCM_CTS;
		if (tty->wait_mask & SERIAL_EV_CTS)
		{
			DEBUG_SVC("SERIAL_EV_CTS %s", (bytes & TIOCM_CTS) ? "ON" : "OFF");
			*result |= SERIAL_EV_CTS;
			status = TRUE;
		}
	}

	if (status)
		tty->event_pending = 0;

	return status;
}

static BOOL tty_get_termios(SERIAL_TTY* tty)
{
	speed_t speed;
	struct termios* ptermios;
	ptermios = tty->ptermios;

	DEBUG_SVC("tcgetattr? %d", tcgetattr(tty->fd, ptermios) >= 0);

	if (tcgetattr(tty->fd, ptermios) < 0)
		return FALSE;

	speed = cfgetispeed(ptermios);

	switch (speed)
	{
#ifdef B75
		case B75:
			tty->baud_rate = 75;
			break;
#endif
#ifdef B110
		case B110:
			tty->baud_rate = 110;
			break;
#endif
#ifdef B134
		case B134:
			tty->baud_rate = 134;
			break;
#endif
#ifdef B150
		case B150:
			tty->baud_rate = 150;
			break;
#endif
#ifdef B300
		case B300:
			tty->baud_rate = 300;
			break;
#endif
#ifdef B600
		case B600:
			tty->baud_rate = 600;
			break;
#endif
#ifdef B1200
		case B1200:
			tty->baud_rate = 1200;
			break;
#endif
#ifdef B1800
		case B1800:
			tty->baud_rate = 1800;
			break;
#endif
#ifdef B2400
		case B2400:
			tty->baud_rate = 2400;
			break;
#endif
#ifdef B4800
		case B4800:
			tty->baud_rate = 4800;
			break;
#endif
#ifdef B9600
		case B9600:
			tty->baud_rate = 9600;
			break;
#endif
#ifdef B19200
		case B19200:
			tty->baud_rate = 19200;
			break;
#endif
#ifdef B38400
		case B38400:
			tty->baud_rate = 38400;
			break;
#endif
#ifdef B57600
		case B57600:
			tty->baud_rate = 57600;
			break;
#endif
#ifdef B115200
		case B115200:
			tty->baud_rate = 115200;
			break;
#endif
#ifdef B230400
		case B230400:
			tty->baud_rate = 230400;
			break;
#endif
#ifdef B460800
		case B460800:
			tty->baud_rate = 460800;
			break;
#endif
		default:
			tty->baud_rate = 9600;
			break;
	}

	speed = cfgetospeed(ptermios);
	tty->dtr = (speed == B0) ? 0 : 1;

	tty->stop_bits = (ptermios->c_cflag & CSTOPB) ? SERIAL_STOP_BITS_2 : SERIAL_STOP_BITS_1;
	tty->parity =
		(ptermios->c_cflag & PARENB) ? ((ptermios->c_cflag & PARODD) ? SERIAL_ODD_PARITY :
						SERIAL_EVEN_PARITY) : SERIAL_NO_PARITY;
	switch (ptermios->c_cflag & CSIZE)
	{
		case CS5:
			tty->word_length = 5;
			break;
		case CS6:
			tty->word_length = 6;
			break;
		case CS7:
			tty->word_length = 7;
			break;
		default:
			tty->word_length = 8;
			break;
	}

	if (ptermios->c_cflag & CRTSCTS)
	{
		tty->control = SERIAL_DTR_CONTROL | SERIAL_CTS_HANDSHAKE | SERIAL_ERROR_ABORT;
	}
	else
	{
		tty->control = SERIAL_DTR_CONTROL | SERIAL_ERROR_ABORT;
	}

	tty->xonoff = SERIAL_DSR_SENSITIVITY;

	if (ptermios->c_iflag & IXON)
		tty->xonoff |= SERIAL_XON_HANDSHAKE;

	if (ptermios->c_iflag & IXOFF)
		tty->xonoff |= SERIAL_XOFF_HANDSHAKE;

	tty->chars[SERIAL_CHAR_XON] = ptermios->c_cc[VSTART];
	tty->chars[SERIAL_CHAR_XOFF] = ptermios->c_cc[VSTOP];
	tty->chars[SERIAL_CHAR_EOF] = ptermios->c_cc[VEOF];
	tty->chars[SERIAL_CHAR_BREAK] = ptermios->c_cc[VINTR];
	tty->chars[SERIAL_CHAR_ERROR] = ptermios->c_cc[VKILL];

	tty->timeout = ptermios->c_cc[VTIME];

	return TRUE;
}

static void tty_set_termios(SERIAL_TTY* tty)
{
	speed_t speed;
	struct termios* ptermios;

	ptermios = tty->ptermios;

	switch (tty->baud_rate)
	{
#ifdef B75
		case 75:
			speed = B75;
			break;
#endif
#ifdef B110
		case 110:
			speed = B110;
			break;
#endif
#ifdef B134
		case 134:
			speed = B134;
			break;
#endif
#ifdef B150
		case 150:
			speed = B150;
			break;
#endif
#ifdef B300
		case 300:
			speed = B300;
			break;
#endif
#ifdef B600
		case 600:
			speed = B600;
			break;
#endif
#ifdef B1200
		case 1200:
			speed = B1200;
			break;
#endif
#ifdef B1800
		case 1800:
			speed = B1800;
			break;
#endif
#ifdef B2400
		case 2400:
			speed = B2400;
			break;
#endif
#ifdef B4800
		case 4800:
			speed = B4800;
			break;
#endif
#ifdef B9600
		case 9600:
			speed = B9600;
			break;
#endif
#ifdef B19200
		case 19200:
			speed = B19200;
			break;
#endif
#ifdef B38400
		case 38400:
			speed = B38400;
			break;
#endif
#ifdef B57600
		case 57600:
			speed = B57600;
			break;
#endif
#ifdef B115200
		case 115200:
			speed = B115200;
			break;
#endif
#ifdef B230400
		case 230400:
			speed = B115200;
			break;
#endif
#ifdef B460800
		case 460800:
			speed = B115200;
			break;
#endif
		default:
			speed = B9600;
			break;
	}

#ifdef CBAUD
	ptermios->c_cflag &= ~CBAUD;
	ptermios->c_cflag |= speed;
#else
	/* on systems with separate ispeed and ospeed, we can remember the speed
	   in ispeed while changing DTR with ospeed */
	cfsetispeed(tty->ptermios, speed);
	cfsetospeed(tty->ptermios, tty->dtr ? speed : 0);
#endif

	ptermios->c_cflag &= ~(CSTOPB | PARENB | PARODD | CSIZE | CRTSCTS);

	switch (tty->stop_bits)
	{
		case SERIAL_STOP_BITS_2:
			ptermios->c_cflag |= CSTOPB;
			break;

		default:
			ptermios->c_cflag &= ~CSTOPB;
			break;
	}

	switch (tty->parity)
	{
		case SERIAL_EVEN_PARITY:
			ptermios->c_cflag |= PARENB;
			break;

		case SERIAL_ODD_PARITY:
			ptermios->c_cflag |= PARENB | PARODD;
			break;

		case SERIAL_NO_PARITY:
			ptermios->c_cflag &= ~(PARENB | PARODD);
			break;
	}

	switch (tty->word_length)
	{
		case 5:
			ptermios->c_cflag |= CS5;
			break;
		case 6:
			ptermios->c_cflag |= CS6;
			break;
		case 7:
			ptermios->c_cflag |= CS7;
			break;
		default:
			ptermios->c_cflag |= CS8;
			break;
	}

#if 0
	if (tty->rts)
		ptermios->c_cflag |= CRTSCTS;
	else
		ptermios->c_cflag &= ~CRTSCTS;
#endif

	if (tty->control & SERIAL_CTS_HANDSHAKE)
	{
		ptermios->c_cflag |= CRTSCTS;
	}
	else
	{
		ptermios->c_cflag &= ~CRTSCTS;
	}


	if (tty->xonoff & SERIAL_XON_HANDSHAKE)
	{
		ptermios->c_iflag |= IXON | IMAXBEL;
	}
	if (tty->xonoff & SERIAL_XOFF_HANDSHAKE)
	{
		ptermios->c_iflag |= IXOFF | IMAXBEL;
	}

	if ((tty->xonoff & (SERIAL_XOFF_HANDSHAKE | SERIAL_XON_HANDSHAKE)) == 0)
	{
		ptermios->c_iflag &= ~IXON;
		ptermios->c_iflag &= ~IXOFF;
	}

	ptermios->c_cc[VSTART] = tty->chars[SERIAL_CHAR_XON];
	ptermios->c_cc[VSTOP] = tty->chars[SERIAL_CHAR_XOFF];
	ptermios->c_cc[VEOF] = tty->chars[SERIAL_CHAR_EOF];
	ptermios->c_cc[VINTR] = tty->chars[SERIAL_CHAR_BREAK];
	ptermios->c_cc[VKILL] = tty->chars[SERIAL_CHAR_ERROR];

	tcsetattr(tty->fd, TCSANOW, ptermios);
}

static UINT32 tty_write_data(SERIAL_TTY* tty, BYTE* data, int len)
{
	ssize_t status;

	status = write(tty->fd, data, len);

	if (status < 0)
		return tty_get_error_status();

	tty->event_txempty = status;

	return STATUS_SUCCESS;
}

static int tty_get_error_status()
{
	switch (errno)
	{
		case EACCES:
		case ENOTDIR:
		case ENFILE:
			return STATUS_ACCESS_DENIED;
		case EISDIR:
			return STATUS_FILE_IS_A_DIRECTORY;
		case EEXIST:
			return STATUS_OBJECT_NAME_COLLISION;
		case EBADF:
			return STATUS_INVALID_HANDLE;
		default:
			return STATUS_NO_SUCH_FILE;
	}
}
