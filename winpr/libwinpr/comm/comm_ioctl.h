/**
 * WinPR: Windows Portable Runtime
 * Serial Communication API
 *
 * Copyright 2011 O.S. Systems Software Ltda.
 * Copyright 2011 Eduardo Fiss Beloni <beloni@ossystems.com.br>
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

#ifndef WINPR_COMM_IOCTL_H_
#define WINPR_COMM_IOCTL_H_

#ifndef _WIN32

#include <termios.h>

#include <winpr/io.h>
#include <winpr/tchar.h>
#include <winpr/wtypes.h>

#include "comm.h"

/* Ntddser.h http://msdn.microsoft.com/en-us/cc308432.aspx
 * Ntddpar.h http://msdn.microsoft.com/en-us/cc308431.aspx
 */

#ifdef __cplusplus 
extern "C" { 
#endif


#define IOCTL_SERIAL_SET_BAUD_RATE	0x001B0004
#define IOCTL_SERIAL_GET_BAUD_RATE	0x001B0050

/* #define IOCTL_SERIAL_SET_LINE_CONTROL	0x001B000C */
/* IOCTL_SERIAL_GET_LINE_CONTROL 0x001B0054 */
/* IOCTL_SERIAL_SET_TIMEOUTS 0x001B001C */
/* IOCTL_SERIAL_GET_TIMEOUTS 0x001B0020 */

/* GET_CHARS and SET_CHARS are swapped in the RDP docs [MS-RDPESP] */
#define IOCTL_SERIAL_GET_CHARS		0x001B0058
#define IOCTL_SERIAL_SET_CHARS		0x001B005C

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
#define IOCTL_SERIAL_GET_PROPERTIES	0x001B0074
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



typedef struct _SERIAL_BAUD_RATE
{
	ULONG BaudRate;
} SERIAL_BAUD_RATE, *PSERIAL_BAUD_RATE;


typedef struct _SERIAL_CHARS
{
	UCHAR EofChar;
	UCHAR ErrorChar;
	UCHAR BreakChar;
	UCHAR EventChar;
	UCHAR XonChar;
	UCHAR XoffChar;
} SERIAL_CHARS, *PSERIAL_CHARS;


/**
 * A function might be NULL if not supported by the underlying remote driver.
 *
 * FIXME: better have to use input and output buffers for all functions?
 */
typedef struct _REMOTE_SERIAL_DRIVER
{
	REMOTE_SERIAL_DRIVER_ID id;
	TCHAR *name;
	BOOL (*set_baud_rate)(WINPR_COMM *pComm, const SERIAL_BAUD_RATE *pBaudRate);
	BOOL (*get_baud_rate)(WINPR_COMM *pComm, SERIAL_BAUD_RATE *pBaudRate);
	BOOL (*get_properties)(WINPR_COMM *pComm, COMMPROP *pProperties);
	BOOL (*set_serial_chars)(WINPR_COMM *pComm, const SERIAL_CHARS *pSerialChars);
	BOOL (*get_serial_chars)(WINPR_COMM *pComm, SERIAL_CHARS *pSerialChars);

} REMOTE_SERIAL_DRIVER;


int _comm_ioctl_tcsetattr(int fd, int optional_actions, const struct termios *termios_p);

#ifdef __cplusplus
}
#endif

#endif /* _WIN32 */

#endif /* WINPR_COMM_IOCTL_H_ */
