/**
 * WinPR: Windows Portable Runtime
 * Smart Card API
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef WINPR_SMARTCARD_H
#define WINPR_SMARTCARD_H

#include <winpr/winpr.h>
#include <winpr/wtypes.h>

#include <winpr/io.h>
#include <winpr/error.h>

#ifndef _WINSCARD_H_
#define _WINSCARD_H_	/* do not include winscard.h */
#endif

#ifndef SCARD_S_SUCCESS

#define SCARD_S_SUCCESS	NO_ERROR

#define SCARD_F_INTERNAL_ERROR			((DWORD)0x80100001L)
#define SCARD_E_CANCELLED			((DWORD)0x80100002L)
#define SCARD_E_INVALID_HANDLE			((DWORD)0x80100003L)
#define SCARD_E_INVALID_PARAMETER		((DWORD)0x80100004L)
#define SCARD_E_INVALID_TARGET			((DWORD)0x80100005L)
#define SCARD_E_NO_MEMORY			((DWORD)0x80100006L)
#define SCARD_F_WAITED_TOO_LONG			((DWORD)0x80100007L)
#define SCARD_E_INSUFFICIENT_BUFFER		((DWORD)0x80100008L)
#define SCARD_E_UNKNOWN_READER			((DWORD)0x80100009L)
#define SCARD_E_TIMEOUT				((DWORD)0x8010000AL)
#define SCARD_E_SHARING_VIOLATION		((DWORD)0x8010000BL)
#define SCARD_E_NO_SMARTCARD			((DWORD)0x8010000CL)
#define SCARD_E_UNKNOWN_CARD			((DWORD)0x8010000DL)
#define SCARD_E_CANT_DISPOSE			((DWORD)0x8010000EL)
#define SCARD_E_PROTO_MISMATCH			((DWORD)0x8010000FL)
#define SCARD_E_NOT_READY			((DWORD)0x80100010L)
#define SCARD_E_INVALID_VALUE			((DWORD)0x80100011L)
#define SCARD_E_SYSTEM_CANCELLED		((DWORD)0x80100012L)
#define SCARD_F_COMM_ERROR			((DWORD)0x80100013L)
#define SCARD_F_UNKNOWN_ERROR			((DWORD)0x80100014L)
#define SCARD_E_INVALID_ATR			((DWORD)0x80100015L)
#define SCARD_E_NOT_TRANSACTED			((DWORD)0x80100016L)
#define SCARD_E_READER_UNAVAILABLE		((DWORD)0x80100017L)
#define SCARD_P_SHUTDOWN			((DWORD)0x80100018L)
#define SCARD_E_PCI_TOO_SMALL			((DWORD)0x80100019L)
#define SCARD_E_READER_UNSUPPORTED		((DWORD)0x8010001AL)
#define SCARD_E_DUPLICATE_READER		((DWORD)0x8010001BL)
#define SCARD_E_CARD_UNSUPPORTED		((DWORD)0x8010001CL)
#define SCARD_E_NO_SERVICE			((DWORD)0x8010001DL)
#define SCARD_E_SERVICE_STOPPED			((DWORD)0x8010001EL)
#define SCARD_E_UNEXPECTED			((DWORD)0x8010001FL)
#define SCARD_E_ICC_INSTALLATION		((DWORD)0x80100020L)
#define SCARD_E_ICC_CREATEORDER			((DWORD)0x80100021L)
#define SCARD_E_UNSUPPORTED_FEATURE		((DWORD)0x80100022L)
#define SCARD_E_DIR_NOT_FOUND			((DWORD)0x80100023L)
#define SCARD_E_FILE_NOT_FOUND			((DWORD)0x80100024L)
#define SCARD_E_NO_DIR				((DWORD)0x80100025L)
#define SCARD_E_NO_FILE				((DWORD)0x80100026L)
#define SCARD_E_NO_ACCESS			((DWORD)0x80100027L)
#define SCARD_E_WRITE_TOO_MANY			((DWORD)0x80100028L)
#define SCARD_E_BAD_SEEK			((DWORD)0x80100029L)
#define SCARD_E_INVALID_CHV			((DWORD)0x8010002AL)
#define SCARD_E_UNKNOWN_RES_MNG			((DWORD)0x8010002BL)
#define SCARD_E_NO_SUCH_CERTIFICATE		((DWORD)0x8010002CL)
#define SCARD_E_CERTIFICATE_UNAVAILABLE		((DWORD)0x8010002DL)
#define SCARD_E_NO_READERS_AVAILABLE		((DWORD)0x8010002EL)
#define SCARD_E_COMM_DATA_LOST			((DWORD)0x8010002FL)
#define SCARD_E_NO_KEY_CONTAINER		((DWORD)0x80100030L)
#define SCARD_E_SERVER_TOO_BUSY			((DWORD)0x80100031L)
#define SCARD_E_PIN_CACHE_EXPIRED		((DWORD)0x80100032L)
#define SCARD_E_NO_PIN_CACHE			((DWORD)0x80100033L)
#define SCARD_E_READ_ONLY_CARD			((DWORD)0x80100034L)

#define SCARD_W_UNSUPPORTED_CARD		((DWORD)0x80100065L)
#define SCARD_W_UNRESPONSIVE_CARD		((DWORD)0x80100066L)
#define SCARD_W_UNPOWERED_CARD			((DWORD)0x80100067L)
#define SCARD_W_RESET_CARD			((DWORD)0x80100068L)
#define SCARD_W_REMOVED_CARD			((DWORD)0x80100069L)
#define SCARD_W_SECURITY_VIOLATION		((DWORD)0x8010006AL)
#define SCARD_W_WRONG_CHV			((DWORD)0x8010006BL)
#define SCARD_W_CHV_BLOCKED			((DWORD)0x8010006CL)
#define SCARD_W_EOF				((DWORD)0x8010006DL)
#define SCARD_W_CANCELLED_BY_USER		((DWORD)0x8010006EL)
#define SCARD_W_CARD_NOT_AUTHENTICATED		((DWORD)0x8010006FL)
#define SCARD_W_CACHE_ITEM_NOT_FOUND		((DWORD)0x80100070L)
#define SCARD_W_CACHE_ITEM_STALE		((DWORD)0x80100071L)
#define SCARD_W_CACHE_ITEM_TOO_BIG		((DWORD)0x80100072L)

#endif

#define SCARD_ATR_LENGTH		33

#define SCARD_PROTOCOL_UNDEFINED	0x00000000
#define SCARD_PROTOCOL_T0		0x00000001
#define SCARD_PROTOCOL_T1		0x00000002
#define SCARD_PROTOCOL_RAW		0x00010000

#define SCARD_PROTOCOL_Tx		(SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1)
#define SCARD_PROTOCOL_DEFAULT		0x80000000
#define SCARD_PROTOCOL_OPTIMAL		0x00000000

#define SCARD_POWER_DOWN		0
#define SCARD_COLD_RESET		1
#define SCARD_WARM_RESET		2

#define SCARD_CTL_CODE(code)		CTL_CODE(FILE_DEVICE_SMARTCARD, (code), METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_SMARTCARD_POWER		SCARD_CTL_CODE(1)
#define IOCTL_SMARTCARD_GET_ATTRIBUTE	SCARD_CTL_CODE(2)
#define IOCTL_SMARTCARD_SET_ATTRIBUTE	SCARD_CTL_CODE(3)
#define IOCTL_SMARTCARD_CONFISCATE	SCARD_CTL_CODE(4)
#define IOCTL_SMARTCARD_TRANSMIT	SCARD_CTL_CODE(5)
#define IOCTL_SMARTCARD_EJECT		SCARD_CTL_CODE(6)
#define IOCTL_SMARTCARD_SWALLOW		SCARD_CTL_CODE(7)
#define IOCTL_SMARTCARD_IS_PRESENT	SCARD_CTL_CODE(10)
#define IOCTL_SMARTCARD_IS_ABSENT	SCARD_CTL_CODE(11)
#define IOCTL_SMARTCARD_SET_PROTOCOL	SCARD_CTL_CODE(12)
#define IOCTL_SMARTCARD_GET_STATE	SCARD_CTL_CODE(14)
#define IOCTL_SMARTCARD_GET_LAST_ERROR	SCARD_CTL_CODE(15)
#define IOCTL_SMARTCARD_GET_PERF_CNTR	SCARD_CTL_CODE(16)

#define MAXIMUM_ATTR_STRING_LENGTH	32
#define MAXIMUM_SMARTCARD_READERS	10

#define SCARD_ATTR_VALUE(Class, Tag)	((((ULONG)(Class)) << 16) | ((ULONG)(Tag)))

#define SCARD_CLASS_VENDOR_INFO		1
#define SCARD_CLASS_COMMUNICATIONS	2
#define SCARD_CLASS_PROTOCOL		3
#define SCARD_CLASS_POWER_MGMT		4
#define SCARD_CLASS_SECURITY		5
#define SCARD_CLASS_MECHANICAL		6
#define SCARD_CLASS_VENDOR_DEFINED	7
#define SCARD_CLASS_IFD_PROTOCOL	8
#define SCARD_CLASS_ICC_STATE		9
#define SCARD_CLASS_PERF		0x7FFE
#define SCARD_CLASS_SYSTEM		0x7FFF

