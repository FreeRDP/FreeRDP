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

#include <winpr/config.h>

#include <winpr/crt.h>
#include <winpr/library.h>
#include <winpr/smartcard.h>

#include "smartcard_windows.h"

static HMODULE g_WinSCardModule = nullptr;

static SCardApiFunctionTable Windows_SCardApiFunctionTable = {
	0, /* dwVersion */
	0, /* dwFlags */

	nullptr, /* SCardEstablishContext */
	nullptr, /* SCardReleaseContext */
	nullptr, /* SCardIsValidContext */
	nullptr, /* SCardListReaderGroupsA */
	nullptr, /* SCardListReaderGroupsW */
	nullptr, /* SCardListReadersA */
	nullptr, /* SCardListReadersW */
	nullptr, /* SCardListCardsA */
	nullptr, /* SCardListCardsW */
	nullptr, /* SCardListInterfacesA */
	nullptr, /* SCardListInterfacesW */
	nullptr, /* SCardGetProviderIdA */
	nullptr, /* SCardGetProviderIdW */
	nullptr, /* SCardGetCardTypeProviderNameA */
	nullptr, /* SCardGetCardTypeProviderNameW */
	nullptr, /* SCardIntroduceReaderGroupA */
	nullptr, /* SCardIntroduceReaderGroupW */
	nullptr, /* SCardForgetReaderGroupA */
	nullptr, /* SCardForgetReaderGroupW */
	nullptr, /* SCardIntroduceReaderA */
	nullptr, /* SCardIntroduceReaderW */
	nullptr, /* SCardForgetReaderA */
	nullptr, /* SCardForgetReaderW */
	nullptr, /* SCardAddReaderToGroupA */
	nullptr, /* SCardAddReaderToGroupW */
	nullptr, /* SCardRemoveReaderFromGroupA */
	nullptr, /* SCardRemoveReaderFromGroupW */
	nullptr, /* SCardIntroduceCardTypeA */
	nullptr, /* SCardIntroduceCardTypeW */
	nullptr, /* SCardSetCardTypeProviderNameA */
	nullptr, /* SCardSetCardTypeProviderNameW */
	nullptr, /* SCardForgetCardTypeA */
	nullptr, /* SCardForgetCardTypeW */
	nullptr, /* SCardFreeMemory */
	nullptr, /* SCardAccessStartedEvent */
	nullptr, /* SCardReleaseStartedEvent */
	nullptr, /* SCardLocateCardsA */
	nullptr, /* SCardLocateCardsW */
	nullptr, /* SCardLocateCardsByATRA */
	nullptr, /* SCardLocateCardsByATRW */
	nullptr, /* SCardGetStatusChangeA */
	nullptr, /* SCardGetStatusChangeW */
	nullptr, /* SCardCancel */
	nullptr, /* SCardConnectA */
	nullptr, /* SCardConnectW */
	nullptr, /* SCardReconnect */
	nullptr, /* SCardDisconnect */
	nullptr, /* SCardBeginTransaction */
	nullptr, /* SCardEndTransaction */
	nullptr, /* SCardCancelTransaction */
	nullptr, /* SCardState */
	nullptr, /* SCardStatusA */
	nullptr, /* SCardStatusW */
	nullptr, /* SCardTransmit */
	nullptr, /* SCardGetTransmitCount */
	nullptr, /* SCardControl */
	nullptr, /* SCardGetAttrib */
	nullptr, /* SCardSetAttrib */
	nullptr, /* SCardUIDlgSelectCardA */
	nullptr, /* SCardUIDlgSelectCardW */
	nullptr, /* GetOpenCardNameA */
	nullptr, /* GetOpenCardNameW */
	nullptr, /* SCardDlgExtendedError */
	nullptr, /* SCardReadCacheA */
	nullptr, /* SCardReadCacheW */
	nullptr, /* SCardWriteCacheA */
	nullptr, /* SCardWriteCacheW */
	nullptr, /* SCardGetReaderIconA */
	nullptr, /* SCardGetReaderIconW */
	nullptr, /* SCardGetDeviceTypeIdA */
	nullptr, /* SCardGetDeviceTypeIdW */
	nullptr, /* SCardGetReaderDeviceInstanceIdA */
	nullptr, /* SCardGetReaderDeviceInstanceIdW */
	nullptr, /* SCardListReadersWithDeviceInstanceIdA */
	nullptr, /* SCardListReadersWithDeviceInstanceIdW */
	nullptr  /* SCardAudit */
};

const SCardApiFunctionTable* Windows_GetSCardApiFunctionTable(void)
{
	return &Windows_SCardApiFunctionTable;
}

int Windows_InitializeSCardApi(void)
{
	g_WinSCardModule = LoadLibraryA("WinSCard.dll");

	if (!g_WinSCardModule)
		return -1;

	WinSCard_LoadApiTableFunctions(&Windows_SCardApiFunctionTable, g_WinSCardModule);
	return 1;
}
