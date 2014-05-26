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

#include "smartcard_inspect.h"

static PSCardApiFunctionTable g_SCardApi = NULL;

/**
 * Standard Windows Smart Card API
 */

WINSCARDAPI LONG WINAPI Inspect_SCardEstablishContext(DWORD dwScope,
		LPCVOID pvReserved1, LPCVOID pvReserved2, LPSCARDCONTEXT phContext)
{
	LONG status;

	status = g_SCardApi->pfnSCardEstablishContext(dwScope, pvReserved1, pvReserved2, phContext);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardReleaseContext(SCARDCONTEXT hContext)
{
	LONG status;

	status = g_SCardApi->pfnSCardReleaseContext(hContext);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardIsValidContext(SCARDCONTEXT hContext)
{
	LONG status;

	status = g_SCardApi->pfnSCardIsValidContext(hContext);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardListReaderGroupsA(SCARDCONTEXT hContext,
		LPSTR mszGroups, LPDWORD pcchGroups)
{
	LONG status;

	status = g_SCardApi->pfnSCardListReaderGroupsA(hContext, mszGroups, pcchGroups);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardListReaderGroupsW(SCARDCONTEXT hContext,
		LPWSTR mszGroups, LPDWORD pcchGroups)
{
	LONG status;

	status = g_SCardApi->pfnSCardListReaderGroupsW(hContext, mszGroups, pcchGroups);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardListReadersA(SCARDCONTEXT hContext,
		LPCSTR mszGroups, LPSTR mszReaders, LPDWORD pcchReaders)
{
	LONG status;

	status = g_SCardApi->pfnSCardListReadersA(hContext, mszGroups, mszReaders, pcchReaders);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardListReadersW(SCARDCONTEXT hContext,
		LPCWSTR mszGroups, LPWSTR mszReaders, LPDWORD pcchReaders)
{
	LONG status;

	status = g_SCardApi->pfnSCardListReadersW(hContext, mszGroups, mszReaders, pcchReaders);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardListCardsA(SCARDCONTEXT hContext,
		LPCBYTE pbAtr, LPCGUID rgquidInterfaces, DWORD cguidInterfaceCount, CHAR* mszCards, LPDWORD pcchCards)
{
	LONG status;

	status = g_SCardApi->pfnSCardListCardsA(hContext, pbAtr,
			rgquidInterfaces, cguidInterfaceCount, mszCards, pcchCards);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardListCardsW(SCARDCONTEXT hContext,
		LPCBYTE pbAtr, LPCGUID rgquidInterfaces, DWORD cguidInterfaceCount, WCHAR* mszCards, LPDWORD pcchCards)
{
	LONG status;

	status = g_SCardApi->pfnSCardListCardsW(hContext, pbAtr,
			rgquidInterfaces, cguidInterfaceCount, mszCards, pcchCards);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardListInterfacesA(SCARDCONTEXT hContext,
		LPCSTR szCard, LPGUID pguidInterfaces, LPDWORD pcguidInterfaces)
{
	LONG status;

	status = g_SCardApi->pfnSCardListInterfacesA(hContext, szCard, pguidInterfaces, pcguidInterfaces);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardListInterfacesW(SCARDCONTEXT hContext,
		LPCWSTR szCard, LPGUID pguidInterfaces, LPDWORD pcguidInterfaces)
{
	LONG status;

	status = g_SCardApi->pfnSCardListInterfacesW(hContext, szCard, pguidInterfaces, pcguidInterfaces);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardGetProviderIdA(SCARDCONTEXT hContext,
		LPCSTR szCard, LPGUID pguidProviderId)
{
	LONG status;

	status = g_SCardApi->pfnSCardGetProviderIdA(hContext, szCard, pguidProviderId);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardGetProviderIdW(SCARDCONTEXT hContext,
		LPCWSTR szCard, LPGUID pguidProviderId)
{
	LONG status;

	status = g_SCardApi->pfnSCardGetProviderIdW(hContext, szCard, pguidProviderId);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardGetCardTypeProviderNameA(SCARDCONTEXT hContext,
		LPCSTR szCardName, DWORD dwProviderId, CHAR* szProvider, LPDWORD pcchProvider)
{
	LONG status;

	status = g_SCardApi->pfnSCardGetCardTypeProviderNameA(hContext, szCardName,
			dwProviderId, szProvider, pcchProvider);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardGetCardTypeProviderNameW(SCARDCONTEXT hContext,
		LPCWSTR szCardName, DWORD dwProviderId, WCHAR* szProvider, LPDWORD pcchProvider)
{
	LONG status;

	status = g_SCardApi->pfnSCardGetCardTypeProviderNameW(hContext, szCardName,
			dwProviderId, szProvider, pcchProvider);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardIntroduceReaderGroupA(SCARDCONTEXT hContext, LPCSTR szGroupName)
{
	LONG status;

	status = g_SCardApi->pfnSCardIntroduceReaderGroupA(hContext, szGroupName);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardIntroduceReaderGroupW(SCARDCONTEXT hContext, LPCWSTR szGroupName)
{
	LONG status;

	status = g_SCardApi->pfnSCardIntroduceReaderGroupW(hContext, szGroupName);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardForgetReaderGroupA(SCARDCONTEXT hContext, LPCSTR szGroupName)
{
	LONG status;

	status = g_SCardApi->pfnSCardForgetReaderGroupA(hContext, szGroupName);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardForgetReaderGroupW(SCARDCONTEXT hContext, LPCWSTR szGroupName)
{
	LONG status;

	status = g_SCardApi->pfnSCardForgetReaderGroupW(hContext, szGroupName);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardIntroduceReaderA(SCARDCONTEXT hContext,
		LPCSTR szReaderName, LPCSTR szDeviceName)
{
	LONG status;

	status = g_SCardApi->pfnSCardIntroduceReaderA(hContext, szReaderName, szDeviceName);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardIntroduceReaderW(SCARDCONTEXT hContext,
		LPCWSTR szReaderName, LPCWSTR szDeviceName)
{
	LONG status;

	status = g_SCardApi->pfnSCardIntroduceReaderW(hContext, szReaderName, szDeviceName);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardForgetReaderA(SCARDCONTEXT hContext, LPCSTR szReaderName)
{
	LONG status;

	status = g_SCardApi->pfnSCardForgetReaderA(hContext, szReaderName);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardForgetReaderW(SCARDCONTEXT hContext, LPCWSTR szReaderName)
{
	LONG status;

	status = g_SCardApi->pfnSCardForgetReaderW(hContext, szReaderName);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardAddReaderToGroupA(SCARDCONTEXT hContext,
		LPCSTR szReaderName, LPCSTR szGroupName)
{
	LONG status;

	status = g_SCardApi->pfnSCardAddReaderToGroupA(hContext, szReaderName, szGroupName);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardAddReaderToGroupW(SCARDCONTEXT hContext,
		LPCWSTR szReaderName, LPCWSTR szGroupName)
{
	LONG status;

	status = g_SCardApi->pfnSCardAddReaderToGroupW(hContext, szReaderName, szGroupName);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardRemoveReaderFromGroupA(SCARDCONTEXT hContext,
		LPCSTR szReaderName, LPCSTR szGroupName)
{
	LONG status;

	status = g_SCardApi->pfnSCardRemoveReaderFromGroupA(hContext, szReaderName, szGroupName);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardRemoveReaderFromGroupW(SCARDCONTEXT hContext,
		LPCWSTR szReaderName, LPCWSTR szGroupName)
{
	LONG status;

	status = g_SCardApi->pfnSCardRemoveReaderFromGroupW(hContext, szReaderName, szGroupName);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardIntroduceCardTypeA(SCARDCONTEXT hContext,
		LPCSTR szCardName, LPCGUID pguidPrimaryProvider, LPCGUID rgguidInterfaces,
		DWORD dwInterfaceCount, LPCBYTE pbAtr, LPCBYTE pbAtrMask, DWORD cbAtrLen)
{
	LONG status;

	status = g_SCardApi->pfnSCardIntroduceCardTypeA(hContext, szCardName, pguidPrimaryProvider,
			rgguidInterfaces, dwInterfaceCount, pbAtr, pbAtrMask, cbAtrLen);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardIntroduceCardTypeW(SCARDCONTEXT hContext,
		LPCWSTR szCardName, LPCGUID pguidPrimaryProvider, LPCGUID rgguidInterfaces,
		DWORD dwInterfaceCount, LPCBYTE pbAtr, LPCBYTE pbAtrMask, DWORD cbAtrLen)
{
	LONG status;

	status = g_SCardApi->pfnSCardIntroduceCardTypeW(hContext, szCardName, pguidPrimaryProvider,
			rgguidInterfaces, dwInterfaceCount, pbAtr, pbAtrMask, cbAtrLen);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardSetCardTypeProviderNameA(SCARDCONTEXT hContext,
		LPCSTR szCardName, DWORD dwProviderId, LPCSTR szProvider)
{
	LONG status;

	status = g_SCardApi->pfnSCardSetCardTypeProviderNameA(hContext, szCardName, dwProviderId, szProvider);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardSetCardTypeProviderNameW(SCARDCONTEXT hContext,
		LPCWSTR szCardName, DWORD dwProviderId, LPCWSTR szProvider)
{
	LONG status;

	status = g_SCardApi->pfnSCardSetCardTypeProviderNameW(hContext, szCardName, dwProviderId, szProvider);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardForgetCardTypeA(SCARDCONTEXT hContext, LPCSTR szCardName)
{
	LONG status;

	status = g_SCardApi->pfnSCardForgetCardTypeA(hContext, szCardName);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardForgetCardTypeW(SCARDCONTEXT hContext, LPCWSTR szCardName)
{
	LONG status;

	status = g_SCardApi->pfnSCardForgetCardTypeW(hContext, szCardName);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardFreeMemory(SCARDCONTEXT hContext, LPCVOID pvMem)
{
	LONG status;

	status = g_SCardApi->pfnSCardFreeMemory(hContext, pvMem);

	return status;
}

WINSCARDAPI HANDLE WINAPI Inspect_SCardAccessStartedEvent(void)
{
	HANDLE hEvent;

	hEvent = g_SCardApi->pfnSCardAccessStartedEvent();

	return hEvent;
}

WINSCARDAPI void WINAPI Inspect_SCardReleaseStartedEvent(void)
{
	g_SCardApi->pfnSCardReleaseStartedEvent();
}

WINSCARDAPI LONG WINAPI Inspect_SCardLocateCardsA(SCARDCONTEXT hContext,
		LPCSTR mszCards, LPSCARD_READERSTATEA rgReaderStates, DWORD cReaders)
{
	LONG status;

	status = g_SCardApi->pfnSCardLocateCardsA(hContext, mszCards, rgReaderStates, cReaders);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardLocateCardsW(SCARDCONTEXT hContext,
		LPCWSTR mszCards, LPSCARD_READERSTATEW rgReaderStates, DWORD cReaders)
{
	LONG status;

	status = g_SCardApi->pfnSCardLocateCardsW(hContext, mszCards, rgReaderStates, cReaders);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardLocateCardsByATRA(SCARDCONTEXT hContext,
		LPSCARD_ATRMASK rgAtrMasks, DWORD cAtrs, LPSCARD_READERSTATEA rgReaderStates, DWORD cReaders)
{
	LONG status;

	status = g_SCardApi->pfnSCardLocateCardsByATRA(hContext, rgAtrMasks, cAtrs, rgReaderStates, cReaders);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardLocateCardsByATRW(SCARDCONTEXT hContext,
		LPSCARD_ATRMASK rgAtrMasks, DWORD cAtrs, LPSCARD_READERSTATEW rgReaderStates, DWORD cReaders)
{
	LONG status;

	status = g_SCardApi->pfnSCardLocateCardsByATRW(hContext, rgAtrMasks, cAtrs, rgReaderStates, cReaders);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardGetStatusChangeA(SCARDCONTEXT hContext,
		DWORD dwTimeout, LPSCARD_READERSTATEA rgReaderStates, DWORD cReaders)
{
	LONG status;

	status = g_SCardApi->pfnSCardGetStatusChangeA(hContext, dwTimeout, rgReaderStates, cReaders);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardGetStatusChangeW(SCARDCONTEXT hContext,
		DWORD dwTimeout, LPSCARD_READERSTATEW rgReaderStates, DWORD cReaders)
{
	LONG status;

	status = g_SCardApi->pfnSCardGetStatusChangeW(hContext, dwTimeout, rgReaderStates, cReaders);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardCancel(SCARDCONTEXT hContext)
{
	LONG status;

	status = g_SCardApi->pfnSCardCancel(hContext);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardConnectA(SCARDCONTEXT hContext,
		LPCSTR szReader, DWORD dwShareMode, DWORD dwPreferredProtocols,
		LPSCARDHANDLE phCard, LPDWORD pdwActiveProtocol)
{
	LONG status;

	status = g_SCardApi->pfnSCardConnectA(hContext, szReader, dwShareMode,
			dwPreferredProtocols, phCard, pdwActiveProtocol);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardConnectW(SCARDCONTEXT hContext,
		LPCWSTR szReader, DWORD dwShareMode, DWORD dwPreferredProtocols,
		LPSCARDHANDLE phCard, LPDWORD pdwActiveProtocol)
{
	LONG status;

	status = g_SCardApi->pfnSCardConnectW(hContext, szReader, dwShareMode,
			dwPreferredProtocols, phCard, pdwActiveProtocol);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardReconnect(SCARDHANDLE hCard,
		DWORD dwShareMode, DWORD dwPreferredProtocols, DWORD dwInitialization, LPDWORD pdwActiveProtocol)
{
	LONG status;

	status = g_SCardApi->pfnSCardReconnect(hCard, dwShareMode,
			dwPreferredProtocols, dwInitialization, pdwActiveProtocol);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardDisconnect(SCARDHANDLE hCard, DWORD dwDisposition)
{
	LONG status;

	status = g_SCardApi->pfnSCardDisconnect(hCard, dwDisposition);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardBeginTransaction(SCARDHANDLE hCard)
{
	LONG status;

	status = g_SCardApi->pfnSCardBeginTransaction(hCard);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardEndTransaction(SCARDHANDLE hCard, DWORD dwDisposition)
{
	LONG status;

	status = g_SCardApi->pfnSCardEndTransaction(hCard, dwDisposition);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardCancelTransaction(SCARDHANDLE hCard)
{
	LONG status;

	status = g_SCardApi->pfnSCardCancelTransaction(hCard);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardState(SCARDHANDLE hCard,
		LPDWORD pdwState, LPDWORD pdwProtocol, LPBYTE pbAtr, LPDWORD pcbAtrLen)
{
	LONG status;

	status = g_SCardApi->pfnSCardState(hCard, pdwState, pdwProtocol, pbAtr, pcbAtrLen);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardStatusA(SCARDHANDLE hCard,
		LPSTR mszReaderNames, LPDWORD pcchReaderLen, LPDWORD pdwState,
		LPDWORD pdwProtocol, LPBYTE pbAtr, LPDWORD pcbAtrLen)
{
	LONG status;

	status = g_SCardApi->pfnSCardStatusA(hCard, mszReaderNames, pcchReaderLen,
			pdwState, pdwProtocol, pbAtr, pcbAtrLen);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardStatusW(SCARDHANDLE hCard,
		LPWSTR mszReaderNames, LPDWORD pcchReaderLen, LPDWORD pdwState,
		LPDWORD pdwProtocol, LPBYTE pbAtr, LPDWORD pcbAtrLen)
{
	LONG status;

	status = g_SCardApi->pfnSCardStatusW(hCard, mszReaderNames, pcchReaderLen,
			pdwState, pdwProtocol, pbAtr, pcbAtrLen);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardTransmit(SCARDHANDLE hCard,
		LPCSCARD_IO_REQUEST pioSendPci, LPCBYTE pbSendBuffer, DWORD cbSendLength,
		LPSCARD_IO_REQUEST pioRecvPci, LPBYTE pbRecvBuffer, LPDWORD pcbRecvLength)
{
	LONG status;

	status = g_SCardApi->pfnSCardTransmit(hCard, pioSendPci, pbSendBuffer, cbSendLength,
			pioRecvPci, pbRecvBuffer, pcbRecvLength);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardGetTransmitCount(SCARDHANDLE hCard, LPDWORD pcTransmitCount)
{
	LONG status;

	status = g_SCardApi->pfnSCardGetTransmitCount(hCard, pcTransmitCount);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardControl(SCARDHANDLE hCard,
		DWORD dwControlCode, LPCVOID lpInBuffer, DWORD cbInBufferSize,
		LPVOID lpOutBuffer, DWORD cbOutBufferSize, LPDWORD lpBytesReturned)
{
	LONG status;

	status = g_SCardApi->pfnSCardControl(hCard, dwControlCode, lpInBuffer, cbInBufferSize,
			lpOutBuffer, cbOutBufferSize, lpBytesReturned);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardGetAttrib(SCARDHANDLE hCard, DWORD dwAttrId, LPBYTE pbAttr, LPDWORD pcbAttrLen)
{
	LONG status;

	status = g_SCardApi->pfnSCardGetAttrib(hCard, dwAttrId, pbAttr, pcbAttrLen);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardSetAttrib(SCARDHANDLE hCard, DWORD dwAttrId, LPCBYTE pbAttr, DWORD cbAttrLen)
{
	LONG status;

	status = g_SCardApi->pfnSCardSetAttrib(hCard, dwAttrId, pbAttr, cbAttrLen);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardUIDlgSelectCardA(LPOPENCARDNAMEA_EX pDlgStruc)
{
	LONG status;

	status = g_SCardApi->pfnSCardUIDlgSelectCardA(pDlgStruc);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardUIDlgSelectCardW(LPOPENCARDNAMEW_EX pDlgStruc)
{
	LONG status;

	status = g_SCardApi->pfnSCardUIDlgSelectCardW(pDlgStruc);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_GetOpenCardNameA(LPOPENCARDNAMEA pDlgStruc)
{
	LONG status;

	status = g_SCardApi->pfnGetOpenCardNameA(pDlgStruc);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_GetOpenCardNameW(LPOPENCARDNAMEW pDlgStruc)
{
	LONG status;

	status = g_SCardApi->pfnGetOpenCardNameW(pDlgStruc);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardDlgExtendedError(void)
{
	LONG status;

	status = g_SCardApi->pfnSCardDlgExtendedError();

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardReadCacheA(SCARDCONTEXT hContext,
		UUID* CardIdentifier, DWORD FreshnessCounter, LPSTR LookupName, PBYTE Data, DWORD* DataLen)
{
	LONG status;

	status = g_SCardApi->pfnSCardReadCacheA(hContext, CardIdentifier,
			FreshnessCounter, LookupName, Data, DataLen);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardReadCacheW(SCARDCONTEXT hContext,
		UUID* CardIdentifier,  DWORD FreshnessCounter, LPWSTR LookupName, PBYTE Data, DWORD* DataLen)
{
	LONG status;

	status = g_SCardApi->pfnSCardReadCacheW(hContext, CardIdentifier,
			FreshnessCounter, LookupName, Data, DataLen);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardWriteCacheA(SCARDCONTEXT hContext,
		UUID* CardIdentifier, DWORD FreshnessCounter, LPSTR LookupName, PBYTE Data, DWORD DataLen)
{
	LONG status;

	status = g_SCardApi->pfnSCardWriteCacheA(hContext, CardIdentifier,
			FreshnessCounter, LookupName, Data, DataLen);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardWriteCacheW(SCARDCONTEXT hContext,
		UUID* CardIdentifier, DWORD FreshnessCounter, LPWSTR LookupName, PBYTE Data, DWORD DataLen)
{
	LONG status;

	status = g_SCardApi->pfnSCardWriteCacheW(hContext, CardIdentifier,
			FreshnessCounter, LookupName, Data, DataLen);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardGetReaderIconA(SCARDCONTEXT hContext,
		LPCSTR szReaderName, LPBYTE pbIcon, LPDWORD pcbIcon)
{
	LONG status;

	status = g_SCardApi->pfnSCardGetReaderIconA(hContext, szReaderName, pbIcon, pcbIcon);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardGetReaderIconW(SCARDCONTEXT hContext,
		LPCWSTR szReaderName, LPBYTE pbIcon, LPDWORD pcbIcon)
{
	LONG status;

	status = g_SCardApi->pfnSCardGetReaderIconW(hContext, szReaderName, pbIcon, pcbIcon);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardGetDeviceTypeIdA(SCARDCONTEXT hContext, LPCSTR szReaderName, LPDWORD pdwDeviceTypeId)
{
	LONG status;

	status = g_SCardApi->pfnSCardGetDeviceTypeIdA(hContext, szReaderName, pdwDeviceTypeId);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardGetDeviceTypeIdW(SCARDCONTEXT hContext, LPCWSTR szReaderName, LPDWORD pdwDeviceTypeId)
{
	LONG status;

	status = g_SCardApi->pfnSCardGetDeviceTypeIdW(hContext, szReaderName, pdwDeviceTypeId);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardGetReaderDeviceInstanceIdA(SCARDCONTEXT hContext,
		LPCSTR szReaderName, LPSTR szDeviceInstanceId, LPDWORD pcchDeviceInstanceId)
{
	LONG status;

	status = g_SCardApi->pfnSCardGetReaderDeviceInstanceIdA(hContext, szReaderName,
			szDeviceInstanceId, pcchDeviceInstanceId);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardGetReaderDeviceInstanceIdW(SCARDCONTEXT hContext,
		LPCWSTR szReaderName, LPWSTR szDeviceInstanceId, LPDWORD pcchDeviceInstanceId)
{
	LONG status;

	status = g_SCardApi->pfnSCardGetReaderDeviceInstanceIdW(hContext, szReaderName,
			szDeviceInstanceId, pcchDeviceInstanceId);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardListReadersWithDeviceInstanceIdA(SCARDCONTEXT hContext,
		LPCSTR szDeviceInstanceId, LPSTR mszReaders, LPDWORD pcchReaders)
{
	LONG status;

	status = g_SCardApi->pfnSCardListReadersWithDeviceInstanceIdA(hContext,
			szDeviceInstanceId, mszReaders, pcchReaders);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardListReadersWithDeviceInstanceIdW(SCARDCONTEXT hContext,
		LPCWSTR szDeviceInstanceId, LPWSTR mszReaders, LPDWORD pcchReaders)
{
	LONG status;

	status = g_SCardApi->pfnSCardListReadersWithDeviceInstanceIdW(hContext,
			szDeviceInstanceId, mszReaders, pcchReaders);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardAudit(SCARDCONTEXT hContext, DWORD dwEvent)
{
	LONG status;

	status = g_SCardApi->pfnSCardAudit(hContext, dwEvent);

	return status;
}

/**
 * Extended API
 */

SCardApiFunctionTable Inspect_SCardApiFunctionTable =
{
	0, /* dwVersion */
	0, /* dwFlags */

	Inspect_SCardEstablishContext, /* SCardEstablishContext */
	Inspect_SCardReleaseContext, /* SCardReleaseContext */
	Inspect_SCardIsValidContext, /* SCardIsValidContext */
	Inspect_SCardListReaderGroupsA, /* SCardListReaderGroupsA */
	Inspect_SCardListReaderGroupsW, /* SCardListReaderGroupsW */
	Inspect_SCardListReadersA, /* SCardListReadersA */
	Inspect_SCardListReadersW, /* SCardListReadersW */
	Inspect_SCardListCardsA, /* SCardListCardsA */
	Inspect_SCardListCardsW, /* SCardListCardsW */
	Inspect_SCardListInterfacesA, /* SCardListInterfacesA */
	Inspect_SCardListInterfacesW, /* SCardListInterfacesW */
	Inspect_SCardGetProviderIdA, /* SCardGetProviderIdA */
	Inspect_SCardGetProviderIdW, /* SCardGetProviderIdW */
	Inspect_SCardGetCardTypeProviderNameA, /* SCardGetCardTypeProviderNameA */
	Inspect_SCardGetCardTypeProviderNameW, /* SCardGetCardTypeProviderNameW */
	Inspect_SCardIntroduceReaderGroupA, /* SCardIntroduceReaderGroupA */
	Inspect_SCardIntroduceReaderGroupW, /* SCardIntroduceReaderGroupW */
	Inspect_SCardForgetReaderGroupA, /* SCardForgetReaderGroupA */
	Inspect_SCardForgetReaderGroupW, /* SCardForgetReaderGroupW */
	Inspect_SCardIntroduceReaderA, /* SCardIntroduceReaderA */
	Inspect_SCardIntroduceReaderW, /* SCardIntroduceReaderW */
	Inspect_SCardForgetReaderA, /* SCardForgetReaderA */
	Inspect_SCardForgetReaderW, /* SCardForgetReaderW */
	Inspect_SCardAddReaderToGroupA, /* SCardAddReaderToGroupA */
	Inspect_SCardAddReaderToGroupW, /* SCardAddReaderToGroupW */
	Inspect_SCardRemoveReaderFromGroupA, /* SCardRemoveReaderFromGroupA */
	Inspect_SCardRemoveReaderFromGroupW, /* SCardRemoveReaderFromGroupW */
	Inspect_SCardIntroduceCardTypeA, /* SCardIntroduceCardTypeA */
	Inspect_SCardIntroduceCardTypeW, /* SCardIntroduceCardTypeW */
	Inspect_SCardSetCardTypeProviderNameA, /* SCardSetCardTypeProviderNameA */
	Inspect_SCardSetCardTypeProviderNameW, /* SCardSetCardTypeProviderNameW */
	Inspect_SCardForgetCardTypeA, /* SCardForgetCardTypeA */
	Inspect_SCardForgetCardTypeW, /* SCardForgetCardTypeW */
	Inspect_SCardFreeMemory, /* SCardFreeMemory */
	Inspect_SCardAccessStartedEvent, /* SCardAccessStartedEvent */
	Inspect_SCardReleaseStartedEvent, /* SCardReleaseStartedEvent */
	Inspect_SCardLocateCardsA, /* SCardLocateCardsA */
	Inspect_SCardLocateCardsW, /* SCardLocateCardsW */
	Inspect_SCardLocateCardsByATRA, /* SCardLocateCardsByATRA */
	Inspect_SCardLocateCardsByATRW, /* SCardLocateCardsByATRW */
	Inspect_SCardGetStatusChangeA, /* SCardGetStatusChangeA */
	Inspect_SCardGetStatusChangeW, /* SCardGetStatusChangeW */
	Inspect_SCardCancel, /* SCardCancel */
	Inspect_SCardConnectA, /* SCardConnectA */
	Inspect_SCardConnectW, /* SCardConnectW */
	Inspect_SCardReconnect, /* SCardReconnect */
	Inspect_SCardDisconnect, /* SCardDisconnect */
	Inspect_SCardBeginTransaction, /* SCardBeginTransaction */
	Inspect_SCardEndTransaction, /* SCardEndTransaction */
	Inspect_SCardCancelTransaction, /* SCardCancelTransaction */
	Inspect_SCardState, /* SCardState */
	Inspect_SCardStatusA, /* SCardStatusA */
	Inspect_SCardStatusW, /* SCardStatusW */
	Inspect_SCardTransmit, /* SCardTransmit */
	Inspect_SCardGetTransmitCount, /* SCardGetTransmitCount */
	Inspect_SCardControl, /* SCardControl */
	Inspect_SCardGetAttrib, /* SCardGetAttrib */
	Inspect_SCardSetAttrib, /* SCardSetAttrib */
	Inspect_SCardUIDlgSelectCardA, /* SCardUIDlgSelectCardA */
	Inspect_SCardUIDlgSelectCardW, /* SCardUIDlgSelectCardW */
	Inspect_GetOpenCardNameA, /* GetOpenCardNameA */
	Inspect_GetOpenCardNameW, /* GetOpenCardNameW */
	Inspect_SCardDlgExtendedError, /* SCardDlgExtendedError */
	Inspect_SCardReadCacheA, /* SCardReadCacheA */
	Inspect_SCardReadCacheW, /* SCardReadCacheW */
	Inspect_SCardWriteCacheA, /* SCardWriteCacheA */
	Inspect_SCardWriteCacheW, /* SCardWriteCacheW */
	Inspect_SCardGetReaderIconA, /* SCardGetReaderIconA */
	Inspect_SCardGetReaderIconW, /* SCardGetReaderIconW */
	Inspect_SCardGetDeviceTypeIdA, /* SCardGetDeviceTypeIdA */
	Inspect_SCardGetDeviceTypeIdW, /* SCardGetDeviceTypeIdW */
	Inspect_SCardGetReaderDeviceInstanceIdA, /* SCardGetReaderDeviceInstanceIdA */
	Inspect_SCardGetReaderDeviceInstanceIdW, /* SCardGetReaderDeviceInstanceIdW */
	Inspect_SCardListReadersWithDeviceInstanceIdA, /* SCardListReadersWithDeviceInstanceIdA */
	Inspect_SCardListReadersWithDeviceInstanceIdW, /* SCardListReadersWithDeviceInstanceIdW */
	Inspect_SCardAudit /* SCardAudit */
};

PSCardApiFunctionTable Inspect_RegisterSCardApi(PSCardApiFunctionTable pSCardApi)
{
	g_SCardApi = pSCardApi;
	return &Inspect_SCardApiFunctionTable;
}