#define SCARD_ATTR_VENDOR_NAME			SCARD_ATTR_VALUE(SCARD_CLASS_VENDOR_INFO, 0x0100)
#define SCARD_ATTR_VENDOR_IFD_TYPE		SCARD_ATTR_VALUE(SCARD_CLASS_VENDOR_INFO, 0x0101)
#define SCARD_ATTR_VENDOR_IFD_VERSION		SCARD_ATTR_VALUE(SCARD_CLASS_VENDOR_INFO, 0x0102)
#define SCARD_ATTR_VENDOR_IFD_SERIAL_NO		SCARD_ATTR_VALUE(SCARD_CLASS_VENDOR_INFO, 0x0103)
#define SCARD_ATTR_CHANNEL_ID			SCARD_ATTR_VALUE(SCARD_CLASS_COMMUNICATIONS, 0x0110)
#define SCARD_ATTR_PROTOCOL_TYPES		SCARD_ATTR_VALUE(SCARD_CLASS_PROTOCOL, 0x0120)
#define SCARD_ATTR_DEFAULT_CLK			SCARD_ATTR_VALUE(SCARD_CLASS_PROTOCOL, 0x0121)
#define SCARD_ATTR_MAX_CLK			SCARD_ATTR_VALUE(SCARD_CLASS_PROTOCOL, 0x0122)
#define SCARD_ATTR_DEFAULT_DATA_RATE		SCARD_ATTR_VALUE(SCARD_CLASS_PROTOCOL, 0x0123)
#define SCARD_ATTR_MAX_DATA_RATE		SCARD_ATTR_VALUE(SCARD_CLASS_PROTOCOL, 0x0124)
#define SCARD_ATTR_MAX_IFSD			SCARD_ATTR_VALUE(SCARD_CLASS_PROTOCOL, 0x0125)
#define SCARD_ATTR_POWER_MGMT_SUPPORT		SCARD_ATTR_VALUE(SCARD_CLASS_POWER_MGMT, 0x0131)
#define SCARD_ATTR_USER_TO_CARD_AUTH_DEVICE	SCARD_ATTR_VALUE(SCARD_CLASS_SECURITY, 0x0140)
#define SCARD_ATTR_USER_AUTH_INPUT_DEVICE	SCARD_ATTR_VALUE(SCARD_CLASS_SECURITY, 0x0142)
#define SCARD_ATTR_CHARACTERISTICS		SCARD_ATTR_VALUE(SCARD_CLASS_MECHANICAL, 0x0150)

#define SCARD_ATTR_CURRENT_PROTOCOL_TYPE	SCARD_ATTR_VALUE(SCARD_CLASS_IFD_PROTOCOL, 0x0201)
#define SCARD_ATTR_CURRENT_CLK			SCARD_ATTR_VALUE(SCARD_CLASS_IFD_PROTOCOL, 0x0202)
#define SCARD_ATTR_CURRENT_F			SCARD_ATTR_VALUE(SCARD_CLASS_IFD_PROTOCOL, 0x0203)
#define SCARD_ATTR_CURRENT_D			SCARD_ATTR_VALUE(SCARD_CLASS_IFD_PROTOCOL, 0x0204)
#define SCARD_ATTR_CURRENT_N			SCARD_ATTR_VALUE(SCARD_CLASS_IFD_PROTOCOL, 0x0205)
#define SCARD_ATTR_CURRENT_W			SCARD_ATTR_VALUE(SCARD_CLASS_IFD_PROTOCOL, 0x0206)
#define SCARD_ATTR_CURRENT_IFSC			SCARD_ATTR_VALUE(SCARD_CLASS_IFD_PROTOCOL, 0x0207)
#define SCARD_ATTR_CURRENT_IFSD			SCARD_ATTR_VALUE(SCARD_CLASS_IFD_PROTOCOL, 0x0208)
#define SCARD_ATTR_CURRENT_BWT			SCARD_ATTR_VALUE(SCARD_CLASS_IFD_PROTOCOL, 0x0209)
#define SCARD_ATTR_CURRENT_CWT			SCARD_ATTR_VALUE(SCARD_CLASS_IFD_PROTOCOL, 0x020a)
#define SCARD_ATTR_CURRENT_EBC_ENCODING		SCARD_ATTR_VALUE(SCARD_CLASS_IFD_PROTOCOL, 0x020b)
#define SCARD_ATTR_EXTENDED_BWT			SCARD_ATTR_VALUE(SCARD_CLASS_IFD_PROTOCOL, 0x020c)

#define SCARD_ATTR_ICC_PRESENCE			SCARD_ATTR_VALUE(SCARD_CLASS_ICC_STATE, 0x0300)
#define SCARD_ATTR_ICC_INTERFACE_STATUS		SCARD_ATTR_VALUE(SCARD_CLASS_ICC_STATE, 0x0301)
#define SCARD_ATTR_CURRENT_IO_STATE		SCARD_ATTR_VALUE(SCARD_CLASS_ICC_STATE, 0x0302)
#define SCARD_ATTR_ATR_STRING			SCARD_ATTR_VALUE(SCARD_CLASS_ICC_STATE, 0x0303)
#define SCARD_ATTR_ICC_TYPE_PER_ATR		SCARD_ATTR_VALUE(SCARD_CLASS_ICC_STATE, 0x0304)

#define SCARD_ATTR_ESC_RESET			SCARD_ATTR_VALUE(SCARD_CLASS_VENDOR_DEFINED, 0xA000)
#define SCARD_ATTR_ESC_CANCEL			SCARD_ATTR_VALUE(SCARD_CLASS_VENDOR_DEFINED, 0xA003)
#define SCARD_ATTR_ESC_AUTHREQUEST		SCARD_ATTR_VALUE(SCARD_CLASS_VENDOR_DEFINED, 0xA005)
#define SCARD_ATTR_MAXINPUT			SCARD_ATTR_VALUE(SCARD_CLASS_VENDOR_DEFINED, 0xA007)

#define SCARD_ATTR_DEVICE_UNIT			SCARD_ATTR_VALUE(SCARD_CLASS_SYSTEM, 0x0001)
#define SCARD_ATTR_DEVICE_IN_USE		SCARD_ATTR_VALUE(SCARD_CLASS_SYSTEM, 0x0002)
#define SCARD_ATTR_DEVICE_FRIENDLY_NAME_A	SCARD_ATTR_VALUE(SCARD_CLASS_SYSTEM, 0x0003)
#define SCARD_ATTR_DEVICE_SYSTEM_NAME_A		SCARD_ATTR_VALUE(SCARD_CLASS_SYSTEM, 0x0004)
#define SCARD_ATTR_DEVICE_FRIENDLY_NAME_W	SCARD_ATTR_VALUE(SCARD_CLASS_SYSTEM, 0x0005)
#define SCARD_ATTR_DEVICE_SYSTEM_NAME_W		SCARD_ATTR_VALUE(SCARD_CLASS_SYSTEM, 0x0006)
#define SCARD_ATTR_SUPRESS_T1_IFS_REQUEST	SCARD_ATTR_VALUE(SCARD_CLASS_SYSTEM, 0x0007)

#define SCARD_PERF_NUM_TRANSMISSIONS		SCARD_ATTR_VALUE(SCARD_CLASS_PERF, 0x0001)
#define SCARD_PERF_BYTES_TRANSMITTED		SCARD_ATTR_VALUE(SCARD_CLASS_PERF, 0x0002)
#define SCARD_PERF_TRANSMISSION_TIME		SCARD_ATTR_VALUE(SCARD_CLASS_PERF, 0x0003)

#ifdef UNICODE
#define SCARD_ATTR_DEVICE_FRIENDLY_NAME	SCARD_ATTR_DEVICE_FRIENDLY_NAME_W
#define SCARD_ATTR_DEVICE_SYSTEM_NAME	SCARD_ATTR_DEVICE_SYSTEM_NAME_W
#else
#define SCARD_ATTR_DEVICE_FRIENDLY_NAME	SCARD_ATTR_DEVICE_FRIENDLY_NAME_A
#define SCARD_ATTR_DEVICE_SYSTEM_NAME	SCARD_ATTR_DEVICE_SYSTEM_NAME_A
#endif

#define SCARD_T0_HEADER_LENGTH		7
#define SCARD_T0_CMD_LENGTH		5

#define SCARD_T1_PROLOGUE_LENGTH	3
#define SCARD_T1_EPILOGUE_LENGTH	2
#define SCARD_T1_MAX_IFS		254

#define SCARD_UNKNOWN			0
#define SCARD_ABSENT			1
#define SCARD_PRESENT			2
#define SCARD_SWALLOWED			3
#define SCARD_POWERED			4
#define SCARD_NEGOTIABLE		5
#define SCARD_SPECIFIC			6

#if defined(__APPLE__) | defined(sun)
#pragma pack(1)
#else
#pragma pack(push, 1)
#endif

typedef struct _SCARD_IO_REQUEST
{
	DWORD dwProtocol;
	DWORD cbPciLength;
} SCARD_IO_REQUEST, *PSCARD_IO_REQUEST, *LPSCARD_IO_REQUEST;
typedef const SCARD_IO_REQUEST *LPCSCARD_IO_REQUEST;

typedef struct
{
	BYTE
	bCla,
	bIns,
	bP1,
	bP2,
	bP3;
} SCARD_T0_COMMAND, *LPSCARD_T0_COMMAND;

