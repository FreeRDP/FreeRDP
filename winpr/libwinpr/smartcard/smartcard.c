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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>
#include <winpr/library.h>
#include <winpr/smartcard.h>

#ifndef _WIN32

#include "smartcard.h"

/**
 * libpcsclite.so.1:
 *
 * SCardEstablishContext
 * SCardReleaseContext
 * SCardIsValidContext
 * SCardConnect
 * SCardReconnect
 * SCardDisconnect
 * SCardBeginTransaction
 * SCardEndTransaction
 * SCardStatus
 * SCardGetStatusChange
 * SCardControl
 * SCardTransmit
 * SCardListReaderGroups
 * SCardListReaders
 * SCardFreeMemory
 * SCardCancel
 * SCardGetAttrib
 * SCardSetAttrib
 */

struct _PCSCLiteFunctionTable
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
			DWORD dwTimeout, LPSCARD_READERSTATE rgReaderStates, DWORD cReaders);
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
typedef struct _PCSCLiteFunctionTable PCSCLiteFunctionTable;

static BOOL g_Initialized = FALSE;
static HMODULE g_PCSCLiteModule = NULL;

static PCSCLiteFunctionTable* g_PCSCLite = NULL;

void InitializePCSCLite(void)
{
	if (g_PCSCLite)
		return;

	g_PCSCLiteModule = LoadLibraryA("libpcsclite.so.1");

	if (!g_PCSCLiteModule)
		return;

	g_PCSCLite = calloc(1, sizeof(PCSCLiteFunctionTable));

	if (!g_PCSCLite)
		return;

	g_PCSCLite->pfnSCardEstablishContext = GetProcAddress(g_PCSCLiteModule, "SCardEstablishContext");
	g_PCSCLite->pfnSCardReleaseContext = GetProcAddress(g_PCSCLiteModule, "SCardReleaseContext");
	g_PCSCLite->pfnSCardIsValidContext = GetProcAddress(g_PCSCLiteModule, "SCardIsValidContext");
	g_PCSCLite->pfnSCardConnect = GetProcAddress(g_PCSCLiteModule, "SCardConnect");
	g_PCSCLite->pfnSCardReconnect = GetProcAddress(g_PCSCLiteModule, "SCardReconnect");
	g_PCSCLite->pfnSCardDisconnect = GetProcAddress(g_PCSCLiteModule, "SCardDisconnect");
	g_PCSCLite->pfnSCardBeginTransaction = GetProcAddress(g_PCSCLiteModule, "SCardBeginTransaction");
	g_PCSCLite->pfnSCardEndTransaction = GetProcAddress(g_PCSCLiteModule, "SCardEndTransaction");
	g_PCSCLite->pfnSCardStatus = GetProcAddress(g_PCSCLiteModule, "SCardStatus");
	g_PCSCLite->pfnSCardGetStatusChange = GetProcAddress(g_PCSCLiteModule, "SCardGetStatusChange");
	g_PCSCLite->pfnSCardControl = GetProcAddress(g_PCSCLiteModule, "SCardControl");
	g_PCSCLite->pfnSCardTransmit = GetProcAddress(g_PCSCLiteModule, "SCardTransmit");
	g_PCSCLite->pfnSCardListReaderGroups = GetProcAddress(g_PCSCLiteModule, "SCardListReaderGroups");
	g_PCSCLite->pfnSCardListReaders = GetProcAddress(g_PCSCLiteModule, "SCardListReaders");
	g_PCSCLite->pfnSCardFreeMemory = GetProcAddress(g_PCSCLiteModule, "SCardFreeMemory");
	g_PCSCLite->pfnSCardCancel = GetProcAddress(g_PCSCLiteModule, "SCardCancel");
	g_PCSCLite->pfnSCardGetAttrib = GetProcAddress(g_PCSCLiteModule, "SCardGetAttrib");
	g_PCSCLite->pfnSCardSetAttrib = GetProcAddress(g_PCSCLiteModule, "SCardSetAttrib");
}

void InitializeSCardStubs(void)
{
	if (g_Initialized)
		return;

	g_Initialized = TRUE;

	InitializePCSCLite();
}

/**
 * Standard Windows Smart Card API
 */

