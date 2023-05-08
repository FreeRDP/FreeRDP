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

#ifndef WINPR_SMARTCARD_PRIVATE_H
#define WINPR_SMARTCARD_PRIVATE_H

#include <winpr/smartcard.h>

/**
 * Extended API
 */

typedef LONG(WINAPI* fnSCardEstablishContext)(DWORD dwScope, LPCVOID pvReserved1,
                                              LPCVOID pvReserved2, LPSCARDCONTEXT phContext);

typedef LONG(WINAPI* fnSCardReleaseContext)(SCARDCONTEXT hContext);

typedef LONG(WINAPI* fnSCardIsValidContext)(SCARDCONTEXT hContext);

typedef LONG(WINAPI* fnSCardListReaderGroupsA)(SCARDCONTEXT hContext, LPSTR mszGroups,
                                               LPDWORD pcchGroups);
typedef LONG(WINAPI* fnSCardListReaderGroupsW)(SCARDCONTEXT hContext, LPWSTR mszGroups,
                                               LPDWORD pcchGroups);

typedef LONG(WINAPI* fnSCardListReadersA)(SCARDCONTEXT hContext, LPCSTR mszGroups, LPSTR mszReaders,
                                          LPDWORD pcchReaders);
typedef LONG(WINAPI* fnSCardListReadersW)(SCARDCONTEXT hContext, LPCWSTR mszGroups,
                                          LPWSTR mszReaders, LPDWORD pcchReaders);

typedef LONG(WINAPI* fnSCardListCardsA)(SCARDCONTEXT hContext, LPCBYTE pbAtr,
                                        LPCGUID rgquidInterfaces, DWORD cguidInterfaceCount,
                                        CHAR* mszCards, LPDWORD pcchCards);

typedef LONG(WINAPI* fnSCardListCardsW)(SCARDCONTEXT hContext, LPCBYTE pbAtr,
                                        LPCGUID rgquidInterfaces, DWORD cguidInterfaceCount,
                                        WCHAR* mszCards, LPDWORD pcchCards);

typedef LONG(WINAPI* fnSCardListInterfacesA)(SCARDCONTEXT hContext, LPCSTR szCard,
                                             LPGUID pguidInterfaces, LPDWORD pcguidInterfaces);
typedef LONG(WINAPI* fnSCardListInterfacesW)(SCARDCONTEXT hContext, LPCWSTR szCard,
                                             LPGUID pguidInterfaces, LPDWORD pcguidInterfaces);

typedef LONG(WINAPI* fnSCardGetProviderIdA)(SCARDCONTEXT hContext, LPCSTR szCard,
                                            LPGUID pguidProviderId);
typedef LONG(WINAPI* fnSCardGetProviderIdW)(SCARDCONTEXT hContext, LPCWSTR szCard,
                                            LPGUID pguidProviderId);

typedef LONG(WINAPI* fnSCardGetCardTypeProviderNameA)(SCARDCONTEXT hContext, LPCSTR szCardName,
                                                      DWORD dwProviderId, CHAR* szProvider,
                                                      LPDWORD pcchProvider);
typedef LONG(WINAPI* fnSCardGetCardTypeProviderNameW)(SCARDCONTEXT hContext, LPCWSTR szCardName,
                                                      DWORD dwProviderId, WCHAR* szProvider,
                                                      LPDWORD pcchProvider);

typedef LONG(WINAPI* fnSCardIntroduceReaderGroupA)(SCARDCONTEXT hContext, LPCSTR szGroupName);
typedef LONG(WINAPI* fnSCardIntroduceReaderGroupW)(SCARDCONTEXT hContext, LPCWSTR szGroupName);

typedef LONG(WINAPI* fnSCardForgetReaderGroupA)(SCARDCONTEXT hContext, LPCSTR szGroupName);
typedef LONG(WINAPI* fnSCardForgetReaderGroupW)(SCARDCONTEXT hContext, LPCWSTR szGroupName);

typedef LONG(WINAPI* fnSCardIntroduceReaderA)(SCARDCONTEXT hContext, LPCSTR szReaderName,
                                              LPCSTR szDeviceName);
typedef LONG(WINAPI* fnSCardIntroduceReaderW)(SCARDCONTEXT hContext, LPCWSTR szReaderName,
                                              LPCWSTR szDeviceName);

typedef LONG(WINAPI* fnSCardForgetReaderA)(SCARDCONTEXT hContext, LPCSTR szReaderName);
typedef LONG(WINAPI* fnSCardForgetReaderW)(SCARDCONTEXT hContext, LPCWSTR szReaderName);