typedef struct
{
	SCARD_IO_REQUEST ioRequest;
	BYTE
	bSw1,
	bSw2;
	union
	{
		SCARD_T0_COMMAND CmdBytes;
		BYTE rgbHeader[5];
	} DUMMYUNIONNAME;
} SCARD_T0_REQUEST;

typedef SCARD_T0_REQUEST *PSCARD_T0_REQUEST, *LPSCARD_T0_REQUEST;

typedef struct
{
	SCARD_IO_REQUEST ioRequest;
} SCARD_T1_REQUEST;
typedef SCARD_T1_REQUEST *PSCARD_T1_REQUEST, *LPSCARD_T1_REQUEST;

#define SCARD_READER_SWALLOWS		0x00000001
#define SCARD_READER_EJECTS		0x00000002
#define SCARD_READER_CONFISCATES	0x00000004

#define SCARD_READER_TYPE_SERIAL	0x01
#define SCARD_READER_TYPE_PARALELL	0x02
#define SCARD_READER_TYPE_KEYBOARD	0x04
#define SCARD_READER_TYPE_SCSI		0x08
#define SCARD_READER_TYPE_IDE		0x10
#define SCARD_READER_TYPE_USB		0x20
#define SCARD_READER_TYPE_PCMCIA	0x40
#define SCARD_READER_TYPE_TPM		0x80
#define SCARD_READER_TYPE_NFC		0x100
#define SCARD_READER_TYPE_UICC		0x200
#define SCARD_READER_TYPE_VENDOR	0xF0

#ifndef WINSCARDAPI
#define WINSCARDAPI	WINPR_API
#endif

typedef ULONG_PTR SCARDCONTEXT;
typedef SCARDCONTEXT *PSCARDCONTEXT, *LPSCARDCONTEXT;

typedef ULONG_PTR SCARDHANDLE;
typedef SCARDHANDLE *PSCARDHANDLE, *LPSCARDHANDLE;

#define SCARD_AUTOALLOCATE		(DWORD)(-1)

#define SCARD_SCOPE_USER		0
#define SCARD_SCOPE_TERMINAL		1
#define SCARD_SCOPE_SYSTEM		2

#define SCARD_STATE_UNAWARE		0x00000000
#define SCARD_STATE_IGNORE		0x00000001
#define SCARD_STATE_CHANGED		0x00000002
#define SCARD_STATE_UNKNOWN		0x00000004
#define SCARD_STATE_UNAVAILABLE		0x00000008
#define SCARD_STATE_EMPTY		0x00000010
#define SCARD_STATE_PRESENT		0x00000020
#define SCARD_STATE_ATRMATCH		0x00000040
#define SCARD_STATE_EXCLUSIVE		0x00000080
#define SCARD_STATE_INUSE		0x00000100
#define SCARD_STATE_MUTE		0x00000200
#define SCARD_STATE_UNPOWERED		0x00000400

#define SCARD_SHARE_EXCLUSIVE		1
#define SCARD_SHARE_SHARED		2
#define SCARD_SHARE_DIRECT		3

#define SCARD_LEAVE_CARD		0
#define SCARD_RESET_CARD		1
#define SCARD_UNPOWER_CARD		2
#define SCARD_EJECT_CARD		3

#define SC_DLG_MINIMAL_UI		0x01
#define SC_DLG_NO_UI			0x02
#define SC_DLG_FORCE_UI			0x04

#define SCERR_NOCARDNAME		0x4000
#define SCERR_NOGUIDS			0x8000

typedef SCARDHANDLE (WINAPI *LPOCNCONNPROCA)(SCARDCONTEXT hSCardContext, LPSTR szReader, LPSTR mszCards, PVOID pvUserData);
typedef SCARDHANDLE (WINAPI *LPOCNCONNPROCW)(SCARDCONTEXT hSCardContext, LPWSTR szReader, LPWSTR mszCards, PVOID pvUserData);

typedef BOOL (WINAPI *LPOCNCHKPROC)(SCARDCONTEXT hSCardContext, SCARDHANDLE hCard, PVOID pvUserData);
typedef void (WINAPI *LPOCNDSCPROC)(SCARDCONTEXT hSCardContext, SCARDHANDLE hCard, PVOID pvUserData);

#define SCARD_READER_SEL_AUTH_PACKAGE	((DWORD)-629)

#define SCARD_AUDIT_CHV_FAILURE		0x0
#define SCARD_AUDIT_CHV_SUCCESS		0x1

#define SCardListCardTypes SCardListCards

#define PCSCardIntroduceCardType(hContext, szCardName, pbAtr, pbAtrMask, cbAtrLen, pguidPrimaryProvider, rgguidInterfaces, dwInterfaceCount) \
	SCardIntroduceCardType(hContext, szCardName, pguidPrimaryProvider, rgguidInterfaces, dwInterfaceCount, pbAtr, pbAtrMask, cbAtrLen)

#define SCardGetReaderCapabilities SCardGetAttrib
#define SCardSetReaderCapabilities SCardSetAttrib

typedef struct
{
	LPCSTR szReader;
	LPVOID pvUserData;
	DWORD dwCurrentState;
	DWORD dwEventState;
	DWORD cbAtr;
	BYTE rgbAtr[36];
} SCARD_READERSTATEA, *PSCARD_READERSTATEA, *LPSCARD_READERSTATEA;

typedef struct
{
	LPCWSTR szReader;
	LPVOID pvUserData;
	DWORD dwCurrentState;
	DWORD dwEventState;
	DWORD cbAtr;
	BYTE rgbAtr[36];
} SCARD_READERSTATEW, *PSCARD_READERSTATEW, *LPSCARD_READERSTATEW;

typedef struct _SCARD_ATRMASK
{
	DWORD cbAtr;
	BYTE rgbAtr[36];
	BYTE rgbMask[36];
} SCARD_ATRMASK, *PSCARD_ATRMASK, *LPSCARD_ATRMASK;

typedef struct
{
	DWORD dwStructSize;
	LPSTR lpstrGroupNames;
	DWORD nMaxGroupNames;
	LPCGUID rgguidInterfaces;
	DWORD cguidInterfaces;
	LPSTR lpstrCardNames;
	DWORD nMaxCardNames;
	LPOCNCHKPROC lpfnCheck;
	LPOCNCONNPROCA lpfnConnect;
	LPOCNDSCPROC lpfnDisconnect;
	LPVOID pvUserData;
	DWORD dwShareMode;
	DWORD dwPreferredProtocols;
} OPENCARD_SEARCH_CRITERIAA, *POPENCARD_SEARCH_CRITERIAA, *LPOPENCARD_SEARCH_CRITERIAA;

typedef struct
{
	DWORD dwStructSize;
	LPWSTR lpstrGroupNames;
	DWORD nMaxGroupNames;
	LPCGUID rgguidInterfaces;
	DWORD cguidInterfaces;
	LPWSTR lpstrCardNames;
	DWORD nMaxCardNames;
	LPOCNCHKPROC lpfnCheck;
	LPOCNCONNPROCW lpfnConnect;
	LPOCNDSCPROC lpfnDisconnect;
	LPVOID pvUserData;
	DWORD dwShareMode;
	DWORD dwPreferredProtocols;
} OPENCARD_SEARCH_CRITERIAW, *POPENCARD_SEARCH_CRITERIAW, *LPOPENCARD_SEARCH_CRITERIAW;

typedef struct
{
	DWORD dwStructSize;
	SCARDCONTEXT hSCardContext;
	HWND hwndOwner;
	DWORD dwFlags;
	LPCSTR lpstrTitle;
	LPCSTR lpstrSearchDesc;
	HICON hIcon;
	POPENCARD_SEARCH_CRITERIAA pOpenCardSearchCriteria;
	LPOCNCONNPROCA lpfnConnect;
	LPVOID pvUserData;
	DWORD dwShareMode;
	DWORD dwPreferredProtocols;
	LPSTR lpstrRdr;
	DWORD nMaxRdr;
	LPSTR lpstrCard;
	DWORD nMaxCard;
	DWORD dwActiveProtocol;
	SCARDHANDLE hCardHandle;
} OPENCARDNAME_EXA, *POPENCARDNAME_EXA, *LPOPENCARDNAME_EXA;

typedef struct
{
	DWORD dwStructSize;
	SCARDCONTEXT hSCardContext;
	HWND hwndOwner;
	DWORD dwFlags;
	LPCWSTR lpstrTitle;
	LPCWSTR lpstrSearchDesc;
	HICON hIcon;
	POPENCARD_SEARCH_CRITERIAW pOpenCardSearchCriteria;
	LPOCNCONNPROCW lpfnConnect;
	LPVOID pvUserData;
	DWORD dwShareMode;
	DWORD dwPreferredProtocols;
	LPWSTR lpstrRdr;
	DWORD nMaxRdr;
	LPWSTR lpstrCard;
	DWORD nMaxCard;
	DWORD dwActiveProtocol;
	SCARDHANDLE hCardHandle;
} OPENCARDNAME_EXW, *POPENCARDNAME_EXW, *LPOPENCARDNAME_EXW;

#define OPENCARDNAMEA_EX OPENCARDNAME_EXA
#define OPENCARDNAMEW_EX OPENCARDNAME_EXW
#define POPENCARDNAMEA_EX POPENCARDNAME_EXA
#define POPENCARDNAMEW_EX POPENCARDNAME_EXW
#define LPOPENCARDNAMEA_EX LPOPENCARDNAME_EXA
#define LPOPENCARDNAMEW_EX LPOPENCARDNAME_EXW

