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

#ifndef WINPR_COMM_H
#define WINPR_COMM_H

#include <winpr/collections.h>
#include <winpr/file.h>
#include <winpr/winpr.h>
#include <winpr/wtypes.h>


#if defined __linux__ && !defined ANDROID

#define NOPARITY		0
#define ODDPARITY		1
#define EVENPARITY		2
#define MARKPARITY		3
#define SPACEPARITY		4

#define ONESTOPBIT		0
#define ONE5STOPBITS		1
#define TWOSTOPBITS		2

#ifndef IGNORE
#define IGNORE			0
#endif

#define CBR_110			110
#define CBR_300			300
#define CBR_600			600
#define CBR_1200		1200
#define CBR_2400		2400
#define CBR_4800		4800
#define CBR_9600		9600
#define CBR_14400		14400
#define CBR_19200		19200
#define CBR_38400		38400
#define CBR_56000		56000
#define CBR_57600		57600
#define CBR_115200		115200
#define CBR_128000		128000
#define CBR_256000		256000

#define CE_RXOVER		0x0001
#define CE_OVERRUN		0x0002
#define CE_RXPARITY		0x0004
#define CE_FRAME		0x0008
#define CE_BREAK		0x0010
#define CE_TXFULL		0x0100
#define CE_PTO			0x0200
#define CE_IOE			0x0400
#define CE_DNS			0x0800
#define CE_OOP			0x1000
#define CE_MODE			0x8000

#define IE_BADID		(-1)
#define IE_OPEN			(-2)
#define IE_NOPEN		(-3)
#define IE_MEMORY		(-4)
#define IE_DEFAULT		(-5)
#define IE_HARDWARE		(-10)
#define IE_BYTESIZE		(-11)
#define IE_BAUDRATE		(-12)

#define EV_RXCHAR		0x0001
#define EV_RXFLAG		0x0002
#define EV_TXEMPTY		0x0004
#define EV_CTS			0x0008
#define EV_DSR			0x0010
#define EV_RLSD			0x0020
#define EV_BREAK		0x0040
#define EV_ERR			0x0080
#define EV_RING			0x0100
#define EV_PERR			0x0200
#define EV_RX80FULL		0x0400
#define EV_EVENT1		0x0800
#define EV_EVENT2		0x1000

#define SETXOFF			1
#define SETXON			2
#define SETRTS			3
#define CLRRTS			4
#define SETDTR			5
#define CLRDTR			6
#define RESETDEV		7
#define SETBREAK		8
#define CLRBREAK		9

#define PURGE_TXABORT		0x0001
#define PURGE_RXABORT		0x0002
#define PURGE_TXCLEAR		0x0004
#define PURGE_RXCLEAR		0x0008

#define LPTx			0x80

#define MS_CTS_ON		((DWORD)0x0010)
#define MS_DSR_ON		((DWORD)0x0020)
#define MS_RING_ON		((DWORD)0x0040)
#define MS_RLSD_ON		((DWORD)0x0080)

#define SP_SERIALCOMM		((DWORD)0x00000001)

#define PST_UNSPECIFIED		((DWORD)0x00000000)
#define PST_RS232		((DWORD)0x00000001)
#define PST_PARALLELPORT	((DWORD)0x00000002)
#define PST_RS422		((DWORD)0x00000003)
#define PST_RS423		((DWORD)0x00000004)
#define PST_RS449		((DWORD)0x00000005)
#define PST_MODEM		((DWORD)0x00000006)
#define PST_FAX			((DWORD)0x00000021)
#define PST_SCANNER		((DWORD)0x00000022)
#define PST_NETWORK_BRIDGE	((DWORD)0x00000100)
#define PST_LAT			((DWORD)0x00000101)
#define PST_TCPIP_TELNET	((DWORD)0x00000102)
#define PST_X25			((DWORD)0x00000103)