typedef LONG(WINAPI* fnSCardAddReaderToGroupA)(SCARDCONTEXT hContext, LPCSTR szReaderName,
                                               LPCSTR szGroupName);
typedef LONG(WINAPI* fnSCardAddReaderToGroupW)(SCARDCONTEXT hContext, LPCWSTR szReaderName,
                                               LPCWSTR szGroupName);

typedef LONG(WINAPI* fnSCardRemoveReaderFromGroupA)(SCARDCONTEXT hContext, LPCSTR szReaderName,
                                                    LPCSTR szGroupName);
typedef LONG(WINAPI* fnSCardRemoveReaderFromGroupW)(SCARDCONTEXT hContext, LPCWSTR szReaderName,
                                                    LPCWSTR szGroupName);

typedef LONG(WINAPI* fnSCardIntroduceCardTypeA)(SCARDCONTEXT hContext, LPCSTR szCardName,
                                                LPCGUID pguidPrimaryProvider,
                                                LPCGUID rgguidInterfaces, DWORD dwInterfaceCount,
                                                LPCBYTE pbAtr, LPCBYTE pbAtrMask, DWORD cbAtrLen);
typedef LONG(WINAPI* fnSCardIntroduceCardTypeW)(SCARDCONTEXT hContext, LPCWSTR szCardName,
                                                LPCGUID pguidPrimaryProvider,
                                                LPCGUID rgguidInterfaces, DWORD dwInterfaceCount,
                                                LPCBYTE pbAtr, LPCBYTE pbAtrMask, DWORD cbAtrLen);

typedef LONG(WINAPI* fnSCardSetCardTypeProviderNameA)(SCARDCONTEXT hContext, LPCSTR szCardName,
                                                      DWORD dwProviderId, LPCSTR szProvider);
typedef LONG(WINAPI* fnSCardSetCardTypeProviderNameW)(SCARDCONTEXT hContext, LPCWSTR szCardName,
                                                      DWORD dwProviderId, LPCWSTR szProvider);

typedef LONG(WINAPI* fnSCardForgetCardTypeA)(SCARDCONTEXT hContext, LPCSTR szCardName);
typedef LONG(WINAPI* fnSCardForgetCardTypeW)(SCARDCONTEXT hContext, LPCWSTR szCardName);

typedef LONG(WINAPI* fnSCardFreeMemory)(SCARDCONTEXT hContext, LPVOID pvMem);

typedef HANDLE(WINAPI* fnSCardAccessStartedEvent)(void);

typedef void(WINAPI* fnSCardReleaseStartedEvent)(void);

typedef LONG(WINAPI* fnSCardLocateCardsA)(SCARDCONTEXT hContext, LPCSTR mszCards,
                                          LPSCARD_READERSTATEA rgReaderStates, DWORD cReaders);
typedef LONG(WINAPI* fnSCardLocateCardsW)(SCARDCONTEXT hContext, LPCWSTR mszCards,
                                          LPSCARD_READERSTATEW rgReaderStates, DWORD cReaders);

typedef LONG(WINAPI* fnSCardLocateCardsByATRA)(SCARDCONTEXT hContext, LPSCARD_ATRMASK rgAtrMasks,
                                               DWORD cAtrs, LPSCARD_READERSTATEA rgReaderStates,
                                               DWORD cReaders);
typedef LONG(WINAPI* fnSCardLocateCardsByATRW)(SCARDCONTEXT hContext, LPSCARD_ATRMASK rgAtrMasks,
                                               DWORD cAtrs, LPSCARD_READERSTATEW rgReaderStates,
                                               DWORD cReaders);

typedef LONG(WINAPI* fnSCardGetStatusChangeA)(SCARDCONTEXT hContext, DWORD dwTimeout,
                                              LPSCARD_READERSTATEA rgReaderStates, DWORD cReaders);
typedef LONG(WINAPI* fnSCardGetStatusChangeW)(SCARDCONTEXT hContext, DWORD dwTimeout,
                                              LPSCARD_READERSTATEW rgReaderStates, DWORD cReaders);

typedef LONG(WINAPI* fnSCardCancel)(SCARDCONTEXT hContext);

typedef LONG(WINAPI* fnSCardConnectA)(SCARDCONTEXT hContext, LPCSTR szReader, DWORD dwShareMode,
                                      DWORD dwPreferredProtocols, LPSCARDHANDLE phCard,
                                      LPDWORD pdwActiveProtocol);