typedef enum
{
	RSR_MATCH_TYPE_READER_AND_CONTAINER = 1,
	RSR_MATCH_TYPE_SERIAL_NUMBER,
	RSR_MATCH_TYPE_ALL_CARDS
} READER_SEL_REQUEST_MATCH_TYPE;

typedef struct
{
	DWORD dwShareMode;
	DWORD dwPreferredProtocols;
	READER_SEL_REQUEST_MATCH_TYPE MatchType;
	union
	{
		struct
		{
			DWORD cbReaderNameOffset;
			DWORD cchReaderNameLength;
			DWORD cbContainerNameOffset;
			DWORD cchContainerNameLength;
			DWORD dwDesiredCardModuleVersion;
			DWORD dwCspFlags;
		} ReaderAndContainerParameter;
		struct
		{
			DWORD cbSerialNumberOffset;
			DWORD cbSerialNumberLength;
			DWORD dwDesiredCardModuleVersion;
		} SerialNumberParameter;
	};
} READER_SEL_REQUEST, *PREADER_SEL_REQUEST;

typedef struct
{
	DWORD cbReaderNameOffset;
	DWORD cchReaderNameLength;
	DWORD cbCardNameOffset;
	DWORD cchCardNameLength;
} READER_SEL_RESPONSE, *PREADER_SEL_RESPONSE;

typedef struct
{
	DWORD dwStructSize;
	HWND hwndOwner;
	SCARDCONTEXT hSCardContext;
	LPSTR lpstrGroupNames;
	DWORD nMaxGroupNames;
	LPSTR lpstrCardNames;
	DWORD nMaxCardNames;
	LPCGUID rgguidInterfaces;
	DWORD cguidInterfaces;
	LPSTR lpstrRdr;
	DWORD nMaxRdr;
	LPSTR lpstrCard;
	DWORD nMaxCard;
	LPCSTR lpstrTitle;
	DWORD dwFlags;
	LPVOID pvUserData;
	DWORD dwShareMode;
	DWORD dwPreferredProtocols;
	DWORD dwActiveProtocol;
	LPOCNCONNPROCA lpfnConnect;
	LPOCNCHKPROC lpfnCheck;
	LPOCNDSCPROC lpfnDisconnect;
	SCARDHANDLE hCardHandle;
} OPENCARDNAMEA, *POPENCARDNAMEA, *LPOPENCARDNAMEA;

typedef struct
{
	DWORD dwStructSize;
	HWND hwndOwner;
	SCARDCONTEXT hSCardContext;
	LPWSTR lpstrGroupNames;
	DWORD nMaxGroupNames;
	LPWSTR lpstrCardNames;
	DWORD nMaxCardNames;
	LPCGUID rgguidInterfaces;
	DWORD cguidInterfaces;
	LPWSTR lpstrRdr;
	DWORD nMaxRdr;
	LPWSTR lpstrCard;
	DWORD nMaxCard;
	LPCWSTR lpstrTitle;
	DWORD dwFlags;
	LPVOID pvUserData;
	DWORD dwShareMode;
	DWORD dwPreferredProtocols;
	DWORD dwActiveProtocol;
	LPOCNCONNPROCW lpfnConnect;
	LPOCNCHKPROC lpfnCheck;
	LPOCNDSCPROC lpfnDisconnect;
	SCARDHANDLE hCardHandle;
} OPENCARDNAMEW, *POPENCARDNAMEW, *LPOPENCARDNAMEW;

#if defined(__APPLE__) | defined(sun)
#pragma pack()
#else
#pragma pack(pop)
#endif

#ifdef UNICODE
#define LPOCNCONNPROC			LPOCNCONNPROCW
#define SCARD_READERSTATE		SCARD_READERSTATEW
#define PSCARD_READERSTATE		PSCARD_READERSTATEW
#define LPSCARD_READERSTATE		LPSCARD_READERSTATEW
#define OPENCARD_SEARCH_CRITERIA	OPENCARD_SEARCH_CRITERIAW
#define LOPENCARD_SEARCH_CRITERIA	LOPENCARD_SEARCH_CRITERIAW
#define LPOPENCARD_SEARCH_CRITERIA	LPOPENCARD_SEARCH_CRITERIAW
#define OPENCARDNAME_EX			OPENCARDNAME_EXW
#define LOPENCARDNAME_EX		LOPENCARDNAME_EXW
#define LPOPENCARDNAME_EX		LPOPENCARDNAME_EXW
#define OPENCARDNAME			OPENCARDNAMEW
#define LOPENCARDNAME			LOPENCARDNAMEW
#define LPOPENCARDNAME			LPOPENCARDNAMEW
#else
#define LPOCNCONNPROC			LPOCNCONNPROCA
#define SCARD_READERSTATE		SCARD_READERSTATEA
#define PSCARD_READERSTATE		PSCARD_READERSTATEA
#define LPSCARD_READERSTATE		LPSCARD_READERSTATEA
#define OPENCARD_SEARCH_CRITERIA	OPENCARD_SEARCH_CRITERIAA
#define LOPENCARD_SEARCH_CRITERIA	LOPENCARD_SEARCH_CRITERIAA
#define LPOPENCARD_SEARCH_CRITERIA	LPOPENCARD_SEARCH_CRITERIAA
#define OPENCARDNAME_EX			OPENCARDNAME_EXA
#define LOPENCARDNAME_EX		LOPENCARDNAME_EXA
#define LPOPENCARDNAME_EX		LPOPENCARDNAME_EXA
#define OPENCARDNAME			OPENCARDNAMEA
#define LOPENCARDNAME			LOPENCARDNAMEA
#define LPOPENCARDNAME			LPOPENCARDNAMEA
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern const SCARD_IO_REQUEST g_rgSCardT0Pci;
extern const SCARD_IO_REQUEST g_rgSCardT1Pci;
extern const SCARD_IO_REQUEST g_rgSCardRawPci;

#define SCARD_PCI_T0	(&g_rgSCardT0Pci)
#define SCARD_PCI_T1	(&g_rgSCardT1Pci)
#define SCARD_PCI_RAW	(&g_rgSCardRawPci)

WINSCARDAPI LONG WINAPI SCardEstablishContext(DWORD dwScope,
		LPCVOID pvReserved1, LPCVOID pvReserved2, LPSCARDCONTEXT phContext);

WINSCARDAPI LONG WINAPI SCardReleaseContext(SCARDCONTEXT hContext);

WINSCARDAPI LONG WINAPI SCardIsValidContext(SCARDCONTEXT hContext);

WINSCARDAPI LONG WINAPI SCardListReaderGroupsA(SCARDCONTEXT hContext,
		LPSTR mszGroups, LPDWORD pcchGroups);
WINSCARDAPI LONG WINAPI SCardListReaderGroupsW(SCARDCONTEXT hContext,
		LPWSTR mszGroups, LPDWORD pcchGroups);

WINSCARDAPI LONG WINAPI SCardListReadersA(SCARDCONTEXT hContext,
		LPCSTR mszGroups, LPSTR mszReaders, LPDWORD pcchReaders);
WINSCARDAPI LONG WINAPI SCardListReadersW(SCARDCONTEXT hContext,
		LPCWSTR mszGroups, LPWSTR mszReaders, LPDWORD pcchReaders);

WINSCARDAPI LONG WINAPI SCardListCardsA(SCARDCONTEXT hContext,
		LPCBYTE pbAtr, LPCGUID rgquidInterfaces, DWORD cguidInterfaceCount, CHAR* mszCards, LPDWORD pcchCards);

WINSCARDAPI LONG WINAPI SCardListCardsW(SCARDCONTEXT hContext,
		LPCBYTE pbAtr, LPCGUID rgquidInterfaces, DWORD cguidInterfaceCount, WCHAR* mszCards, LPDWORD pcchCards);

WINSCARDAPI LONG WINAPI SCardListInterfacesA(SCARDCONTEXT hContext,
		LPCSTR szCard, LPGUID pguidInterfaces, LPDWORD pcguidInterfaces);
WINSCARDAPI LONG WINAPI SCardListInterfacesW(SCARDCONTEXT hContext,
		LPCWSTR szCard, LPGUID pguidInterfaces, LPDWORD pcguidInterfaces);

WINSCARDAPI LONG WINAPI SCardGetProviderIdA(SCARDCONTEXT hContext,
		LPCSTR szCard, LPGUID pguidProviderId);
WINSCARDAPI LONG WINAPI SCardGetProviderIdW(SCARDCONTEXT hContext,
		LPCWSTR szCard, LPGUID pguidProviderId);

WINSCARDAPI LONG WINAPI SCardGetCardTypeProviderNameA(SCARDCONTEXT hContext,
		LPCSTR szCardName, DWORD dwProviderId, CHAR* szProvider, LPDWORD pcchProvider);
WINSCARDAPI LONG WINAPI SCardGetCardTypeProviderNameW(SCARDCONTEXT hContext,
		LPCWSTR szCardName, DWORD dwProviderId, WCHAR* szProvider, LPDWORD pcchProvider);

WINSCARDAPI LONG WINAPI SCardIntroduceReaderGroupA(SCARDCONTEXT hContext, LPCSTR szGroupName);
WINSCARDAPI LONG WINAPI SCardIntroduceReaderGroupW(SCARDCONTEXT hContext, LPCWSTR szGroupName);