#define PCF_DTRDSR		((DWORD)0x0001)
#define PCF_RTSCTS		((DWORD)0x0002)
#define PCF_RLSD		((DWORD)0x0004)
#define PCF_PARITY_CHECK	((DWORD)0x0008)
#define PCF_XONXOFF		((DWORD)0x0010)
#define PCF_SETXCHAR		((DWORD)0x0020)
#define PCF_TOTALTIMEOUTS	((DWORD)0x0040)
#define PCF_INTTIMEOUTS		((DWORD)0x0080)
#define PCF_SPECIALCHARS	((DWORD)0x0100)
#define PCF_16BITMODE		((DWORD)0x0200)

#define SP_PARITY		((DWORD)0x0001)
#define SP_BAUD			((DWORD)0x0002)
#define SP_DATABITS		((DWORD)0x0004)
#define SP_STOPBITS		((DWORD)0x0008)
#define SP_HANDSHAKING		((DWORD)0x0010)
#define SP_PARITY_CHECK		((DWORD)0x0020)
#define SP_RLSD			((DWORD)0x0040)

#define BAUD_075		((DWORD)0x00000001)
#define BAUD_110		((DWORD)0x00000002)
#define BAUD_134_5		((DWORD)0x00000004)
#define BAUD_150		((DWORD)0x00000008)
#define BAUD_300		((DWORD)0x00000010)
#define BAUD_600		((DWORD)0x00000020)
#define BAUD_1200		((DWORD)0x00000040)
#define BAUD_1800		((DWORD)0x00000080)
#define BAUD_2400		((DWORD)0x00000100)
#define BAUD_4800		((DWORD)0x00000200)
#define BAUD_7200		((DWORD)0x00000400)
#define BAUD_9600		((DWORD)0x00000800)
#define BAUD_14400		((DWORD)0x00001000)
#define BAUD_19200		((DWORD)0x00002000)
#define BAUD_38400		((DWORD)0x00004000)
#define BAUD_56K		((DWORD)0x00008000)
#define BAUD_128K		((DWORD)0x00010000)
#define BAUD_115200		((DWORD)0x00020000)
#define BAUD_57600		((DWORD)0x00040000)
#define BAUD_USER		((DWORD)0x10000000)

#define DATABITS_5		((WORD)0x0001)
#define DATABITS_6		((WORD)0x0002)
#define DATABITS_7		((WORD)0x0004)
#define DATABITS_8		((WORD)0x0008)
#define DATABITS_16		((WORD)0x0010)
#define DATABITS_16X		((WORD)0x0020)

#define STOPBITS_10		((WORD)0x0001)
#define STOPBITS_15		((WORD)0x0002)
#define STOPBITS_20		((WORD)0x0004)

#define PARITY_NONE		((WORD)0x0100)
#define PARITY_ODD		((WORD)0x0200)
#define PARITY_EVEN		((WORD)0x0400)
#define PARITY_MARK		((WORD)0x0800)
#define PARITY_SPACE		((WORD)0x1000)

#define COMMPROP_INITIALIZED	((DWORD)0xE73CF52E)

#define DTR_CONTROL_DISABLE	0x00
#define DTR_CONTROL_ENABLE	0x01
#define DTR_CONTROL_HANDSHAKE	0x02

#define RTS_CONTROL_DISABLE	0x00
#define RTS_CONTROL_ENABLE	0x01
#define RTS_CONTROL_HANDSHAKE	0x02
#define RTS_CONTROL_TOGGLE	0x03