typedef LONG(WINAPI* fnSCardConnectW)(SCARDCONTEXT hContext, LPCWSTR szReader, DWORD dwShareMode,
                                      DWORD dwPreferredProtocols, LPSCARDHANDLE phCard,
                                      LPDWORD pdwActiveProtocol);

typedef LONG(WINAPI* fnSCardReconnect)(SCARDHANDLE hCard, DWORD dwShareMode,
                                       DWORD dwPreferredProtocols, DWORD dwInitialization,
                                       LPDWORD pdwActiveProtocol);

typedef LONG(WINAPI* fnSCardDisconnect)(SCARDHANDLE hCard, DWORD dwDisposition);

typedef LONG(WINAPI* fnSCardBeginTransaction)(SCARDHANDLE hCard);

typedef LONG(WINAPI* fnSCardEndTransaction)(SCARDHANDLE hCard, DWORD dwDisposition);

typedef LONG(WINAPI* fnSCardCancelTransaction)(SCARDHANDLE hCard);

typedef LONG(WINAPI* fnSCardState)(SCARDHANDLE hCard, LPDWORD pdwState, LPDWORD pdwProtocol,
                                   LPBYTE pbAtr, LPDWORD pcbAtrLen);

typedef LONG(WINAPI* fnSCardStatusA)(SCARDHANDLE hCard, LPSTR mszReaderNames, LPDWORD pcchReaderLen,
                                     LPDWORD pdwState, LPDWORD pdwProtocol, LPBYTE pbAtr,
                                     LPDWORD pcbAtrLen);
typedef LONG(WINAPI* fnSCardStatusW)(SCARDHANDLE hCard, LPWSTR mszReaderNames,
                                     LPDWORD pcchReaderLen, LPDWORD pdwState, LPDWORD pdwProtocol,
                                     LPBYTE pbAtr, LPDWORD pcbAtrLen);

typedef LONG(WINAPI* fnSCardTransmit)(SCARDHANDLE hCard, LPCSCARD_IO_REQUEST pioSendPci,
                                      LPCBYTE pbSendBuffer, DWORD cbSendLength,
                                      LPSCARD_IO_REQUEST pioRecvPci, LPBYTE pbRecvBuffer,
                                      LPDWORD pcbRecvLength);

typedef LONG(WINAPI* fnSCardGetTransmitCount)(SCARDHANDLE hCard, LPDWORD pcTransmitCount);

typedef LONG(WINAPI* fnSCardControl)(SCARDHANDLE hCard, DWORD dwControlCode, LPCVOID lpInBuffer,
                                     DWORD cbInBufferSize, LPVOID lpOutBuffer,
                                     DWORD cbOutBufferSize, LPDWORD lpBytesReturned);

typedef LONG(WINAPI* fnSCardGetAttrib)(SCARDHANDLE hCard, DWORD dwAttrId, LPBYTE pbAttr,
                                       LPDWORD pcbAttrLen);

typedef LONG(WINAPI* fnSCardSetAttrib)(SCARDHANDLE hCard, DWORD dwAttrId, LPCBYTE pbAttr,
                                       DWORD cbAttrLen);

typedef LONG(WINAPI* fnSCardUIDlgSelectCardA)(LPOPENCARDNAMEA_EX pDlgStruc);
typedef LONG(WINAPI* fnSCardUIDlgSelectCardW)(LPOPENCARDNAMEW_EX pDlgStruc);

typedef LONG(WINAPI* fnGetOpenCardNameA)(LPOPENCARDNAMEA pDlgStruc);
typedef LONG(WINAPI* fnGetOpenCardNameW)(LPOPENCARDNAMEW pDlgStruc);

typedef LONG(WINAPI* fnSCardDlgExtendedError)(void);

typedef LONG(WINAPI* fnSCardReadCacheA)(SCARDCONTEXT hContext, UUID* CardIdentifier,
                                        DWORD FreshnessCounter, LPSTR LookupName, PBYTE Data,
                                        DWORD* DataLen);
typedef LONG(WINAPI* fnSCardReadCacheW)(SCARDCONTEXT hContext, UUID* CardIdentifier,
                                        DWORD FreshnessCounter, LPWSTR LookupName, PBYTE Data,
                                        DWORD* DataLen);

typedef LONG(WINAPI* fnSCardWriteCacheA)(SCARDCONTEXT hContext, UUID* CardIdentifier,
                                         DWORD FreshnessCounter, LPSTR LookupName, PBYTE Data,
                                         DWORD DataLen);