WINSCARDAPI LONG WINAPI SCardForgetReaderGroupA(SCARDCONTEXT hContext, LPCSTR szGroupName);
WINSCARDAPI LONG WINAPI SCardForgetReaderGroupW(SCARDCONTEXT hContext, LPCWSTR szGroupName);

WINSCARDAPI LONG WINAPI SCardIntroduceReaderA(SCARDCONTEXT hContext,
		LPCSTR szReaderName, LPCSTR szDeviceName);
WINSCARDAPI LONG WINAPI SCardIntroduceReaderW(SCARDCONTEXT hContext,
		LPCWSTR szReaderName, LPCWSTR szDeviceName);

WINSCARDAPI LONG WINAPI SCardForgetReaderA(SCARDCONTEXT hContext, LPCSTR szReaderName);
WINSCARDAPI LONG WINAPI SCardForgetReaderW(SCARDCONTEXT hContext, LPCWSTR szReaderName);

WINSCARDAPI LONG WINAPI SCardAddReaderToGroupA(SCARDCONTEXT hContext,
		LPCSTR szReaderName, LPCSTR szGroupName);
WINSCARDAPI LONG WINAPI SCardAddReaderToGroupW(SCARDCONTEXT hContext,
		LPCWSTR szReaderName, LPCWSTR szGroupName);

WINSCARDAPI LONG WINAPI SCardRemoveReaderFromGroupA(SCARDCONTEXT hContext,
		LPCSTR szReaderName, LPCSTR szGroupName);
WINSCARDAPI LONG WINAPI SCardRemoveReaderFromGroupW(SCARDCONTEXT hContext,
		LPCWSTR szReaderName, LPCWSTR szGroupName);

WINSCARDAPI LONG WINAPI SCardIntroduceCardTypeA(SCARDCONTEXT hContext,
		LPCSTR szCardName, LPCGUID pguidPrimaryProvider, LPCGUID rgguidInterfaces,
		DWORD dwInterfaceCount, LPCBYTE pbAtr, LPCBYTE pbAtrMask, DWORD cbAtrLen);
WINSCARDAPI LONG WINAPI SCardIntroduceCardTypeW(SCARDCONTEXT hContext,
		LPCWSTR szCardName, LPCGUID pguidPrimaryProvider, LPCGUID rgguidInterfaces,
		DWORD dwInterfaceCount, LPCBYTE pbAtr, LPCBYTE pbAtrMask, DWORD cbAtrLen);

WINSCARDAPI LONG WINAPI SCardSetCardTypeProviderNameA(SCARDCONTEXT hContext,
		LPCSTR szCardName, DWORD dwProviderId, LPCSTR szProvider);
WINSCARDAPI LONG WINAPI SCardSetCardTypeProviderNameW(SCARDCONTEXT hContext,
		LPCWSTR szCardName, DWORD dwProviderId, LPCWSTR szProvider);

WINSCARDAPI LONG WINAPI SCardForgetCardTypeA(SCARDCONTEXT hContext, LPCSTR szCardName);
WINSCARDAPI LONG WINAPI SCardForgetCardTypeW(SCARDCONTEXT hContext, LPCWSTR szCardName);

WINSCARDAPI LONG WINAPI SCardFreeMemory(SCARDCONTEXT hContext, LPCVOID pvMem);

WINSCARDAPI HANDLE WINAPI SCardAccessStartedEvent(void);

WINSCARDAPI void WINAPI SCardReleaseStartedEvent(void);

WINSCARDAPI LONG WINAPI SCardLocateCardsA(SCARDCONTEXT hContext,
		LPCSTR mszCards, LPSCARD_READERSTATEA rgReaderStates, DWORD cReaders);
WINSCARDAPI LONG WINAPI SCardLocateCardsW(SCARDCONTEXT hContext,
		LPCWSTR mszCards, LPSCARD_READERSTATEW rgReaderStates, DWORD cReaders);

WINSCARDAPI LONG WINAPI SCardLocateCardsByATRA(SCARDCONTEXT hContext,
		LPSCARD_ATRMASK rgAtrMasks, DWORD cAtrs, LPSCARD_READERSTATEA rgReaderStates, DWORD cReaders);
WINSCARDAPI LONG WINAPI SCardLocateCardsByATRW(SCARDCONTEXT hContext,
		LPSCARD_ATRMASK rgAtrMasks, DWORD cAtrs, LPSCARD_READERSTATEW rgReaderStates, DWORD cReaders);

WINSCARDAPI LONG WINAPI SCardGetStatusChangeA(SCARDCONTEXT hContext,
		DWORD dwTimeout, LPSCARD_READERSTATEA rgReaderStates, DWORD cReaders);
WINSCARDAPI LONG WINAPI SCardGetStatusChangeW(SCARDCONTEXT hContext,
		DWORD dwTimeout, LPSCARD_READERSTATEW rgReaderStates, DWORD cReaders);

WINSCARDAPI LONG WINAPI SCardCancel(SCARDCONTEXT hContext);

WINSCARDAPI LONG WINAPI SCardConnectA(SCARDCONTEXT hContext,
		LPCSTR szReader, DWORD dwShareMode, DWORD dwPreferredProtocols,
		LPSCARDHANDLE phCard, LPDWORD pdwActiveProtocol);
WINSCARDAPI LONG WINAPI SCardConnectW(SCARDCONTEXT hContext,
		LPCWSTR szReader, DWORD dwShareMode, DWORD dwPreferredProtocols,
		LPSCARDHANDLE phCard, LPDWORD pdwActiveProtocol);

WINSCARDAPI LONG WINAPI SCardReconnect(SCARDHANDLE hCard,
		DWORD dwShareMode, DWORD dwPreferredProtocols, DWORD dwInitialization, LPDWORD pdwActiveProtocol);

WINSCARDAPI LONG WINAPI SCardDisconnect(SCARDHANDLE hCard, DWORD dwDisposition);

WINSCARDAPI LONG WINAPI SCardBeginTransaction(SCARDHANDLE hCard);

WINSCARDAPI LONG WINAPI SCardEndTransaction(SCARDHANDLE hCard, DWORD dwDisposition);

WINSCARDAPI LONG WINAPI SCardCancelTransaction(SCARDHANDLE hCard);

WINSCARDAPI LONG WINAPI SCardState(SCARDHANDLE hCard,
		LPDWORD pdwState, LPDWORD pdwProtocol, LPBYTE pbAtr, LPDWORD pcbAtrLen);

WINSCARDAPI LONG WINAPI SCardStatusA(SCARDHANDLE hCard,
		LPSTR mszReaderNames, LPDWORD pcchReaderLen, LPDWORD pdwState,
		LPDWORD pdwProtocol, LPBYTE pbAtr, LPDWORD pcbAtrLen);
WINSCARDAPI LONG WINAPI SCardStatusW(SCARDHANDLE hCard,
		LPWSTR mszReaderNames, LPDWORD pcchReaderLen, LPDWORD pdwState,
		LPDWORD pdwProtocol, LPBYTE pbAtr, LPDWORD pcbAtrLen);

WINSCARDAPI LONG WINAPI SCardTransmit(SCARDHANDLE hCard,
		LPCSCARD_IO_REQUEST pioSendPci, LPCBYTE pbSendBuffer, DWORD cbSendLength,
		LPSCARD_IO_REQUEST pioRecvPci, LPBYTE pbRecvBuffer, LPDWORD pcbRecvLength);

WINSCARDAPI LONG WINAPI SCardGetTransmitCount(SCARDHANDLE hCard, LPDWORD pcTransmitCount);

WINSCARDAPI LONG WINAPI SCardControl(SCARDHANDLE hCard,
		DWORD dwControlCode, LPCVOID lpInBuffer, DWORD cbInBufferSize,
		LPVOID lpOutBuffer, DWORD cbOutBufferSize, LPDWORD lpBytesReturned);

WINSCARDAPI LONG WINAPI SCardGetAttrib(SCARDHANDLE hCard, DWORD dwAttrId, LPBYTE pbAttr, LPDWORD pcbAttrLen);

WINSCARDAPI LONG WINAPI SCardSetAttrib(SCARDHANDLE hCard, DWORD dwAttrId, LPCBYTE pbAttr, DWORD cbAttrLen);

WINSCARDAPI LONG WINAPI SCardUIDlgSelectCardA(LPOPENCARDNAMEA_EX pDlgStruc);
WINSCARDAPI LONG WINAPI SCardUIDlgSelectCardW(LPOPENCARDNAMEW_EX pDlgStruc);

WINSCARDAPI LONG WINAPI GetOpenCardNameA(LPOPENCARDNAMEA pDlgStruc);
WINSCARDAPI LONG WINAPI GetOpenCardNameW(LPOPENCARDNAMEW pDlgStruc);

WINSCARDAPI LONG WINAPI SCardDlgExtendedError(void);

WINSCARDAPI LONG WINAPI SCardReadCacheA(SCARDCONTEXT hContext,
		UUID* CardIdentifier, DWORD FreshnessCounter, LPSTR LookupName, PBYTE Data, DWORD* DataLen);
WINSCARDAPI LONG WINAPI SCardReadCacheW(SCARDCONTEXT hContext,
		UUID* CardIdentifier,  DWORD FreshnessCounter, LPWSTR LookupName, PBYTE Data, DWORD* DataLen);

