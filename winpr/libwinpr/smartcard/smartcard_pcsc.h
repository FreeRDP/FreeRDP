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

#ifndef WINPR_SMARTCARD_PCSC_PRIVATE_H
#define WINPR_SMARTCARD_PCSC_PRIVATE_H

#ifndef _WIN32

#include <winpr/platform.h>
#include <winpr/smartcard.h>

/**
 * On Windows, DWORD and ULONG are defined to unsigned long.
 * However, 64-bit Windows uses the LLP64 model which defines
 * unsigned long as a 4-byte type, while most non-Windows
 * systems use the LP64 model where unsigned long is 8 bytes.
 *
 * WinPR correctly defines DWORD and ULONG to be 4-byte types
 * regardless of LLP64/LP64, but this has the side effect of
 * breaking compatibility with the broken pcsc-lite types.
 *
 * To make matters worse, pcsc-lite correctly defines
 * the data types on OS X, but not on other platforms.
 */

#ifdef __APPLE__
typedef unsigned int PCSC_DWORD;
typedef PCSC_DWORD* PCSC_PDWORD, *PCSC_LPDWORD;
typedef unsigned int PCSC_ULONG;
typedef PCSC_ULONG* PCSC_PULONG;
typedef int PCSC_LONG;
#else
typedef unsigned long PCSC_DWORD;
typedef PCSC_DWORD* PCSC_PDWORD, *PCSC_LPDWORD;
typedef unsigned long PCSC_ULONG;
typedef PCSC_ULONG* PCSC_PULONG;
typedef long PCSC_LONG;
#endif

#define PCSC_SCARD_UNKNOWN			0x0001
#define PCSC_SCARD_ABSENT			0x0002
#define PCSC_SCARD_PRESENT			0x0004
#define PCSC_SCARD_SWALLOWED			0x0008
#define PCSC_SCARD_POWERED			0x0010
#define PCSC_SCARD_NEGOTIABLE			0x0020
#define PCSC_SCARD_SPECIFIC			0x0040

#define PCSC_SCARD_PROTOCOL_RAW			0x00000004
#define PCSC_SCARD_PROTOCOL_T15			0x00000008

#define PCSC_MAX_BUFFER_SIZE			264
#define PCSC_MAX_BUFFER_SIZE_EXTENDED		(4 + 3 + (1 << 16) + 3 + 2)

#define PCSC_MAX_ATR_SIZE			33

#define PCSC_SCARD_AUTOALLOCATE			(PCSC_DWORD)(-1)

#define PCSC_SCARD_PCI_T0			(&g_PCSC_rgSCardT0Pci)
#define PCSC_SCARD_PCI_T1			(&g_PCSC_rgSCardT1Pci)
#define PCSC_SCARD_PCI_RAW			(&g_PCSC_rgSCardRawPci)

#define PCSC_SCARD_CTL_CODE(code)		(0x42000000 + (code))
#define PCSC_CM_IOCTL_GET_FEATURE_REQUEST	PCSC_SCARD_CTL_CODE(3400)

/**
 * pcsc-lite defines SCARD_READERSTATE, SCARD_IO_REQUEST as packed
 * on Mac OS X only and uses default packing everywhere else.
 */

#ifdef __APPLE__
#pragma pack(push, 1)
#endif

typedef struct
{
	LPCSTR szReader;
	LPVOID pvUserData;
	PCSC_DWORD dwCurrentState;
	PCSC_DWORD dwEventState;
	PCSC_DWORD cbAtr;
	BYTE rgbAtr[PCSC_MAX_ATR_SIZE]; /* WinSCard: 36, PCSC: 33 */
}
PCSC_SCARD_READERSTATE;

typedef struct
{
	PCSC_DWORD dwProtocol;
	PCSC_DWORD cbPciLength;
} PCSC_SCARD_IO_REQUEST;

#ifdef __APPLE__
#pragma pack(pop)
#endif

#pragma pack(push, 1)

typedef struct
{
	BYTE tag;
	BYTE length;
	UINT32 value;
} PCSC_TLV_STRUCTURE;

#pragma pack(pop)