// http://msdn.microsoft.com/en-us/library/windows/desktop/aa363214%28v=vs.85%29.aspx
typedef struct _DCB
{
	DWORD DCBlength;
	DWORD BaudRate;
	DWORD fBinary:1;
	DWORD fParity:1;
	DWORD fOutxCtsFlow:1;
	DWORD fOutxDsrFlow:1;
	DWORD fDtrControl:2;
	DWORD fDsrSensitivity:1;
	DWORD fTXContinueOnXoff:1;
	DWORD fOutX:1;
	DWORD fInX:1;
	DWORD fErrorChar:1;
	DWORD fNull:1;
	DWORD fRtsControl:2;
	DWORD fAbortOnError:1;
	DWORD fDummy2:17;
	WORD wReserved;
	WORD XonLim;
	WORD XoffLim;
	BYTE ByteSize;
	BYTE Parity;
	BYTE StopBits;
	char XonChar;
	char XoffChar;
	char ErrorChar;
	char EofChar;
	char EvtChar;
	WORD wReserved1;
} DCB, *LPDCB;

typedef struct _COMM_CONFIG
{
	DWORD dwSize;
	WORD wVersion;
	WORD wReserved;
	DCB dcb;
	DWORD dwProviderSubType;
	DWORD dwProviderOffset;
	DWORD dwProviderSize;
	WCHAR wcProviderData[1];
} COMMCONFIG, *LPCOMMCONFIG;

typedef struct _COMMPROP
{
	WORD wPacketLength;
	WORD wPacketVersion;
	DWORD dwServiceMask;
	DWORD dwReserved1;
	DWORD dwMaxTxQueue;
	DWORD dwMaxRxQueue;
	DWORD dwMaxBaud;
	DWORD dwProvSubType;
	DWORD dwProvCapabilities;
	DWORD dwSettableParams;
	DWORD dwSettableBaud;
	WORD wSettableData;
	WORD wSettableStopParity;
	DWORD dwCurrentTxQueue;
	DWORD dwCurrentRxQueue;
	DWORD dwProvSpec1;
	DWORD dwProvSpec2;
	WCHAR wcProvChar[1];
} COMMPROP, *LPCOMMPROP;

typedef struct _COMMTIMEOUTS
{
	DWORD ReadIntervalTimeout;
	DWORD ReadTotalTimeoutMultiplier;
	DWORD ReadTotalTimeoutConstant;
	DWORD WriteTotalTimeoutMultiplier;
	DWORD WriteTotalTimeoutConstant;
} COMMTIMEOUTS, *LPCOMMTIMEOUTS;

typedef struct _COMSTAT
{
	DWORD fCtsHold:1;
	DWORD fDsrHold:1;
	DWORD fRlsdHold:1;
	DWORD fXoffHold:1;
	DWORD fXoffSent:1;
	DWORD fEof:1;
	DWORD fTxim:1;
	DWORD fReserved:25;
	DWORD cbInQue;
	DWORD cbOutQue;
} COMSTAT, *LPCOMSTAT;