WINSCARDAPI LONG WINAPI SCardWriteCacheA(SCARDCONTEXT hContext,
		UUID* CardIdentifier, DWORD FreshnessCounter, LPSTR LookupName, PBYTE Data, DWORD DataLen);
WINSCARDAPI LONG WINAPI SCardWriteCacheW(SCARDCONTEXT hContext,
		UUID* CardIdentifier, DWORD FreshnessCounter, LPWSTR LookupName, PBYTE Data, DWORD DataLen);

WINSCARDAPI LONG WINAPI SCardGetReaderIconA(SCARDCONTEXT hContext,
		LPCSTR szReaderName, LPBYTE pbIcon, LPDWORD pcbIcon);
WINSCARDAPI LONG WINAPI SCardGetReaderIconW(SCARDCONTEXT hContext,
		LPCWSTR szReaderName, LPBYTE pbIcon, LPDWORD pcbIcon);

WINSCARDAPI LONG WINAPI SCardGetDeviceTypeIdA(SCARDCONTEXT hContext, LPCSTR szReaderName, LPDWORD pdwDeviceTypeId);
WINSCARDAPI LONG WINAPI SCardGetDeviceTypeIdW(SCARDCONTEXT hContext, LPCWSTR szReaderName, LPDWORD pdwDeviceTypeId);

WINSCARDAPI LONG WINAPI SCardGetReaderDeviceInstanceIdA(SCARDCONTEXT hContext,
		LPCSTR szReaderName, LPSTR szDeviceInstanceId, LPDWORD pcchDeviceInstanceId);
WINSCARDAPI LONG WINAPI SCardGetReaderDeviceInstanceIdW(SCARDCONTEXT hContext,
		LPCWSTR szReaderName, LPWSTR szDeviceInstanceId, LPDWORD pcchDeviceInstanceId);

WINSCARDAPI LONG WINAPI SCardListReadersWithDeviceInstanceIdA(SCARDCONTEXT hContext,
		LPCSTR szDeviceInstanceId, LPSTR mszReaders, LPDWORD pcchReaders);
WINSCARDAPI LONG WINAPI SCardListReadersWithDeviceInstanceIdW(SCARDCONTEXT hContext,
		LPCWSTR szDeviceInstanceId, LPWSTR mszReaders, LPDWORD pcchReaders);

WINSCARDAPI LONG WINAPI SCardAudit(SCARDCONTEXT hContext, DWORD dwEvent);

#ifdef UNICODE
#define SCardListReaderGroups			SCardListReaderGroupsW
#define SCardListReaders			SCardListReadersW
#define SCardListCards				SCardListCardsW
#define SCardListInterfaces			SCardListInterfacesW
#define SCardGetProviderId			SCardGetProviderIdW
#define SCardGetCardTypeProviderName		SCardGetCardTypeProviderNameW
#define SCardIntroduceReaderGroup		SCardIntroduceReaderGroupW
#define SCardForgetReaderGroup			SCardForgetReaderGroupW
#define SCardIntroduceReader			SCardIntroduceReaderW
#define SCardForgetReader			SCardForgetReaderW
#define SCardAddReaderToGroup			SCardAddReaderToGroupW
#define SCardRemoveReaderFromGroup		SCardRemoveReaderFromGroupW
#define SCardIntroduceCardType			SCardIntroduceCardTypeW
#define SCardSetCardTypeProviderName		SCardSetCardTypeProviderNameW
#define SCardForgetCardType			SCardForgetCardTypeW
#define SCardLocateCards			SCardLocateCardsW
#define SCardLocateCardsByATR			SCardLocateCardsByATRW
#define SCardGetStatusChange			SCardGetStatusChangeW
#define SCardConnect				SCardConnectW
#define SCardStatus				SCardStatusW
#define SCardUIDlgSelectCard			SCardUIDlgSelectCardW
#define GetOpenCardName				GetOpenCardNameW
#define SCardReadCache				SCardReadCacheW
#define SCardWriteCache				SCardWriteCacheW
#define SCardGetReaderIcon			SCardGetReaderIconW
#define SCardGetDeviceTypeId			SCardGetDeviceTypeIdW
#define SCardGetReaderDeviceInstanceId		SCardGetReaderDeviceInstanceIdW
#define SCardListReadersWithDeviceInstanceId	SCardListReadersWithDeviceInstanceIdW
#else
#define SCardListReaderGroups			SCardListReaderGroupsA
#define SCardListReaders			SCardListReadersA
#define SCardListCards				SCardListCardsA
#define SCardListInterfaces			SCardListInterfacesA
#define SCardGetProviderId			SCardGetProviderIdA
#define SCardGetCardTypeProviderName		SCardGetCardTypeProviderNameA
#define SCardIntroduceReaderGroup		SCardIntroduceReaderGroupA
#define SCardForgetReaderGroup			SCardForgetReaderGroupA
#define SCardIntroduceReader			SCardIntroduceReaderA
#define SCardForgetReader			SCardForgetReaderA
#define SCardAddReaderToGroup			SCardAddReaderToGroupA
#define SCardRemoveReaderFromGroup		SCardRemoveReaderFromGroupA
#define SCardIntroduceCardType			SCardIntroduceCardTypeA
#define SCardSetCardTypeProviderName		SCardSetCardTypeProviderNameA
#define SCardForgetCardType			SCardForgetCardTypeA
#define SCardLocateCards			SCardLocateCardsA
#define SCardLocateCardsByATR			SCardLocateCardsByATRA
#define SCardGetStatusChange			SCardGetStatusChangeA
#define SCardConnect				SCardConnectA
#define SCardStatus				SCardStatusA
#define SCardUIDlgSelectCard			SCardUIDlgSelectCardA
#define GetOpenCardName				GetOpenCardNameA
#define SCardReadCache				SCardReadCacheA
#define SCardWriteCache				SCardWriteCacheA
#define SCardGetReaderIcon			SCardGetReaderIconA
#define SCardGetDeviceTypeId			SCardGetDeviceTypeIdA
#define SCardGetReaderDeviceInstanceId		SCardGetReaderDeviceInstanceIdA
#define SCardListReadersWithDeviceInstanceId	SCardListReadersWithDeviceInstanceIdA
#endif

#ifdef __cplusplus
}
#endif

/**
 * Extended API
 */

typedef LONG (WINAPI * fnSCardEstablishContext)(DWORD dwScope,
		LPCVOID pvReserved1, LPCVOID pvReserved2, LPSCARDCONTEXT phContext);

typedef LONG (WINAPI * fnSCardReleaseContext)(SCARDCONTEXT hContext);

typedef LONG (WINAPI * fnSCardIsValidContext)(SCARDCONTEXT hContext);

typedef LONG (WINAPI * fnSCardListReaderGroupsA)(SCARDCONTEXT hContext,
		LPSTR mszGroups, LPDWORD pcchGroups);
typedef LONG (WINAPI * fnSCardListReaderGroupsW)(SCARDCONTEXT hContext,
		LPWSTR mszGroups, LPDWORD pcchGroups);

typedef LONG (WINAPI * fnSCardListReadersA)(SCARDCONTEXT hContext,
		LPCSTR mszGroups, LPSTR mszReaders, LPDWORD pcchReaders);
typedef LONG (WINAPI * fnSCardListReadersW)(SCARDCONTEXT hContext,
		LPCWSTR mszGroups, LPWSTR mszReaders, LPDWORD pcchReaders);

typedef LONG (WINAPI * fnSCardListCardsA)(SCARDCONTEXT hContext,
		LPCBYTE pbAtr, LPCGUID rgquidInterfaces, DWORD cguidInterfaceCount, CHAR* mszCards, LPDWORD pcchCards);

typedef LONG (WINAPI * fnSCardListCardsW)(SCARDCONTEXT hContext,
		LPCBYTE pbAtr, LPCGUID rgquidInterfaces, DWORD cguidInterfaceCount, WCHAR* mszCards, LPDWORD pcchCards);

typedef LONG (WINAPI * fnSCardListInterfacesA)(SCARDCONTEXT hContext,
		LPCSTR szCard, LPGUID pguidInterfaces, LPDWORD pcguidInterfaces);
typedef LONG (WINAPI * fnSCardListInterfacesW)(SCARDCONTEXT hContext,
		LPCWSTR szCard, LPGUID pguidInterfaces, LPDWORD pcguidInterfaces);

typedef LONG (WINAPI * fnSCardGetProviderIdA)(SCARDCONTEXT hContext,
		LPCSTR szCard, LPGUID pguidProviderId);
typedef LONG (WINAPI * fnSCardGetProviderIdW)(SCARDCONTEXT hContext,
		LPCWSTR szCard, LPGUID pguidProviderId);

typedef LONG (WINAPI * fnSCardGetCardTypeProviderNameA)(SCARDCONTEXT hContext,
		LPCSTR szCardName, DWORD dwProviderId, CHAR* szProvider, LPDWORD pcchProvider);
typedef LONG (WINAPI * fnSCardGetCardTypeProviderNameW)(SCARDCONTEXT hContext,
		LPCWSTR szCardName, DWORD dwProviderId, WCHAR* szProvider, LPDWORD pcchProvider);

