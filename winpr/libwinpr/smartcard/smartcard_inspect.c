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
#include <winpr/wlog.h>
#include <winpr/file.h>
#include <winpr/path.h>
#include <winpr/library.h>
#include <winpr/smartcard.h>

#include "smartcard_inspect.h"

#include "../log.h"
#define TAG WINPR_TAG("smartcard.inspect")

#define xstr(s) str(s)
#define str(s) #s

#define SCARDAPI_STUB_CALL_LONG(status, _name, ...)                                      \
	if (!g_SCardApi || !g_SCardApi->pfn##_name)                                          \
	{                                                                                    \
		WLog_DBG(TAG, "Missing function pointer g_SCardApi=%p->" xstr(pfn##_name) "=%p", \
		         g_SCardApi, g_SCardApi ? g_SCardApi->pfn##_name : NULL);                \
		status = SCARD_E_NO_SERVICE;                                                     \
	}                                                                                    \
	else                                                                                 \
		status = g_SCardApi->pfn##_name(__VA_ARGS__)

#define SCARDAPI_STUB_CALL_HANDLE(status, _name, ...)                                    \
	if (!g_SCardApi || !g_SCardApi->pfn##_name)                                          \
	{                                                                                    \
		WLog_DBG(TAG, "Missing function pointer g_SCardApi=%p->" xstr(pfn##_name) "=%p", \
		         g_SCardApi, g_SCardApi ? g_SCardApi->pfn##_name : NULL);                \
		status = NULL;                                                                   \
	}                                                                                    \
	else                                                                                 \
		status = g_SCardApi->pfn##_name(__VA_ARGS__)

#define SCARDAPI_STUB_CALL_VOID(_name, ...)                                              \
	if (!g_SCardApi || !g_SCardApi->pfn##_name)                                          \
	{                                                                                    \
		WLog_DBG(TAG, "Missing function pointer g_SCardApi=%p->" xstr(pfn##_name) "=%p", \
		         g_SCardApi, g_SCardApi ? g_SCardApi->pfn##_name : NULL);                \
	}                                                                                    \
	else                                                                                 \
		g_SCardApi->pfn##_name(__VA_ARGS__)

static const DWORD g_LogLevel = WLOG_DEBUG;
static wLog* g_Log = NULL;

static const SCardApiFunctionTable* g_SCardApi = NULL;

/**
 * Standard Windows Smart Card API
 */

static LONG WINAPI Inspect_SCardEstablishContext(DWORD dwScope, LPCVOID pvReserved1,
                                                 LPCVOID pvReserved2, LPSCARDCONTEXT phContext)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardEstablishContext { dwScope: %s (0x%08" PRIX32 ")",
	           SCardGetScopeString(dwScope), dwScope);

	SCARDAPI_STUB_CALL_LONG(status, SCardEstablishContext, dwScope, pvReserved1, pvReserved2,
	                        phContext);

	WLog_Print(g_Log, g_LogLevel, "SCardEstablishContext } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardReleaseContext(SCARDCONTEXT hContext)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardReleaseContext { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardReleaseContext, hContext);

	WLog_Print(g_Log, g_LogLevel, "SCardReleaseContext } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardIsValidContext(SCARDCONTEXT hContext)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardIsValidContext { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardIsValidContext, hContext);

	WLog_Print(g_Log, g_LogLevel, "SCardIsValidContext } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardListReaderGroupsA(SCARDCONTEXT hContext, LPSTR mszGroups,
                                                  LPDWORD pcchGroups)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardListReaderGroupsA { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardListReaderGroupsA, hContext, mszGroups, pcchGroups);

	WLog_Print(g_Log, g_LogLevel, "SCardListReaderGroupsA } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardListReaderGroupsW(SCARDCONTEXT hContext, LPWSTR mszGroups,
                                                  LPDWORD pcchGroups)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardListReaderGroupsW { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardListReaderGroupsW, hContext, mszGroups, pcchGroups);

	WLog_Print(g_Log, g_LogLevel, "SCardListReaderGroupsW } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardListReadersA(SCARDCONTEXT hContext, LPCSTR mszGroups,
                                             LPSTR mszReaders, LPDWORD pcchReaders)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardListReadersA { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardListReadersA, hContext, mszGroups, mszReaders,
	                        pcchReaders);

	WLog_Print(g_Log, g_LogLevel, "SCardListReadersA } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardListReadersW(SCARDCONTEXT hContext, LPCWSTR mszGroups,
                                             LPWSTR mszReaders, LPDWORD pcchReaders)
{
	LONG status;
	WLog_Print(g_Log, g_LogLevel, "SCardListReadersW { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardListReadersW, hContext, mszGroups, mszReaders,
	                        pcchReaders);
	WLog_Print(g_Log, g_LogLevel, "SCardListReadersW } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardListCardsA(SCARDCONTEXT hContext, LPCBYTE pbAtr,
                                           LPCGUID rgquidInterfaces, DWORD cguidInterfaceCount,
                                           CHAR* mszCards, LPDWORD pcchCards)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardListCardsA { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardListCardsA, hContext, pbAtr, rgquidInterfaces,
	                        cguidInterfaceCount, mszCards, pcchCards);

	WLog_Print(g_Log, g_LogLevel, "SCardListCardsA } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardListCardsW(SCARDCONTEXT hContext, LPCBYTE pbAtr,
                                           LPCGUID rgquidInterfaces, DWORD cguidInterfaceCount,
                                           WCHAR* mszCards, LPDWORD pcchCards)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardListCardsW { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardListCardsW, hContext, pbAtr, rgquidInterfaces,
	                        cguidInterfaceCount, mszCards, pcchCards);

	WLog_Print(g_Log, g_LogLevel, "SCardListCardsW } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardListInterfacesA(SCARDCONTEXT hContext, LPCSTR szCard,
                                                LPGUID pguidInterfaces, LPDWORD pcguidInterfaces)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardListInterfacesA { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardListInterfacesA, hContext, szCard, pguidInterfaces,
	                        pcguidInterfaces);

	WLog_Print(g_Log, g_LogLevel, "SCardListInterfacesA } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardListInterfacesW(SCARDCONTEXT hContext, LPCWSTR szCard,
                                                LPGUID pguidInterfaces, LPDWORD pcguidInterfaces)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardListInterfacesW { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardListInterfacesW, hContext, szCard, pguidInterfaces,
	                        pcguidInterfaces);

	WLog_Print(g_Log, g_LogLevel, "SCardListInterfacesW } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardGetProviderIdA(SCARDCONTEXT hContext, LPCSTR szCard,
                                               LPGUID pguidProviderId)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardGetProviderIdA { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardGetProviderIdA, hContext, szCard, pguidProviderId);

	WLog_Print(g_Log, g_LogLevel, "SCardGetProviderIdA } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardGetProviderIdW(SCARDCONTEXT hContext, LPCWSTR szCard,
                                               LPGUID pguidProviderId)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardGetProviderIdW { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardGetProviderIdW, hContext, szCard, pguidProviderId);

	WLog_Print(g_Log, g_LogLevel, "SCardGetProviderIdW } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardGetCardTypeProviderNameA(SCARDCONTEXT hContext, LPCSTR szCardName,
                                                         DWORD dwProviderId, CHAR* szProvider,
                                                         LPDWORD pcchProvider)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardGetCardTypeProviderNameA { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardGetCardTypeProviderNameA, hContext, szCardName,
	                        dwProviderId, szProvider, pcchProvider);

	WLog_Print(g_Log, g_LogLevel, "SCardGetCardTypeProviderNameA } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardGetCardTypeProviderNameW(SCARDCONTEXT hContext, LPCWSTR szCardName,
                                                         DWORD dwProviderId, WCHAR* szProvider,
                                                         LPDWORD pcchProvider)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardGetCardTypeProviderNameW { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardGetCardTypeProviderNameW, hContext, szCardName,
	                        dwProviderId, szProvider, pcchProvider);

	WLog_Print(g_Log, g_LogLevel, "SCardGetCardTypeProviderNameW } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardIntroduceReaderGroupA(SCARDCONTEXT hContext, LPCSTR szGroupName)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardIntroduceReaderGroupA { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardIntroduceReaderGroupA, hContext, szGroupName);

	WLog_Print(g_Log, g_LogLevel, "SCardIntroduceReaderGroupA } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardIntroduceReaderGroupW(SCARDCONTEXT hContext, LPCWSTR szGroupName)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardIntroduceReaderGroupW { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardIntroduceReaderGroupW, hContext, szGroupName);

	WLog_Print(g_Log, g_LogLevel, "SCardIntroduceReaderGroupW } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardForgetReaderGroupA(SCARDCONTEXT hContext, LPCSTR szGroupName)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardForgetReaderGroupA { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardForgetReaderGroupA, hContext, szGroupName);

	WLog_Print(g_Log, g_LogLevel, "SCardForgetReaderGroupA } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardForgetReaderGroupW(SCARDCONTEXT hContext, LPCWSTR szGroupName)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardForgetReaderGroupW { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardForgetReaderGroupW, hContext, szGroupName);

	WLog_Print(g_Log, g_LogLevel, "SCardForgetReaderGroupW } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardIntroduceReaderA(SCARDCONTEXT hContext, LPCSTR szReaderName,
                                                 LPCSTR szDeviceName)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardIntroduceReaderA { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardIntroduceReaderA, hContext, szReaderName, szDeviceName);

	WLog_Print(g_Log, g_LogLevel, "SCardIntroduceReaderA } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardIntroduceReaderW(SCARDCONTEXT hContext, LPCWSTR szReaderName,
                                                 LPCWSTR szDeviceName)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardIntroduceReaderW { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardIntroduceReaderW, hContext, szReaderName, szDeviceName);

	WLog_Print(g_Log, g_LogLevel, "SCardIntroduceReaderW } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardForgetReaderA(SCARDCONTEXT hContext, LPCSTR szReaderName)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardForgetReaderA { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardForgetReaderA, hContext, szReaderName);

	WLog_Print(g_Log, g_LogLevel, "SCardForgetReaderA } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardForgetReaderW(SCARDCONTEXT hContext, LPCWSTR szReaderName)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardForgetReaderW { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardForgetReaderW, hContext, szReaderName);

	WLog_Print(g_Log, g_LogLevel, "SCardForgetReaderW } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardAddReaderToGroupA(SCARDCONTEXT hContext, LPCSTR szReaderName,
                                                  LPCSTR szGroupName)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardAddReaderToGroupA { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardAddReaderToGroupA, hContext, szReaderName, szGroupName);

	WLog_Print(g_Log, g_LogLevel, "SCardAddReaderToGroupA } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardAddReaderToGroupW(SCARDCONTEXT hContext, LPCWSTR szReaderName,
                                                  LPCWSTR szGroupName)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardAddReaderToGroupW { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardAddReaderToGroupW, hContext, szReaderName, szGroupName);

	WLog_Print(g_Log, g_LogLevel, "SCardAddReaderToGroupW } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardRemoveReaderFromGroupA(SCARDCONTEXT hContext, LPCSTR szReaderName,
                                                       LPCSTR szGroupName)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardRemoveReaderFromGroupA { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardRemoveReaderFromGroupA, hContext, szReaderName,
	                        szGroupName);

	WLog_Print(g_Log, g_LogLevel, "SCardRemoveReaderFromGroupA } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardRemoveReaderFromGroupW(SCARDCONTEXT hContext, LPCWSTR szReaderName,
                                                       LPCWSTR szGroupName)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardRemoveReaderFromGroupW { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardRemoveReaderFromGroupW, hContext, szReaderName,
	                        szGroupName);

	WLog_Print(g_Log, g_LogLevel, "SCardRemoveReaderFromGroupW } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardIntroduceCardTypeA(SCARDCONTEXT hContext, LPCSTR szCardName,
                                                   LPCGUID pguidPrimaryProvider,
                                                   LPCGUID rgguidInterfaces, DWORD dwInterfaceCount,
                                                   LPCBYTE pbAtr, LPCBYTE pbAtrMask, DWORD cbAtrLen)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardIntroduceCardTypeA { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardIntroduceCardTypeA, hContext, szCardName,
	                        pguidPrimaryProvider, rgguidInterfaces, dwInterfaceCount, pbAtr,
	                        pbAtrMask, cbAtrLen);

	WLog_Print(g_Log, g_LogLevel, "SCardIntroduceCardTypeA } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardIntroduceCardTypeW(SCARDCONTEXT hContext, LPCWSTR szCardName,
                                                   LPCGUID pguidPrimaryProvider,
                                                   LPCGUID rgguidInterfaces, DWORD dwInterfaceCount,
                                                   LPCBYTE pbAtr, LPCBYTE pbAtrMask, DWORD cbAtrLen)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardIntroduceCardTypeW { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardIntroduceCardTypeW, hContext, szCardName,
	                        pguidPrimaryProvider, rgguidInterfaces, dwInterfaceCount, pbAtr,
	                        pbAtrMask, cbAtrLen);

	WLog_Print(g_Log, g_LogLevel, "SCardIntroduceCardTypeW } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardSetCardTypeProviderNameA(SCARDCONTEXT hContext, LPCSTR szCardName,
                                                         DWORD dwProviderId, LPCSTR szProvider)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardSetCardTypeProviderNameA { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardSetCardTypeProviderNameA, hContext, szCardName,
	                        dwProviderId, szProvider);

	WLog_Print(g_Log, g_LogLevel, "SCardSetCardTypeProviderNameA } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardSetCardTypeProviderNameW(SCARDCONTEXT hContext, LPCWSTR szCardName,
                                                         DWORD dwProviderId, LPCWSTR szProvider)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardSetCardTypeProviderNameA { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardSetCardTypeProviderNameW, hContext, szCardName,
	                        dwProviderId, szProvider);

	WLog_Print(g_Log, g_LogLevel, "SCardSetCardTypeProviderNameW } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardForgetCardTypeA(SCARDCONTEXT hContext, LPCSTR szCardName)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardForgetCardTypeA { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardForgetCardTypeA, hContext, szCardName);

	WLog_Print(g_Log, g_LogLevel, "SCardForgetCardTypeA } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardForgetCardTypeW(SCARDCONTEXT hContext, LPCWSTR szCardName)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardForgetCardTypeW { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardForgetCardTypeW, hContext, szCardName);

	WLog_Print(g_Log, g_LogLevel, "SCardForgetCardTypeW } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardFreeMemory(SCARDCONTEXT hContext, LPVOID pvMem)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardFreeMemory { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardFreeMemory, hContext, pvMem);

	WLog_Print(g_Log, g_LogLevel, "SCardFreeMemory } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static HANDLE WINAPI Inspect_SCardAccessStartedEvent(void)
{
	HANDLE hEvent;

	WLog_Print(g_Log, g_LogLevel, "SCardAccessStartedEvent {");

	SCARDAPI_STUB_CALL_HANDLE(hEvent, SCardAccessStartedEvent);

	WLog_Print(g_Log, g_LogLevel, "SCardAccessStartedEvent } hEvent: %p", hEvent);

	return hEvent;
}

static void WINAPI Inspect_SCardReleaseStartedEvent(void)
{
	WLog_Print(g_Log, g_LogLevel, "SCardReleaseStartedEvent {");

	SCARDAPI_STUB_CALL_VOID(SCardReleaseStartedEvent);

	WLog_Print(g_Log, g_LogLevel, "SCardReleaseStartedEvent }");
}

static LONG WINAPI Inspect_SCardLocateCardsA(SCARDCONTEXT hContext, LPCSTR mszCards,
                                             LPSCARD_READERSTATEA rgReaderStates, DWORD cReaders)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardLocateCardsA { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardLocateCardsA, hContext, mszCards, rgReaderStates,
	                        cReaders);

	WLog_Print(g_Log, g_LogLevel, "SCardLocateCardsA } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardLocateCardsW(SCARDCONTEXT hContext, LPCWSTR mszCards,
                                             LPSCARD_READERSTATEW rgReaderStates, DWORD cReaders)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardLocateCardsW { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardLocateCardsW, hContext, mszCards, rgReaderStates,
	                        cReaders);

	WLog_Print(g_Log, g_LogLevel, "SCardLocateCardsW } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardLocateCardsByATRA(SCARDCONTEXT hContext, LPSCARD_ATRMASK rgAtrMasks,
                                                  DWORD cAtrs, LPSCARD_READERSTATEA rgReaderStates,
                                                  DWORD cReaders)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardLocateCardsByATRA { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardLocateCardsByATRA, hContext, rgAtrMasks, cAtrs,
	                        rgReaderStates, cReaders);

	WLog_Print(g_Log, g_LogLevel, "SCardLocateCardsByATRA } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardLocateCardsByATRW(SCARDCONTEXT hContext, LPSCARD_ATRMASK rgAtrMasks,
                                                  DWORD cAtrs, LPSCARD_READERSTATEW rgReaderStates,
                                                  DWORD cReaders)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardLocateCardsByATRW { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardLocateCardsByATRW, hContext, rgAtrMasks, cAtrs,
	                        rgReaderStates, cReaders);

	WLog_Print(g_Log, g_LogLevel, "SCardLocateCardsByATRW } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardGetStatusChangeA(SCARDCONTEXT hContext, DWORD dwTimeout,
                                                 LPSCARD_READERSTATEA rgReaderStates,
                                                 DWORD cReaders)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardGetStatusChangeA { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardGetStatusChangeA, hContext, dwTimeout, rgReaderStates,
	                        cReaders);

	WLog_Print(g_Log, g_LogLevel, "SCardGetStatusChangeA } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardGetStatusChangeW(SCARDCONTEXT hContext, DWORD dwTimeout,
                                                 LPSCARD_READERSTATEW rgReaderStates,
                                                 DWORD cReaders)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardGetStatusChangeW { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardGetStatusChangeW, hContext, dwTimeout, rgReaderStates,
	                        cReaders);

	WLog_Print(g_Log, g_LogLevel, "SCardGetStatusChangeW } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardCancel(SCARDCONTEXT hContext)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardCancel { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardCancel, hContext);

	WLog_Print(g_Log, g_LogLevel, "SCardCancel } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardConnectA(SCARDCONTEXT hContext, LPCSTR szReader, DWORD dwShareMode,
                                         DWORD dwPreferredProtocols, LPSCARDHANDLE phCard,
                                         LPDWORD pdwActiveProtocol)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardConnectA { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardConnectA, hContext, szReader, dwShareMode,
	                        dwPreferredProtocols, phCard, pdwActiveProtocol);

	WLog_Print(g_Log, g_LogLevel, "SCardConnectA } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardConnectW(SCARDCONTEXT hContext, LPCWSTR szReader, DWORD dwShareMode,
                                         DWORD dwPreferredProtocols, LPSCARDHANDLE phCard,
                                         LPDWORD pdwActiveProtocol)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardConnectW { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardConnectW, hContext, szReader, dwShareMode,
	                        dwPreferredProtocols, phCard, pdwActiveProtocol);

	WLog_Print(g_Log, g_LogLevel, "SCardConnectW } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardReconnect(SCARDHANDLE hCard, DWORD dwShareMode,
                                          DWORD dwPreferredProtocols, DWORD dwInitialization,
                                          LPDWORD pdwActiveProtocol)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardReconnect { hCard: %p", (void*)hCard);

	SCARDAPI_STUB_CALL_LONG(status, SCardReconnect, hCard, dwShareMode, dwPreferredProtocols,
	                        dwInitialization, pdwActiveProtocol);

	WLog_Print(g_Log, g_LogLevel, "SCardReconnect } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardDisconnect(SCARDHANDLE hCard, DWORD dwDisposition)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardDisconnect { hCard: %p", (void*)hCard);

	SCARDAPI_STUB_CALL_LONG(status, SCardDisconnect, hCard, dwDisposition);

	WLog_Print(g_Log, g_LogLevel, "SCardDisconnect } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardBeginTransaction(SCARDHANDLE hCard)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardBeginTransaction { hCard: %p", (void*)hCard);

	SCARDAPI_STUB_CALL_LONG(status, SCardBeginTransaction, hCard);

	WLog_Print(g_Log, g_LogLevel, "SCardBeginTransaction } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardEndTransaction(SCARDHANDLE hCard, DWORD dwDisposition)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardEndTransaction { hCard: %p", (void*)hCard);

	SCARDAPI_STUB_CALL_LONG(status, SCardEndTransaction, hCard, dwDisposition);

	WLog_Print(g_Log, g_LogLevel, "SCardEndTransaction } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardCancelTransaction(SCARDHANDLE hCard)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardCancelTransaction { hCard: %p", (void*)hCard);

	SCARDAPI_STUB_CALL_LONG(status, SCardCancelTransaction, hCard);

	WLog_Print(g_Log, g_LogLevel, "SCardCancelTransaction } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardState(SCARDHANDLE hCard, LPDWORD pdwState, LPDWORD pdwProtocol,
                                      LPBYTE pbAtr, LPDWORD pcbAtrLen)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardState { hCard: %p", (void*)hCard);

	SCARDAPI_STUB_CALL_LONG(status, SCardState, hCard, pdwState, pdwProtocol, pbAtr, pcbAtrLen);

	WLog_Print(g_Log, g_LogLevel, "SCardState } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardStatusA(SCARDHANDLE hCard, LPSTR mszReaderNames,
                                        LPDWORD pcchReaderLen, LPDWORD pdwState,
                                        LPDWORD pdwProtocol, LPBYTE pbAtr, LPDWORD pcbAtrLen)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardStatusA { hCard: %p", (void*)hCard);

	SCARDAPI_STUB_CALL_LONG(status, SCardStatusA, hCard, mszReaderNames, pcchReaderLen, pdwState,
	                        pdwProtocol, pbAtr, pcbAtrLen);

	WLog_Print(g_Log, g_LogLevel, "SCardStatusA } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardStatusW(SCARDHANDLE hCard, LPWSTR mszReaderNames,
                                        LPDWORD pcchReaderLen, LPDWORD pdwState,
                                        LPDWORD pdwProtocol, LPBYTE pbAtr, LPDWORD pcbAtrLen)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardStatusW { hCard: %p", (void*)hCard);

	SCARDAPI_STUB_CALL_LONG(status, SCardStatusW, hCard, mszReaderNames, pcchReaderLen, pdwState,
	                        pdwProtocol, pbAtr, pcbAtrLen);

	WLog_Print(g_Log, g_LogLevel, "SCardStatusW } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardTransmit(SCARDHANDLE hCard, LPCSCARD_IO_REQUEST pioSendPci,
                                         LPCBYTE pbSendBuffer, DWORD cbSendLength,
                                         LPSCARD_IO_REQUEST pioRecvPci, LPBYTE pbRecvBuffer,
                                         LPDWORD pcbRecvLength)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardTransmit { hCard: %p", (void*)hCard);

	SCARDAPI_STUB_CALL_LONG(status, SCardTransmit, hCard, pioSendPci, pbSendBuffer, cbSendLength,
	                        pioRecvPci, pbRecvBuffer, pcbRecvLength);

	WLog_Print(g_Log, g_LogLevel, "SCardTransmit } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardGetTransmitCount(SCARDHANDLE hCard, LPDWORD pcTransmitCount)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardGetTransmitCount { hCard: %p", (void*)hCard);

	SCARDAPI_STUB_CALL_LONG(status, SCardGetTransmitCount, hCard, pcTransmitCount);

	WLog_Print(g_Log, g_LogLevel, "SCardGetTransmitCount } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardControl(SCARDHANDLE hCard, DWORD dwControlCode, LPCVOID lpInBuffer,
                                        DWORD cbInBufferSize, LPVOID lpOutBuffer,
                                        DWORD cbOutBufferSize, LPDWORD lpBytesReturned)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardControl { hCard: %p", (void*)hCard);

	SCARDAPI_STUB_CALL_LONG(status, SCardControl, hCard, dwControlCode, lpInBuffer, cbInBufferSize,
	                        lpOutBuffer, cbOutBufferSize, lpBytesReturned);

	WLog_Print(g_Log, g_LogLevel, "SCardControl } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardGetAttrib(SCARDHANDLE hCard, DWORD dwAttrId, LPBYTE pbAttr,
                                          LPDWORD pcbAttrLen)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardGetAttrib { hCard: %p", (void*)hCard);

	SCARDAPI_STUB_CALL_LONG(status, SCardGetAttrib, hCard, dwAttrId, pbAttr, pcbAttrLen);

	WLog_Print(g_Log, g_LogLevel, "SCardGetAttrib } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardSetAttrib(SCARDHANDLE hCard, DWORD dwAttrId, LPCBYTE pbAttr,
                                          DWORD cbAttrLen)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardSetAttrib { hCard: %p", (void*)hCard);

	SCARDAPI_STUB_CALL_LONG(status, SCardSetAttrib, hCard, dwAttrId, pbAttr, cbAttrLen);

	WLog_Print(g_Log, g_LogLevel, "SCardSetAttrib } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardUIDlgSelectCardA(LPOPENCARDNAMEA_EX pDlgStruc)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardUIDlgSelectCardA {");

	SCARDAPI_STUB_CALL_LONG(status, SCardUIDlgSelectCardA, pDlgStruc);

	WLog_Print(g_Log, g_LogLevel, "SCardUIDlgSelectCardA } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardUIDlgSelectCardW(LPOPENCARDNAMEW_EX pDlgStruc)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardUIDlgSelectCardW {");

	SCARDAPI_STUB_CALL_LONG(status, SCardUIDlgSelectCardW, pDlgStruc);

	WLog_Print(g_Log, g_LogLevel, "SCardUIDlgSelectCardW } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_GetOpenCardNameA(LPOPENCARDNAMEA pDlgStruc)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "GetOpenCardNameA {");

	SCARDAPI_STUB_CALL_LONG(status, GetOpenCardNameA, pDlgStruc);

	WLog_Print(g_Log, g_LogLevel, "GetOpenCardNameA } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_GetOpenCardNameW(LPOPENCARDNAMEW pDlgStruc)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "GetOpenCardNameW {");

	SCARDAPI_STUB_CALL_LONG(status, GetOpenCardNameW, pDlgStruc);

	WLog_Print(g_Log, g_LogLevel, "GetOpenCardNameW } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardDlgExtendedError(void)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardDlgExtendedError {");

	SCARDAPI_STUB_CALL_LONG(status, SCardDlgExtendedError);

	WLog_Print(g_Log, g_LogLevel, "SCardDlgExtendedError } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardReadCacheA(SCARDCONTEXT hContext, UUID* CardIdentifier,
                                           DWORD FreshnessCounter, LPSTR LookupName, PBYTE Data,
                                           DWORD* DataLen)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardReadCacheA { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardReadCacheA, hContext, CardIdentifier, FreshnessCounter,
	                        LookupName, Data, DataLen);

	WLog_Print(g_Log, g_LogLevel, "SCardReadCacheA } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardReadCacheW(SCARDCONTEXT hContext, UUID* CardIdentifier,
                                           DWORD FreshnessCounter, LPWSTR LookupName, PBYTE Data,
                                           DWORD* DataLen)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardReadCacheW { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardReadCacheW, hContext, CardIdentifier, FreshnessCounter,
	                        LookupName, Data, DataLen);

	WLog_Print(g_Log, g_LogLevel, "SCardReadCacheW } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardWriteCacheA(SCARDCONTEXT hContext, UUID* CardIdentifier,
                                            DWORD FreshnessCounter, LPSTR LookupName, PBYTE Data,
                                            DWORD DataLen)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardWriteCacheA { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardWriteCacheA, hContext, CardIdentifier, FreshnessCounter,
	                        LookupName, Data, DataLen);

	WLog_Print(g_Log, g_LogLevel, "SCardWriteCacheA } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardWriteCacheW(SCARDCONTEXT hContext, UUID* CardIdentifier,
                                            DWORD FreshnessCounter, LPWSTR LookupName, PBYTE Data,
                                            DWORD DataLen)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardWriteCacheW { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardWriteCacheW, hContext, CardIdentifier, FreshnessCounter,
	                        LookupName, Data, DataLen);

	WLog_Print(g_Log, g_LogLevel, "SCardWriteCacheW } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardGetReaderIconA(SCARDCONTEXT hContext, LPCSTR szReaderName,
                                               LPBYTE pbIcon, LPDWORD pcbIcon)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardGetReaderIconA { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardGetReaderIconA, hContext, szReaderName, pbIcon, pcbIcon);

	WLog_Print(g_Log, g_LogLevel, "SCardGetReaderIconA } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardGetReaderIconW(SCARDCONTEXT hContext, LPCWSTR szReaderName,
                                               LPBYTE pbIcon, LPDWORD pcbIcon)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardGetReaderIconW { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardGetReaderIconW, hContext, szReaderName, pbIcon, pcbIcon);

	WLog_Print(g_Log, g_LogLevel, "SCardGetReaderIconW } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardGetDeviceTypeIdA(SCARDCONTEXT hContext, LPCSTR szReaderName,
                                                 LPDWORD pdwDeviceTypeId)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardGetDeviceTypeIdA { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardGetDeviceTypeIdA, hContext, szReaderName, pdwDeviceTypeId);

	WLog_Print(g_Log, g_LogLevel, "SCardGetDeviceTypeIdA } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardGetDeviceTypeIdW(SCARDCONTEXT hContext, LPCWSTR szReaderName,
                                                 LPDWORD pdwDeviceTypeId)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardGetDeviceTypeIdW { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardGetDeviceTypeIdW, hContext, szReaderName, pdwDeviceTypeId);

	WLog_Print(g_Log, g_LogLevel, "SCardGetDeviceTypeIdW } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardGetReaderDeviceInstanceIdA(SCARDCONTEXT hContext,
                                                           LPCSTR szReaderName,
                                                           LPSTR szDeviceInstanceId,
                                                           LPDWORD pcchDeviceInstanceId)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardGetReaderDeviceInstanceIdA { hContext: %p",
	           (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardGetReaderDeviceInstanceIdA, hContext, szReaderName,
	                        szDeviceInstanceId, pcchDeviceInstanceId);

	WLog_Print(g_Log, g_LogLevel, "SCardGetReaderDeviceInstanceIdA } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardGetReaderDeviceInstanceIdW(SCARDCONTEXT hContext,
                                                           LPCWSTR szReaderName,
                                                           LPWSTR szDeviceInstanceId,
                                                           LPDWORD pcchDeviceInstanceId)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardGetReaderDeviceInstanceIdW { hContext: %p",
	           (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardGetReaderDeviceInstanceIdW, hContext, szReaderName,
	                        szDeviceInstanceId, pcchDeviceInstanceId);

	WLog_Print(g_Log, g_LogLevel, "SCardGetReaderDeviceInstanceIdW } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardListReadersWithDeviceInstanceIdA(SCARDCONTEXT hContext,
                                                                 LPCSTR szDeviceInstanceId,
                                                                 LPSTR mszReaders,
                                                                 LPDWORD pcchReaders)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardListReadersWithDeviceInstanceIdA { hContext: %p",
	           (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardListReadersWithDeviceInstanceIdA, hContext,
	                        szDeviceInstanceId, mszReaders, pcchReaders);

	WLog_Print(g_Log, g_LogLevel,
	           "SCardListReadersWithDeviceInstanceIdA } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardListReadersWithDeviceInstanceIdW(SCARDCONTEXT hContext,
                                                                 LPCWSTR szDeviceInstanceId,
                                                                 LPWSTR mszReaders,
                                                                 LPDWORD pcchReaders)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardListReadersWithDeviceInstanceIdW { hContext: %p",
	           (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardListReadersWithDeviceInstanceIdW, hContext,
	                        szDeviceInstanceId, mszReaders, pcchReaders);

	WLog_Print(g_Log, g_LogLevel,
	           "SCardListReadersWithDeviceInstanceIdW } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

static LONG WINAPI Inspect_SCardAudit(SCARDCONTEXT hContext, DWORD dwEvent)
{
	LONG status;

	WLog_Print(g_Log, g_LogLevel, "SCardAudit { hContext: %p", (void*)hContext);

	SCARDAPI_STUB_CALL_LONG(status, SCardAudit, hContext, dwEvent);

	WLog_Print(g_Log, g_LogLevel, "SCardAudit } status: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(status), status);

	return status;
}

/**
 * Extended API
 */

static const SCardApiFunctionTable Inspect_SCardApiFunctionTable = {
	0, /* dwVersion */
	0, /* dwFlags */

	Inspect_SCardEstablishContext,                 /* SCardEstablishContext */
	Inspect_SCardReleaseContext,                   /* SCardReleaseContext */
	Inspect_SCardIsValidContext,                   /* SCardIsValidContext */
	Inspect_SCardListReaderGroupsA,                /* SCardListReaderGroupsA */
	Inspect_SCardListReaderGroupsW,                /* SCardListReaderGroupsW */
	Inspect_SCardListReadersA,                     /* SCardListReadersA */
	Inspect_SCardListReadersW,                     /* SCardListReadersW */
	Inspect_SCardListCardsA,                       /* SCardListCardsA */
	Inspect_SCardListCardsW,                       /* SCardListCardsW */
	Inspect_SCardListInterfacesA,                  /* SCardListInterfacesA */
	Inspect_SCardListInterfacesW,                  /* SCardListInterfacesW */
	Inspect_SCardGetProviderIdA,                   /* SCardGetProviderIdA */
	Inspect_SCardGetProviderIdW,                   /* SCardGetProviderIdW */
	Inspect_SCardGetCardTypeProviderNameA,         /* SCardGetCardTypeProviderNameA */
	Inspect_SCardGetCardTypeProviderNameW,         /* SCardGetCardTypeProviderNameW */
	Inspect_SCardIntroduceReaderGroupA,            /* SCardIntroduceReaderGroupA */
	Inspect_SCardIntroduceReaderGroupW,            /* SCardIntroduceReaderGroupW */
	Inspect_SCardForgetReaderGroupA,               /* SCardForgetReaderGroupA */
	Inspect_SCardForgetReaderGroupW,               /* SCardForgetReaderGroupW */
	Inspect_SCardIntroduceReaderA,                 /* SCardIntroduceReaderA */
	Inspect_SCardIntroduceReaderW,                 /* SCardIntroduceReaderW */
	Inspect_SCardForgetReaderA,                    /* SCardForgetReaderA */
	Inspect_SCardForgetReaderW,                    /* SCardForgetReaderW */
	Inspect_SCardAddReaderToGroupA,                /* SCardAddReaderToGroupA */
	Inspect_SCardAddReaderToGroupW,                /* SCardAddReaderToGroupW */
	Inspect_SCardRemoveReaderFromGroupA,           /* SCardRemoveReaderFromGroupA */
	Inspect_SCardRemoveReaderFromGroupW,           /* SCardRemoveReaderFromGroupW */
	Inspect_SCardIntroduceCardTypeA,               /* SCardIntroduceCardTypeA */
	Inspect_SCardIntroduceCardTypeW,               /* SCardIntroduceCardTypeW */
	Inspect_SCardSetCardTypeProviderNameA,         /* SCardSetCardTypeProviderNameA */
	Inspect_SCardSetCardTypeProviderNameW,         /* SCardSetCardTypeProviderNameW */
	Inspect_SCardForgetCardTypeA,                  /* SCardForgetCardTypeA */
	Inspect_SCardForgetCardTypeW,                  /* SCardForgetCardTypeW */
	Inspect_SCardFreeMemory,                       /* SCardFreeMemory */
	Inspect_SCardAccessStartedEvent,               /* SCardAccessStartedEvent */
	Inspect_SCardReleaseStartedEvent,              /* SCardReleaseStartedEvent */
	Inspect_SCardLocateCardsA,                     /* SCardLocateCardsA */
	Inspect_SCardLocateCardsW,                     /* SCardLocateCardsW */
	Inspect_SCardLocateCardsByATRA,                /* SCardLocateCardsByATRA */
	Inspect_SCardLocateCardsByATRW,                /* SCardLocateCardsByATRW */
	Inspect_SCardGetStatusChangeA,                 /* SCardGetStatusChangeA */
	Inspect_SCardGetStatusChangeW,                 /* SCardGetStatusChangeW */
	Inspect_SCardCancel,                           /* SCardCancel */
	Inspect_SCardConnectA,                         /* SCardConnectA */
	Inspect_SCardConnectW,                         /* SCardConnectW */
	Inspect_SCardReconnect,                        /* SCardReconnect */
	Inspect_SCardDisconnect,                       /* SCardDisconnect */
	Inspect_SCardBeginTransaction,                 /* SCardBeginTransaction */
	Inspect_SCardEndTransaction,                   /* SCardEndTransaction */
	Inspect_SCardCancelTransaction,                /* SCardCancelTransaction */
	Inspect_SCardState,                            /* SCardState */
	Inspect_SCardStatusA,                          /* SCardStatusA */
	Inspect_SCardStatusW,                          /* SCardStatusW */
	Inspect_SCardTransmit,                         /* SCardTransmit */
	Inspect_SCardGetTransmitCount,                 /* SCardGetTransmitCount */
	Inspect_SCardControl,                          /* SCardControl */
	Inspect_SCardGetAttrib,                        /* SCardGetAttrib */
	Inspect_SCardSetAttrib,                        /* SCardSetAttrib */
	Inspect_SCardUIDlgSelectCardA,                 /* SCardUIDlgSelectCardA */
	Inspect_SCardUIDlgSelectCardW,                 /* SCardUIDlgSelectCardW */
	Inspect_GetOpenCardNameA,                      /* GetOpenCardNameA */
	Inspect_GetOpenCardNameW,                      /* GetOpenCardNameW */
	Inspect_SCardDlgExtendedError,                 /* SCardDlgExtendedError */
	Inspect_SCardReadCacheA,                       /* SCardReadCacheA */
	Inspect_SCardReadCacheW,                       /* SCardReadCacheW */
	Inspect_SCardWriteCacheA,                      /* SCardWriteCacheA */
	Inspect_SCardWriteCacheW,                      /* SCardWriteCacheW */
	Inspect_SCardGetReaderIconA,                   /* SCardGetReaderIconA */
	Inspect_SCardGetReaderIconW,                   /* SCardGetReaderIconW */
	Inspect_SCardGetDeviceTypeIdA,                 /* SCardGetDeviceTypeIdA */
	Inspect_SCardGetDeviceTypeIdW,                 /* SCardGetDeviceTypeIdW */
	Inspect_SCardGetReaderDeviceInstanceIdA,       /* SCardGetReaderDeviceInstanceIdA */
	Inspect_SCardGetReaderDeviceInstanceIdW,       /* SCardGetReaderDeviceInstanceIdW */
	Inspect_SCardListReadersWithDeviceInstanceIdA, /* SCardListReadersWithDeviceInstanceIdA */
	Inspect_SCardListReadersWithDeviceInstanceIdW, /* SCardListReadersWithDeviceInstanceIdW */
	Inspect_SCardAudit                             /* SCardAudit */
};

static void Inspect_InitLog(void)
{
	if (g_Log)
		return;

	if (!(g_Log = WLog_Get("WinSCard")))
		return;
}

const SCardApiFunctionTable* Inspect_RegisterSCardApi(const SCardApiFunctionTable* pSCardApi)
{
	g_SCardApi = pSCardApi;

	Inspect_InitLog();

	return &Inspect_SCardApiFunctionTable;
}
