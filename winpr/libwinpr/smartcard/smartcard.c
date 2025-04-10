/**
 * WinPR: Windows Portable Runtime
 * Smart Card API
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2020 Armin Novak <armin.novak@thincast.com>
 * Copyright 2020 Thincast Technologies GmbH
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

#include <winpr/config.h>

#include <winpr/crt.h>
#include <winpr/library.h>
#include <winpr/smartcard.h>
#include <winpr/synch.h>
#include <winpr/wlog.h>
#include <winpr/assert.h>

#include "../log.h"

#include "smartcard.h"

#if defined(WITH_SMARTCARD_INSPECT)
#include "smartcard_inspect.h"
#endif

static INIT_ONCE g_Initialized = INIT_ONCE_STATIC_INIT;
static const SCardApiFunctionTable* g_SCardApi = NULL;

#define TAG WINPR_TAG("smartcard")

#define xstr(s) str(s)
#define str(s) #s

#define SCARDAPI_STUB_CALL_LONG(_name, ...)                                              \
	InitOnceExecuteOnce(&g_Initialized, InitializeSCardApiStubs, NULL, NULL);            \
	if (!g_SCardApi || !g_SCardApi->pfn##_name)                                          \
	{                                                                                    \
		WLog_DBG(TAG, "Missing function pointer g_SCardApi=%p->" xstr(pfn##_name) "=%p", \
		         g_SCardApi, g_SCardApi ? g_SCardApi->pfn##_name : NULL);                \
		return SCARD_E_NO_SERVICE;                                                       \
	}                                                                                    \
	return g_SCardApi->pfn##_name(__VA_ARGS__)

#define SCARDAPI_STUB_CALL_HANDLE(_name)                                                 \
	InitOnceExecuteOnce(&g_Initialized, InitializeSCardApiStubs, NULL, NULL);            \
	if (!g_SCardApi || !g_SCardApi->pfn##_name)                                          \
	{                                                                                    \
		WLog_DBG(TAG, "Missing function pointer g_SCardApi=%p->" xstr(pfn##_name) "=%p", \
		         g_SCardApi, g_SCardApi ? g_SCardApi->pfn##_name : NULL);                \
		return NULL;                                                                     \
	}                                                                                    \
	return g_SCardApi->pfn##_name()

#define SCARDAPI_STUB_CALL_VOID(_name)                                                   \
	InitOnceExecuteOnce(&g_Initialized, InitializeSCardApiStubs, NULL, NULL);            \
	if (!g_SCardApi || !g_SCardApi->pfn##_name)                                          \
	{                                                                                    \
		WLog_DBG(TAG, "Missing function pointer g_SCardApi=%p->" xstr(pfn##_name) "=%p", \
		         g_SCardApi, g_SCardApi ? g_SCardApi->pfn##_name : NULL);                \
		return;                                                                          \
	}                                                                                    \
	g_SCardApi->pfn##_name()

/**
 * Standard Windows Smart Card API
 */

const SCARD_IO_REQUEST g_rgSCardT0Pci = { SCARD_PROTOCOL_T0, 8 };
const SCARD_IO_REQUEST g_rgSCardT1Pci = { SCARD_PROTOCOL_T1, 8 };
const SCARD_IO_REQUEST g_rgSCardRawPci = { SCARD_PROTOCOL_RAW, 8 };

static BOOL CALLBACK InitializeSCardApiStubs(WINPR_ATTR_UNUSED PINIT_ONCE once,
                                             WINPR_ATTR_UNUSED PVOID param,
                                             WINPR_ATTR_UNUSED PVOID* context)
{
#ifdef _WIN32
	if (Windows_InitializeSCardApi() >= 0)
		g_SCardApi = Windows_GetSCardApiFunctionTable();
#else
#if defined(WITH_SMARTCARD_PCSC)
	if (PCSC_InitializeSCardApi() >= 0)
		g_SCardApi = PCSC_GetSCardApiFunctionTable();
#endif
#endif

#if defined(WITH_SMARTCARD_INSPECT)
	g_SCardApi = Inspect_RegisterSCardApi(g_SCardApi);
#endif
	return TRUE;
}

WINSCARDAPI LONG WINAPI SCardEstablishContext(DWORD dwScope, LPCVOID pvReserved1,
                                              LPCVOID pvReserved2, LPSCARDCONTEXT phContext)
{
	SCARDAPI_STUB_CALL_LONG(SCardEstablishContext, dwScope, pvReserved1, pvReserved2, phContext);
}

WINSCARDAPI LONG WINAPI SCardReleaseContext(SCARDCONTEXT hContext)
{
	SCARDAPI_STUB_CALL_LONG(SCardReleaseContext, hContext);
}

WINSCARDAPI LONG WINAPI SCardIsValidContext(SCARDCONTEXT hContext)
{
	SCARDAPI_STUB_CALL_LONG(SCardIsValidContext, hContext);
}

WINSCARDAPI LONG WINAPI SCardListReaderGroupsA(SCARDCONTEXT hContext, LPSTR mszGroups,
                                               LPDWORD pcchGroups)
{
	SCARDAPI_STUB_CALL_LONG(SCardListReaderGroupsA, hContext, mszGroups, pcchGroups);
}

WINSCARDAPI LONG WINAPI SCardListReaderGroupsW(SCARDCONTEXT hContext, LPWSTR mszGroups,
                                               LPDWORD pcchGroups)
{
	SCARDAPI_STUB_CALL_LONG(SCardListReaderGroupsW, hContext, mszGroups, pcchGroups);
}

WINSCARDAPI LONG WINAPI SCardListReadersA(SCARDCONTEXT hContext, LPCSTR mszGroups, LPSTR mszReaders,
                                          LPDWORD pcchReaders)
{
	SCARDAPI_STUB_CALL_LONG(SCardListReadersA, hContext, mszGroups, mszReaders, pcchReaders);
}

WINSCARDAPI LONG WINAPI SCardListReadersW(SCARDCONTEXT hContext, LPCWSTR mszGroups,
                                          LPWSTR mszReaders, LPDWORD pcchReaders)
{
	SCARDAPI_STUB_CALL_LONG(SCardListReadersW, hContext, mszGroups, mszReaders, pcchReaders);
}

WINSCARDAPI LONG WINAPI SCardListCardsA(SCARDCONTEXT hContext, LPCBYTE pbAtr,
                                        LPCGUID rgquidInterfaces, DWORD cguidInterfaceCount,
                                        CHAR* mszCards, LPDWORD pcchCards)
{
	SCARDAPI_STUB_CALL_LONG(SCardListCardsA, hContext, pbAtr, rgquidInterfaces, cguidInterfaceCount,
	                        mszCards, pcchCards);
}

WINSCARDAPI LONG WINAPI SCardListCardsW(SCARDCONTEXT hContext, LPCBYTE pbAtr,
                                        LPCGUID rgquidInterfaces, DWORD cguidInterfaceCount,
                                        WCHAR* mszCards, LPDWORD pcchCards)
{
	SCARDAPI_STUB_CALL_LONG(SCardListCardsW, hContext, pbAtr, rgquidInterfaces, cguidInterfaceCount,
	                        mszCards, pcchCards);
}

WINSCARDAPI LONG WINAPI SCardListInterfacesA(SCARDCONTEXT hContext, LPCSTR szCard,
                                             LPGUID pguidInterfaces, LPDWORD pcguidInterfaces)
{
	SCARDAPI_STUB_CALL_LONG(SCardListInterfacesA, hContext, szCard, pguidInterfaces,
	                        pcguidInterfaces);
}

WINSCARDAPI LONG WINAPI SCardListInterfacesW(SCARDCONTEXT hContext, LPCWSTR szCard,
                                             LPGUID pguidInterfaces, LPDWORD pcguidInterfaces)
{
	SCARDAPI_STUB_CALL_LONG(SCardListInterfacesW, hContext, szCard, pguidInterfaces,
	                        pcguidInterfaces);
}

