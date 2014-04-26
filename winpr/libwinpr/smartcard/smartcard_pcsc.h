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

#define PCSC_SCARD_UNKNOWN		0x0001
#define PCSC_SCARD_ABSENT		0x0002
#define PCSC_SCARD_PRESENT		0x0004
#define PCSC_SCARD_SWALLOWED		0x0008
#define PCSC_SCARD_POWERED		0x0010
#define PCSC_SCARD_NEGOTIABLE		0x0020
#define PCSC_SCARD_SPECIFIC		0x0040

#define PCSC_SCARD_PROTOCOL_RAW		0x00000004
#define PCSC_SCARD_PROTOCOL_T15		0x00000008

#define PCSC_MAX_BUFFER_SIZE		264
#define PCSC_MAX_BUFFER_SIZE_EXTENDED	(4 + 3 + (1 << 16) + 3 + 2)

#define PCSC_MAX_ATR_SIZE		33

typedef struct
{
	LPCSTR szReader;
	LPVOID pvUserData;
	DWORD dwCurrentState;
	DWORD dwEventState;
	DWORD cbAtr;
	BYTE rgbAtr[PCSC_MAX_ATR_SIZE]; /* WinSCard: 36, PCSC: 33 */
}
PCSC_SCARD_READERSTATE;

struct _PCSCFunctionTable
{
	LONG (* pfnSCardEstablishContext)(DWORD dwScope, LPCVOID pvReserved1, LPCVOID pvReserved2, LPSCARDCONTEXT phContext);
	LONG (* pfnSCardReleaseContext)(SCARDCONTEXT hContext);
	LONG (* pfnSCardIsValidContext)(SCARDCONTEXT hContext);
	LONG (* pfnSCardConnect)(SCARDCONTEXT hContext,
			LPCSTR szReader, DWORD dwShareMode, DWORD dwPreferredProtocols,
			LPSCARDHANDLE phCard, LPDWORD pdwActiveProtocol);
	LONG (* pfnSCardReconnect)(SCARDHANDLE hCard,
			DWORD dwShareMode, DWORD dwPreferredProtocols,
			DWORD dwInitialization, LPDWORD pdwActiveProtocol);
	LONG (* pfnSCardDisconnect)(SCARDHANDLE hCard, DWORD dwDisposition);
	LONG (* pfnSCardBeginTransaction)(SCARDHANDLE hCard);
	LONG (* pfnSCardEndTransaction)(SCARDHANDLE hCard, DWORD dwDisposition);
	LONG (* pfnSCardStatus)(SCARDHANDLE hCard,
			LPSTR mszReaderName, LPDWORD pcchReaderLen, LPDWORD pdwState,
			LPDWORD pdwProtocol, LPBYTE pbAtr, LPDWORD pcbAtrLen);
	LONG (* pfnSCardGetStatusChange)(SCARDCONTEXT hContext,
			DWORD dwTimeout, PCSC_SCARD_READERSTATE* rgReaderStates, DWORD cReaders);
	LONG (* pfnSCardControl)(SCARDHANDLE hCard,
			DWORD dwControlCode, LPCVOID pbSendBuffer, DWORD cbSendLength,
			LPVOID pbRecvBuffer, DWORD cbRecvLength, LPDWORD lpBytesReturned);
	LONG (* pfnSCardTransmit)(SCARDHANDLE hCard,
			const SCARD_IO_REQUEST* pioSendPci, LPCBYTE pbSendBuffer, DWORD cbSendLength,
			SCARD_IO_REQUEST* pioRecvPci, LPBYTE pbRecvBuffer, LPDWORD pcbRecvLength);
	LONG (* pfnSCardListReaderGroups)(SCARDCONTEXT hContext, LPSTR mszGroups, LPDWORD pcchGroups);
	LONG (* pfnSCardListReaders)(SCARDCONTEXT hContext,
			LPCSTR mszGroups, LPSTR mszReaders, LPDWORD pcchReaders);
	LONG (* pfnSCardFreeMemory)(SCARDCONTEXT hContext, LPCVOID pvMem);
	LONG (* pfnSCardCancel)(SCARDCONTEXT hContext);
	LONG (* pfnSCardGetAttrib)(SCARDHANDLE hCard, DWORD dwAttrId, LPBYTE pbAttr, LPDWORD pcbAttrLen);
	LONG (* pfnSCardSetAttrib)(SCARDHANDLE hCard, DWORD dwAttrId, LPCBYTE pbAttr, DWORD cbAttrLen);
};
typedef struct _PCSCFunctionTable PCSCFunctionTable;

int PCSC_InitializeSCardApi(void);
PSCardApiFunctionTable PCSC_GetSCardApiFunctionTable(void);

#endif

#endif /* WINPR_SMARTCARD_PCSC_PRIVATE_H */
