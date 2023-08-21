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

static HMODULE g_WinSCardModule = NULL;

static SCardApiFunctionTable Windows_SCardApiFunctionTable = {
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
	NULL  /* SCardAudit */
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