WINSCARDAPI LONG WINAPI SCardEstablishContext(DWORD dwScope,
		LPCVOID pvReserved1, LPCVOID pvReserved2, LPSCARDCONTEXT phContext)
{
	InitializeSCardStubs();

	if (g_PCSCLite && g_PCSCLite->pfnSCardEstablishContext)
	{
		return g_PCSCLite->pfnSCardEstablishContext(dwScope, pvReserved1, pvReserved2, phContext);
	}

	return 0;
}

WINSCARDAPI LONG WINAPI SCardReleaseContext(SCARDCONTEXT hContext)
{
	InitializeSCardStubs();

	if (g_PCSCLite && g_PCSCLite->pfnSCardReleaseContext)
	{
		return g_PCSCLite->pfnSCardReleaseContext(hContext);
	}

	return 0;
}

WINSCARDAPI LONG WINAPI SCardIsValidContext(SCARDCONTEXT hContext)
{
	InitializeSCardStubs();

	if (g_PCSCLite && g_PCSCLite->pfnSCardIsValidContext)
	{
		return g_PCSCLite->pfnSCardIsValidContext(hContext);
	}

	return 0;
}

WINSCARDAPI LONG WINAPI SCardListReaderGroupsA(SCARDCONTEXT hContext,
		LPSTR mszGroups, LPDWORD pcchGroups)
{
	InitializeSCardStubs();

	if (g_PCSCLite && g_PCSCLite->pfnSCardListReaderGroups)
	{
		return g_PCSCLite->pfnSCardListReaderGroups(hContext, mszGroups, pcchGroups);
	}

	return 0;
}

WINSCARDAPI LONG WINAPI SCardListReaderGroupsW(SCARDCONTEXT hContext,
		LPWSTR mszGroups, LPDWORD pcchGroups)
{
	InitializeSCardStubs();

	return 0;
}

WINSCARDAPI LONG WINAPI SCardListReadersA(SCARDCONTEXT hContext,
		LPCSTR mszGroups, LPSTR mszReaders, LPDWORD pcchReaders)
{
	InitializeSCardStubs();

	if (g_PCSCLite && g_PCSCLite->pfnSCardListReaders)
	{
		return g_PCSCLite->pfnSCardListReaders(hContext, mszGroups, mszReaders, pcchReaders);
	}

	return 0;
}

WINSCARDAPI LONG WINAPI SCardListReadersW(SCARDCONTEXT hContext,
		LPCWSTR mszGroups, LPWSTR mszReaders, LPDWORD pcchReaders)
{
	InitializeSCardStubs();

	return 0;
}

WINSCARDAPI LONG WINAPI SCardListCardsA(SCARDCONTEXT hContext,
		LPCBYTE pbAtr, LPCGUID rgquidInterfaces, DWORD cguidInterfaceCount, CHAR* mszCards, LPDWORD pcchCards)
{
	return 0;
}

WINSCARDAPI LONG WINAPI SCardListCardsW(SCARDCONTEXT hContext,
		LPCBYTE pbAtr, LPCGUID rgquidInterfaces, DWORD cguidInterfaceCount, WCHAR* mszCards, LPDWORD pcchCards)
{
	return 0;
}

WINSCARDAPI LONG WINAPI SCardListInterfacesA(SCARDCONTEXT hContext,
		LPCSTR szCard, LPGUID pguidInterfaces, LPDWORD pcguidInterfaces)
{
	return 0;
}
WINSCARDAPI LONG WINAPI SCardListInterfacesW(SCARDCONTEXT hContext,
		LPCWSTR szCard, LPGUID pguidInterfaces, LPDWORD pcguidInterfaces)
{
	return 0;
}

WINSCARDAPI LONG WINAPI SCardGetProviderIdA(SCARDCONTEXT hContext,
		LPCSTR szCard, LPGUID pguidProviderId)
{
	return 0;
}
WINSCARDAPI LONG WINAPI SCardGetProviderIdW(SCARDCONTEXT hContext,
		LPCWSTR szCard, LPGUID pguidProviderId)
{
	return 0;
}

WINSCARDAPI LONG WINAPI SCardGetCardTypeProviderNameA(SCARDCONTEXT hContext,
		LPCSTR szCardName, DWORD dwProviderId, CHAR* szProvider, LPDWORD pcchProvider)
{
	return 0;
}
WINSCARDAPI LONG WINAPI SCardGetCardTypeProviderNameW(SCARDCONTEXT hContext,
		LPCWSTR szCardName, DWORD dwProviderId, WCHAR* szProvider, LPDWORD pcchProvider)
{
	return 0;
}