#ifdef __cplusplus
extern "C" {
#endif

WINPR_API BOOL BuildCommDCBA(LPCSTR lpDef, LPDCB lpDCB);
WINPR_API BOOL BuildCommDCBW(LPCWSTR lpDef, LPDCB lpDCB);

WINPR_API BOOL BuildCommDCBAndTimeoutsA(LPCSTR lpDef, LPDCB lpDCB, LPCOMMTIMEOUTS lpCommTimeouts);
WINPR_API BOOL BuildCommDCBAndTimeoutsW(LPCWSTR lpDef, LPDCB lpDCB, LPCOMMTIMEOUTS lpCommTimeouts);

WINPR_API BOOL CommConfigDialogA(LPCSTR lpszName, HWND hWnd, LPCOMMCONFIG lpCC);
WINPR_API BOOL CommConfigDialogW(LPCWSTR lpszName, HWND hWnd, LPCOMMCONFIG lpCC);

WINPR_API BOOL GetCommConfig(HANDLE hCommDev, LPCOMMCONFIG lpCC, LPDWORD lpdwSize);
WINPR_API BOOL SetCommConfig(HANDLE hCommDev, LPCOMMCONFIG lpCC, DWORD dwSize);

WINPR_API BOOL GetCommMask(HANDLE hFile, PDWORD lpEvtMask);
WINPR_API BOOL SetCommMask(HANDLE hFile, DWORD dwEvtMask);

WINPR_API BOOL GetCommModemStatus(HANDLE hFile, PDWORD lpModemStat);
WINPR_API BOOL GetCommProperties(HANDLE hFile, LPCOMMPROP lpCommProp);

WINPR_API BOOL GetCommState(HANDLE hFile, LPDCB lpDCB);
WINPR_API BOOL SetCommState(HANDLE hFile, LPDCB lpDCB);

WINPR_API BOOL GetCommTimeouts(HANDLE hFile, LPCOMMTIMEOUTS lpCommTimeouts);
WINPR_API BOOL SetCommTimeouts(HANDLE hFile, LPCOMMTIMEOUTS lpCommTimeouts);

WINPR_API BOOL GetDefaultCommConfigA(LPCSTR lpszName, LPCOMMCONFIG lpCC, LPDWORD lpdwSize);
WINPR_API BOOL GetDefaultCommConfigW(LPCWSTR lpszName, LPCOMMCONFIG lpCC, LPDWORD lpdwSize);

WINPR_API BOOL SetDefaultCommConfigA(LPCSTR lpszName, LPCOMMCONFIG lpCC, DWORD dwSize);
WINPR_API BOOL SetDefaultCommConfigW(LPCWSTR lpszName, LPCOMMCONFIG lpCC, DWORD dwSize);

WINPR_API BOOL SetCommBreak(HANDLE hFile);
WINPR_API BOOL ClearCommBreak(HANDLE hFile);
WINPR_API BOOL ClearCommError(HANDLE hFile, PDWORD lpErrors, LPCOMSTAT lpStat);

WINPR_API BOOL PurgeComm(HANDLE hFile, DWORD dwFlags);
WINPR_API BOOL SetupComm(HANDLE hFile, DWORD dwInQueue, DWORD dwOutQueue);

WINPR_API BOOL EscapeCommFunction(HANDLE hFile, DWORD dwFunc);

WINPR_API BOOL TransmitCommChar(HANDLE hFile, char cChar);

WINPR_API BOOL WaitCommEvent(HANDLE hFile, PDWORD lpEvtMask, LPOVERLAPPED lpOverlapped);

#ifdef UNICODE
#define BuildCommDCB		BuildCommDCBW
#define BuildCommDCBAndTimeouts	BuildCommDCBAndTimeoutsW
#define CommConfigDialog	CommConfigDialogW
#define GetDefaultCommConfig	GetDefaultCommConfigW
#define SetDefaultCommConfig	SetDefaultCommConfigW
#else
#define BuildCommDCB		BuildCommDCBA
#define BuildCommDCBAndTimeouts BuildCommDCBAndTimeoutsA
#define CommConfigDialog	CommConfigDialogA
#define GetDefaultCommConfig	GetDefaultCommConfigA
#define SetDefaultCommConfig	SetDefaultCommConfigA
#endif

/* Extended API */

/* FIXME: MAXULONG should be defined arround winpr/limits.h */
#ifndef MAXULONG
#define MAXULONG (4294967295UL)
#endif


/**
 * IOCTLs table according the server's serial driver:
 * http://msdn.microsoft.com/en-us/library/windows/hardware/dn265347%28v=vs.85%29.aspx
 */
typedef enum _SERIAL_DRIVER_ID
{
	SerialDriverUnknown = 0,
	SerialDriverSerialSys,
	SerialDriverSerCxSys,
	SerialDriverSerCx2Sys /* default fallback, see also CommDeviceIoControl() */
} SERIAL_DRIVER_ID;


/*
 * About DefineCommDevice() / QueryDosDevice()
 *
 * Did something close to QueryDosDevice() and DefineDosDevice() but with
 * folowing constraints:
 *   - mappings are stored in a static array.
 *   - QueryCommDevice returns only the mappings that have been defined through DefineCommDevice()
 */
WINPR_API BOOL DefineCommDevice(/* DWORD dwFlags,*/ LPCTSTR lpDeviceName, LPCTSTR lpTargetPath);
WINPR_API DWORD QueryCommDevice(LPCTSTR lpDeviceName, LPTSTR lpTargetPath, DWORD ucchMax);
WINPR_API BOOL IsCommDevice(LPCTSTR lpDeviceName);

/**
 * A handle can only be created on defined devices with DefineCommDevice(). This
 * also ensures that CommCreateFileA() has been registered through
 * RegisterHandleCreator().
 */
WINPR_API HANDLE CommCreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes,
				DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);