WINSCARDAPI LONG WINAPI SCardGetProviderIdA(SCARDCONTEXT hContext, LPCSTR szCard,
                                            LPGUID pguidProviderId)
{
	SCARDAPI_STUB_CALL_LONG(SCardGetProviderIdA, hContext, szCard, pguidProviderId);
}

WINSCARDAPI LONG WINAPI SCardGetProviderIdW(SCARDCONTEXT hContext, LPCWSTR szCard,
                                            LPGUID pguidProviderId)
{
	SCARDAPI_STUB_CALL_LONG(SCardGetProviderIdW, hContext, szCard, pguidProviderId);
}

WINSCARDAPI LONG WINAPI SCardGetCardTypeProviderNameA(SCARDCONTEXT hContext, LPCSTR szCardName,
                                                      DWORD dwProviderId, CHAR* szProvider,
                                                      LPDWORD pcchProvider)
{
	SCARDAPI_STUB_CALL_LONG(SCardGetCardTypeProviderNameA, hContext, szCardName, dwProviderId,
	                        szProvider, pcchProvider);
}

WINSCARDAPI LONG WINAPI SCardGetCardTypeProviderNameW(SCARDCONTEXT hContext, LPCWSTR szCardName,
                                                      DWORD dwProviderId, WCHAR* szProvider,
                                                      LPDWORD pcchProvider)
{
	SCARDAPI_STUB_CALL_LONG(SCardGetCardTypeProviderNameW, hContext, szCardName, dwProviderId,
	                        szProvider, pcchProvider);
}

WINSCARDAPI LONG WINAPI SCardIntroduceReaderGroupA(SCARDCONTEXT hContext, LPCSTR szGroupName)
{
	SCARDAPI_STUB_CALL_LONG(SCardIntroduceReaderGroupA, hContext, szGroupName);
}

WINSCARDAPI LONG WINAPI SCardIntroduceReaderGroupW(SCARDCONTEXT hContext, LPCWSTR szGroupName)
{
	SCARDAPI_STUB_CALL_LONG(SCardIntroduceReaderGroupW, hContext, szGroupName);
}

WINSCARDAPI LONG WINAPI SCardForgetReaderGroupA(SCARDCONTEXT hContext, LPCSTR szGroupName)
{
	SCARDAPI_STUB_CALL_LONG(SCardForgetReaderGroupA, hContext, szGroupName);
}

WINSCARDAPI LONG WINAPI SCardForgetReaderGroupW(SCARDCONTEXT hContext, LPCWSTR szGroupName)
{
	SCARDAPI_STUB_CALL_LONG(SCardForgetReaderGroupW, hContext, szGroupName);
}

WINSCARDAPI LONG WINAPI SCardIntroduceReaderA(SCARDCONTEXT hContext, LPCSTR szReaderName,
                                              LPCSTR szDeviceName)
{
	SCARDAPI_STUB_CALL_LONG(SCardIntroduceReaderA, hContext, szReaderName, szDeviceName);
}

WINSCARDAPI LONG WINAPI SCardIntroduceReaderW(SCARDCONTEXT hContext, LPCWSTR szReaderName,
                                              LPCWSTR szDeviceName)
{
	SCARDAPI_STUB_CALL_LONG(SCardIntroduceReaderW, hContext, szReaderName, szDeviceName);
}

WINSCARDAPI LONG WINAPI SCardForgetReaderA(SCARDCONTEXT hContext, LPCSTR szReaderName)
{
	SCARDAPI_STUB_CALL_LONG(SCardForgetReaderA, hContext, szReaderName);
}

WINSCARDAPI LONG WINAPI SCardForgetReaderW(SCARDCONTEXT hContext, LPCWSTR szReaderName)
{
	SCARDAPI_STUB_CALL_LONG(SCardForgetReaderW, hContext, szReaderName);
}

WINSCARDAPI LONG WINAPI SCardAddReaderToGroupA(SCARDCONTEXT hContext, LPCSTR szReaderName,
                                               LPCSTR szGroupName)
{
	SCARDAPI_STUB_CALL_LONG(SCardAddReaderToGroupA, hContext, szReaderName, szGroupName);
}

WINSCARDAPI LONG WINAPI SCardAddReaderToGroupW(SCARDCONTEXT hContext, LPCWSTR szReaderName,
                                               LPCWSTR szGroupName)
{
	SCARDAPI_STUB_CALL_LONG(SCardAddReaderToGroupW, hContext, szReaderName, szGroupName);
}

WINSCARDAPI LONG WINAPI SCardRemoveReaderFromGroupA(SCARDCONTEXT hContext, LPCSTR szReaderName,
                                                    LPCSTR szGroupName)
{
	SCARDAPI_STUB_CALL_LONG(SCardRemoveReaderFromGroupA, hContext, szReaderName, szGroupName);
}

WINSCARDAPI LONG WINAPI SCardRemoveReaderFromGroupW(SCARDCONTEXT hContext, LPCWSTR szReaderName,
                                                    LPCWSTR szGroupName)
{
	SCARDAPI_STUB_CALL_LONG(SCardRemoveReaderFromGroupW, hContext, szReaderName, szGroupName);
}

WINSCARDAPI LONG WINAPI SCardIntroduceCardTypeA(SCARDCONTEXT hContext, LPCSTR szCardName,
                                                LPCGUID pguidPrimaryProvider,
                                                LPCGUID rgguidInterfaces, DWORD dwInterfaceCount,
                                                LPCBYTE pbAtr, LPCBYTE pbAtrMask, DWORD cbAtrLen)
{
	SCARDAPI_STUB_CALL_LONG(SCardIntroduceCardTypeA, hContext, szCardName, pguidPrimaryProvider,
	                        rgguidInterfaces, dwInterfaceCount, pbAtr, pbAtrMask, cbAtrLen);
}

WINSCARDAPI LONG WINAPI SCardIntroduceCardTypeW(SCARDCONTEXT hContext, LPCWSTR szCardName,
                                                LPCGUID pguidPrimaryProvider,
                                                LPCGUID rgguidInterfaces, DWORD dwInterfaceCount,
                                                LPCBYTE pbAtr, LPCBYTE pbAtrMask, DWORD cbAtrLen)
{
	SCARDAPI_STUB_CALL_LONG(SCardIntroduceCardTypeW, hContext, szCardName, pguidPrimaryProvider,
	                        rgguidInterfaces, dwInterfaceCount, pbAtr, pbAtrMask, cbAtrLen);
}

WINSCARDAPI LONG WINAPI SCardSetCardTypeProviderNameA(SCARDCONTEXT hContext, LPCSTR szCardName,
                                                      DWORD dwProviderId, LPCSTR szProvider)
{
	SCARDAPI_STUB_CALL_LONG(SCardSetCardTypeProviderNameA, hContext, szCardName, dwProviderId,
	                        szProvider);
}

WINSCARDAPI LONG WINAPI SCardSetCardTypeProviderNameW(SCARDCONTEXT hContext, LPCWSTR szCardName,
                                                      DWORD dwProviderId, LPCWSTR szProvider)
{
	SCARDAPI_STUB_CALL_LONG(SCardSetCardTypeProviderNameW, hContext, szCardName, dwProviderId,
	                        szProvider);
}

WINSCARDAPI LONG WINAPI SCardForgetCardTypeA(SCARDCONTEXT hContext, LPCSTR szCardName)
{
	SCARDAPI_STUB_CALL_LONG(SCardForgetCardTypeA, hContext, szCardName);
}

WINSCARDAPI LONG WINAPI SCardForgetCardTypeW(SCARDCONTEXT hContext, LPCWSTR szCardName)
{
	SCARDAPI_STUB_CALL_LONG(SCardForgetCardTypeW, hContext, szCardName);
}

WINSCARDAPI LONG WINAPI SCardFreeMemory(SCARDCONTEXT hContext, LPVOID pvMem)
{
	SCARDAPI_STUB_CALL_LONG(SCardFreeMemory, hContext, pvMem);
}

