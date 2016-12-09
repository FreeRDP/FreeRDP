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
#include <winpr/wlog.h>
#include <winpr/file.h>
#include <winpr/path.h>
#include <winpr/library.h>
#include <winpr/smartcard.h>

#include "smartcard_inspect.h"

static wLog* g_Log = NULL;

static PSCardApiFunctionTable g_SCardApi = NULL;

/**
 * Standard Windows Smart Card API
 */

WINSCARDAPI LONG WINAPI Inspect_SCardEstablishContext(DWORD dwScope,
		LPCVOID pvReserved1, LPCVOID pvReserved2, LPSCARDCONTEXT phContext)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardEstablishContext { dwScope: %s (0x%08X)",
			SCardGetScopeString(dwScope), dwScope);

	status = g_SCardApi->pfnSCardEstablishContext(dwScope, pvReserved1, pvReserved2, phContext);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardEstablishContext } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardReleaseContext(SCARDCONTEXT hContext)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardReleaseContext { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardReleaseContext(hContext);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardReleaseContext } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardIsValidContext(SCARDCONTEXT hContext)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardIsValidContext { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardIsValidContext(hContext);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardIsValidContext } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardListReaderGroupsA(SCARDCONTEXT hContext,
		LPSTR mszGroups, LPDWORD pcchGroups)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardListReaderGroupsA { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardListReaderGroupsA(hContext, mszGroups, pcchGroups);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardListReaderGroupsA } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardListReaderGroupsW(SCARDCONTEXT hContext,
		LPWSTR mszGroups, LPDWORD pcchGroups)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardListReaderGroupsW { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardListReaderGroupsW(hContext, mszGroups, pcchGroups);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardListReaderGroupsW } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardListReadersA(SCARDCONTEXT hContext,
		LPCSTR mszGroups, LPSTR mszReaders, LPDWORD pcchReaders)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardListReadersA { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardListReadersA(hContext, mszGroups, mszReaders, pcchReaders);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardListReadersA } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardListReadersW(SCARDCONTEXT hContext,
		LPCWSTR mszGroups, LPWSTR mszReaders, LPDWORD pcchReaders)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardListReadersW { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardListReadersW(hContext, mszGroups, mszReaders, pcchReaders);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardListReadersW } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardListCardsA(SCARDCONTEXT hContext,
		LPCBYTE pbAtr, LPCGUID rgquidInterfaces, DWORD cguidInterfaceCount, CHAR* mszCards, LPDWORD pcchCards)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardListCardsA { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardListCardsA(hContext, pbAtr,
			rgquidInterfaces, cguidInterfaceCount, mszCards, pcchCards);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardListCardsA } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardListCardsW(SCARDCONTEXT hContext,
		LPCBYTE pbAtr, LPCGUID rgquidInterfaces, DWORD cguidInterfaceCount, WCHAR* mszCards, LPDWORD pcchCards)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardListCardsW { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardListCardsW(hContext, pbAtr,
			rgquidInterfaces, cguidInterfaceCount, mszCards, pcchCards);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardListCardsW } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardListInterfacesA(SCARDCONTEXT hContext,
		LPCSTR szCard, LPGUID pguidInterfaces, LPDWORD pcguidInterfaces)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardListInterfacesA { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardListInterfacesA(hContext, szCard, pguidInterfaces, pcguidInterfaces);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardListInterfacesA } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardListInterfacesW(SCARDCONTEXT hContext,
		LPCWSTR szCard, LPGUID pguidInterfaces, LPDWORD pcguidInterfaces)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardListInterfacesW { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardListInterfacesW(hContext, szCard, pguidInterfaces, pcguidInterfaces);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardListInterfacesW } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardGetProviderIdA(SCARDCONTEXT hContext,
		LPCSTR szCard, LPGUID pguidProviderId)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardGetProviderIdA { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardGetProviderIdA(hContext, szCard, pguidProviderId);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardGetProviderIdA } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardGetProviderIdW(SCARDCONTEXT hContext,
		LPCWSTR szCard, LPGUID pguidProviderId)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardGetProviderIdW { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardGetProviderIdW(hContext, szCard, pguidProviderId);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardGetProviderIdW } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardGetCardTypeProviderNameA(SCARDCONTEXT hContext,
		LPCSTR szCardName, DWORD dwProviderId, CHAR* szProvider, LPDWORD pcchProvider)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardGetCardTypeProviderNameA { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardGetCardTypeProviderNameA(hContext, szCardName,
			dwProviderId, szProvider, pcchProvider);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardGetCardTypeProviderNameA } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardGetCardTypeProviderNameW(SCARDCONTEXT hContext,
		LPCWSTR szCardName, DWORD dwProviderId, WCHAR* szProvider, LPDWORD pcchProvider)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardGetCardTypeProviderNameW { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardGetCardTypeProviderNameW(hContext, szCardName,
			dwProviderId, szProvider, pcchProvider);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardGetCardTypeProviderNameW } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardIntroduceReaderGroupA(SCARDCONTEXT hContext, LPCSTR szGroupName)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardIntroduceReaderGroupA { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardIntroduceReaderGroupA(hContext, szGroupName);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardIntroduceReaderGroupA } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardIntroduceReaderGroupW(SCARDCONTEXT hContext, LPCWSTR szGroupName)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardIntroduceReaderGroupW { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardIntroduceReaderGroupW(hContext, szGroupName);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardIntroduceReaderGroupW } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardForgetReaderGroupA(SCARDCONTEXT hContext, LPCSTR szGroupName)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardForgetReaderGroupA { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardForgetReaderGroupA(hContext, szGroupName);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardForgetReaderGroupA } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardForgetReaderGroupW(SCARDCONTEXT hContext, LPCWSTR szGroupName)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardForgetReaderGroupW { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardForgetReaderGroupW(hContext, szGroupName);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardForgetReaderGroupW } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardIntroduceReaderA(SCARDCONTEXT hContext,
		LPCSTR szReaderName, LPCSTR szDeviceName)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardIntroduceReaderA { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardIntroduceReaderA(hContext, szReaderName, szDeviceName);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardIntroduceReaderA } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardIntroduceReaderW(SCARDCONTEXT hContext,
		LPCWSTR szReaderName, LPCWSTR szDeviceName)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardIntroduceReaderW { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardIntroduceReaderW(hContext, szReaderName, szDeviceName);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardIntroduceReaderW } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardForgetReaderA(SCARDCONTEXT hContext, LPCSTR szReaderName)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardForgetReaderA { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardForgetReaderA(hContext, szReaderName);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardForgetReaderA } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardForgetReaderW(SCARDCONTEXT hContext, LPCWSTR szReaderName)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardForgetReaderW { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardForgetReaderW(hContext, szReaderName);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardForgetReaderW } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardAddReaderToGroupA(SCARDCONTEXT hContext,
		LPCSTR szReaderName, LPCSTR szGroupName)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardAddReaderToGroupA { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardAddReaderToGroupA(hContext, szReaderName, szGroupName);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardAddReaderToGroupA } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardAddReaderToGroupW(SCARDCONTEXT hContext,
		LPCWSTR szReaderName, LPCWSTR szGroupName)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardAddReaderToGroupW { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardAddReaderToGroupW(hContext, szReaderName, szGroupName);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardAddReaderToGroupW } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardRemoveReaderFromGroupA(SCARDCONTEXT hContext,
		LPCSTR szReaderName, LPCSTR szGroupName)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardRemoveReaderFromGroupA { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardRemoveReaderFromGroupA(hContext, szReaderName, szGroupName);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardRemoveReaderFromGroupA } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardRemoveReaderFromGroupW(SCARDCONTEXT hContext,
		LPCWSTR szReaderName, LPCWSTR szGroupName)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardRemoveReaderFromGroupW { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardRemoveReaderFromGroupW(hContext, szReaderName, szGroupName);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardRemoveReaderFromGroupW } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardIntroduceCardTypeA(SCARDCONTEXT hContext,
		LPCSTR szCardName, LPCGUID pguidPrimaryProvider, LPCGUID rgguidInterfaces,
		DWORD dwInterfaceCount, LPCBYTE pbAtr, LPCBYTE pbAtrMask, DWORD cbAtrLen)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardIntroduceCardTypeA { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardIntroduceCardTypeA(hContext, szCardName, pguidPrimaryProvider,
			rgguidInterfaces, dwInterfaceCount, pbAtr, pbAtrMask, cbAtrLen);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardIntroduceCardTypeA } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardIntroduceCardTypeW(SCARDCONTEXT hContext,
		LPCWSTR szCardName, LPCGUID pguidPrimaryProvider, LPCGUID rgguidInterfaces,
		DWORD dwInterfaceCount, LPCBYTE pbAtr, LPCBYTE pbAtrMask, DWORD cbAtrLen)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardIntroduceCardTypeW { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardIntroduceCardTypeW(hContext, szCardName, pguidPrimaryProvider,
			rgguidInterfaces, dwInterfaceCount, pbAtr, pbAtrMask, cbAtrLen);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardIntroduceCardTypeW } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardSetCardTypeProviderNameA(SCARDCONTEXT hContext,
		LPCSTR szCardName, DWORD dwProviderId, LPCSTR szProvider)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardSetCardTypeProviderNameA { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardSetCardTypeProviderNameA(hContext, szCardName, dwProviderId, szProvider);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardSetCardTypeProviderNameA } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardSetCardTypeProviderNameW(SCARDCONTEXT hContext,
		LPCWSTR szCardName, DWORD dwProviderId, LPCWSTR szProvider)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardSetCardTypeProviderNameA { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardSetCardTypeProviderNameW(hContext, szCardName, dwProviderId, szProvider);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardSetCardTypeProviderNameW } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardForgetCardTypeA(SCARDCONTEXT hContext, LPCSTR szCardName)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardForgetCardTypeA { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardForgetCardTypeA(hContext, szCardName);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardForgetCardTypeA } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardForgetCardTypeW(SCARDCONTEXT hContext, LPCWSTR szCardName)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardForgetCardTypeW { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardForgetCardTypeW(hContext, szCardName);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardForgetCardTypeW } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardFreeMemory(SCARDCONTEXT hContext, LPCVOID pvMem)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardFreeMemory { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardFreeMemory(hContext, pvMem);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardFreeMemory } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI HANDLE WINAPI Inspect_SCardAccessStartedEvent(void)
{
	HANDLE hEvent;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardAccessStartedEvent {");

	hEvent = g_SCardApi->pfnSCardAccessStartedEvent();

	WLog_Print(g_Log, WLOG_DEBUG, "SCardAccessStartedEvent } hEvent: %p", hEvent);

	return hEvent;
}

WINSCARDAPI void WINAPI Inspect_SCardReleaseStartedEvent(void)
{
	WLog_Print(g_Log, WLOG_DEBUG, "SCardReleaseStartedEvent {");

	g_SCardApi->pfnSCardReleaseStartedEvent();

	WLog_Print(g_Log, WLOG_DEBUG, "SCardReleaseStartedEvent }");
}

WINSCARDAPI LONG WINAPI Inspect_SCardLocateCardsA(SCARDCONTEXT hContext,
		LPCSTR mszCards, LPSCARD_READERSTATEA rgReaderStates, DWORD cReaders)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardLocateCardsA { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardLocateCardsA(hContext, mszCards, rgReaderStates, cReaders);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardLocateCardsA } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardLocateCardsW(SCARDCONTEXT hContext,
		LPCWSTR mszCards, LPSCARD_READERSTATEW rgReaderStates, DWORD cReaders)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardLocateCardsW { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardLocateCardsW(hContext, mszCards, rgReaderStates, cReaders);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardLocateCardsW } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardLocateCardsByATRA(SCARDCONTEXT hContext,
		LPSCARD_ATRMASK rgAtrMasks, DWORD cAtrs, LPSCARD_READERSTATEA rgReaderStates, DWORD cReaders)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardLocateCardsByATRA { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardLocateCardsByATRA(hContext, rgAtrMasks, cAtrs, rgReaderStates, cReaders);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardLocateCardsByATRA } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardLocateCardsByATRW(SCARDCONTEXT hContext,
		LPSCARD_ATRMASK rgAtrMasks, DWORD cAtrs, LPSCARD_READERSTATEW rgReaderStates, DWORD cReaders)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardLocateCardsByATRW { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardLocateCardsByATRW(hContext, rgAtrMasks, cAtrs, rgReaderStates, cReaders);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardLocateCardsByATRW } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardGetStatusChangeA(SCARDCONTEXT hContext,
		DWORD dwTimeout, LPSCARD_READERSTATEA rgReaderStates, DWORD cReaders)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardGetStatusChangeA { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardGetStatusChangeA(hContext, dwTimeout, rgReaderStates, cReaders);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardGetStatusChangeA } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardGetStatusChangeW(SCARDCONTEXT hContext,
		DWORD dwTimeout, LPSCARD_READERSTATEW rgReaderStates, DWORD cReaders)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardGetStatusChangeW { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardGetStatusChangeW(hContext, dwTimeout, rgReaderStates, cReaders);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardGetStatusChangeW } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardCancel(SCARDCONTEXT hContext)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardCancel { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardCancel(hContext);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardCancel } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardConnectA(SCARDCONTEXT hContext,
		LPCSTR szReader, DWORD dwShareMode, DWORD dwPreferredProtocols,
		LPSCARDHANDLE phCard, LPDWORD pdwActiveProtocol)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardConnectA { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardConnectA(hContext, szReader, dwShareMode,
			dwPreferredProtocols, phCard, pdwActiveProtocol);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardConnectA } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardConnectW(SCARDCONTEXT hContext,
		LPCWSTR szReader, DWORD dwShareMode, DWORD dwPreferredProtocols,
		LPSCARDHANDLE phCard, LPDWORD pdwActiveProtocol)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardConnectW { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardConnectW(hContext, szReader, dwShareMode,
			dwPreferredProtocols, phCard, pdwActiveProtocol);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardConnectW } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardReconnect(SCARDHANDLE hCard,
		DWORD dwShareMode, DWORD dwPreferredProtocols, DWORD dwInitialization, LPDWORD pdwActiveProtocol)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardReconnect { hCard: %p", hCard);

	status = g_SCardApi->pfnSCardReconnect(hCard, dwShareMode,
			dwPreferredProtocols, dwInitialization, pdwActiveProtocol);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardReconnect } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardDisconnect(SCARDHANDLE hCard, DWORD dwDisposition)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardDisconnect { hCard: %p", hCard);

	status = g_SCardApi->pfnSCardDisconnect(hCard, dwDisposition);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardDisconnect } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardBeginTransaction(SCARDHANDLE hCard)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardBeginTransaction { hCard: %p", hCard);

	status = g_SCardApi->pfnSCardBeginTransaction(hCard);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardBeginTransaction } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardEndTransaction(SCARDHANDLE hCard, DWORD dwDisposition)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardEndTransaction { hCard: %p", hCard);

	status = g_SCardApi->pfnSCardEndTransaction(hCard, dwDisposition);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardEndTransaction } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardCancelTransaction(SCARDHANDLE hCard)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardCancelTransaction { hCard: %p", hCard);

	status = g_SCardApi->pfnSCardCancelTransaction(hCard);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardCancelTransaction } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardState(SCARDHANDLE hCard,
		LPDWORD pdwState, LPDWORD pdwProtocol, LPBYTE pbAtr, LPDWORD pcbAtrLen)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardState { hCard: %p", hCard);

	status = g_SCardApi->pfnSCardState(hCard, pdwState, pdwProtocol, pbAtr, pcbAtrLen);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardState } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardStatusA(SCARDHANDLE hCard,
		LPSTR mszReaderNames, LPDWORD pcchReaderLen, LPDWORD pdwState,
		LPDWORD pdwProtocol, LPBYTE pbAtr, LPDWORD pcbAtrLen)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardStatusA { hCard: %p", hCard);

	status = g_SCardApi->pfnSCardStatusA(hCard, mszReaderNames, pcchReaderLen,
			pdwState, pdwProtocol, pbAtr, pcbAtrLen);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardStatusA } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardStatusW(SCARDHANDLE hCard,
		LPWSTR mszReaderNames, LPDWORD pcchReaderLen, LPDWORD pdwState,
		LPDWORD pdwProtocol, LPBYTE pbAtr, LPDWORD pcbAtrLen)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardStatusW { hCard: %p", hCard);

	status = g_SCardApi->pfnSCardStatusW(hCard, mszReaderNames, pcchReaderLen,
			pdwState, pdwProtocol, pbAtr, pcbAtrLen);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardStatusW } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardTransmit(SCARDHANDLE hCard,
		LPCSCARD_IO_REQUEST pioSendPci, LPCBYTE pbSendBuffer, DWORD cbSendLength,
		LPSCARD_IO_REQUEST pioRecvPci, LPBYTE pbRecvBuffer, LPDWORD pcbRecvLength)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardTransmit { hCard: %p", hCard);

	status = g_SCardApi->pfnSCardTransmit(hCard, pioSendPci, pbSendBuffer, cbSendLength,
			pioRecvPci, pbRecvBuffer, pcbRecvLength);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardTransmit } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardGetTransmitCount(SCARDHANDLE hCard, LPDWORD pcTransmitCount)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardGetTransmitCount { hCard: %p", hCard);

	status = g_SCardApi->pfnSCardGetTransmitCount(hCard, pcTransmitCount);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardGetTransmitCount } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardControl(SCARDHANDLE hCard,
		DWORD dwControlCode, LPCVOID lpInBuffer, DWORD cbInBufferSize,
		LPVOID lpOutBuffer, DWORD cbOutBufferSize, LPDWORD lpBytesReturned)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardControl { hCard: %p", hCard);

	status = g_SCardApi->pfnSCardControl(hCard, dwControlCode, lpInBuffer, cbInBufferSize,
			lpOutBuffer, cbOutBufferSize, lpBytesReturned);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardControl } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardGetAttrib(SCARDHANDLE hCard, DWORD dwAttrId, LPBYTE pbAttr, LPDWORD pcbAttrLen)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardGetAttrib { hCard: %p", hCard);

	status = g_SCardApi->pfnSCardGetAttrib(hCard, dwAttrId, pbAttr, pcbAttrLen);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardGetAttrib } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardSetAttrib(SCARDHANDLE hCard, DWORD dwAttrId, LPCBYTE pbAttr, DWORD cbAttrLen)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardSetAttrib { hCard: %p", hCard);

	status = g_SCardApi->pfnSCardSetAttrib(hCard, dwAttrId, pbAttr, cbAttrLen);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardSetAttrib } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardUIDlgSelectCardA(LPOPENCARDNAMEA_EX pDlgStruc)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardUIDlgSelectCardA {");

	status = g_SCardApi->pfnSCardUIDlgSelectCardA(pDlgStruc);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardUIDlgSelectCardA } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardUIDlgSelectCardW(LPOPENCARDNAMEW_EX pDlgStruc)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardUIDlgSelectCardW {");

	status = g_SCardApi->pfnSCardUIDlgSelectCardW(pDlgStruc);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardUIDlgSelectCardW } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_GetOpenCardNameA(LPOPENCARDNAMEA pDlgStruc)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "GetOpenCardNameA {");

	status = g_SCardApi->pfnGetOpenCardNameA(pDlgStruc);

	WLog_Print(g_Log, WLOG_DEBUG, "GetOpenCardNameA } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_GetOpenCardNameW(LPOPENCARDNAMEW pDlgStruc)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "GetOpenCardNameW {");

	status = g_SCardApi->pfnGetOpenCardNameW(pDlgStruc);

	WLog_Print(g_Log, WLOG_DEBUG, "GetOpenCardNameW } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardDlgExtendedError(void)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardDlgExtendedError {");

	status = g_SCardApi->pfnSCardDlgExtendedError();

	WLog_Print(g_Log, WLOG_DEBUG, "SCardDlgExtendedError } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardReadCacheA(SCARDCONTEXT hContext,
		UUID* CardIdentifier, DWORD FreshnessCounter, LPSTR LookupName, PBYTE Data, DWORD* DataLen)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardReadCacheA { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardReadCacheA(hContext, CardIdentifier,
			FreshnessCounter, LookupName, Data, DataLen);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardReadCacheA } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardReadCacheW(SCARDCONTEXT hContext,
		UUID* CardIdentifier,  DWORD FreshnessCounter, LPWSTR LookupName, PBYTE Data, DWORD* DataLen)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardReadCacheW { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardReadCacheW(hContext, CardIdentifier,
			FreshnessCounter, LookupName, Data, DataLen);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardReadCacheW } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardWriteCacheA(SCARDCONTEXT hContext,
		UUID* CardIdentifier, DWORD FreshnessCounter, LPSTR LookupName, PBYTE Data, DWORD DataLen)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardWriteCacheA { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardWriteCacheA(hContext, CardIdentifier,
			FreshnessCounter, LookupName, Data, DataLen);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardWriteCacheA } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardWriteCacheW(SCARDCONTEXT hContext,
		UUID* CardIdentifier, DWORD FreshnessCounter, LPWSTR LookupName, PBYTE Data, DWORD DataLen)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardWriteCacheW { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardWriteCacheW(hContext, CardIdentifier,
			FreshnessCounter, LookupName, Data, DataLen);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardWriteCacheW } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardGetReaderIconA(SCARDCONTEXT hContext,
		LPCSTR szReaderName, LPBYTE pbIcon, LPDWORD pcbIcon)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardGetReaderIconA { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardGetReaderIconA(hContext, szReaderName, pbIcon, pcbIcon);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardGetReaderIconA } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardGetReaderIconW(SCARDCONTEXT hContext,
		LPCWSTR szReaderName, LPBYTE pbIcon, LPDWORD pcbIcon)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardGetReaderIconW { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardGetReaderIconW(hContext, szReaderName, pbIcon, pcbIcon);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardGetReaderIconW } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardGetDeviceTypeIdA(SCARDCONTEXT hContext, LPCSTR szReaderName, LPDWORD pdwDeviceTypeId)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardGetDeviceTypeIdA { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardGetDeviceTypeIdA(hContext, szReaderName, pdwDeviceTypeId);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardGetDeviceTypeIdA } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardGetDeviceTypeIdW(SCARDCONTEXT hContext, LPCWSTR szReaderName, LPDWORD pdwDeviceTypeId)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardGetDeviceTypeIdW { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardGetDeviceTypeIdW(hContext, szReaderName, pdwDeviceTypeId);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardGetDeviceTypeIdW } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardGetReaderDeviceInstanceIdA(SCARDCONTEXT hContext,
		LPCSTR szReaderName, LPSTR szDeviceInstanceId, LPDWORD pcchDeviceInstanceId)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardGetReaderDeviceInstanceIdA { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardGetReaderDeviceInstanceIdA(hContext, szReaderName,
			szDeviceInstanceId, pcchDeviceInstanceId);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardGetReaderDeviceInstanceIdA } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardGetReaderDeviceInstanceIdW(SCARDCONTEXT hContext,
		LPCWSTR szReaderName, LPWSTR szDeviceInstanceId, LPDWORD pcchDeviceInstanceId)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardGetReaderDeviceInstanceIdW { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardGetReaderDeviceInstanceIdW(hContext, szReaderName,
			szDeviceInstanceId, pcchDeviceInstanceId);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardGetReaderDeviceInstanceIdW } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardListReadersWithDeviceInstanceIdA(SCARDCONTEXT hContext,
		LPCSTR szDeviceInstanceId, LPSTR mszReaders, LPDWORD pcchReaders)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardListReadersWithDeviceInstanceIdA { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardListReadersWithDeviceInstanceIdA(hContext,
			szDeviceInstanceId, mszReaders, pcchReaders);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardListReadersWithDeviceInstanceIdA } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardListReadersWithDeviceInstanceIdW(SCARDCONTEXT hContext,
		LPCWSTR szDeviceInstanceId, LPWSTR mszReaders, LPDWORD pcchReaders)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardListReadersWithDeviceInstanceIdW { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardListReadersWithDeviceInstanceIdW(hContext,
			szDeviceInstanceId, mszReaders, pcchReaders);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardListReadersWithDeviceInstanceIdW } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

	return status;
}

WINSCARDAPI LONG WINAPI Inspect_SCardAudit(SCARDCONTEXT hContext, DWORD dwEvent)
{
	LONG status;

	WLog_Print(g_Log, WLOG_DEBUG, "SCardAudit { hContext: %p", hContext);

	status = g_SCardApi->pfnSCardAudit(hContext, dwEvent);

	WLog_Print(g_Log, WLOG_DEBUG, "SCardAudit } status: %s (0x%08X)",
			SCardGetErrorString(status), status);

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

#ifndef _WIN32
#define SMARTCARD_INSPECT_FILEPATH "/tmp"
#else
#define SMARTCARD_INSPECT_FILEPATH "C:\\Windows\\Temp"
#endif

void Inspect_InitLog()
{
	wLogLayout* layout;
	wLogAppender* appender;
	const char* filepath = SMARTCARD_INSPECT_FILEPATH;

	if (g_Log)
		return;

	if (!PathFileExistsA(filepath))
		if (!PathMakePathA(filepath, NULL))
			return;

	if (!(g_Log = WLog_Get("WinSCard")))
		return;

	WLog_SetLogLevel(g_Log, WLOG_DEBUG);
	WLog_SetLogAppenderType(g_Log, WLOG_APPENDER_FILE);
	appender = WLog_GetLogAppender(g_Log);
	if (!appender)
		return;

	WLog_ConfigureAppender(appender, "outputfilename", "WinSCard.txt");
	WLog_ConfigureAppender(appender, "outputfilepath", (void *)filepath);

	layout = WLog_GetLogLayout(g_Log);
	WLog_Layout_SetPrefixFormat(g_Log, layout, "[%mn] ");

	WLog_OpenAppender(g_Log);
}

PSCardApiFunctionTable Inspect_RegisterSCardApi(PSCardApiFunctionTable pSCardApi)
{
	g_SCardApi = pSCardApi;

	Inspect_InitLog();

	return &Inspect_SCardApiFunctionTable;
}