typedef LONG (WINAPI * fnSCardIntroduceReaderGroupA)(SCARDCONTEXT hContext, LPCSTR szGroupName);
typedef LONG (WINAPI * fnSCardIntroduceReaderGroupW)(SCARDCONTEXT hContext, LPCWSTR szGroupName);

typedef LONG (WINAPI * fnSCardForgetReaderGroupA)(SCARDCONTEXT hContext, LPCSTR szGroupName);
typedef LONG (WINAPI * fnSCardForgetReaderGroupW)(SCARDCONTEXT hContext, LPCWSTR szGroupName);

typedef LONG (WINAPI * fnSCardIntroduceReaderA)(SCARDCONTEXT hContext,
		LPCSTR szReaderName, LPCSTR szDeviceName);
typedef LONG (WINAPI * fnSCardIntroduceReaderW)(SCARDCONTEXT hContext,
		LPCWSTR szReaderName, LPCWSTR szDeviceName);

typedef LONG (WINAPI * fnSCardForgetReaderA)(SCARDCONTEXT hContext, LPCSTR szReaderName);
typedef LONG (WINAPI * fnSCardForgetReaderW)(SCARDCONTEXT hContext, LPCWSTR szReaderName);

typedef LONG (WINAPI * fnSCardAddReaderToGroupA)(SCARDCONTEXT hContext,
		LPCSTR szReaderName, LPCSTR szGroupName);
typedef LONG (WINAPI * fnSCardAddReaderToGroupW)(SCARDCONTEXT hContext,
		LPCWSTR szReaderName, LPCWSTR szGroupName);

typedef LONG (WINAPI * fnSCardRemoveReaderFromGroupA)(SCARDCONTEXT hContext,
		LPCSTR szReaderName, LPCSTR szGroupName);
typedef LONG (WINAPI * fnSCardRemoveReaderFromGroupW)(SCARDCONTEXT hContext,
		LPCWSTR szReaderName, LPCWSTR szGroupName);

typedef LONG (WINAPI * fnSCardIntroduceCardTypeA)(SCARDCONTEXT hContext,
		LPCSTR szCardName, LPCGUID pguidPrimaryProvider, LPCGUID rgguidInterfaces,
		DWORD dwInterfaceCount, LPCBYTE pbAtr, LPCBYTE pbAtrMask, DWORD cbAtrLen);
typedef LONG (WINAPI * fnSCardIntroduceCardTypeW)(SCARDCONTEXT hContext,
		LPCWSTR szCardName, LPCGUID pguidPrimaryProvider, LPCGUID rgguidInterfaces,
		DWORD dwInterfaceCount, LPCBYTE pbAtr, LPCBYTE pbAtrMask, DWORD cbAtrLen);

typedef LONG (WINAPI * fnSCardSetCardTypeProviderNameA)(SCARDCONTEXT hContext,
		LPCSTR szCardName, DWORD dwProviderId, LPCSTR szProvider);
typedef LONG (WINAPI * fnSCardSetCardTypeProviderNameW)(SCARDCONTEXT hContext,
		LPCWSTR szCardName, DWORD dwProviderId, LPCWSTR szProvider);

typedef LONG (WINAPI * fnSCardForgetCardTypeA)(SCARDCONTEXT hContext, LPCSTR szCardName);
typedef LONG (WINAPI * fnSCardForgetCardTypeW)(SCARDCONTEXT hContext, LPCWSTR szCardName);

typedef LONG (WINAPI * fnSCardFreeMemory)(SCARDCONTEXT hContext, LPCVOID pvMem);

typedef HANDLE (WINAPI * fnSCardAccessStartedEvent)(void);

typedef void (WINAPI * fnSCardReleaseStartedEvent)(void);

typedef LONG (WINAPI * fnSCardLocateCardsA)(SCARDCONTEXT hContext,
		LPCSTR mszCards, LPSCARD_READERSTATEA rgReaderStates, DWORD cReaders);
typedef LONG (WINAPI * fnSCardLocateCardsW)(SCARDCONTEXT hContext,
		LPCWSTR mszCards, LPSCARD_READERSTATEW rgReaderStates, DWORD cReaders);

typedef LONG (WINAPI * fnSCardLocateCardsByATRA)(SCARDCONTEXT hContext,
		LPSCARD_ATRMASK rgAtrMasks, DWORD cAtrs, LPSCARD_READERSTATEA rgReaderStates, DWORD cReaders);
typedef LONG (WINAPI * fnSCardLocateCardsByATRW)(SCARDCONTEXT hContext,
		LPSCARD_ATRMASK rgAtrMasks, DWORD cAtrs, LPSCARD_READERSTATEW rgReaderStates, DWORD cReaders);

typedef LONG (WINAPI * fnSCardGetStatusChangeA)(SCARDCONTEXT hContext,
		DWORD dwTimeout, LPSCARD_READERSTATEA rgReaderStates, DWORD cReaders);
typedef LONG (WINAPI * fnSCardGetStatusChangeW)(SCARDCONTEXT hContext,
		DWORD dwTimeout, LPSCARD_READERSTATEW rgReaderStates, DWORD cReaders);

typedef LONG (WINAPI * fnSCardCancel)(SCARDCONTEXT hContext);

typedef LONG (WINAPI * fnSCardConnectA)(SCARDCONTEXT hContext,
		LPCSTR szReader, DWORD dwShareMode, DWORD dwPreferredProtocols,
		LPSCARDHANDLE phCard, LPDWORD pdwActiveProtocol);
typedef LONG (WINAPI * fnSCardConnectW)(SCARDCONTEXT hContext,
		LPCWSTR szReader, DWORD dwShareMode, DWORD dwPreferredProtocols,
		LPSCARDHANDLE phCard, LPDWORD pdwActiveProtocol);

typedef LONG (WINAPI * fnSCardReconnect)(SCARDHANDLE hCard,
		DWORD dwShareMode, DWORD dwPreferredProtocols, DWORD dwInitialization, LPDWORD pdwActiveProtocol);

typedef LONG (WINAPI * fnSCardDisconnect)(SCARDHANDLE hCard, DWORD dwDisposition);

typedef LONG (WINAPI * fnSCardBeginTransaction)(SCARDHANDLE hCard);

typedef LONG (WINAPI * fnSCardEndTransaction)(SCARDHANDLE hCard, DWORD dwDisposition);

typedef LONG (WINAPI * fnSCardCancelTransaction)(SCARDHANDLE hCard);

typedef LONG (WINAPI * fnSCardState)(SCARDHANDLE hCard,
		LPDWORD pdwState, LPDWORD pdwProtocol, LPBYTE pbAtr, LPDWORD pcbAtrLen);

typedef LONG (WINAPI * fnSCardStatusA)(SCARDHANDLE hCard,
		LPSTR mszReaderNames, LPDWORD pcchReaderLen, LPDWORD pdwState,
		LPDWORD pdwProtocol, LPBYTE pbAtr, LPDWORD pcbAtrLen);
typedef LONG (WINAPI * fnSCardStatusW)(SCARDHANDLE hCard,
		LPWSTR mszReaderNames, LPDWORD pcchReaderLen, LPDWORD pdwState,
		LPDWORD pdwProtocol, LPBYTE pbAtr, LPDWORD pcbAtrLen);

typedef LONG (WINAPI * fnSCardTransmit)(SCARDHANDLE hCard,
		LPCSCARD_IO_REQUEST pioSendPci, LPCBYTE pbSendBuffer, DWORD cbSendLength,
		LPSCARD_IO_REQUEST pioRecvPci, LPBYTE pbRecvBuffer, LPDWORD pcbRecvLength);

typedef LONG (WINAPI * fnSCardGetTransmitCount)(SCARDHANDLE hCard, LPDWORD pcTransmitCount);

typedef LONG (WINAPI * fnSCardControl)(SCARDHANDLE hCard,
		DWORD dwControlCode, LPCVOID lpInBuffer, DWORD cbInBufferSize,
		LPVOID lpOutBuffer, DWORD cbOutBufferSize, LPDWORD lpBytesReturned);

typedef LONG (WINAPI * fnSCardGetAttrib)(SCARDHANDLE hCard, DWORD dwAttrId, LPBYTE pbAttr, LPDWORD pcbAttrLen);

typedef LONG (WINAPI * fnSCardSetAttrib)(SCARDHANDLE hCard, DWORD dwAttrId, LPCBYTE pbAttr, DWORD cbAttrLen);

typedef LONG (WINAPI * fnSCardUIDlgSelectCardA)(LPOPENCARDNAMEA_EX pDlgStruc);
typedef LONG (WINAPI * fnSCardUIDlgSelectCardW)(LPOPENCARDNAMEW_EX pDlgStruc);

typedef LONG (WINAPI * fnGetOpenCardNameA)(LPOPENCARDNAMEA pDlgStruc);
typedef LONG (WINAPI * fnGetOpenCardNameW)(LPOPENCARDNAMEW pDlgStruc);

typedef LONG (WINAPI * fnSCardDlgExtendedError)(void);

typedef LONG (WINAPI * fnSCardReadCacheA)(SCARDCONTEXT hContext,
		UUID* CardIdentifier, DWORD FreshnessCounter, LPSTR LookupName, PBYTE Data, DWORD* DataLen);
typedef LONG (WINAPI * fnSCardReadCacheW)(SCARDCONTEXT hContext,
		UUID* CardIdentifier,  DWORD FreshnessCounter, LPWSTR LookupName, PBYTE Data, DWORD* DataLen);