WINSCARDAPI LONG WINAPI SCardIntroduceReaderGroupA(SCARDCONTEXT hContext, LPCSTR szGroupName)
{
	return 0;
}
WINSCARDAPI LONG WINAPI SCardIntroduceReaderGroupW(SCARDCONTEXT hContext, LPCWSTR szGroupName)
{
	return 0;
}

WINSCARDAPI LONG WINAPI SCardForgetReaderGroupA(SCARDCONTEXT hContext, LPCSTR szGroupName)
{
	return 0;
}
WINSCARDAPI LONG WINAPI SCardForgetReaderGroupW(SCARDCONTEXT hContext, LPCWSTR szGroupName)
{
	return 0;
}

WINSCARDAPI LONG WINAPI SCardIntroduceReaderA(SCARDCONTEXT hContext,
		LPCSTR szReaderName, LPCSTR szDeviceName)
{
	return 0;
}
WINSCARDAPI LONG WINAPI SCardIntroduceReaderW(SCARDCONTEXT hContext,
		LPCWSTR szReaderName, LPCWSTR szDeviceName)
{
	return 0;
}

WINSCARDAPI LONG WINAPI SCardForgetReaderA(SCARDCONTEXT hContext, LPCSTR szReaderName)
{
	return 0;
}
WINSCARDAPI LONG WINAPI SCardForgetReaderW(SCARDCONTEXT hContext, LPCWSTR szReaderName)
{
	return 0;
}

WINSCARDAPI LONG WINAPI SCardAddReaderToGroupA(SCARDCONTEXT hContext,
		LPCSTR szReaderName, LPCSTR szGroupName)
{
	return 0;
}
WINSCARDAPI LONG WINAPI SCardAddReaderToGroupW(SCARDCONTEXT hContext,
		LPCWSTR szReaderName, LPCWSTR szGroupName)
{
	return 0;
}

WINSCARDAPI LONG WINAPI SCardRemoveReaderFromGroupA( SCARDCONTEXT hContext,
		LPCSTR szReaderName, LPCSTR szGroupName)
{
	return 0;
}
WINSCARDAPI LONG WINAPI SCardRemoveReaderFromGroupW(SCARDCONTEXT hContext,
		LPCWSTR szReaderName, LPCWSTR szGroupName)
{
	return 0;
}

WINSCARDAPI LONG WINAPI SCardIntroduceCardTypeA(SCARDCONTEXT hContext,
		LPCSTR szCardName, LPCGUID pguidPrimaryProvider, LPCGUID rgguidInterfaces,
		DWORD dwInterfaceCount, LPCBYTE pbAtr, LPCBYTE pbAtrMask, DWORD cbAtrLen)
{
	return 0;
}
WINSCARDAPI LONG WINAPI SCardIntroduceCardTypeW(SCARDCONTEXT hContext,
		LPCWSTR szCardName, LPCGUID pguidPrimaryProvider, LPCGUID rgguidInterfaces,
		DWORD dwInterfaceCount, LPCBYTE pbAtr, LPCBYTE pbAtrMask, DWORD cbAtrLen)
{
	return 0;
}

WINSCARDAPI LONG WINAPI SCardSetCardTypeProviderNameA(SCARDCONTEXT hContext,
		LPCSTR szCardName, DWORD dwProviderId, LPCSTR szProvider)
{
	return 0;
}
WINSCARDAPI LONG WINAPI SCardSetCardTypeProviderNameW(SCARDCONTEXT hContext,
		LPCWSTR szCardName, DWORD dwProviderId, LPCWSTR szProvider)
{
	return 0;
}

WINSCARDAPI LONG WINAPI SCardForgetCardTypeA(SCARDCONTEXT hContext, LPCSTR szCardName)
{
	return 0;
}
WINSCARDAPI LONG WINAPI SCardForgetCardTypeW(SCARDCONTEXT hContext, LPCWSTR szCardName)
{
	return 0;
}

