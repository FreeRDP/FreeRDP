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
#include <winpr/smartcard.h>

#ifndef _WIN32

#include "smartcard.h"

/**
 * libpcsclite.so.1:
 *
 * SCardBeginTransaction
 * SCardCancel
 * SCardConnect
 * SCardControl
 * SCardDisconnect
 * SCardEndTransaction
 * SCardEstablishContext
 * SCardFreeMemory
 * SCardGetAttrib
 * SCardGetStatusChange
 * SCardIsValidContext
 * SCardListReaderGroups
 * SCardListReaders
 * SCardReconnect
 * SCardReleaseContext
 * SCardSetAttrib
 * SCardStatus
 * SCardTransmit
 */

WINSCARDAPI LONG WINAPI SCardEstablishContext(DWORD dwScope,
		LPCVOID pvReserved1, LPCVOID pvReserved2, LPSCARDCONTEXT phContext)
{
	return 0;
}

WINSCARDAPI LONG WINAPI SCardReleaseContext(SCARDCONTEXT hContext)
{
	return 0;
}

WINSCARDAPI LONG WINAPI SCardIsValidContext(SCARDCONTEXT hContext)
{
	return 0;
}

WINSCARDAPI LONG WINAPI SCardListReaderGroupsA(SCARDCONTEXT hContext,
		LPSTR mszGroups, LPDWORD pcchGroups)
{
	return 0;
}
WINSCARDAPI LONG WINAPI SCardListReaderGroupsW(SCARDCONTEXT hContext,
		LPWSTR mszGroups, LPDWORD pcchGroups)
{
	return 0;
}

WINSCARDAPI LONG WINAPI SCardListReadersA(SCARDCONTEXT hContext,
		LPCSTR mszGroups, LPSTR mszReaders, LPDWORD pcchReaders)
{
	return 0;
}
WINSCARDAPI LONG WINAPI SCardListReadersW(SCARDCONTEXT hContext,
		LPCWSTR mszGroups, LPWSTR mszReaders, LPDWORD pcchReaders)
{
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
	return 0;
}
WINSCARDAPI LONG WINAPI SCardGetStatusChangeW(SCARDCONTEXT hContext,
		DWORD dwTimeout, LPSCARD_READERSTATEW rgReaderStates, DWORD cReaders)
{
	return 0;
}

WINSCARDAPI LONG WINAPI SCardConnectA(SCARDCONTEXT hContext,
		LPCSTR szReader, DWORD dwShareMode, DWORD dwPreferredProtocols,
		LPSCARDHANDLE phCard, LPDWORD pdwActiveProtocol)
{
	return 0;
}
WINSCARDAPI LONG WINAPI SCardConnectW(SCARDCONTEXT hContext,
		LPCWSTR szReader, DWORD dwShareMode, DWORD dwPreferredProtocols,
		LPSCARDHANDLE phCard, LPDWORD pdwActiveProtocol)
{
	return 0;
}

WINSCARDAPI LONG WINAPI SCardReconnect(SCARDHANDLE hCard,
		DWORD dwShareMode, DWORD dwPreferredProtocols, DWORD dwInitialization, LPDWORD pdwActiveProtocol)
{
	return 0;
}

WINSCARDAPI LONG WINAPI SCardDisconnect(SCARDHANDLE hCard, DWORD dwDisposition)
{
	return 0;
}

WINSCARDAPI LONG WINAPI SCardBeginTransaction(SCARDHANDLE hCard)
{
	return 0;
}

WINSCARDAPI LONG WINAPI SCardEndTransaction(SCARDHANDLE hCard, DWORD dwDisposition)
{
	return 0;
}

WINSCARDAPI LONG WINAPI SCardCancelTransaction(SCARDHANDLE hCard)
{
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
	return 0;
}
WINSCARDAPI LONG WINAPI SCardStatusW(SCARDHANDLE hCard,
		LPWSTR mszReaderNames, LPDWORD pcchReaderLen, LPDWORD pdwState,
		LPDWORD pdwProtocol, LPBYTE pbAtr, LPDWORD pcbAtrLen)
{
	return 0;
}

WINSCARDAPI LONG WINAPI SCardTransmit(SCARDHANDLE hCard,
		LPCSCARD_IO_REQUEST pioSendPci, LPCBYTE pbSendBuffer, DWORD cbSendLength,
		LPSCARD_IO_REQUEST pioRecvPci, LPBYTE pbRecvBuffer, LPDWORD pcbRecvLength)
{
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
	return 0;
}

WINSCARDAPI LONG WINAPI SCardGetAttrib(SCARDHANDLE hCard, DWORD dwAttrId, LPBYTE pbAttr, LPDWORD pcbAttrLen)
{
	return 0;
}

WINSCARDAPI LONG WINAPI SCardSetAttrib(SCARDHANDLE hCard, DWORD dwAttrId, LPCBYTE pbAttr, DWORD cbAttrLen)
{
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