typedef LONG(WINAPI* fnSCardWriteCacheW)(SCARDCONTEXT hContext, UUID* CardIdentifier,
                                         DWORD FreshnessCounter, LPWSTR LookupName, PBYTE Data,
                                         DWORD DataLen);

typedef LONG(WINAPI* fnSCardGetReaderIconA)(SCARDCONTEXT hContext, LPCSTR szReaderName,
                                            LPBYTE pbIcon, LPDWORD pcbIcon);
typedef LONG(WINAPI* fnSCardGetReaderIconW)(SCARDCONTEXT hContext, LPCWSTR szReaderName,
                                            LPBYTE pbIcon, LPDWORD pcbIcon);

typedef LONG(WINAPI* fnSCardGetDeviceTypeIdA)(SCARDCONTEXT hContext, LPCSTR szReaderName,
                                              LPDWORD pdwDeviceTypeId);
typedef LONG(WINAPI* fnSCardGetDeviceTypeIdW)(SCARDCONTEXT hContext, LPCWSTR szReaderName,
                                              LPDWORD pdwDeviceTypeId);

typedef LONG(WINAPI* fnSCardGetReaderDeviceInstanceIdA)(SCARDCONTEXT hContext, LPCSTR szReaderName,
                                                        LPSTR szDeviceInstanceId,
                                                        LPDWORD pcchDeviceInstanceId);
typedef LONG(WINAPI* fnSCardGetReaderDeviceInstanceIdW)(SCARDCONTEXT hContext, LPCWSTR szReaderName,
                                                        LPWSTR szDeviceInstanceId,
                                                        LPDWORD pcchDeviceInstanceId);

typedef LONG(WINAPI* fnSCardListReadersWithDeviceInstanceIdA)(SCARDCONTEXT hContext,
                                                              LPCSTR szDeviceInstanceId,
                                                              LPSTR mszReaders,
                                                              LPDWORD pcchReaders);
typedef LONG(WINAPI* fnSCardListReadersWithDeviceInstanceIdW)(SCARDCONTEXT hContext,
                                                              LPCWSTR szDeviceInstanceId,
                                                              LPWSTR mszReaders,
                                                              LPDWORD pcchReaders);

typedef LONG(WINAPI* fnSCardAudit)(SCARDCONTEXT hContext, DWORD dwEvent);