WINSCARDAPI LONG WINAPI SCardFreeMemory(SCARDCONTEXT hContext, LPCVOID pvMem)
{
	InitializeSCardStubs();

	if (g_PCSCLite && g_PCSCLite->pfnSCardFreeMemory)
	{
		return g_PCSCLite->pfnSCardFreeMemory(hContext, pvMem);
	}

	return 0;
}

WINSCARDAPI HANDLE WINAPI SCardAccessStartedEvent(void)
{
	return 0;
}

WINSCARDAPI void WINAPI SCardReleaseStartedEvent(void)
{

}

WINSCARDAPI LONG WINAPI SCardLocateCardsA(SCARDCONTEXT hContext,
		LPCSTR mszCards, LPSCARD_READERSTATEA rgReaderStates, DWORD cReaders)
{
	return 0;
}
WINSCARDAPI LONG WINAPI SCardLocateCardsW(SCARDCONTEXT hContext,
		LPCWSTR mszCards, LPSCARD_READERSTATEW rgReaderStates, DWORD cReaders)
{
	return 0;
}

WINSCARDAPI LONG WINAPI SCardLocateCardsByATRA(SCARDCONTEXT hContext,
		LPSCARD_ATRMASK rgAtrMasks, DWORD cAtrs, LPSCARD_READERSTATEA rgReaderStates, DWORD cReaders)
{
	return 0;
}

WINSCARDAPI LONG WINAPI SCardLocateCardsByATRW(SCARDCONTEXT hContext,
		LPSCARD_ATRMASK rgAtrMasks, DWORD cAtrs, LPSCARD_READERSTATEW rgReaderStates, DWORD cReaders)
{
	return 0;
}

WINSCARDAPI LONG WINAPI SCardGetStatusChangeA(SCARDCONTEXT hContext,
		DWORD dwTimeout, LPSCARD_READERSTATEA rgReaderStates, DWORD cReaders)
{
	InitializeSCardStubs();

	if (g_PCSCLite && g_PCSCLite->pfnSCardGetStatusChange)
	{
		return g_PCSCLite->pfnSCardGetStatusChange(hContext, dwTimeout, rgReaderStates, cReaders);
	}

	return 0;
}

WINSCARDAPI LONG WINAPI SCardGetStatusChangeW(SCARDCONTEXT hContext,
		DWORD dwTimeout, LPSCARD_READERSTATEW rgReaderStates, DWORD cReaders)
{
	InitializeSCardStubs();

	return 0;
}

WINSCARDAPI LONG WINAPI SCardConnectA(SCARDCONTEXT hContext,
		LPCSTR szReader, DWORD dwShareMode, DWORD dwPreferredProtocols,
		LPSCARDHANDLE phCard, LPDWORD pdwActiveProtocol)
{
	InitializeSCardStubs();

	if (g_PCSCLite && g_PCSCLite->pfnSCardConnect)
	{
		return g_PCSCLite->pfnSCardConnect(hContext, szReader,
				dwShareMode, dwPreferredProtocols, phCard, pdwActiveProtocol);
	}

	return 0;
}

WINSCARDAPI LONG WINAPI SCardConnectW(SCARDCONTEXT hContext,
		LPCWSTR szReader, DWORD dwShareMode, DWORD dwPreferredProtocols,
		LPSCARDHANDLE phCard, LPDWORD pdwActiveProtocol)
{
	InitializeSCardStubs();

	return 0;
}

WINSCARDAPI LONG WINAPI SCardReconnect(SCARDHANDLE hCard,
		DWORD dwShareMode, DWORD dwPreferredProtocols, DWORD dwInitialization, LPDWORD pdwActiveProtocol)
{
	InitializeSCardStubs();

	if (g_PCSCLite && g_PCSCLite->pfnSCardReconnect)
	{
		return g_PCSCLite->pfnSCardReconnect(hCard, dwShareMode,
				dwPreferredProtocols, dwInitialization, pdwActiveProtocol);
	}

	return 0;
}

WINSCARDAPI LONG WINAPI SCardDisconnect(SCARDHANDLE hCard, DWORD dwDisposition)
{
	InitializeSCardStubs();

	if (g_PCSCLite && g_PCSCLite->pfnSCardDisconnect)
	{
		return g_PCSCLite->pfnSCardDisconnect(hCard, dwDisposition);
	}

	return 0;
}