WINSCARDAPI HANDLE WINAPI SCardAccessStartedEvent(void)
{
	SCARDAPI_STUB_CALL_HANDLE(SCardAccessStartedEvent);
}

WINSCARDAPI void WINAPI SCardReleaseStartedEvent(void)
{
	SCARDAPI_STUB_CALL_VOID(SCardReleaseStartedEvent);
}

WINSCARDAPI LONG WINAPI SCardLocateCardsA(SCARDCONTEXT hContext, LPCSTR mszCards,
                                          LPSCARD_READERSTATEA rgReaderStates, DWORD cReaders)
{
	SCARDAPI_STUB_CALL_LONG(SCardLocateCardsA, hContext, mszCards, rgReaderStates, cReaders);
}

WINSCARDAPI LONG WINAPI SCardLocateCardsW(SCARDCONTEXT hContext, LPCWSTR mszCards,
                                          LPSCARD_READERSTATEW rgReaderStates, DWORD cReaders)
{
	SCARDAPI_STUB_CALL_LONG(SCardLocateCardsW, hContext, mszCards, rgReaderStates, cReaders);
}

WINSCARDAPI LONG WINAPI SCardLocateCardsByATRA(SCARDCONTEXT hContext, LPSCARD_ATRMASK rgAtrMasks,
                                               DWORD cAtrs, LPSCARD_READERSTATEA rgReaderStates,
                                               DWORD cReaders)
{
	SCARDAPI_STUB_CALL_LONG(SCardLocateCardsByATRA, hContext, rgAtrMasks, cAtrs, rgReaderStates,
	                        cReaders);
}

WINSCARDAPI LONG WINAPI SCardLocateCardsByATRW(SCARDCONTEXT hContext, LPSCARD_ATRMASK rgAtrMasks,
                                               DWORD cAtrs, LPSCARD_READERSTATEW rgReaderStates,
                                               DWORD cReaders)
{
	SCARDAPI_STUB_CALL_LONG(SCardLocateCardsByATRW, hContext, rgAtrMasks, cAtrs, rgReaderStates,
	                        cReaders);
}

WINSCARDAPI LONG WINAPI SCardGetStatusChangeA(SCARDCONTEXT hContext, DWORD dwTimeout,
                                              LPSCARD_READERSTATEA rgReaderStates, DWORD cReaders)
{
	SCARDAPI_STUB_CALL_LONG(SCardGetStatusChangeA, hContext, dwTimeout, rgReaderStates, cReaders);
}

WINSCARDAPI LONG WINAPI SCardGetStatusChangeW(SCARDCONTEXT hContext, DWORD dwTimeout,
                                              LPSCARD_READERSTATEW rgReaderStates, DWORD cReaders)
{
	SCARDAPI_STUB_CALL_LONG(SCardGetStatusChangeW, hContext, dwTimeout, rgReaderStates, cReaders);
}

WINSCARDAPI LONG WINAPI SCardCancel(SCARDCONTEXT hContext)
{
	SCARDAPI_STUB_CALL_LONG(SCardCancel, hContext);
}

WINSCARDAPI LONG WINAPI SCardConnectA(SCARDCONTEXT hContext, LPCSTR szReader, DWORD dwShareMode,
                                      DWORD dwPreferredProtocols, LPSCARDHANDLE phCard,
                                      LPDWORD pdwActiveProtocol)
{
	SCARDAPI_STUB_CALL_LONG(SCardConnectA, hContext, szReader, dwShareMode, dwPreferredProtocols,
	                        phCard, pdwActiveProtocol);
}

WINSCARDAPI LONG WINAPI SCardConnectW(SCARDCONTEXT hContext, LPCWSTR szReader, DWORD dwShareMode,
                                      DWORD dwPreferredProtocols, LPSCARDHANDLE phCard,
                                      LPDWORD pdwActiveProtocol)
{
	SCARDAPI_STUB_CALL_LONG(SCardConnectW, hContext, szReader, dwShareMode, dwPreferredProtocols,
	                        phCard, pdwActiveProtocol);
}

WINSCARDAPI LONG WINAPI SCardReconnect(SCARDHANDLE hCard, DWORD dwShareMode,
                                       DWORD dwPreferredProtocols, DWORD dwInitialization,
                                       LPDWORD pdwActiveProtocol)
{
	SCARDAPI_STUB_CALL_LONG(SCardReconnect, hCard, dwShareMode, dwPreferredProtocols,
	                        dwInitialization, pdwActiveProtocol);
}

WINSCARDAPI LONG WINAPI SCardDisconnect(SCARDHANDLE hCard, DWORD dwDisposition)
{
	SCARDAPI_STUB_CALL_LONG(SCardDisconnect, hCard, dwDisposition);
}

WINSCARDAPI LONG WINAPI SCardBeginTransaction(SCARDHANDLE hCard)
{
	SCARDAPI_STUB_CALL_LONG(SCardBeginTransaction, hCard);
}

WINSCARDAPI LONG WINAPI SCardEndTransaction(SCARDHANDLE hCard, DWORD dwDisposition)
{
	SCARDAPI_STUB_CALL_LONG(SCardEndTransaction, hCard, dwDisposition);
}

WINSCARDAPI LONG WINAPI SCardCancelTransaction(SCARDHANDLE hCard)
{
	SCARDAPI_STUB_CALL_LONG(SCardCancelTransaction, hCard);
}

WINSCARDAPI LONG WINAPI SCardState(SCARDHANDLE hCard, LPDWORD pdwState, LPDWORD pdwProtocol,
                                   LPBYTE pbAtr, LPDWORD pcbAtrLen)
{
	SCARDAPI_STUB_CALL_LONG(SCardState, hCard, pdwState, pdwProtocol, pbAtr, pcbAtrLen);
}

WINSCARDAPI LONG WINAPI SCardStatusA(SCARDHANDLE hCard, LPSTR mszReaderNames, LPDWORD pcchReaderLen,
                                     LPDWORD pdwState, LPDWORD pdwProtocol, LPBYTE pbAtr,
                                     LPDWORD pcbAtrLen)
{
	SCARDAPI_STUB_CALL_LONG(SCardStatusA, hCard, mszReaderNames, pcchReaderLen, pdwState,
	                        pdwProtocol, pbAtr, pcbAtrLen);
}

WINSCARDAPI LONG WINAPI SCardStatusW(SCARDHANDLE hCard, LPWSTR mszReaderNames,
                                     LPDWORD pcchReaderLen, LPDWORD pdwState, LPDWORD pdwProtocol,
                                     LPBYTE pbAtr, LPDWORD pcbAtrLen)
{
	SCARDAPI_STUB_CALL_LONG(SCardStatusW, hCard, mszReaderNames, pcchReaderLen, pdwState,
	                        pdwProtocol, pbAtr, pcbAtrLen);
}

WINSCARDAPI LONG WINAPI SCardTransmit(SCARDHANDLE hCard, LPCSCARD_IO_REQUEST pioSendPci,
                                      LPCBYTE pbSendBuffer, DWORD cbSendLength,
                                      LPSCARD_IO_REQUEST pioRecvPci, LPBYTE pbRecvBuffer,
                                      LPDWORD pcbRecvLength)
{
	SCARDAPI_STUB_CALL_LONG(SCardTransmit, hCard, pioSendPci, pbSendBuffer, cbSendLength,
	                        pioRecvPci, pbRecvBuffer, pcbRecvLength);
}

WINSCARDAPI LONG WINAPI SCardGetTransmitCount(SCARDHANDLE hCard, LPDWORD pcTransmitCount)
{
	SCARDAPI_STUB_CALL_LONG(SCardGetTransmitCount, hCard, pcTransmitCount);
}

