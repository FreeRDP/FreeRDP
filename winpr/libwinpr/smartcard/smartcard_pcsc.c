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

#include "smartcard_pcsc.h"

static HMODULE g_PCSCModule = NULL;
static PCSCFunctionTable g_PCSC = { 0 };

/**
 * Standard Windows Smart Card API (PCSC)
 */

WINSCARDAPI LONG WINAPI PCSC_SCardEstablishContext(DWORD dwScope,
		LPCVOID pvReserved1, LPCVOID pvReserved2, LPSCARDCONTEXT phContext)
{
	if (g_PCSC.pfnSCardEstablishContext)
	{
		return g_PCSC.pfnSCardEstablishContext(dwScope, pvReserved1, pvReserved2, phContext);
	}

	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardReleaseContext(SCARDCONTEXT hContext)
{
	if (g_PCSC.pfnSCardReleaseContext)
	{
		return g_PCSC.pfnSCardReleaseContext(hContext);
	}

	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardIsValidContext(SCARDCONTEXT hContext)
{
	if (g_PCSC.pfnSCardIsValidContext)
	{
		return g_PCSC.pfnSCardIsValidContext(hContext);
	}

	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardListReaderGroupsA(SCARDCONTEXT hContext,
		LPSTR mszGroups, LPDWORD pcchGroups)
{
	if (g_PCSC.pfnSCardListReaderGroups)
	{
		return g_PCSC.pfnSCardListReaderGroups(hContext, mszGroups, pcchGroups);
	}

	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardListReaderGroupsW(SCARDCONTEXT hContext,
		LPWSTR mszGroups, LPDWORD pcchGroups)
{
	if (g_PCSC.pfnSCardListReaderGroups)
	{
		mszGroups = NULL;
		pcchGroups = 0;

		/* FIXME: unicode conversion */

		return g_PCSC.pfnSCardListReaderGroups(hContext, (LPSTR) mszGroups, pcchGroups);
	}

	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardListReadersA(SCARDCONTEXT hContext,
		LPCSTR mszGroups, LPSTR mszReaders, LPDWORD pcchReaders)
{
	if (g_PCSC.pfnSCardListReaders)
	{
		return g_PCSC.pfnSCardListReaders(hContext, mszGroups, mszReaders, pcchReaders);
	}

	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardListReadersW(SCARDCONTEXT hContext,
		LPCWSTR mszGroups, LPWSTR mszReaders, LPDWORD pcchReaders)
{
	if (g_PCSC.pfnSCardListReaders)
	{
		mszGroups = NULL;

		mszReaders = NULL;
		pcchReaders = 0;

		/* FIXME: unicode conversion */

		return g_PCSC.pfnSCardListReaders(hContext, (LPSTR) mszGroups, (LPSTR) mszReaders, pcchReaders);
	}

	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardListCardsA(SCARDCONTEXT hContext,
		LPCBYTE pbAtr, LPCGUID rgquidInterfaces, DWORD cguidInterfaceCount, CHAR* mszCards, LPDWORD pcchCards)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardListCardsW(SCARDCONTEXT hContext,
		LPCBYTE pbAtr, LPCGUID rgquidInterfaces, DWORD cguidInterfaceCount, WCHAR* mszCards, LPDWORD pcchCards)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardListInterfacesA(SCARDCONTEXT hContext,
		LPCSTR szCard, LPGUID pguidInterfaces, LPDWORD pcguidInterfaces)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardListInterfacesW(SCARDCONTEXT hContext,
		LPCWSTR szCard, LPGUID pguidInterfaces, LPDWORD pcguidInterfaces)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardGetProviderIdA(SCARDCONTEXT hContext,
		LPCSTR szCard, LPGUID pguidProviderId)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardGetProviderIdW(SCARDCONTEXT hContext,
		LPCWSTR szCard, LPGUID pguidProviderId)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardGetCardTypeProviderNameA(SCARDCONTEXT hContext,
		LPCSTR szCardName, DWORD dwProviderId, CHAR* szProvider, LPDWORD pcchProvider)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardGetCardTypeProviderNameW(SCARDCONTEXT hContext,
		LPCWSTR szCardName, DWORD dwProviderId, WCHAR* szProvider, LPDWORD pcchProvider)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardIntroduceReaderGroupA(SCARDCONTEXT hContext, LPCSTR szGroupName)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardIntroduceReaderGroupW(SCARDCONTEXT hContext, LPCWSTR szGroupName)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardForgetReaderGroupA(SCARDCONTEXT hContext, LPCSTR szGroupName)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardForgetReaderGroupW(SCARDCONTEXT hContext, LPCWSTR szGroupName)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardIntroduceReaderA(SCARDCONTEXT hContext,
		LPCSTR szReaderName, LPCSTR szDeviceName)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardIntroduceReaderW(SCARDCONTEXT hContext,
		LPCWSTR szReaderName, LPCWSTR szDeviceName)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardForgetReaderA(SCARDCONTEXT hContext, LPCSTR szReaderName)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardForgetReaderW(SCARDCONTEXT hContext, LPCWSTR szReaderName)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardAddReaderToGroupA(SCARDCONTEXT hContext,
		LPCSTR szReaderName, LPCSTR szGroupName)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardAddReaderToGroupW(SCARDCONTEXT hContext,
		LPCWSTR szReaderName, LPCWSTR szGroupName)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardRemoveReaderFromGroupA(SCARDCONTEXT hContext,
		LPCSTR szReaderName, LPCSTR szGroupName)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardRemoveReaderFromGroupW(SCARDCONTEXT hContext,
		LPCWSTR szReaderName, LPCWSTR szGroupName)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardIntroduceCardTypeA(SCARDCONTEXT hContext,
		LPCSTR szCardName, LPCGUID pguidPrimaryProvider, LPCGUID rgguidInterfaces,
		DWORD dwInterfaceCount, LPCBYTE pbAtr, LPCBYTE pbAtrMask, DWORD cbAtrLen)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardIntroduceCardTypeW(SCARDCONTEXT hContext,
		LPCWSTR szCardName, LPCGUID pguidPrimaryProvider, LPCGUID rgguidInterfaces,
		DWORD dwInterfaceCount, LPCBYTE pbAtr, LPCBYTE pbAtrMask, DWORD cbAtrLen)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardSetCardTypeProviderNameA(SCARDCONTEXT hContext,
		LPCSTR szCardName, DWORD dwProviderId, LPCSTR szProvider)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardSetCardTypeProviderNameW(SCARDCONTEXT hContext,
		LPCWSTR szCardName, DWORD dwProviderId, LPCWSTR szProvider)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardForgetCardTypeA(SCARDCONTEXT hContext, LPCSTR szCardName)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardForgetCardTypeW(SCARDCONTEXT hContext, LPCWSTR szCardName)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardFreeMemory(SCARDCONTEXT hContext, LPCVOID pvMem)
{
	if (g_PCSC.pfnSCardFreeMemory)
	{
		return g_PCSC.pfnSCardFreeMemory(hContext, pvMem);
	}

	return 0;
}

WINSCARDAPI HANDLE WINAPI PCSC_SCardAccessStartedEvent(void)
{
	return 0;
}

WINSCARDAPI void WINAPI PCSC_SCardReleaseStartedEvent(void)
{

}

WINSCARDAPI LONG WINAPI PCSC_SCardLocateCardsA(SCARDCONTEXT hContext,
		LPCSTR mszCards, LPSCARD_READERSTATEA rgReaderStates, DWORD cReaders)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardLocateCardsW(SCARDCONTEXT hContext,
		LPCWSTR mszCards, LPSCARD_READERSTATEW rgReaderStates, DWORD cReaders)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardLocateCardsByATRA(SCARDCONTEXT hContext,
		LPSCARD_ATRMASK rgAtrMasks, DWORD cAtrs, LPSCARD_READERSTATEA rgReaderStates, DWORD cReaders)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardLocateCardsByATRW(SCARDCONTEXT hContext,
		LPSCARD_ATRMASK rgAtrMasks, DWORD cAtrs, LPSCARD_READERSTATEW rgReaderStates, DWORD cReaders)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardGetStatusChangeA(SCARDCONTEXT hContext,
		DWORD dwTimeout, LPSCARD_READERSTATEA rgReaderStates, DWORD cReaders)
{
	if (g_PCSC.pfnSCardGetStatusChange)
	{
		return g_PCSC.pfnSCardGetStatusChange(hContext, dwTimeout, rgReaderStates, cReaders);
	}

	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardGetStatusChangeW(SCARDCONTEXT hContext,
		DWORD dwTimeout, LPSCARD_READERSTATEW rgReaderStates, DWORD cReaders)
{
	if (g_PCSC.pfnSCardGetStatusChange)
	{
		SCARD_READERSTATEA rgReaderStatesA;

		cReaders = 0;
		ZeroMemory(&rgReaderStatesA, sizeof(SCARD_READERSTATEA));

		/* FIXME: unicode conversion */
		return g_PCSC.pfnSCardGetStatusChange(hContext, dwTimeout, &rgReaderStatesA, cReaders);
	}

	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardCancel(SCARDCONTEXT hContext)
{
	if (g_PCSC.pfnSCardCancel)
	{
		return g_PCSC.pfnSCardCancel(hContext);
	}

	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardConnectA(SCARDCONTEXT hContext,
		LPCSTR szReader, DWORD dwShareMode, DWORD dwPreferredProtocols,
		LPSCARDHANDLE phCard, LPDWORD pdwActiveProtocol)
{
	if (g_PCSC.pfnSCardConnect)
	{
		return g_PCSC.pfnSCardConnect(hContext, szReader,
				dwShareMode, dwPreferredProtocols, phCard, pdwActiveProtocol);
	}

	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardConnectW(SCARDCONTEXT hContext,
		LPCWSTR szReader, DWORD dwShareMode, DWORD dwPreferredProtocols,
		LPSCARDHANDLE phCard, LPDWORD pdwActiveProtocol)
{
	if (g_PCSC.pfnSCardConnect)
	{
		LONG status;
		LPSTR szReaderA = NULL;

		if (szReader)
			ConvertFromUnicode(CP_UTF8, 0, szReader, -1, &szReaderA, 0, NULL, NULL);

		status = g_PCSC.pfnSCardConnect(hContext, szReaderA,
				dwShareMode, dwPreferredProtocols, phCard, pdwActiveProtocol);

		free(szReaderA);

		return status;
	}

	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardReconnect(SCARDHANDLE hCard,
		DWORD dwShareMode, DWORD dwPreferredProtocols, DWORD dwInitialization, LPDWORD pdwActiveProtocol)
{
	if (g_PCSC.pfnSCardReconnect)
	{
		return g_PCSC.pfnSCardReconnect(hCard, dwShareMode,
				dwPreferredProtocols, dwInitialization, pdwActiveProtocol);
	}

	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardDisconnect(SCARDHANDLE hCard, DWORD dwDisposition)
{
	if (g_PCSC.pfnSCardDisconnect)
	{
		return g_PCSC.pfnSCardDisconnect(hCard, dwDisposition);
	}

	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardBeginTransaction(SCARDHANDLE hCard)
{
	if (g_PCSC.pfnSCardBeginTransaction)
	{
		return g_PCSC.pfnSCardBeginTransaction(hCard);
	}

	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardEndTransaction(SCARDHANDLE hCard, DWORD dwDisposition)
{
	if (g_PCSC.pfnSCardEndTransaction)
	{
		return g_PCSC.pfnSCardEndTransaction(hCard, dwDisposition);
	}

	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardCancelTransaction(SCARDHANDLE hCard)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardState(SCARDHANDLE hCard,
		LPDWORD pdwState, LPDWORD pdwProtocol, LPBYTE pbAtr, LPDWORD pcbAtrLen)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardStatusA(SCARDHANDLE hCard,
		LPSTR mszReaderNames, LPDWORD pcchReaderLen, LPDWORD pdwState,
		LPDWORD pdwProtocol, LPBYTE pbAtr, LPDWORD pcbAtrLen)
{
	if (g_PCSC.pfnSCardStatus)
	{
		return g_PCSC.pfnSCardStatus(hCard, mszReaderNames, pcchReaderLen,
				pdwState, pdwProtocol, pbAtr, pcbAtrLen);
	}

	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardStatusW(SCARDHANDLE hCard,
		LPWSTR mszReaderNames, LPDWORD pcchReaderLen, LPDWORD pdwState,
		LPDWORD pdwProtocol, LPBYTE pbAtr, LPDWORD pcbAtrLen)
{
	if (g_PCSC.pfnSCardStatus)
	{
		mszReaderNames = NULL;
		pcchReaderLen = 0;

		/* FIXME: unicode conversion */

		return g_PCSC.pfnSCardStatus(hCard, (LPSTR) mszReaderNames, pcchReaderLen,
				pdwState, pdwProtocol, pbAtr, pcbAtrLen);
	}

	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardTransmit(SCARDHANDLE hCard,
		LPCSCARD_IO_REQUEST pioSendPci, LPCBYTE pbSendBuffer, DWORD cbSendLength,
		LPSCARD_IO_REQUEST pioRecvPci, LPBYTE pbRecvBuffer, LPDWORD pcbRecvLength)
{
	if (g_PCSC.pfnSCardTransmit)
	{
		return g_PCSC.pfnSCardTransmit(hCard, pioSendPci, pbSendBuffer,
				cbSendLength, pioRecvPci, pbRecvBuffer, pcbRecvLength);
	}

	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardGetTransmitCount(SCARDHANDLE hCard, LPDWORD pcTransmitCount)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardControl(SCARDHANDLE hCard,
		DWORD dwControlCode, LPCVOID lpInBuffer, DWORD cbInBufferSize,
		LPVOID lpOutBuffer, DWORD cbOutBufferSize, LPDWORD lpBytesReturned)
{
	if (g_PCSC.pfnSCardControl)
	{
		return g_PCSC.pfnSCardControl(hCard,
				dwControlCode, lpInBuffer, cbInBufferSize,
				lpOutBuffer, cbOutBufferSize, lpBytesReturned);
	}

	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardGetAttrib(SCARDHANDLE hCard, DWORD dwAttrId, LPBYTE pbAttr, LPDWORD pcbAttrLen)
{
	if (g_PCSC.pfnSCardGetAttrib)
	{
		return g_PCSC.pfnSCardGetAttrib(hCard, dwAttrId, pbAttr, pcbAttrLen);
	}

	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardSetAttrib(SCARDHANDLE hCard, DWORD dwAttrId, LPCBYTE pbAttr, DWORD cbAttrLen)
{
	if (g_PCSC.pfnSCardSetAttrib)
	{
		return g_PCSC.pfnSCardSetAttrib(hCard, dwAttrId, pbAttr, cbAttrLen);
	}

	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardUIDlgSelectCardA(LPOPENCARDNAMEA_EX pDlgStruc)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardUIDlgSelectCardW(LPOPENCARDNAMEW_EX pDlgStruc)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_GetOpenCardNameA(LPOPENCARDNAMEA pDlgStruc)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_GetOpenCardNameW(LPOPENCARDNAMEW pDlgStruc)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardDlgExtendedError(void)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardReadCacheA(SCARDCONTEXT hContext,
		UUID* CardIdentifier, DWORD FreshnessCounter, LPSTR LookupName, PBYTE Data, DWORD* DataLen)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardReadCacheW(SCARDCONTEXT hContext,
		UUID* CardIdentifier,  DWORD FreshnessCounter, LPWSTR LookupName, PBYTE Data, DWORD* DataLen)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardWriteCacheA(SCARDCONTEXT hContext,
		UUID* CardIdentifier, DWORD FreshnessCounter, LPSTR LookupName, PBYTE Data, DWORD DataLen)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardWriteCacheW(SCARDCONTEXT hContext,
		UUID* CardIdentifier, DWORD FreshnessCounter, LPWSTR LookupName, PBYTE Data, DWORD DataLen)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardGetReaderIconA(SCARDCONTEXT hContext,
		LPCSTR szReaderName, LPBYTE pbIcon, LPDWORD pcbIcon)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardGetReaderIconW(SCARDCONTEXT hContext,
		LPCWSTR szReaderName, LPBYTE pbIcon, LPDWORD pcbIcon)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardGetDeviceTypeIdA(SCARDCONTEXT hContext, LPCSTR szReaderName, LPDWORD pdwDeviceTypeId)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardGetDeviceTypeIdW(SCARDCONTEXT hContext, LPCWSTR szReaderName, LPDWORD pdwDeviceTypeId)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardGetReaderDeviceInstanceIdA(SCARDCONTEXT hContext,
		LPCSTR szReaderName, LPSTR szDeviceInstanceId, LPDWORD pcchDeviceInstanceId)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardGetReaderDeviceInstanceIdW(SCARDCONTEXT hContext,
		LPCWSTR szReaderName, LPWSTR szDeviceInstanceId, LPDWORD pcchDeviceInstanceId)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardListReadersWithDeviceInstanceIdA(SCARDCONTEXT hContext,
		LPCSTR szDeviceInstanceId, LPSTR mszReaders, LPDWORD pcchReaders)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardListReadersWithDeviceInstanceIdW(SCARDCONTEXT hContext,
		LPCWSTR szDeviceInstanceId, LPWSTR mszReaders, LPDWORD pcchReaders)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardAudit(SCARDCONTEXT hContext, DWORD dwEvent)
{
	return 0;
}

SCardApiFunctionTable PCSC_SCardApiFunctionTable =
{
	0, /* dwVersion */
	0, /* dwFlags */

	PCSC_SCardEstablishContext, /* SCardEstablishContext */
	PCSC_SCardReleaseContext, /* SCardReleaseContext */
	PCSC_SCardIsValidContext, /* SCardIsValidContext */
	PCSC_SCardListReaderGroupsA, /* SCardListReaderGroupsA */
	PCSC_SCardListReaderGroupsW, /* SCardListReaderGroupsW */
	PCSC_SCardListReadersA, /* SCardListReadersA */
	PCSC_SCardListReadersW, /* SCardListReadersW */
	PCSC_SCardListCardsA, /* SCardListCardsA */
	PCSC_SCardListCardsW, /* SCardListCardsW */
	PCSC_SCardListInterfacesA, /* SCardListInterfacesA */
	PCSC_SCardListInterfacesW, /* SCardListInterfacesW */
	PCSC_SCardGetProviderIdA, /* SCardGetProviderIdA */
	PCSC_SCardGetProviderIdW, /* SCardGetProviderIdW */
	PCSC_SCardGetCardTypeProviderNameA, /* SCardGetCardTypeProviderNameA */
	PCSC_SCardGetCardTypeProviderNameW, /* SCardGetCardTypeProviderNameW */
	PCSC_SCardIntroduceReaderGroupA, /* SCardIntroduceReaderGroupA */
	PCSC_SCardIntroduceReaderGroupW, /* SCardIntroduceReaderGroupW */
	PCSC_SCardForgetReaderGroupA, /* SCardForgetReaderGroupA */
	PCSC_SCardForgetReaderGroupW, /* SCardForgetReaderGroupW */
	PCSC_SCardIntroduceReaderA, /* SCardIntroduceReaderA */
	PCSC_SCardIntroduceReaderW, /* SCardIntroduceReaderW */
	PCSC_SCardForgetReaderA, /* SCardForgetReaderA */
	PCSC_SCardForgetReaderW, /* SCardForgetReaderW */
	PCSC_SCardAddReaderToGroupA, /* SCardAddReaderToGroupA */
	PCSC_SCardAddReaderToGroupW, /* SCardAddReaderToGroupW */
	PCSC_SCardRemoveReaderFromGroupA, /* SCardRemoveReaderFromGroupA */
	PCSC_SCardRemoveReaderFromGroupW, /* SCardRemoveReaderFromGroupW */
	PCSC_SCardIntroduceCardTypeA, /* SCardIntroduceCardTypeA */
	PCSC_SCardIntroduceCardTypeW, /* SCardIntroduceCardTypeW */
	PCSC_SCardSetCardTypeProviderNameA, /* SCardSetCardTypeProviderNameA */
	PCSC_SCardSetCardTypeProviderNameW, /* SCardSetCardTypeProviderNameW */
	PCSC_SCardForgetCardTypeA, /* SCardForgetCardTypeA */
	PCSC_SCardForgetCardTypeW, /* SCardForgetCardTypeW */
	PCSC_SCardFreeMemory, /* SCardFreeMemory */
	PCSC_SCardAccessStartedEvent, /* SCardAccessStartedEvent */
	PCSC_SCardReleaseStartedEvent, /* SCardReleaseStartedEvent */
	PCSC_SCardLocateCardsA, /* SCardLocateCardsA */
	PCSC_SCardLocateCardsW, /* SCardLocateCardsW */
	PCSC_SCardLocateCardsByATRA, /* SCardLocateCardsByATRA */
	PCSC_SCardLocateCardsByATRW, /* SCardLocateCardsByATRW */
	PCSC_SCardGetStatusChangeA, /* SCardGetStatusChangeA */
	PCSC_SCardGetStatusChangeW, /* SCardGetStatusChangeW */
	PCSC_SCardCancel, /* SCardCancel */
	PCSC_SCardConnectA, /* SCardConnectA */
	PCSC_SCardConnectW, /* SCardConnectW */
	PCSC_SCardReconnect, /* SCardReconnect */
	PCSC_SCardDisconnect, /* SCardDisconnect */
	PCSC_SCardBeginTransaction, /* SCardBeginTransaction */
	PCSC_SCardEndTransaction, /* SCardEndTransaction */
	PCSC_SCardCancelTransaction, /* SCardCancelTransaction */
	PCSC_SCardState, /* SCardState */
	PCSC_SCardStatusA, /* SCardStatusA */
	PCSC_SCardStatusW, /* SCardStatusW */
	PCSC_SCardTransmit, /* SCardTransmit */
	PCSC_SCardGetTransmitCount, /* SCardGetTransmitCount */
	PCSC_SCardControl, /* SCardControl */
	PCSC_SCardGetAttrib, /* SCardGetAttrib */
	PCSC_SCardSetAttrib, /* SCardSetAttrib */
	PCSC_SCardUIDlgSelectCardA, /* SCardUIDlgSelectCardA */
	PCSC_SCardUIDlgSelectCardW, /* SCardUIDlgSelectCardW */
	PCSC_GetOpenCardNameA, /* GetOpenCardNameA */
	PCSC_GetOpenCardNameW, /* GetOpenCardNameW */
	PCSC_SCardDlgExtendedError, /* SCardDlgExtendedError */
	PCSC_SCardReadCacheA, /* SCardReadCacheA */
	PCSC_SCardReadCacheW, /* SCardReadCacheW */
	PCSC_SCardWriteCacheA, /* SCardWriteCacheA */
	PCSC_SCardWriteCacheW, /* SCardWriteCacheW */
	PCSC_SCardGetReaderIconA, /* SCardGetReaderIconA */
	PCSC_SCardGetReaderIconW, /* SCardGetReaderIconW */
	PCSC_SCardGetDeviceTypeIdA, /* SCardGetDeviceTypeIdA */
	PCSC_SCardGetDeviceTypeIdW, /* SCardGetDeviceTypeIdW */
	PCSC_SCardGetReaderDeviceInstanceIdA, /* SCardGetReaderDeviceInstanceIdA */
	PCSC_SCardGetReaderDeviceInstanceIdW, /* SCardGetReaderDeviceInstanceIdW */
	PCSC_SCardListReadersWithDeviceInstanceIdA, /* SCardListReadersWithDeviceInstanceIdA */
	PCSC_SCardListReadersWithDeviceInstanceIdW, /* SCardListReadersWithDeviceInstanceIdW */
	PCSC_SCardAudit /* SCardAudit */
};

PSCardApiFunctionTable PCSC_GetSCardApiFunctionTable(void)
{
	return &PCSC_SCardApiFunctionTable;
}

int PCSC_InitializeSCardApi(void)
{
	g_PCSCModule = LoadLibraryA("libpcsclite.so");

	if (!g_PCSCModule)
		return -1;

	g_PCSC.pfnSCardEstablishContext = GetProcAddress(g_PCSCModule, "SCardEstablishContext");
	g_PCSC.pfnSCardReleaseContext = GetProcAddress(g_PCSCModule, "SCardReleaseContext");
	g_PCSC.pfnSCardIsValidContext = GetProcAddress(g_PCSCModule, "SCardIsValidContext");
	g_PCSC.pfnSCardConnect = GetProcAddress(g_PCSCModule, "SCardConnect");
	g_PCSC.pfnSCardReconnect = GetProcAddress(g_PCSCModule, "SCardReconnect");
	g_PCSC.pfnSCardDisconnect = GetProcAddress(g_PCSCModule, "SCardDisconnect");
	g_PCSC.pfnSCardBeginTransaction = GetProcAddress(g_PCSCModule, "SCardBeginTransaction");
	g_PCSC.pfnSCardEndTransaction = GetProcAddress(g_PCSCModule, "SCardEndTransaction");
	g_PCSC.pfnSCardStatus = GetProcAddress(g_PCSCModule, "SCardStatus");
	g_PCSC.pfnSCardGetStatusChange = GetProcAddress(g_PCSCModule, "SCardGetStatusChange");
	g_PCSC.pfnSCardControl = GetProcAddress(g_PCSCModule, "SCardControl");
	g_PCSC.pfnSCardTransmit = GetProcAddress(g_PCSCModule, "SCardTransmit");
	g_PCSC.pfnSCardListReaderGroups = GetProcAddress(g_PCSCModule, "SCardListReaderGroups");
	g_PCSC.pfnSCardListReaders = GetProcAddress(g_PCSCModule, "SCardListReaders");
	g_PCSC.pfnSCardFreeMemory = GetProcAddress(g_PCSCModule, "SCardFreeMemory");
	g_PCSC.pfnSCardCancel = GetProcAddress(g_PCSCModule, "SCardCancel");
	g_PCSC.pfnSCardGetAttrib = GetProcAddress(g_PCSCModule, "SCardGetAttrib");
	g_PCSC.pfnSCardSetAttrib = GetProcAddress(g_PCSCModule, "SCardSetAttrib");

	return 1;
}