WINSCARDAPI LONG WINAPI SCardBeginTransaction(SCARDHANDLE hCard)
{
	InitializeSCardStubs();

	if (g_PCSCLite && g_PCSCLite->pfnSCardBeginTransaction)
	{
		return g_PCSCLite->pfnSCardBeginTransaction(hCard);
	}

	return 0;
}

WINSCARDAPI LONG WINAPI SCardEndTransaction(SCARDHANDLE hCard, DWORD dwDisposition)
{
	InitializeSCardStubs();

	if (g_PCSCLite && g_PCSCLite->pfnSCardEndTransaction)
	{
		return g_PCSCLite->pfnSCardEndTransaction(hCard, dwDisposition);
	}

	return 0;
}

WINSCARDAPI LONG WINAPI SCardCancelTransaction(SCARDHANDLE hCard)
{
	InitializeSCardStubs();

	return 0;
}

WINSCARDAPI LONG WINAPI SCardState(SCARDHANDLE hCard,
		LPDWORD pdwState, LPDWORD pdwProtocol, LPBYTE pbAtr, LPDWORD pcbAtrLen)
{
	return 0;
}

WINSCARDAPI LONG WINAPI SCardStatusA(SCARDHANDLE hCard,
		LPSTR mszReaderNames, LPDWORD pcchReaderLen, LPDWORD pdwState,
		LPDWORD pdwProtocol, LPBYTE pbAtr, LPDWORD pcbAtrLen)
{
	InitializeSCardStubs();

	if (g_PCSCLite && g_PCSCLite->pfnSCardStatus)
	{
		return g_PCSCLite->pfnSCardStatus(hCard, mszReaderNames, pcchReaderLen,
				pdwState, pdwProtocol, pbAtr, pcbAtrLen);
	}

	return 0;
}

WINSCARDAPI LONG WINAPI SCardStatusW(SCARDHANDLE hCard,
		LPWSTR mszReaderNames, LPDWORD pcchReaderLen, LPDWORD pdwState,
		LPDWORD pdwProtocol, LPBYTE pbAtr, LPDWORD pcbAtrLen)
{
	InitializeSCardStubs();

	return 0;
}

WINSCARDAPI LONG WINAPI SCardTransmit(SCARDHANDLE hCard,
		LPCSCARD_IO_REQUEST pioSendPci, LPCBYTE pbSendBuffer, DWORD cbSendLength,
		LPSCARD_IO_REQUEST pioRecvPci, LPBYTE pbRecvBuffer, LPDWORD pcbRecvLength)
{
	InitializeSCardStubs();

	if (g_PCSCLite && g_PCSCLite->pfnSCardTransmit)
	{
		return g_PCSCLite->pfnSCardTransmit(hCard, pioSendPci, pbSendBuffer,
				cbSendLength, pioRecvPci, pbRecvBuffer, pcbRecvLength);
	}

	return 0;
}

WINSCARDAPI LONG WINAPI SCardGetTransmitCount(SCARDHANDLE hCard, LPDWORD pcTransmitCount)
{
	return 0;
}

WINSCARDAPI LONG WINAPI SCardControl(SCARDHANDLE hCard,
		DWORD dwControlCode, LPCVOID lpInBuffer, DWORD cbInBufferSize,
		LPVOID lpOutBuffer, DWORD cbOutBufferSize, LPDWORD lpBytesReturned)
{
	InitializeSCardStubs();

	if (g_PCSCLite && g_PCSCLite->pfnSCardControl)
	{
		return g_PCSCLite->pfnSCardControl(hCard,
				dwControlCode, lpInBuffer, cbInBufferSize,
				lpOutBuffer, cbOutBufferSize, lpBytesReturned);
	}

	return 0;
}

WINSCARDAPI LONG WINAPI SCardGetAttrib(SCARDHANDLE hCard, DWORD dwAttrId, LPBYTE pbAttr, LPDWORD pcbAttrLen)
{
	InitializeSCardStubs();

	if (g_PCSCLite && g_PCSCLite->pfnSCardGetAttrib)
	{
		return g_PCSCLite->pfnSCardGetAttrib(hCard, dwAttrId, pbAttr, pcbAttrLen);
	}

	return 0;
}