typedef struct
{
	DWORD dwVersion;
	DWORD dwFlags;

	fnSCardEstablishContext pfnSCardEstablishContext;
	fnSCardReleaseContext pfnSCardReleaseContext;
	fnSCardIsValidContext pfnSCardIsValidContext;
	fnSCardListReaderGroupsA pfnSCardListReaderGroupsA;
	fnSCardListReaderGroupsW pfnSCardListReaderGroupsW;
	fnSCardListReadersA pfnSCardListReadersA;
	fnSCardListReadersW pfnSCardListReadersW;
	fnSCardListCardsA pfnSCardListCardsA;
	fnSCardListCardsW pfnSCardListCardsW;
	fnSCardListInterfacesA pfnSCardListInterfacesA;
	fnSCardListInterfacesW pfnSCardListInterfacesW;
	fnSCardGetProviderIdA pfnSCardGetProviderIdA;
	fnSCardGetProviderIdW pfnSCardGetProviderIdW;
	fnSCardGetCardTypeProviderNameA pfnSCardGetCardTypeProviderNameA;
	fnSCardGetCardTypeProviderNameW pfnSCardGetCardTypeProviderNameW;
	fnSCardIntroduceReaderGroupA pfnSCardIntroduceReaderGroupA;
	fnSCardIntroduceReaderGroupW pfnSCardIntroduceReaderGroupW;
	fnSCardForgetReaderGroupA pfnSCardForgetReaderGroupA;
	fnSCardForgetReaderGroupW pfnSCardForgetReaderGroupW;
	fnSCardIntroduceReaderA pfnSCardIntroduceReaderA;
	fnSCardIntroduceReaderW pfnSCardIntroduceReaderW;
	fnSCardForgetReaderA pfnSCardForgetReaderA;
	fnSCardForgetReaderW pfnSCardForgetReaderW;
	fnSCardAddReaderToGroupA pfnSCardAddReaderToGroupA;
	fnSCardAddReaderToGroupW pfnSCardAddReaderToGroupW;
	fnSCardRemoveReaderFromGroupA pfnSCardRemoveReaderFromGroupA;
	fnSCardRemoveReaderFromGroupW pfnSCardRemoveReaderFromGroupW;
	fnSCardIntroduceCardTypeA pfnSCardIntroduceCardTypeA;
	fnSCardIntroduceCardTypeW pfnSCardIntroduceCardTypeW;
	fnSCardSetCardTypeProviderNameA pfnSCardSetCardTypeProviderNameA;
	fnSCardSetCardTypeProviderNameW pfnSCardSetCardTypeProviderNameW;
	fnSCardForgetCardTypeA pfnSCardForgetCardTypeA;
	fnSCardForgetCardTypeW pfnSCardForgetCardTypeW;
	fnSCardFreeMemory pfnSCardFreeMemory;
	fnSCardAccessStartedEvent pfnSCardAccessStartedEvent;
	fnSCardReleaseStartedEvent pfnSCardReleaseStartedEvent;
	fnSCardLocateCardsA pfnSCardLocateCardsA;
	fnSCardLocateCardsW pfnSCardLocateCardsW;
	fnSCardLocateCardsByATRA pfnSCardLocateCardsByATRA;
	fnSCardLocateCardsByATRW pfnSCardLocateCardsByATRW;
	fnSCardGetStatusChangeA pfnSCardGetStatusChangeA;
	fnSCardGetStatusChangeW pfnSCardGetStatusChangeW;
	fnSCardCancel pfnSCardCancel;
	fnSCardConnectA pfnSCardConnectA;
	fnSCardConnectW pfnSCardConnectW;
	fnSCardReconnect pfnSCardReconnect;
	fnSCardDisconnect pfnSCardDisconnect;
	fnSCardBeginTransaction pfnSCardBeginTransaction;
	fnSCardEndTransaction pfnSCardEndTransaction;
	fnSCardCancelTransaction pfnSCardCancelTransaction;
	fnSCardState pfnSCardState;
	fnSCardStatusA pfnSCardStatusA;
	fnSCardStatusW pfnSCardStatusW;
	fnSCardTransmit pfnSCardTransmit;
	fnSCardGetTransmitCount pfnSCardGetTransmitCount;
	fnSCardControl pfnSCardControl;
	fnSCardGetAttrib pfnSCardGetAttrib;
	fnSCardSetAttrib pfnSCardSetAttrib;
	fnSCardUIDlgSelectCardA pfnSCardUIDlgSelectCardA;
	fnSCardUIDlgSelectCardW pfnSCardUIDlgSelectCardW;
	fnGetOpenCardNameA pfnGetOpenCardNameA;
	fnGetOpenCardNameW pfnGetOpenCardNameW;
	fnSCardDlgExtendedError pfnSCardDlgExtendedError;
	fnSCardReadCacheA pfnSCardReadCacheA;
	fnSCardReadCacheW pfnSCardReadCacheW;
	fnSCardWriteCacheA pfnSCardWriteCacheA;
	fnSCardWriteCacheW pfnSCardWriteCacheW;
	fnSCardGetReaderIconA pfnSCardGetReaderIconA;
	fnSCardGetReaderIconW pfnSCardGetReaderIconW;
	fnSCardGetDeviceTypeIdA pfnSCardGetDeviceTypeIdA;
	fnSCardGetDeviceTypeIdW pfnSCardGetDeviceTypeIdW;
	fnSCardGetReaderDeviceInstanceIdA pfnSCardGetReaderDeviceInstanceIdA;
	fnSCardGetReaderDeviceInstanceIdW pfnSCardGetReaderDeviceInstanceIdW;
	fnSCardListReadersWithDeviceInstanceIdA pfnSCardListReadersWithDeviceInstanceIdA;
	fnSCardListReadersWithDeviceInstanceIdW pfnSCardListReadersWithDeviceInstanceIdW;
	fnSCardAudit pfnSCardAudit;
} SCardApiFunctionTable;
typedef SCardApiFunctionTable* PSCardApiFunctionTable;

extern const SCARD_IO_REQUEST winpr_g_rgSCardT0Pci;
extern const SCARD_IO_REQUEST winpr_g_rgSCardT1Pci;
extern const SCARD_IO_REQUEST winpr_g_rgSCardRawPci;

#define WINPR_SCARD_PCI_T0 (&winpr_g_rgSCardT0Pci)
#define WINPR_SCARD_PCI_T1 (&winpr_g_rgSCardT1Pci)
#define WINPR_SCARD_PCI_RAW (&winpr_g_rgSCardRawPci)

#endif /* WINPR_SMARTCARD_PRIVATE_H */