WINSCARDAPI LONG WINAPI SCardControl(SCARDHANDLE hCard, DWORD dwControlCode, LPCVOID lpInBuffer,
                                     DWORD cbInBufferSize, LPVOID lpOutBuffer,
                                     DWORD cbOutBufferSize, LPDWORD lpBytesReturned)
{
	SCARDAPI_STUB_CALL_LONG(SCardControl, hCard, dwControlCode, lpInBuffer, cbInBufferSize,
	                        lpOutBuffer, cbOutBufferSize, lpBytesReturned);
}

WINSCARDAPI LONG WINAPI SCardGetAttrib(SCARDHANDLE hCard, DWORD dwAttrId, LPBYTE pbAttr,
                                       LPDWORD pcbAttrLen)
{
	SCARDAPI_STUB_CALL_LONG(SCardGetAttrib, hCard, dwAttrId, pbAttr, pcbAttrLen);
}

WINSCARDAPI LONG WINAPI SCardSetAttrib(SCARDHANDLE hCard, DWORD dwAttrId, LPCBYTE pbAttr,
                                       DWORD cbAttrLen)
{
	SCARDAPI_STUB_CALL_LONG(SCardSetAttrib, hCard, dwAttrId, pbAttr, cbAttrLen);
}

WINSCARDAPI LONG WINAPI SCardUIDlgSelectCardA(LPOPENCARDNAMEA_EX pDlgStruc)
{
	SCARDAPI_STUB_CALL_LONG(SCardUIDlgSelectCardA, pDlgStruc);
}

WINSCARDAPI LONG WINAPI SCardUIDlgSelectCardW(LPOPENCARDNAMEW_EX pDlgStruc)
{
	SCARDAPI_STUB_CALL_LONG(SCardUIDlgSelectCardW, pDlgStruc);
}

WINSCARDAPI LONG WINAPI GetOpenCardNameA(LPOPENCARDNAMEA pDlgStruc)
{
	SCARDAPI_STUB_CALL_LONG(GetOpenCardNameA, pDlgStruc);
}

WINSCARDAPI LONG WINAPI GetOpenCardNameW(LPOPENCARDNAMEW pDlgStruc)
{
	SCARDAPI_STUB_CALL_LONG(GetOpenCardNameW, pDlgStruc);
}

WINSCARDAPI LONG WINAPI SCardDlgExtendedError(void)
{
	SCARDAPI_STUB_CALL_LONG(SCardDlgExtendedError);
}

WINSCARDAPI LONG WINAPI SCardReadCacheA(SCARDCONTEXT hContext, UUID* CardIdentifier,
                                        DWORD FreshnessCounter, LPSTR LookupName, PBYTE Data,
                                        DWORD* DataLen)
{
	SCARDAPI_STUB_CALL_LONG(SCardReadCacheA, hContext, CardIdentifier, FreshnessCounter, LookupName,
	                        Data, DataLen);
}

WINSCARDAPI LONG WINAPI SCardReadCacheW(SCARDCONTEXT hContext, UUID* CardIdentifier,
                                        DWORD FreshnessCounter, LPWSTR LookupName, PBYTE Data,
                                        DWORD* DataLen)
{
	SCARDAPI_STUB_CALL_LONG(SCardReadCacheW, hContext, CardIdentifier, FreshnessCounter, LookupName,
	                        Data, DataLen);
}

WINSCARDAPI LONG WINAPI SCardWriteCacheA(SCARDCONTEXT hContext, UUID* CardIdentifier,
                                         DWORD FreshnessCounter, LPSTR LookupName, PBYTE Data,
                                         DWORD DataLen)
{
	SCARDAPI_STUB_CALL_LONG(SCardWriteCacheA, hContext, CardIdentifier, FreshnessCounter,
	                        LookupName, Data, DataLen);
}

WINSCARDAPI LONG WINAPI SCardWriteCacheW(SCARDCONTEXT hContext, UUID* CardIdentifier,
                                         DWORD FreshnessCounter, LPWSTR LookupName, PBYTE Data,
                                         DWORD DataLen)
{
	SCARDAPI_STUB_CALL_LONG(SCardWriteCacheW, hContext, CardIdentifier, FreshnessCounter,
	                        LookupName, Data, DataLen);
}

WINSCARDAPI LONG WINAPI SCardGetReaderIconA(SCARDCONTEXT hContext, LPCSTR szReaderName,
                                            LPBYTE pbIcon, LPDWORD pcbIcon)
{
	SCARDAPI_STUB_CALL_LONG(SCardGetReaderIconA, hContext, szReaderName, pbIcon, pcbIcon);
}

WINSCARDAPI LONG WINAPI SCardGetReaderIconW(SCARDCONTEXT hContext, LPCWSTR szReaderName,
                                            LPBYTE pbIcon, LPDWORD pcbIcon)
{
	SCARDAPI_STUB_CALL_LONG(SCardGetReaderIconW, hContext, szReaderName, pbIcon, pcbIcon);
}

WINSCARDAPI LONG WINAPI SCardGetDeviceTypeIdA(SCARDCONTEXT hContext, LPCSTR szReaderName,
                                              LPDWORD pdwDeviceTypeId)
{
	SCARDAPI_STUB_CALL_LONG(SCardGetDeviceTypeIdA, hContext, szReaderName, pdwDeviceTypeId);
}

WINSCARDAPI LONG WINAPI SCardGetDeviceTypeIdW(SCARDCONTEXT hContext, LPCWSTR szReaderName,
                                              LPDWORD pdwDeviceTypeId)
{
	SCARDAPI_STUB_CALL_LONG(SCardGetDeviceTypeIdW, hContext, szReaderName, pdwDeviceTypeId);
}

WINSCARDAPI LONG WINAPI SCardGetReaderDeviceInstanceIdA(SCARDCONTEXT hContext, LPCSTR szReaderName,
                                                        LPSTR szDeviceInstanceId,
                                                        LPDWORD pcchDeviceInstanceId)
{
	SCARDAPI_STUB_CALL_LONG(SCardGetReaderDeviceInstanceIdA, hContext, szReaderName,
	                        szDeviceInstanceId, pcchDeviceInstanceId);
}

WINSCARDAPI LONG WINAPI SCardGetReaderDeviceInstanceIdW(SCARDCONTEXT hContext, LPCWSTR szReaderName,
                                                        LPWSTR szDeviceInstanceId,
                                                        LPDWORD pcchDeviceInstanceId)
{
	SCARDAPI_STUB_CALL_LONG(SCardGetReaderDeviceInstanceIdW, hContext, szReaderName,
	                        szDeviceInstanceId, pcchDeviceInstanceId);
}

WINSCARDAPI LONG WINAPI SCardListReadersWithDeviceInstanceIdA(SCARDCONTEXT hContext,
                                                              LPCSTR szDeviceInstanceId,
                                                              LPSTR mszReaders, LPDWORD pcchReaders)
{
	SCARDAPI_STUB_CALL_LONG(SCardListReadersWithDeviceInstanceIdA, hContext, szDeviceInstanceId,
	                        mszReaders, pcchReaders);
}

WINSCARDAPI LONG WINAPI SCardListReadersWithDeviceInstanceIdW(SCARDCONTEXT hContext,
                                                              LPCWSTR szDeviceInstanceId,
                                                              LPWSTR mszReaders,
                                                              LPDWORD pcchReaders)
{
	SCARDAPI_STUB_CALL_LONG(SCardListReadersWithDeviceInstanceIdW, hContext, szDeviceInstanceId,
	                        mszReaders, pcchReaders);
}

WINSCARDAPI LONG WINAPI SCardAudit(SCARDCONTEXT hContext, DWORD dwEvent)
{
	SCARDAPI_STUB_CALL_LONG(SCardAudit, hContext, dwEvent);
}

/**
 * Extended API
 */