WINSCARDAPI LONG WINAPI SCardSetAttrib(SCARDHANDLE hCard, DWORD dwAttrId, LPCBYTE pbAttr, DWORD cbAttrLen)
{
	InitializeSCardStubs();

	if (g_PCSCLite && g_PCSCLite->pfnSCardSetAttrib)
	{
		return g_PCSCLite->pfnSCardSetAttrib(hCard, dwAttrId, pbAttr, cbAttrLen);
	}

	return 0;
}

WINSCARDAPI LONG WINAPI SCardUIDlgSelectCardA(LPOPENCARDNAMEA_EX pDlgStruc)
{
	return 0;
}
WINSCARDAPI LONG WINAPI SCardUIDlgSelectCardW(LPOPENCARDNAMEW_EX pDlgStruc)
{
	return 0;
}

WINSCARDAPI LONG WINAPI GetOpenCardNameA(LPOPENCARDNAMEA pDlgStruc)
{
	return 0;
}
WINSCARDAPI LONG WINAPI GetOpenCardNameW(LPOPENCARDNAMEW pDlgStruc)
{
	return 0;
}

WINSCARDAPI LONG WINAPI SCardDlgExtendedError(void)
{
	return 0;
}

WINSCARDAPI LONG WINAPI SCardReadCacheA(SCARDCONTEXT hContext,
		UUID* CardIdentifier, DWORD FreshnessCounter, LPSTR LookupName, PBYTE Data, DWORD* DataLen)
{
	return 0;
}
WINSCARDAPI LONG WINAPI SCardReadCacheW(SCARDCONTEXT hContext,
		UUID* CardIdentifier,  DWORD FreshnessCounter, LPWSTR LookupName, PBYTE Data, DWORD* DataLen)
{
	return 0;
}

WINSCARDAPI LONG WINAPI SCardWriteCacheA(SCARDCONTEXT hContext,
		UUID* CardIdentifier, DWORD FreshnessCounter, LPSTR LookupName, PBYTE Data, DWORD DataLen)
{
	return 0;
}
WINSCARDAPI LONG WINAPI SCardWriteCacheW(SCARDCONTEXT hContext,
		UUID* CardIdentifier, DWORD FreshnessCounter, LPWSTR LookupName, PBYTE Data, DWORD DataLen)
{
	return 0;
}

WINSCARDAPI LONG WINAPI SCardGetReaderIconA(SCARDCONTEXT hContext,
		LPCSTR szReaderName, LPBYTE pbIcon, LPDWORD pcbIcon)
{
	return 0;
}
WINSCARDAPI LONG WINAPI SCardGetReaderIconW(SCARDCONTEXT hContext,
		LPCWSTR szReaderName, LPBYTE pbIcon, LPDWORD pcbIcon)
{
	return 0;
}

WINSCARDAPI LONG WINAPI SCardGetDeviceTypeIdA(SCARDCONTEXT hContext, LPCSTR szReaderName, LPDWORD pdwDeviceTypeId)
{
	return 0;
}
WINSCARDAPI LONG WINAPI SCardGetDeviceTypeIdW(SCARDCONTEXT hContext, LPCWSTR szReaderName, LPDWORD pdwDeviceTypeId)
{
	return 0;
}

WINSCARDAPI LONG WINAPI SCardGetReaderDeviceInstanceIdA(SCARDCONTEXT hContext,
		LPCSTR szReaderName, LPSTR szDeviceInstanceId, LPDWORD pcchDeviceInstanceId)
{
	return 0;
}
WINSCARDAPI LONG WINAPI SCardGetReaderDeviceInstanceIdW(SCARDCONTEXT hContext,
		LPCWSTR szReaderName, LPWSTR szDeviceInstanceId, LPDWORD pcchDeviceInstanceId)
{
	return 0;
}

WINSCARDAPI LONG WINAPI SCardListReadersWithDeviceInstanceIdA(SCARDCONTEXT hContext,
		LPCSTR szDeviceInstanceId, LPSTR mszReaders, LPDWORD pcchReaders)
{
	return 0;
}
WINSCARDAPI LONG WINAPI SCardListReadersWithDeviceInstanceIdW(SCARDCONTEXT hContext,
		LPCWSTR szDeviceInstanceId, LPWSTR mszReaders, LPDWORD pcchReaders)
{
	return 0;
}

WINSCARDAPI LONG WINAPI SCardAudit(SCARDCONTEXT hContext, DWORD dwEvent)
{
	return 0;
}

#endif