#define IOCTL_SERIAL_SET_BAUD_RATE	0x001B0004
#define IOCTL_SERIAL_GET_BAUD_RATE	0x001B0050
#define IOCTL_SERIAL_SET_LINE_CONTROL	0x001B000C
#define IOCTL_SERIAL_GET_LINE_CONTROL	0x001B0054
#define IOCTL_SERIAL_SET_TIMEOUTS	0x001B001C
#define IOCTL_SERIAL_GET_TIMEOUTS	0x001B0020
/* GET_CHARS and SET_CHARS are swapped in the RDP docs [MS-RDPESP] */
#define IOCTL_SERIAL_GET_CHARS		0x001B0058
#define IOCTL_SERIAL_SET_CHARS		0x001B005C

#define IOCTL_SERIAL_SET_DTR		0x001B0024
#define IOCTL_SERIAL_CLR_DTR		0x001B0028
#define IOCTL_SERIAL_RESET_DEVICE	0x001B002C
#define IOCTL_SERIAL_SET_RTS		0x001B0030
#define IOCTL_SERIAL_CLR_RTS		0x001B0034
#define IOCTL_SERIAL_SET_XOFF		0x001B0038
#define IOCTL_SERIAL_SET_XON		0x001B003C
#define IOCTL_SERIAL_SET_BREAK_ON	0x001B0010
#define IOCTL_SERIAL_SET_BREAK_OFF	0x001B0014
#define IOCTL_SERIAL_SET_QUEUE_SIZE	0x001B0008
#define IOCTL_SERIAL_GET_WAIT_MASK	0x001B0040
#define IOCTL_SERIAL_SET_WAIT_MASK	0x001B0044
#define IOCTL_SERIAL_WAIT_ON_MASK	0x001B0048
#define IOCTL_SERIAL_IMMEDIATE_CHAR	0x001B0018
#define IOCTL_SERIAL_PURGE		0x001B004C
#define IOCTL_SERIAL_GET_HANDFLOW	0x001B0060
#define IOCTL_SERIAL_SET_HANDFLOW	0x001B0064
#define IOCTL_SERIAL_GET_MODEMSTATUS	0x001B0068
#define IOCTL_SERIAL_GET_DTRRTS		0x001B0078

/* according to [MS-RDPESP] it should be 0x001B0084, but servers send 0x001B006C */
#define IOCTL_SERIAL_GET_COMMSTATUS	0x001B006C

#define IOCTL_SERIAL_GET_PROPERTIES	0x001B0074
/* IOCTL_SERIAL_XOFF_COUNTER 0x001B0070 */
/* IOCTL_SERIAL_LSRMST_INSERT 0x001B007C */
#define IOCTL_SERIAL_CONFIG_SIZE	0x001B0080
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

/* http://msdn.microsoft.com/en-us/library/windows/hardware/ff551803(v=vs.85).aspx */
#define IOCTL_USBPRINT_GET_1284_ID	0x220034


typedef struct __SERIAL_IOCTL_NAME
{
	ULONG number;
	const char* name;
} _SERIAL_IOCTL_NAME;