struct _PCSCFunctionTable
{
	PCSC_LONG(* pfnSCardEstablishContext)(PCSC_DWORD dwScope,
	                                      LPCVOID pvReserved1, LPCVOID pvReserved2, LPSCARDCONTEXT phContext);
	PCSC_LONG(* pfnSCardReleaseContext)(SCARDCONTEXT hContext);
	PCSC_LONG(* pfnSCardIsValidContext)(SCARDCONTEXT hContext);
	PCSC_LONG(* pfnSCardConnect)(SCARDCONTEXT hContext,
	                             LPCSTR szReader, PCSC_DWORD dwShareMode, PCSC_DWORD dwPreferredProtocols,
	                             LPSCARDHANDLE phCard, PCSC_LPDWORD pdwActiveProtocol);
	PCSC_LONG(* pfnSCardReconnect)(SCARDHANDLE hCard,
	                               PCSC_DWORD dwShareMode, PCSC_DWORD dwPreferredProtocols,
	                               PCSC_DWORD dwInitialization, PCSC_LPDWORD pdwActiveProtocol);
	PCSC_LONG(* pfnSCardDisconnect)(SCARDHANDLE hCard, PCSC_DWORD dwDisposition);
	PCSC_LONG(* pfnSCardBeginTransaction)(SCARDHANDLE hCard);
	PCSC_LONG(* pfnSCardEndTransaction)(SCARDHANDLE hCard, PCSC_DWORD dwDisposition);
	PCSC_LONG(* pfnSCardStatus)(SCARDHANDLE hCard,
	                            LPSTR mszReaderName, PCSC_LPDWORD pcchReaderLen, PCSC_LPDWORD pdwState,
	                            PCSC_LPDWORD pdwProtocol, LPBYTE pbAtr, PCSC_LPDWORD pcbAtrLen);
	PCSC_LONG(* pfnSCardGetStatusChange)(SCARDCONTEXT hContext,
	                                     PCSC_DWORD dwTimeout, PCSC_SCARD_READERSTATE* rgReaderStates, PCSC_DWORD cReaders);
	PCSC_LONG(* pfnSCardControl)(SCARDHANDLE hCard,
	                             PCSC_DWORD dwControlCode, LPCVOID pbSendBuffer, PCSC_DWORD cbSendLength,
	                             LPVOID pbRecvBuffer, PCSC_DWORD cbRecvLength, PCSC_LPDWORD lpBytesReturned);
	PCSC_LONG(* pfnSCardTransmit)(SCARDHANDLE hCard,
	                              const PCSC_SCARD_IO_REQUEST* pioSendPci, LPCBYTE pbSendBuffer, PCSC_DWORD cbSendLength,
	                              PCSC_SCARD_IO_REQUEST* pioRecvPci, LPBYTE pbRecvBuffer, PCSC_LPDWORD pcbRecvLength);
	PCSC_LONG(* pfnSCardListReaderGroups)(SCARDCONTEXT hContext, LPSTR mszGroups,
	                                      PCSC_LPDWORD pcchGroups);
	PCSC_LONG(* pfnSCardListReaders)(SCARDCONTEXT hContext,
	                                 LPCSTR mszGroups, LPSTR mszReaders, PCSC_LPDWORD pcchReaders);
	PCSC_LONG(* pfnSCardFreeMemory)(SCARDCONTEXT hContext, LPCVOID pvMem);
	PCSC_LONG(* pfnSCardCancel)(SCARDCONTEXT hContext);
	PCSC_LONG(* pfnSCardGetAttrib)(SCARDHANDLE hCard, PCSC_DWORD dwAttrId, LPBYTE pbAttr,
	                               PCSC_LPDWORD pcbAttrLen);
	PCSC_LONG(* pfnSCardSetAttrib)(SCARDHANDLE hCard, PCSC_DWORD dwAttrId, LPCBYTE pbAttr,
	                               PCSC_DWORD cbAttrLen);
};
typedef struct _PCSCFunctionTable PCSCFunctionTable;

int PCSC_InitializeSCardApi(void);
PSCardApiFunctionTable PCSC_GetSCardApiFunctionTable(void);

#endif

#endif /* WINPR_SMARTCARD_PCSC_PRIVATE_H */
