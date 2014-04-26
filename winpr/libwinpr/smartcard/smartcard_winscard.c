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

#ifdef _WIN32

#include <winpr/crt.h>
#include <winpr/library.h>
#include <winpr/smartcard.h>

#include "smartcard_winscard.h"

static HMODULE g_WinSCardModule = NULL;

SCardApiFunctionTable WinSCard_SCardApiFunctionTable =
{
	0, /* dwVersion */
	0, /* dwFlags */

	NULL, /* SCardEstablishContext */
	NULL, /* SCardReleaseContext */
	NULL, /* SCardIsValidContext */
	NULL, /* SCardListReaderGroupsA */
	NULL, /* SCardListReaderGroupsW */
	NULL, /* SCardListReadersA */
	NULL, /* SCardListReadersW */
	NULL, /* SCardListCardsA */
	NULL, /* SCardListCardsW */
	NULL, /* SCardListInterfacesA */
	NULL, /* SCardListInterfacesW */
	NULL, /* SCardGetProviderIdA */
	NULL, /* SCardGetProviderIdW */
	NULL, /* SCardGetCardTypeProviderNameA */
	NULL, /* SCardGetCardTypeProviderNameW */
	NULL, /* SCardIntroduceReaderGroupA */
	NULL, /* SCardIntroduceReaderGroupW */
	NULL, /* SCardForgetReaderGroupA */
	NULL, /* SCardForgetReaderGroupW */
	NULL, /* SCardIntroduceReaderA */
	NULL, /* SCardIntroduceReaderW */
	NULL, /* SCardForgetReaderA */
	NULL, /* SCardForgetReaderW */
	NULL, /* SCardAddReaderToGroupA */
	NULL, /* SCardAddReaderToGroupW */
	NULL, /* SCardRemoveReaderFromGroupA */
	NULL, /* SCardRemoveReaderFromGroupW */
	NULL, /* SCardIntroduceCardTypeA */
	NULL, /* SCardIntroduceCardTypeW */
	NULL, /* SCardSetCardTypeProviderNameA */
	NULL, /* SCardSetCardTypeProviderNameW */
	NULL, /* SCardForgetCardTypeA */
	NULL, /* SCardForgetCardTypeW */
	NULL, /* SCardFreeMemory */
	NULL, /* SCardAccessStartedEvent */
	NULL, /* SCardReleaseStartedEvent */
	NULL, /* SCardLocateCardsA */
	NULL, /* SCardLocateCardsW */
	NULL, /* SCardLocateCardsByATRA */
	NULL, /* SCardLocateCardsByATRW */
	NULL, /* SCardGetStatusChangeA */
	NULL, /* SCardGetStatusChangeW */
	NULL, /* SCardCancel */
	NULL, /* SCardConnectA */
	NULL, /* SCardConnectW */
	NULL, /* SCardReconnect */
	NULL, /* SCardDisconnect */
	NULL, /* SCardBeginTransaction */
	NULL, /* SCardEndTransaction */
	NULL, /* SCardCancelTransaction */
	NULL, /* SCardState */
	NULL, /* SCardStatusA */
	NULL, /* SCardStatusW */
	NULL, /* SCardTransmit */
	NULL, /* SCardGetTransmitCount */
	NULL, /* SCardControl */
	NULL, /* SCardGetAttrib */
	NULL, /* SCardSetAttrib */
	NULL, /* SCardUIDlgSelectCardA */
	NULL, /* SCardUIDlgSelectCardW */
	NULL, /* GetOpenCardNameA */
	NULL, /* GetOpenCardNameW */
	NULL, /* SCardDlgExtendedError */
	NULL, /* SCardReadCacheA */
	NULL, /* SCardReadCacheW */
	NULL, /* SCardWriteCacheA */
	NULL, /* SCardWriteCacheW */
	NULL, /* SCardGetReaderIconA */
	NULL, /* SCardGetReaderIconW */
	NULL, /* SCardGetDeviceTypeIdA */
	NULL, /* SCardGetDeviceTypeIdW */
	NULL, /* SCardGetReaderDeviceInstanceIdA */
	NULL, /* SCardGetReaderDeviceInstanceIdW */
	NULL, /* SCardListReadersWithDeviceInstanceIdA */
	NULL, /* SCardListReadersWithDeviceInstanceIdW */
	NULL /* SCardAudit */
};

PSCardApiFunctionTable WinSCard_GetSCardApiFunctionTable(void)
{
	return &WinSCard_SCardApiFunctionTable;
}

int WinSCard_InitializeSCardApi(void)
{
	g_WinSCardModule = LoadLibraryA("winscard.dll");

	if (!g_WinSCardModule)
		return -1;

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

	return 1;
}

#endif