typedef LONG (WINAPI * fnSCardWriteCacheA)(SCARDCONTEXT hContext,
		UUID* CardIdentifier, DWORD FreshnessCounter, LPSTR LookupName, PBYTE Data, DWORD DataLen);
typedef LONG (WINAPI * fnSCardWriteCacheW)(SCARDCONTEXT hContext,
		UUID* CardIdentifier, DWORD FreshnessCounter, LPWSTR LookupName, PBYTE Data, DWORD DataLen);

typedef LONG (WINAPI * fnSCardGetReaderIconA)(SCARDCONTEXT hContext,
		LPCSTR szReaderName, LPBYTE pbIcon, LPDWORD pcbIcon);
typedef LONG (WINAPI * fnSCardGetReaderIconW)(SCARDCONTEXT hContext,
		LPCWSTR szReaderName, LPBYTE pbIcon, LPDWORD pcbIcon);

typedef LONG (WINAPI * fnSCardGetDeviceTypeIdA)(SCARDCONTEXT hContext, LPCSTR szReaderName, LPDWORD pdwDeviceTypeId);
typedef LONG (WINAPI * fnSCardGetDeviceTypeIdW)(SCARDCONTEXT hContext, LPCWSTR szReaderName, LPDWORD pdwDeviceTypeId);

typedef LONG (WINAPI * fnSCardGetReaderDeviceInstanceIdA)(SCARDCONTEXT hContext,
		LPCSTR szReaderName, LPSTR szDeviceInstanceId, LPDWORD pcchDeviceInstanceId);
typedef LONG (WINAPI * fnSCardGetReaderDeviceInstanceIdW)(SCARDCONTEXT hContext,
		LPCWSTR szReaderName, LPWSTR szDeviceInstanceId, LPDWORD pcchDeviceInstanceId);

typedef LONG (WINAPI * fnSCardListReadersWithDeviceInstanceIdA)(SCARDCONTEXT hContext,
		LPCSTR szDeviceInstanceId, LPSTR mszReaders, LPDWORD pcchReaders);
typedef LONG (WINAPI * fnSCardListReadersWithDeviceInstanceIdW)(SCARDCONTEXT hContext,
		LPCWSTR szDeviceInstanceId, LPWSTR mszReaders, LPDWORD pcchReaders);

typedef LONG (WINAPI * fnSCardAudit)(SCARDCONTEXT hContext, DWORD dwEvent);

struct _SCardApiFunctionTable
{
	DWORD dwVersion;
	DWORD dwFlags;

	fnSCardEstablishContext pfnSCardEstablishContext;
	fnSCardReleaseContext pfnSCardReleaseContext;
	fnSCardIsValidContext pfnSCardIsValidContext;
	fnSCardListReaderGroupsA pfnSCardListReaderGroupsA;
	fnSCardListReaderGroupsW pfnSCardListReaderGroupsW;
	fnSCardListReadersA pfnSCardListReadersA;
	fnSCardListReadersW pfnSCardListReadersW;
	fnSCardListCardsA pfnSCardListCardsA;
	fnSCardListCardsW pfnSCardListCardsW;
	fnSCardListInterfacesA pfnSCardListInterfacesA;
	fnSCardListInterfacesW pfnSCardListInterfacesW;
	fnSCardGetProviderIdA pfnSCardGetProviderIdA;
	fnSCardGetProviderIdW pfnSCardGetProviderIdW;
	fnSCardGetCardTypeProviderNameA pfnSCardGetCardTypeProviderNameA;
	fnSCardGetCardTypeProviderNameW pfnSCardGetCardTypeProviderNameW;
	fnSCardIntroduceReaderGroupA pfnSCardIntroduceReaderGroupA;
	fnSCardIntroduceReaderGroupW pfnSCardIntroduceReaderGroupW;
	fnSCardForgetReaderGroupA pfnSCardForgetReaderGroupA;
	fnSCardForgetReaderGroupW pfnSCardForgetReaderGroupW;
	fnSCardIntroduceReaderA pfnSCardIntroduceReaderA;
	fnSCardIntroduceReaderW pfnSCardIntroduceReaderW;
	fnSCardForgetReaderA pfnSCardForgetReaderA;
	fnSCardForgetReaderW pfnSCardForgetReaderW;
	fnSCardAddReaderToGroupA pfnSCardAddReaderToGroupA;
	fnSCardAddReaderToGroupW pfnSCardAddReaderToGroupW;
	fnSCardRemoveReaderFromGroupA pfnSCardRemoveReaderFromGroupA;
	fnSCardRemoveReaderFromGroupW pfnSCardRemoveReaderFromGroupW;
	fnSCardIntroduceCardTypeA pfnSCardIntroduceCardTypeA;
	fnSCardIntroduceCardTypeW pfnSCardIntroduceCardTypeW;
	fnSCardSetCardTypeProviderNameA pfnSCardSetCardTypeProviderNameA;
	fnSCardSetCardTypeProviderNameW pfnSCardSetCardTypeProviderNameW;
	fnSCardForgetCardTypeA pfnSCardForgetCardTypeA;
	fnSCardForgetCardTypeW pfnSCardForgetCardTypeW;
	fnSCardFreeMemory pfnSCardFreeMemory;
	fnSCardAccessStartedEvent pfnSCardAccessStartedEvent;
	fnSCardReleaseStartedEvent pfnSCardReleaseStartedEvent;
	fnSCardLocateCardsA pfnSCardLocateCardsA;
	fnSCardLocateCardsW pfnSCardLocateCardsW;
	fnSCardLocateCardsByATRA pfnSCardLocateCardsByATRA;
	fnSCardLocateCardsByATRW pfnSCardLocateCardsByATRW;
	fnSCardGetStatusChangeA pfnSCardGetStatusChangeA;
	fnSCardGetStatusChangeW pfnSCardGetStatusChangeW;
	fnSCardCancel pfnSCardCancel;
	fnSCardConnectA pfnSCardConnectA;
	fnSCardConnectW pfnSCardConnectW;
	fnSCardReconnect pfnSCardReconnect;
	fnSCardDisconnect pfnSCardDisconnect;
	fnSCardBeginTransaction pfnSCardBeginTransaction;
	fnSCardEndTransaction pfnSCardEndTransaction;
	fnSCardCancelTransaction pfnSCardCancelTransaction;
	fnSCardState pfnSCardState;
	fnSCardStatusA pfnSCardStatusA;
	fnSCardStatusW pfnSCardStatusW;
	fnSCardTransmit pfnSCardTransmit;
	fnSCardGetTransmitCount pfnSCardGetTransmitCount;
	fnSCardControl pfnSCardControl;
	fnSCardGetAttrib pfnSCardGetAttrib;
	fnSCardSetAttrib pfnSCardSetAttrib;
	fnSCardUIDlgSelectCardA pfnSCardUIDlgSelectCardA;
	fnSCardUIDlgSelectCardW pfnSCardUIDlgSelectCardW;
	fnGetOpenCardNameA pfnGetOpenCardNameA;
	fnGetOpenCardNameW pfnGetOpenCardNameW;
	fnSCardDlgExtendedError pfnSCardDlgExtendedError;
	fnSCardReadCacheA pfnSCardReadCacheA;
	fnSCardReadCacheW pfnSCardReadCacheW;
	fnSCardWriteCacheA pfnSCardWriteCacheA;
	fnSCardWriteCacheW pfnSCardWriteCacheW;
	fnSCardGetReaderIconA pfnSCardGetReaderIconA;
	fnSCardGetReaderIconW pfnSCardGetReaderIconW;
	fnSCardGetDeviceTypeIdA pfnSCardGetDeviceTypeIdA;
	fnSCardGetDeviceTypeIdW pfnSCardGetDeviceTypeIdW;
	fnSCardGetReaderDeviceInstanceIdA pfnSCardGetReaderDeviceInstanceIdA;
	fnSCardGetReaderDeviceInstanceIdW pfnSCardGetReaderDeviceInstanceIdW;
	fnSCardListReadersWithDeviceInstanceIdA pfnSCardListReadersWithDeviceInstanceIdA;
	fnSCardListReadersWithDeviceInstanceIdW pfnSCardListReadersWithDeviceInstanceIdW;
	fnSCardAudit pfnSCardAudit;
};

typedef struct _SCardApiFunctionTable SCardApiFunctionTable;
typedef SCardApiFunctionTable* PSCardApiFunctionTable;

#ifdef __cplusplus
extern "C" {
#endif

WINSCARDAPI const char* WINAPI SCardGetErrorString(LONG errorCode);
WINSCARDAPI const char* WINAPI SCardGetAttributeString(DWORD dwAttrId);
WINSCARDAPI const char* WINAPI SCardGetProtocolString(DWORD dwProtocols);
WINSCARDAPI const char* WINAPI SCardGetShareModeString(DWORD dwShareMode);
WINSCARDAPI const char* WINAPI SCardGetDispositionString(DWORD dwDisposition);
WINSCARDAPI const char* WINAPI SCardGetScopeString(DWORD dwScope);
WINSCARDAPI const char* WINAPI SCardGetCardStateString(DWORD dwCardState);
WINSCARDAPI char* WINAPI SCardGetReaderStateString(DWORD dwReaderState);

#ifdef __cplusplus
}
#endif

#endif /* WINPR_SMARTCARD_H */