WINSCARDAPI const char* WINAPI SCardGetErrorString(LONG errorCode)
{
	switch (errorCode)
	{
		case SCARD_S_SUCCESS:
			return "SCARD_S_SUCCESS";

		case SCARD_F_INTERNAL_ERROR:
			return "SCARD_F_INTERNAL_ERROR";

		case SCARD_E_CANCELLED:
			return "SCARD_E_CANCELLED";

		case SCARD_E_INVALID_HANDLE:
			return "SCARD_E_INVALID_HANDLE";

		case SCARD_E_INVALID_PARAMETER:
			return "SCARD_E_INVALID_PARAMETER";

		case SCARD_E_INVALID_TARGET:
			return "SCARD_E_INVALID_TARGET";

		case SCARD_E_NO_MEMORY:
			return "SCARD_E_NO_MEMORY";

		case SCARD_F_WAITED_TOO_LONG:
			return "SCARD_F_WAITED_TOO_LONG";

		case SCARD_E_INSUFFICIENT_BUFFER:
			return "SCARD_E_INSUFFICIENT_BUFFER";

		case SCARD_E_UNKNOWN_READER:
			return "SCARD_E_UNKNOWN_READER";

		case SCARD_E_TIMEOUT:
			return "SCARD_E_TIMEOUT";

		case SCARD_E_SHARING_VIOLATION:
			return "SCARD_E_SHARING_VIOLATION";

		case SCARD_E_NO_SMARTCARD:
			return "SCARD_E_NO_SMARTCARD";

		case SCARD_E_UNKNOWN_CARD:
			return "SCARD_E_UNKNOWN_CARD";

		case SCARD_E_CANT_DISPOSE:
			return "SCARD_E_CANT_DISPOSE";

		case SCARD_E_PROTO_MISMATCH:
			return "SCARD_E_PROTO_MISMATCH";

		case SCARD_E_NOT_READY:
			return "SCARD_E_NOT_READY";

		case SCARD_E_INVALID_VALUE:
			return "SCARD_E_INVALID_VALUE";

		case SCARD_E_SYSTEM_CANCELLED:
			return "SCARD_E_SYSTEM_CANCELLED";

		case SCARD_F_COMM_ERROR:
			return "SCARD_F_COMM_ERROR";

		case SCARD_F_UNKNOWN_ERROR:
			return "SCARD_F_UNKNOWN_ERROR";

		case SCARD_E_INVALID_ATR:
			return "SCARD_E_INVALID_ATR";

		case SCARD_E_NOT_TRANSACTED:
			return "SCARD_E_NOT_TRANSACTED";

		case SCARD_E_READER_UNAVAILABLE:
			return "SCARD_E_READER_UNAVAILABLE";

		case SCARD_P_SHUTDOWN:
			return "SCARD_P_SHUTDOWN";

		case SCARD_E_PCI_TOO_SMALL:
			return "SCARD_E_PCI_TOO_SMALL";

		case SCARD_E_READER_UNSUPPORTED:
			return "SCARD_E_READER_UNSUPPORTED";

		case SCARD_E_DUPLICATE_READER:
			return "SCARD_E_DUPLICATE_READER";

		case SCARD_E_CARD_UNSUPPORTED:
			return "SCARD_E_CARD_UNSUPPORTED";

		case SCARD_E_NO_SERVICE:
			return "SCARD_E_NO_SERVICE";

		case SCARD_E_SERVICE_STOPPED:
			return "SCARD_E_SERVICE_STOPPED";

		case SCARD_E_UNEXPECTED:
			return "SCARD_E_UNEXPECTED";

		case SCARD_E_ICC_INSTALLATION:
			return "SCARD_E_ICC_INSTALLATION";

		case SCARD_E_ICC_CREATEORDER:
			return "SCARD_E_ICC_CREATEORDER";

		case SCARD_E_UNSUPPORTED_FEATURE:
			return "SCARD_E_UNSUPPORTED_FEATURE";

		case SCARD_E_DIR_NOT_FOUND:
			return "SCARD_E_DIR_NOT_FOUND";

		case SCARD_E_FILE_NOT_FOUND:
			return "SCARD_E_FILE_NOT_FOUND";

		case SCARD_E_NO_DIR:
			return "SCARD_E_NO_DIR";

		case SCARD_E_NO_FILE:
			return "SCARD_E_NO_FILE";

		case SCARD_E_NO_ACCESS:
			return "SCARD_E_NO_ACCESS";

		case SCARD_E_WRITE_TOO_MANY:
			return "SCARD_E_WRITE_TOO_MANY";

		case SCARD_E_BAD_SEEK:
			return "SCARD_E_BAD_SEEK";

		case SCARD_E_INVALID_CHV:
			return "SCARD_E_INVALID_CHV";

		case SCARD_E_UNKNOWN_RES_MNG:
			return "SCARD_E_UNKNOWN_RES_MNG";

		case SCARD_E_NO_SUCH_CERTIFICATE:
			return "SCARD_E_NO_SUCH_CERTIFICATE";

		case SCARD_E_CERTIFICATE_UNAVAILABLE:
			return "SCARD_E_CERTIFICATE_UNAVAILABLE";

		case SCARD_E_NO_READERS_AVAILABLE:
			return "SCARD_E_NO_READERS_AVAILABLE";

		case SCARD_E_COMM_DATA_LOST:
			return "SCARD_E_COMM_DATA_LOST";

		case SCARD_E_NO_KEY_CONTAINER:
			return "SCARD_E_NO_KEY_CONTAINER";

		case SCARD_E_SERVER_TOO_BUSY:
			return "SCARD_E_SERVER_TOO_BUSY";

		case SCARD_E_PIN_CACHE_EXPIRED:
			return "SCARD_E_PIN_CACHE_EXPIRED";

		case SCARD_E_NO_PIN_CACHE:
			return "SCARD_E_NO_PIN_CACHE";

		case SCARD_E_READ_ONLY_CARD:
			return "SCARD_E_READ_ONLY_CARD";

		case SCARD_W_UNSUPPORTED_CARD:
			return "SCARD_W_UNSUPPORTED_CARD";

		case SCARD_W_UNRESPONSIVE_CARD:
			return "SCARD_W_UNRESPONSIVE_CARD";

		case SCARD_W_UNPOWERED_CARD:
			return "SCARD_W_UNPOWERED_CARD";

		case SCARD_W_RESET_CARD:
			return "SCARD_W_RESET_CARD";

		case SCARD_W_REMOVED_CARD:
			return "SCARD_W_REMOVED_CARD";

		case SCARD_W_SECURITY_VIOLATION:
			return "SCARD_W_SECURITY_VIOLATION";

		case SCARD_W_WRONG_CHV:
			return "SCARD_W_WRONG_CHV";

		case SCARD_W_CHV_BLOCKED:
			return "SCARD_W_CHV_BLOCKED";

		case SCARD_W_EOF:
			return "SCARD_W_EOF";

		case SCARD_W_CANCELLED_BY_USER:
			return "SCARD_W_CANCELLED_BY_USER";

		case SCARD_W_CARD_NOT_AUTHENTICATED:
			return "SCARD_W_CARD_NOT_AUTHENTICATED";

		case SCARD_W_CACHE_ITEM_NOT_FOUND:
			return "SCARD_W_CACHE_ITEM_NOT_FOUND";

		case SCARD_W_CACHE_ITEM_STALE:
			return "SCARD_W_CACHE_ITEM_STALE";

		case SCARD_W_CACHE_ITEM_TOO_BIG:
			return "SCARD_W_CACHE_ITEM_TOO_BIG";

		default:
			return "SCARD_E_UNKNOWN";
	}
}