static const _SERIAL_IOCTL_NAME _SERIAL_IOCTL_NAMES[] =
{
	{IOCTL_SERIAL_SET_BAUD_RATE,	"IOCTL_SERIAL_SET_BAUD_RATE"},
	{IOCTL_SERIAL_GET_BAUD_RATE,	"IOCTL_SERIAL_GET_BAUD_RATE"},
	{IOCTL_SERIAL_SET_LINE_CONTROL,	"IOCTL_SERIAL_SET_LINE_CONTROL"},
	{IOCTL_SERIAL_GET_LINE_CONTROL,	"IOCTL_SERIAL_GET_LINE_CONTROL"},
	{IOCTL_SERIAL_SET_TIMEOUTS,	"IOCTL_SERIAL_SET_TIMEOUTS"},
	{IOCTL_SERIAL_GET_TIMEOUTS,	"IOCTL_SERIAL_GET_TIMEOUTS"},
	{IOCTL_SERIAL_GET_CHARS,	"IOCTL_SERIAL_GET_CHARS"},
	{IOCTL_SERIAL_SET_CHARS,	"IOCTL_SERIAL_SET_CHARS"},
	{IOCTL_SERIAL_SET_DTR,		"IOCTL_SERIAL_SET_DTR"},
	{IOCTL_SERIAL_CLR_DTR,		"IOCTL_SERIAL_CLR_DTR"},
	{IOCTL_SERIAL_RESET_DEVICE,	"IOCTL_SERIAL_RESET_DEVICE"},
	{IOCTL_SERIAL_SET_RTS,		"IOCTL_SERIAL_SET_RTS"},
	{IOCTL_SERIAL_CLR_RTS,		"IOCTL_SERIAL_CLR_RTS"},
	{IOCTL_SERIAL_SET_XOFF,		"IOCTL_SERIAL_SET_XOFF"},
	{IOCTL_SERIAL_SET_XON,		"IOCTL_SERIAL_SET_XON"},
	{IOCTL_SERIAL_SET_BREAK_ON,  	"IOCTL_SERIAL_SET_BREAK_ON"},
	{IOCTL_SERIAL_SET_BREAK_OFF,	"IOCTL_SERIAL_SET_BREAK_OFF"},
	{IOCTL_SERIAL_SET_QUEUE_SIZE, 	"IOCTL_SERIAL_SET_QUEUE_SIZE"},
	{IOCTL_SERIAL_GET_WAIT_MASK,	"IOCTL_SERIAL_GET_WAIT_MASK"},
	{IOCTL_SERIAL_SET_WAIT_MASK,	"IOCTL_SERIAL_SET_WAIT_MASK"},
	{IOCTL_SERIAL_WAIT_ON_MASK,	"IOCTL_SERIAL_WAIT_ON_MASK"},
	{IOCTL_SERIAL_IMMEDIATE_CHAR,	"IOCTL_SERIAL_IMMEDIATE_CHAR"},
	{IOCTL_SERIAL_PURGE,		"IOCTL_SERIAL_PURGE"},
	{IOCTL_SERIAL_GET_HANDFLOW,	"IOCTL_SERIAL_GET_HANDFLOW"},
	{IOCTL_SERIAL_SET_HANDFLOW,	"IOCTL_SERIAL_SET_HANDFLOW"},
	{IOCTL_SERIAL_GET_MODEMSTATUS,	"IOCTL_SERIAL_GET_MODEMSTATUS"},
	{IOCTL_SERIAL_GET_DTRRTS,	"IOCTL_SERIAL_GET_DTRRTS"},
	{IOCTL_SERIAL_GET_COMMSTATUS,	"IOCTL_SERIAL_GET_COMMSTATUS"},
	{IOCTL_SERIAL_GET_PROPERTIES,	"IOCTL_SERIAL_GET_PROPERTIES"},
	// {IOCTL_SERIAL_XOFF_COUNTER,	"IOCTL_SERIAL_XOFF_COUNTER"},
	// {IOCTL_SERIAL_LSRMST_INSERT,	"IOCTL_SERIAL_LSRMST_INSERT"},
	{IOCTL_SERIAL_CONFIG_SIZE, 	"IOCTL_SERIAL_CONFIG_SIZE"},
	// {IOCTL_SERIAL_GET_STATS,	"IOCTL_SERIAL_GET_STATS"},
	// {IOCTL_SERIAL_CLEAR_STATS,	"IOCTL_SERIAL_CLEAR_STATS"},
	// {IOCTL_SERIAL_GET_MODEM_CONTROL,"IOCTL_SERIAL_GET_MODEM_CONTROL"},
	// {IOCTL_SERIAL_SET_MODEM_CONTROL,"IOCTL_SERIAL_SET_MODEM_CONTROL"},
	// {IOCTL_SERIAL_SET_FIFO_CONTROL,	"IOCTL_SERIAL_SET_FIFO_CONTROL"},

	// {IOCTL_PAR_QUERY_INFORMATION,	"IOCTL_PAR_QUERY_INFORMATION"},
	// {IOCTL_PAR_SET_INFORMATION,	"IOCTL_PAR_SET_INFORMATION"},
	// {IOCTL_PAR_QUERY_DEVICE_ID,	"IOCTL_PAR_QUERY_DEVICE_ID"},
	// {IOCTL_PAR_QUERY_DEVICE_ID_SIZE,"IOCTL_PAR_QUERY_DEVICE_ID_SIZE"},
	// {IOCTL_IEEE1284_GET_MODE,	"IOCTL_IEEE1284_GET_MODE"},
	// {IOCTL_IEEE1284_NEGOTIATE,	"IOCTL_IEEE1284_NEGOTIATE"},
	// {IOCTL_PAR_SET_WRITE_ADDRESS,	"IOCTL_PAR_SET_WRITE_ADDRESS"},
	// {IOCTL_PAR_SET_READ_ADDRESS,	"IOCTL_PAR_SET_READ_ADDRESS"},
	// {IOCTL_PAR_GET_DEVICE_CAPS,	"IOCTL_PAR_GET_DEVICE_CAPS"},
	// {IOCTL_PAR_GET_DEFAULT_MODES,	"IOCTL_PAR_GET_DEFAULT_MODES"},
	// {IOCTL_PAR_QUERY_RAW_DEVICE_ID, "IOCTL_PAR_QUERY_RAW_DEVICE_ID"},
	// {IOCTL_PAR_IS_PORT_FREE,	"IOCTL_PAR_IS_PORT_FREE"},

        {IOCTL_USBPRINT_GET_1284_ID,	"IOCTL_USBPRINT_GET_1284_ID"},

	{0, NULL}
};

/**
 * FIXME: got a proper function name and place
 */
WINPR_API const char* _comm_serial_ioctl_name(ULONG number);

/**
 * FIXME: got a proper function name and place
 */
WINPR_API void _comm_setServerSerialDriver(HANDLE hComm, SERIAL_DRIVER_ID);

/**
 * FIXME: got a proper function name and place
 *
 * permissive mode is disabled by default.
 */
WINPR_API BOOL _comm_set_permissive(HANDLE hDevice, BOOL permissive);


/**
 * FIXME: to be moved in comm_ioctl.h
 */
WINPR_API BOOL CommDeviceIoControl(HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize,
			LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped);

/**
 * FIXME: to be moved in comm_io.h
 */
WINPR_API BOOL CommReadFile(HANDLE hDevice, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
		LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped);

/**
 * FIXME: to be moved in comm_io.h
 */
WINPR_API BOOL CommWriteFile(HANDLE hDevice, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
		LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped);

#ifdef __cplusplus
}
#endif

#endif /* __linux__ */

#endif /* WINPR_COMM_H */

