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

/* Serial I/O Request Interface: http://msdn.microsoft.com/en-us/library/dn265347%28v=vs.85%29.aspx
 * Ntddser.h http://msdn.microsoft.com/en-us/cc308432.aspx
 * Ntddpar.h http://msdn.microsoft.com/en-us/cc308431.aspx
 */

#ifdef __cplusplus 
extern "C" { 
#endif



#define STOP_BIT_1	0 
#define STOP_BITS_1_5	1 
#define STOP_BITS_2	2 
 
#define NO_PARITY	0 
#define ODD_PARITY	1 
#define EVEN_PARITY	2 
#define MARK_PARITY	3 
#define SPACE_PARITY	4 


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


typedef struct _SERIAL_LINE_CONTROL
{
	UCHAR StopBits;
	UCHAR Parity;
	UCHAR WordLength;
} SERIAL_LINE_CONTROL, *PSERIAL_LINE_CONTROL;


typedef struct _SERIAL_HANDFLOW
{
	ULONG ControlHandShake;
	ULONG FlowReplace;
	LONG  XonLimit;
	LONG  XoffLimit;
} SERIAL_HANDFLOW, *PSERIAL_HANDFLOW;


#define SERIAL_DTR_MASK           ((ULONG)0x03)
#define SERIAL_DTR_CONTROL        ((ULONG)0x01) 
#define SERIAL_DTR_HANDSHAKE      ((ULONG)0x02) 
#define SERIAL_CTS_HANDSHAKE      ((ULONG)0x08) 
#define SERIAL_DSR_HANDSHAKE      ((ULONG)0x10) 
#define SERIAL_DCD_HANDSHAKE      ((ULONG)0x20) 
#define SERIAL_OUT_HANDSHAKEMASK  ((ULONG)0x38) 
#define SERIAL_DSR_SENSITIVITY    ((ULONG)0x40) 
#define SERIAL_ERROR_ABORT        ((ULONG)0x80000000) 
#define SERIAL_CONTROL_INVALID    ((ULONG)0x7fffff84) 
#define SERIAL_AUTO_TRANSMIT      ((ULONG)0x01) 
#define SERIAL_AUTO_RECEIVE       ((ULONG)0x02) 
#define SERIAL_ERROR_CHAR         ((ULONG)0x04) 
#define SERIAL_NULL_STRIPPING     ((ULONG)0x08) 
#define SERIAL_BREAK_CHAR         ((ULONG)0x10) 
#define SERIAL_RTS_MASK           ((ULONG)0xc0) 
#define SERIAL_RTS_CONTROL        ((ULONG)0x40) 
#define SERIAL_RTS_HANDSHAKE      ((ULONG)0x80) 
#define SERIAL_TRANSMIT_TOGGLE    ((ULONG)0xc0) 
#define SERIAL_XOFF_CONTINUE      ((ULONG)0x80000000) 
#define SERIAL_FLOW_INVALID       ((ULONG)0x7fffff20) 

#define SERIAL_SP_SERIALCOMM         ((ULONG)0x00000001) 
 
#define SERIAL_SP_UNSPECIFIED       ((ULONG)0x00000000) 
#define SERIAL_SP_RS232             ((ULONG)0x00000001) 
#define SERIAL_SP_PARALLEL          ((ULONG)0x00000002) 
#define SERIAL_SP_RS422             ((ULONG)0x00000003) 
#define SERIAL_SP_RS423             ((ULONG)0x00000004) 
#define SERIAL_SP_RS449             ((ULONG)0x00000005) 
#define SERIAL_SP_MODEM             ((ULONG)0X00000006) 
#define SERIAL_SP_FAX               ((ULONG)0x00000021) 
#define SERIAL_SP_SCANNER           ((ULONG)0x00000022) 
#define SERIAL_SP_BRIDGE            ((ULONG)0x00000100) 
#define SERIAL_SP_LAT               ((ULONG)0x00000101) 
#define SERIAL_SP_TELNET            ((ULONG)0x00000102) 
#define SERIAL_SP_X25               ((ULONG)0x00000103) 


typedef struct _SERIAL_TIMEOUTS
{
	ULONG ReadIntervalTimeout; 
	ULONG ReadTotalTimeoutMultiplier; 
	ULONG ReadTotalTimeoutConstant; 
	ULONG WriteTotalTimeoutMultiplier; 
	ULONG WriteTotalTimeoutConstant; 
} SERIAL_TIMEOUTS,*PSERIAL_TIMEOUTS;


#define SERIAL_MSR_DCTS     0x01
#define SERIAL_MSR_DDSR     0x02
#define SERIAL_MSR_TERI     0x04
#define SERIAL_MSR_DDCD     0x08
#define SERIAL_MSR_CTS      0x10
#define SERIAL_MSR_DSR      0x20
#define SERIAL_MSR_RI       0x40
#define SERIAL_MSR_DCD      0x80


#define SERIAL_EV_RXCHAR           0x0001  
#define SERIAL_EV_RXFLAG           0x0002  
#define SERIAL_EV_TXEMPTY          0x0004  
#define SERIAL_EV_CTS              0x0008  
#define SERIAL_EV_DSR              0x0010  
#define SERIAL_EV_RLSD             0x0020  
#define SERIAL_EV_BREAK            0x0040  
#define SERIAL_EV_ERR              0x0080  
#define SERIAL_EV_RING             0x0100  
#define SERIAL_EV_PERR             0x0200  
#define SERIAL_EV_RX80FULL         0x0400  
#define SERIAL_EV_EVENT1           0x0800  
#define SERIAL_EV_EVENT2           0x1000  


typedef struct _SERIAL_QUEUE_SIZE
{
	ULONG InSize;
	ULONG OutSize;
} SERIAL_QUEUE_SIZE, *PSERIAL_QUEUE_SIZE;


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
	BOOL (*set_line_control)(WINPR_COMM *pComm, const SERIAL_LINE_CONTROL *pLineControl);
	BOOL (*get_line_control)(WINPR_COMM *pComm, SERIAL_LINE_CONTROL *pLineControl);
	BOOL (*set_handflow)(WINPR_COMM *pComm, const SERIAL_HANDFLOW *pHandflow);
	BOOL (*get_handflow)(WINPR_COMM *pComm, SERIAL_HANDFLOW *pHandflow);
	BOOL (*set_timeouts)(WINPR_COMM *pComm, const SERIAL_TIMEOUTS *pTimeouts);
	BOOL (*get_timeouts)(WINPR_COMM *pComm, SERIAL_TIMEOUTS *pTimeouts);
	BOOL (*set_dtr)(WINPR_COMM *pComm);
	BOOL (*clear_dtr)(WINPR_COMM *pComm);
	BOOL (*set_rts)(WINPR_COMM *pComm);
	BOOL (*clear_rts)(WINPR_COMM *pComm);
	BOOL (*get_modemstatus)(WINPR_COMM *pComm, ULONG *pRegister);
	BOOL (*set_wait_mask)(WINPR_COMM *pComm, const ULONG *pWaitMask);
	BOOL (*get_wait_mask)(WINPR_COMM *pComm, ULONG *pWaitMask);
	BOOL (*wait_on_mask)(WINPR_COMM *pComm, ULONG *pOutputMask);
	BOOL (*set_queue_size)(WINPR_COMM *pComm, const SERIAL_QUEUE_SIZE *pQueueSize);

} REMOTE_SERIAL_DRIVER;


int _comm_ioctl_tcsetattr(int fd, int optional_actions, const struct termios *termios_p);

#ifdef __cplusplus
}
#endif

#endif /* _WIN32 */

#endif /* WINPR_COMM_IOCTL_H_ */