WINSCARDAPI const char* WINAPI SCardGetAttributeString(DWORD dwAttrId)
{
	switch (dwAttrId)
	{
		case SCARD_ATTR_VENDOR_NAME:
			return "SCARD_ATTR_VENDOR_NAME";

		case SCARD_ATTR_VENDOR_IFD_TYPE:
			return "SCARD_ATTR_VENDOR_IFD_TYPE";

		case SCARD_ATTR_VENDOR_IFD_VERSION:
			return "SCARD_ATTR_VENDOR_IFD_VERSION";

		case SCARD_ATTR_VENDOR_IFD_SERIAL_NO:
			return "SCARD_ATTR_VENDOR_IFD_SERIAL_NO";

		case SCARD_ATTR_CHANNEL_ID:
			return "SCARD_ATTR_CHANNEL_ID";

		case SCARD_ATTR_PROTOCOL_TYPES:
			return "SCARD_ATTR_PROTOCOL_TYPES";

		case SCARD_ATTR_DEFAULT_CLK:
			return "SCARD_ATTR_DEFAULT_CLK";

		case SCARD_ATTR_MAX_CLK:
			return "SCARD_ATTR_MAX_CLK";

		case SCARD_ATTR_DEFAULT_DATA_RATE:
			return "SCARD_ATTR_DEFAULT_DATA_RATE";

		case SCARD_ATTR_MAX_DATA_RATE:
			return "SCARD_ATTR_MAX_DATA_RATE";

		case SCARD_ATTR_MAX_IFSD:
			return "SCARD_ATTR_MAX_IFSD";

		case SCARD_ATTR_POWER_MGMT_SUPPORT:
			return "SCARD_ATTR_POWER_MGMT_SUPPORT";

		case SCARD_ATTR_USER_TO_CARD_AUTH_DEVICE:
			return "SCARD_ATTR_USER_TO_CARD_AUTH_DEVICE";

		case SCARD_ATTR_USER_AUTH_INPUT_DEVICE:
			return "SCARD_ATTR_USER_AUTH_INPUT_DEVICE";

		case SCARD_ATTR_CHARACTERISTICS:
			return "SCARD_ATTR_CHARACTERISTICS";

		case SCARD_ATTR_CURRENT_PROTOCOL_TYPE:
			return "SCARD_ATTR_CURRENT_PROTOCOL_TYPE";

		case SCARD_ATTR_CURRENT_CLK:
			return "SCARD_ATTR_CURRENT_CLK";

		case SCARD_ATTR_CURRENT_F:
			return "SCARD_ATTR_CURRENT_F";

		case SCARD_ATTR_CURRENT_D:
			return "SCARD_ATTR_CURRENT_D";

		case SCARD_ATTR_CURRENT_N:
			return "SCARD_ATTR_CURRENT_N";

		case SCARD_ATTR_CURRENT_W:
			return "SCARD_ATTR_CURRENT_W";

		case SCARD_ATTR_CURRENT_IFSC:
			return "SCARD_ATTR_CURRENT_IFSC";

		case SCARD_ATTR_CURRENT_IFSD:
			return "SCARD_ATTR_CURRENT_IFSD";

		case SCARD_ATTR_CURRENT_BWT:
			return "SCARD_ATTR_CURRENT_BWT";

		case SCARD_ATTR_CURRENT_CWT:
			return "SCARD_ATTR_CURRENT_CWT";

		case SCARD_ATTR_CURRENT_EBC_ENCODING:
			return "SCARD_ATTR_CURRENT_EBC_ENCODING";

		case SCARD_ATTR_EXTENDED_BWT:
			return "SCARD_ATTR_EXTENDED_BWT";

		case SCARD_ATTR_ICC_PRESENCE:
			return "SCARD_ATTR_ICC_PRESENCE";

		case SCARD_ATTR_ICC_INTERFACE_STATUS:
			return "SCARD_ATTR_ICC_INTERFACE_STATUS";

		case SCARD_ATTR_CURRENT_IO_STATE:
			return "SCARD_ATTR_CURRENT_IO_STATE";

		case SCARD_ATTR_ATR_STRING:
			return "SCARD_ATTR_ATR_STRING";

		case SCARD_ATTR_ICC_TYPE_PER_ATR:
			return "SCARD_ATTR_ICC_TYPE_PER_ATR";

		case SCARD_ATTR_ESC_RESET:
			return "SCARD_ATTR_ESC_RESET";

		case SCARD_ATTR_ESC_CANCEL:
			return "SCARD_ATTR_ESC_CANCEL";

		case SCARD_ATTR_ESC_AUTHREQUEST:
			return "SCARD_ATTR_ESC_AUTHREQUEST";

		case SCARD_ATTR_MAXINPUT:
			return "SCARD_ATTR_MAXINPUT";

		case SCARD_ATTR_DEVICE_UNIT:
			return "SCARD_ATTR_DEVICE_UNIT";

		case SCARD_ATTR_DEVICE_IN_USE:
			return "SCARD_ATTR_DEVICE_IN_USE";

		case SCARD_ATTR_DEVICE_FRIENDLY_NAME_A:
			return "SCARD_ATTR_DEVICE_FRIENDLY_NAME_A";

		case SCARD_ATTR_DEVICE_SYSTEM_NAME_A:
			return "SCARD_ATTR_DEVICE_SYSTEM_NAME_A";

		case SCARD_ATTR_DEVICE_FRIENDLY_NAME_W:
			return "SCARD_ATTR_DEVICE_FRIENDLY_NAME_W";

		case SCARD_ATTR_DEVICE_SYSTEM_NAME_W:
			return "SCARD_ATTR_DEVICE_SYSTEM_NAME_W";

		case SCARD_ATTR_SUPRESS_T1_IFS_REQUEST:
			return "SCARD_ATTR_SUPRESS_T1_IFS_REQUEST";

		default:
			return "SCARD_ATTR_UNKNOWN";
	}
}

WINSCARDAPI const char* WINAPI SCardGetProtocolString(DWORD dwProtocols)
{
	if (dwProtocols == SCARD_PROTOCOL_UNDEFINED)
		return "SCARD_PROTOCOL_UNDEFINED";

	if (dwProtocols == SCARD_PROTOCOL_T0)
		return "SCARD_PROTOCOL_T0";

	if (dwProtocols == SCARD_PROTOCOL_T1)
		return "SCARD_PROTOCOL_T1";

	if (dwProtocols == SCARD_PROTOCOL_Tx)
		return "SCARD_PROTOCOL_Tx";

	if (dwProtocols == SCARD_PROTOCOL_RAW)
		return "SCARD_PROTOCOL_RAW";

	if (dwProtocols == SCARD_PROTOCOL_DEFAULT)
		return "SCARD_PROTOCOL_DEFAULT";

	if (dwProtocols == (SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_RAW))
		return "SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_RAW";

	if (dwProtocols == (SCARD_PROTOCOL_T1 | SCARD_PROTOCOL_RAW))
		return "SCARD_PROTOCOL_T1 | SCARD_PROTOCOL_RAW";

	if (dwProtocols == (SCARD_PROTOCOL_Tx | SCARD_PROTOCOL_RAW))
		return "SCARD_PROTOCOL_Tx | SCARD_PROTOCOL_RAW";

	return "SCARD_PROTOCOL_UNKNOWN";
}

WINSCARDAPI const char* WINAPI SCardGetShareModeString(DWORD dwShareMode)
{
	switch (dwShareMode)
	{
		case SCARD_SHARE_EXCLUSIVE:
			return "SCARD_SHARE_EXCLUSIVE";

		case SCARD_SHARE_SHARED:
			return "SCARD_SHARE_SHARED";

		case SCARD_SHARE_DIRECT:
			return "SCARD_SHARE_DIRECT";

		default:
			return "SCARD_SHARE_UNKNOWN";
	}
}

WINSCARDAPI const char* WINAPI SCardGetDispositionString(DWORD dwDisposition)
{
	switch (dwDisposition)
	{
		case SCARD_LEAVE_CARD:
			return "SCARD_LEAVE_CARD";

		case SCARD_RESET_CARD:
			return "SCARD_RESET_CARD";

		case SCARD_UNPOWER_CARD:
			return "SCARD_UNPOWER_CARD";

		default:
			return "SCARD_UNKNOWN_CARD";
	}
}

WINSCARDAPI const char* WINAPI SCardGetScopeString(DWORD dwScope)
{
	switch (dwScope)
	{
		case SCARD_SCOPE_USER:
			return "SCARD_SCOPE_USER";

		case SCARD_SCOPE_TERMINAL:
			return "SCARD_SCOPE_TERMINAL";

		case SCARD_SCOPE_SYSTEM:
			return "SCARD_SCOPE_SYSTEM";

		default:
			return "SCARD_SCOPE_UNKNOWN";
	}
}

WINSCARDAPI const char* WINAPI SCardGetCardStateString(DWORD dwCardState)
{
	switch (dwCardState)
	{
		case SCARD_UNKNOWN:
			return "SCARD_UNKNOWN";

		case SCARD_ABSENT:
			return "SCARD_ABSENT";

		case SCARD_PRESENT:
			return "SCARD_PRESENT";

		case SCARD_SWALLOWED:
			return "SCARD_SWALLOWED";

		case SCARD_POWERED:
			return "SCARD_POWERED";

		case SCARD_NEGOTIABLE:
			return "SCARD_NEGOTIABLE";

		case SCARD_SPECIFIC:
			return "SCARD_SPECIFIC";

		default:
			return "SCARD_UNKNOWN";
	}
}

WINSCARDAPI char* WINAPI SCardGetReaderStateString(DWORD dwReaderState)
{
	const size_t size = 512;
	char* buffer = calloc(size, sizeof(char));

	if (!buffer)
		return NULL;

	if (dwReaderState & SCARD_STATE_IGNORE)
		winpr_str_append("SCARD_STATE_IGNORE", buffer, size, "|");

	if (dwReaderState & SCARD_STATE_CHANGED)
		winpr_str_append("SCARD_STATE_CHANGED", buffer, size, "|");

	if (dwReaderState & SCARD_STATE_UNKNOWN)
		winpr_str_append("SCARD_STATE_UNKNOWN", buffer, size, "|");

	if (dwReaderState & SCARD_STATE_UNAVAILABLE)
		winpr_str_append("SCARD_STATE_UNAVAILABLE", buffer, size, "|");

	if (dwReaderState & SCARD_STATE_EMPTY)
		winpr_str_append("SCARD_STATE_EMPTY", buffer, size, "|");

	if (dwReaderState & SCARD_STATE_PRESENT)
		winpr_str_append("SCARD_STATE_PRESENT", buffer, size, "|");

	if (dwReaderState & SCARD_STATE_ATRMATCH)
		winpr_str_append("SCARD_STATE_ATRMATCH", buffer, size, "|");

	if (dwReaderState & SCARD_STATE_EXCLUSIVE)
		winpr_str_append("SCARD_STATE_EXCLUSIVE", buffer, size, "|");

	if (dwReaderState & SCARD_STATE_INUSE)
		winpr_str_append("SCARD_STATE_INUSE", buffer, size, "|");

	if (dwReaderState & SCARD_STATE_MUTE)
		winpr_str_append("SCARD_STATE_MUTE", buffer, size, "|");

	if (dwReaderState & SCARD_STATE_UNPOWERED)
		winpr_str_append("SCARD_STATE_UNPOWERED", buffer, size, "|");

	if (!buffer[0])
		winpr_str_append("SCARD_STATE_UNAWARE", buffer, size, "|");

	return buffer;
}

#define WINSCARD_LOAD_PROC(_name)                                                              \
	do                                                                                         \
	{                                                                                          \
		WINPR_PRAGMA_DIAG_PUSH                                                                 \
		WINPR_PRAGMA_DIAG_IGNORED_PEDANTIC                                                     \
		pWinSCardApiTable->pfn##_name = GetProcAddressAs(hWinSCardLibrary, #_name, fn##_name); \
		WINPR_PRAGMA_DIAG_POP                                                                  \
	} while (0)

BOOL WinSCard_LoadApiTableFunctions(PSCardApiFunctionTable pWinSCardApiTable,
                                    HMODULE hWinSCardLibrary)
{
	WINPR_ASSERT(pWinSCardApiTable);
	WINPR_ASSERT(hWinSCardLibrary);

	WINSCARD_LOAD_PROC(SCardEstablishContext);
	WINSCARD_LOAD_PROC(SCardReleaseContext);
	WINSCARD_LOAD_PROC(SCardIsValidContext);
	WINSCARD_LOAD_PROC(SCardListReaderGroupsA);
	WINSCARD_LOAD_PROC(SCardListReaderGroupsW);
	WINSCARD_LOAD_PROC(SCardListReadersA);
	WINSCARD_LOAD_PROC(SCardListReadersW);
	WINSCARD_LOAD_PROC(SCardListCardsA);
	WINSCARD_LOAD_PROC(SCardListCardsW);
	WINSCARD_LOAD_PROC(SCardListInterfacesA);
	WINSCARD_LOAD_PROC(SCardListInterfacesW);
	WINSCARD_LOAD_PROC(SCardGetProviderIdA);
	WINSCARD_LOAD_PROC(SCardGetProviderIdW);
	WINSCARD_LOAD_PROC(SCardGetCardTypeProviderNameA);
	WINSCARD_LOAD_PROC(SCardGetCardTypeProviderNameW);
	WINSCARD_LOAD_PROC(SCardIntroduceReaderGroupA);
	WINSCARD_LOAD_PROC(SCardIntroduceReaderGroupW);
	WINSCARD_LOAD_PROC(SCardForgetReaderGroupA);
	WINSCARD_LOAD_PROC(SCardForgetReaderGroupW);
	WINSCARD_LOAD_PROC(SCardIntroduceReaderA);
	WINSCARD_LOAD_PROC(SCardIntroduceReaderW);
	WINSCARD_LOAD_PROC(SCardForgetReaderA);
	WINSCARD_LOAD_PROC(SCardForgetReaderW);
	WINSCARD_LOAD_PROC(SCardAddReaderToGroupA);
	WINSCARD_LOAD_PROC(SCardAddReaderToGroupW);
	WINSCARD_LOAD_PROC(SCardRemoveReaderFromGroupA);
	WINSCARD_LOAD_PROC(SCardRemoveReaderFromGroupW);
	WINSCARD_LOAD_PROC(SCardIntroduceCardTypeA);
	WINSCARD_LOAD_PROC(SCardIntroduceCardTypeW);
	WINSCARD_LOAD_PROC(SCardSetCardTypeProviderNameA);
	WINSCARD_LOAD_PROC(SCardSetCardTypeProviderNameW);
	WINSCARD_LOAD_PROC(SCardForgetCardTypeA);
	WINSCARD_LOAD_PROC(SCardForgetCardTypeW);
	WINSCARD_LOAD_PROC(SCardFreeMemory);
	WINSCARD_LOAD_PROC(SCardAccessStartedEvent);
	WINSCARD_LOAD_PROC(SCardReleaseStartedEvent);
	WINSCARD_LOAD_PROC(SCardLocateCardsA);
	WINSCARD_LOAD_PROC(SCardLocateCardsW);
	WINSCARD_LOAD_PROC(SCardLocateCardsByATRA);
	WINSCARD_LOAD_PROC(SCardLocateCardsByATRW);
	WINSCARD_LOAD_PROC(SCardGetStatusChangeA);
	WINSCARD_LOAD_PROC(SCardGetStatusChangeW);
	WINSCARD_LOAD_PROC(SCardCancel);
	WINSCARD_LOAD_PROC(SCardConnectA);
	WINSCARD_LOAD_PROC(SCardConnectW);
	WINSCARD_LOAD_PROC(SCardReconnect);
	WINSCARD_LOAD_PROC(SCardDisconnect);
	WINSCARD_LOAD_PROC(SCardBeginTransaction);
	WINSCARD_LOAD_PROC(SCardEndTransaction);
	WINSCARD_LOAD_PROC(SCardCancelTransaction);
	WINSCARD_LOAD_PROC(SCardState);
	WINSCARD_LOAD_PROC(SCardStatusA);
	WINSCARD_LOAD_PROC(SCardStatusW);
	WINSCARD_LOAD_PROC(SCardTransmit);
	WINSCARD_LOAD_PROC(SCardGetTransmitCount);
	WINSCARD_LOAD_PROC(SCardControl);
	WINSCARD_LOAD_PROC(SCardGetAttrib);
	WINSCARD_LOAD_PROC(SCardSetAttrib);
	WINSCARD_LOAD_PROC(SCardUIDlgSelectCardA);
	WINSCARD_LOAD_PROC(SCardUIDlgSelectCardW);
	WINSCARD_LOAD_PROC(GetOpenCardNameA);
	WINSCARD_LOAD_PROC(GetOpenCardNameW);
	WINSCARD_LOAD_PROC(SCardDlgExtendedError);
	WINSCARD_LOAD_PROC(SCardReadCacheA);
	WINSCARD_LOAD_PROC(SCardReadCacheW);
	WINSCARD_LOAD_PROC(SCardWriteCacheA);
	WINSCARD_LOAD_PROC(SCardWriteCacheW);
	WINSCARD_LOAD_PROC(SCardGetReaderIconA);
	WINSCARD_LOAD_PROC(SCardGetReaderIconW);
	WINSCARD_LOAD_PROC(SCardGetDeviceTypeIdA);
	WINSCARD_LOAD_PROC(SCardGetDeviceTypeIdW);
	WINSCARD_LOAD_PROC(SCardGetReaderDeviceInstanceIdA);
	WINSCARD_LOAD_PROC(SCardGetReaderDeviceInstanceIdW);
	WINSCARD_LOAD_PROC(SCardListReadersWithDeviceInstanceIdA);
	WINSCARD_LOAD_PROC(SCardListReadersWithDeviceInstanceIdW);
	WINSCARD_LOAD_PROC(SCardAudit);

	return TRUE;
}

static const SCardApiFunctionTable WinPR_SCardApiFunctionTable = {
	0, /* dwVersion */
	0, /* dwFlags */

	SCardEstablishContext,                 /* SCardEstablishContext */
	SCardReleaseContext,                   /* SCardReleaseContext */
	SCardIsValidContext,                   /* SCardIsValidContext */
	SCardListReaderGroupsA,                /* SCardListReaderGroupsA */
	SCardListReaderGroupsW,                /* SCardListReaderGroupsW */
	SCardListReadersA,                     /* SCardListReadersA */
	SCardListReadersW,                     /* SCardListReadersW */
	SCardListCardsA,                       /* SCardListCardsA */
	SCardListCardsW,                       /* SCardListCardsW */
	SCardListInterfacesA,                  /* SCardListInterfacesA */
	SCardListInterfacesW,                  /* SCardListInterfacesW */
	SCardGetProviderIdA,                   /* SCardGetProviderIdA */
	SCardGetProviderIdW,                   /* SCardGetProviderIdW */
	SCardGetCardTypeProviderNameA,         /* SCardGetCardTypeProviderNameA */
	SCardGetCardTypeProviderNameW,         /* SCardGetCardTypeProviderNameW */
	SCardIntroduceReaderGroupA,            /* SCardIntroduceReaderGroupA */
	SCardIntroduceReaderGroupW,            /* SCardIntroduceReaderGroupW */
	SCardForgetReaderGroupA,               /* SCardForgetReaderGroupA */
	SCardForgetReaderGroupW,               /* SCardForgetReaderGroupW */
	SCardIntroduceReaderA,                 /* SCardIntroduceReaderA */
	SCardIntroduceReaderW,                 /* SCardIntroduceReaderW */
	SCardForgetReaderA,                    /* SCardForgetReaderA */
	SCardForgetReaderW,                    /* SCardForgetReaderW */
	SCardAddReaderToGroupA,                /* SCardAddReaderToGroupA */
	SCardAddReaderToGroupW,                /* SCardAddReaderToGroupW */
	SCardRemoveReaderFromGroupA,           /* SCardRemoveReaderFromGroupA */
	SCardRemoveReaderFromGroupW,           /* SCardRemoveReaderFromGroupW */
	SCardIntroduceCardTypeA,               /* SCardIntroduceCardTypeA */
	SCardIntroduceCardTypeW,               /* SCardIntroduceCardTypeW */
	SCardSetCardTypeProviderNameA,         /* SCardSetCardTypeProviderNameA */
	SCardSetCardTypeProviderNameW,         /* SCardSetCardTypeProviderNameW */
	SCardForgetCardTypeA,                  /* SCardForgetCardTypeA */
	SCardForgetCardTypeW,                  /* SCardForgetCardTypeW */
	SCardFreeMemory,                       /* SCardFreeMemory */
	SCardAccessStartedEvent,               /* SCardAccessStartedEvent */
	SCardReleaseStartedEvent,              /* SCardReleaseStartedEvent */
	SCardLocateCardsA,                     /* SCardLocateCardsA */
	SCardLocateCardsW,                     /* SCardLocateCardsW */
	SCardLocateCardsByATRA,                /* SCardLocateCardsByATRA */
	SCardLocateCardsByATRW,                /* SCardLocateCardsByATRW */
	SCardGetStatusChangeA,                 /* SCardGetStatusChangeA */
	SCardGetStatusChangeW,                 /* SCardGetStatusChangeW */
	SCardCancel,                           /* SCardCancel */
	SCardConnectA,                         /* SCardConnectA */
	SCardConnectW,                         /* SCardConnectW */
	SCardReconnect,                        /* SCardReconnect */
	SCardDisconnect,                       /* SCardDisconnect */
	SCardBeginTransaction,                 /* SCardBeginTransaction */
	SCardEndTransaction,                   /* SCardEndTransaction */
	SCardCancelTransaction,                /* SCardCancelTransaction */
	SCardState,                            /* SCardState */
	SCardStatusA,                          /* SCardStatusA */
	SCardStatusW,                          /* SCardStatusW */
	SCardTransmit,                         /* SCardTransmit */
	SCardGetTransmitCount,                 /* SCardGetTransmitCount */
	SCardControl,                          /* SCardControl */
	SCardGetAttrib,                        /* SCardGetAttrib */
	SCardSetAttrib,                        /* SCardSetAttrib */
	SCardUIDlgSelectCardA,                 /* SCardUIDlgSelectCardA */
	SCardUIDlgSelectCardW,                 /* SCardUIDlgSelectCardW */
	GetOpenCardNameA,                      /* GetOpenCardNameA */
	GetOpenCardNameW,                      /* GetOpenCardNameW */
	SCardDlgExtendedError,                 /* SCardDlgExtendedError */
	SCardReadCacheA,                       /* SCardReadCacheA */
	SCardReadCacheW,                       /* SCardReadCacheW */
	SCardWriteCacheA,                      /* SCardWriteCacheA */
	SCardWriteCacheW,                      /* SCardWriteCacheW */
	SCardGetReaderIconA,                   /* SCardGetReaderIconA */
	SCardGetReaderIconW,                   /* SCardGetReaderIconW */
	SCardGetDeviceTypeIdA,                 /* SCardGetDeviceTypeIdA */
	SCardGetDeviceTypeIdW,                 /* SCardGetDeviceTypeIdW */
	SCardGetReaderDeviceInstanceIdA,       /* SCardGetReaderDeviceInstanceIdA */
	SCardGetReaderDeviceInstanceIdW,       /* SCardGetReaderDeviceInstanceIdW */
	SCardListReadersWithDeviceInstanceIdA, /* SCardListReadersWithDeviceInstanceIdA */
	SCardListReadersWithDeviceInstanceIdW, /* SCardListReadersWithDeviceInstanceIdW */
	SCardAudit                             /* SCardAudit */
};

const SCardApiFunctionTable* WinPR_GetSCardApiFunctionTable(void)
{
	return &WinPR_SCardApiFunctionTable;
}
