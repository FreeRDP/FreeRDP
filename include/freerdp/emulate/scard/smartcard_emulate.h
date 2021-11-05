/**
 * WinPR: Windows Portable Runtime
 * Smart Card API emulation
 *
 * Copyright 2021 Armin Novak <armin.novak@thincast.com>
 * Copyright 2021 Thincast Technologies GmbH
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

#ifndef WINPR_SMARTCARD_EMULATE_PRIVATE_H
#define WINPR_SMARTCARD_EMULATE_PRIVATE_H

#include <winpr/platform.h>
#include <winpr/smartcard.h>

#include <freerdp/api.h>
#include <freerdp/settings.h>

typedef struct smartcard_emulation_context SmartcardEmulationContext;

FREERDP_API SmartcardEmulationContext* Emulate_New(const rdpSettings* settings);
FREERDP_API void Emulate_Free(SmartcardEmulationContext* context);

FREERDP_API BOOL Emulate_IsConfigured(SmartcardEmulationContext* context);

FREERDP_API LONG WINAPI Emulate_SCardEstablishContext(SmartcardEmulationContext* smartcard,
                                                      DWORD dwScope, LPCVOID pvReserved1,
                                                      LPCVOID pvReserved2,
                                                      LPSCARDCONTEXT phContext);

FREERDP_API LONG WINAPI Emulate_SCardReleaseContext(SmartcardEmulationContext* smartcard,
                                                    SCARDCONTEXT hContext);

FREERDP_API LONG WINAPI Emulate_SCardIsValidContext(SmartcardEmulationContext* smartcard,
                                                    SCARDCONTEXT hContext);

FREERDP_API LONG WINAPI Emulate_SCardListReaderGroupsA(SmartcardEmulationContext* smartcard,
                                                       SCARDCONTEXT hContext, LPSTR mszGroups,
                                                       LPDWORD pcchGroups);

FREERDP_API LONG WINAPI Emulate_SCardListReaderGroupsW(SmartcardEmulationContext* smartcard,
                                                       SCARDCONTEXT hContext, LPWSTR mszGroups,
                                                       LPDWORD pcchGroups);

FREERDP_API LONG WINAPI Emulate_SCardListReadersA(SmartcardEmulationContext* smartcard,
                                                  SCARDCONTEXT hContext, LPCSTR mszGroups,
                                                  LPSTR mszReaders, LPDWORD pcchReaders);

FREERDP_API LONG WINAPI Emulate_SCardListReadersW(SmartcardEmulationContext* smartcard,
                                                  SCARDCONTEXT hContext, LPCWSTR mszGroups,
                                                  LPWSTR mszReaders, LPDWORD pcchReaders);

FREERDP_API LONG WINAPI Emulate_SCardListCardsA(SmartcardEmulationContext* smartcard,
                                                SCARDCONTEXT hContext, LPCBYTE pbAtr,
                                                LPCGUID rgquidInterfaces, DWORD cguidInterfaceCount,
                                                CHAR* mszCards, LPDWORD pcchCards);

FREERDP_API LONG WINAPI Emulate_SCardListCardsW(SmartcardEmulationContext* smartcard,
                                                SCARDCONTEXT hContext, LPCBYTE pbAtr,
                                                LPCGUID rgquidInterfaces, DWORD cguidInterfaceCount,
                                                WCHAR* mszCards, LPDWORD pcchCards);

FREERDP_API LONG WINAPI Emulate_SCardListInterfacesA(SmartcardEmulationContext* smartcard,
                                                     SCARDCONTEXT hContext, LPCSTR szCard,
                                                     LPGUID pguidInterfaces,
                                                     LPDWORD pcguidInterfaces);

FREERDP_API LONG WINAPI Emulate_SCardListInterfacesW(SmartcardEmulationContext* smartcard,
                                                     SCARDCONTEXT hContext, LPCWSTR szCard,
                                                     LPGUID pguidInterfaces,
                                                     LPDWORD pcguidInterfaces);

FREERDP_API LONG WINAPI Emulate_SCardGetProviderIdA(SmartcardEmulationContext* smartcard,
                                                    SCARDCONTEXT hContext, LPCSTR szCard,
                                                    LPGUID pguidProviderId);

FREERDP_API LONG WINAPI Emulate_SCardGetProviderIdW(SmartcardEmulationContext* smartcard,
                                                    SCARDCONTEXT hContext, LPCWSTR szCard,
                                                    LPGUID pguidProviderId);

FREERDP_API LONG WINAPI Emulate_SCardGetCardTypeProviderNameA(SmartcardEmulationContext* smartcard,
                                                              SCARDCONTEXT hContext,
                                                              LPCSTR szCardName, DWORD dwProviderId,
                                                              CHAR* szProvider,
                                                              LPDWORD pcchProvider);

FREERDP_API LONG WINAPI Emulate_SCardGetCardTypeProviderNameW(SmartcardEmulationContext* smartcard,
                                                              SCARDCONTEXT hContext,
                                                              LPCWSTR szCardName,
                                                              DWORD dwProviderId, WCHAR* szProvider,
                                                              LPDWORD pcchProvider);

FREERDP_API LONG WINAPI Emulate_SCardIntroduceReaderGroupA(SmartcardEmulationContext* smartcard,
                                                           SCARDCONTEXT hContext,
                                                           LPCSTR szGroupName);

FREERDP_API LONG WINAPI Emulate_SCardIntroduceReaderGroupW(SmartcardEmulationContext* smartcard,
                                                           SCARDCONTEXT hContext,
                                                           LPCWSTR szGroupName);

FREERDP_API LONG WINAPI Emulate_SCardForgetReaderGroupA(SmartcardEmulationContext* smartcard,
                                                        SCARDCONTEXT hContext, LPCSTR szGroupName);

FREERDP_API LONG WINAPI Emulate_SCardForgetReaderGroupW(SmartcardEmulationContext* smartcard,
                                                        SCARDCONTEXT hContext, LPCWSTR szGroupName);

FREERDP_API LONG WINAPI Emulate_SCardIntroduceReaderA(SmartcardEmulationContext* smartcard,
                                                      SCARDCONTEXT hContext, LPCSTR szReaderName,
                                                      LPCSTR szDeviceName);

FREERDP_API LONG WINAPI Emulate_SCardIntroduceReaderW(SmartcardEmulationContext* smartcard,
                                                      SCARDCONTEXT hContext, LPCWSTR szReaderName,
                                                      LPCWSTR szDeviceName);

FREERDP_API LONG WINAPI Emulate_SCardForgetReaderA(SmartcardEmulationContext* smartcard,
                                                   SCARDCONTEXT hContext, LPCSTR szReaderName);

FREERDP_API LONG WINAPI Emulate_SCardForgetReaderW(SmartcardEmulationContext* smartcard,
                                                   SCARDCONTEXT hContext, LPCWSTR szReaderName);

FREERDP_API LONG WINAPI Emulate_SCardAddReaderToGroupA(SmartcardEmulationContext* smartcard,
                                                       SCARDCONTEXT hContext, LPCSTR szReaderName,
                                                       LPCSTR szGroupName);

FREERDP_API LONG WINAPI Emulate_SCardAddReaderToGroupW(SmartcardEmulationContext* smartcard,
                                                       SCARDCONTEXT hContext, LPCWSTR szReaderName,
                                                       LPCWSTR szGroupName);

FREERDP_API LONG WINAPI Emulate_SCardRemoveReaderFromGroupA(SmartcardEmulationContext* smartcard,
                                                            SCARDCONTEXT hContext,
                                                            LPCSTR szReaderName,
                                                            LPCSTR szGroupName);

FREERDP_API LONG WINAPI Emulate_SCardRemoveReaderFromGroupW(SmartcardEmulationContext* smartcard,
                                                            SCARDCONTEXT hContext,
                                                            LPCWSTR szReaderName,
                                                            LPCWSTR szGroupName);

FREERDP_API LONG WINAPI Emulate_SCardIntroduceCardTypeA(SmartcardEmulationContext* smartcard,
                                                        SCARDCONTEXT hContext, LPCSTR szCardName,
                                                        LPCGUID pguidPrimaryProvider,
                                                        LPCGUID rgguidInterfaces,
                                                        DWORD dwInterfaceCount, LPCBYTE pbAtr,
                                                        LPCBYTE pbAtrMask, DWORD cbAtrLen);

FREERDP_API LONG WINAPI Emulate_SCardIntroduceCardTypeW(SmartcardEmulationContext* smartcard,
                                                        SCARDCONTEXT hContext, LPCWSTR szCardName,
                                                        LPCGUID pguidPrimaryProvider,
                                                        LPCGUID rgguidInterfaces,
                                                        DWORD dwInterfaceCount, LPCBYTE pbAtr,
                                                        LPCBYTE pbAtrMask, DWORD cbAtrLen);

FREERDP_API LONG WINAPI Emulate_SCardSetCardTypeProviderNameA(SmartcardEmulationContext* smartcard,
                                                              SCARDCONTEXT hContext,
                                                              LPCSTR szCardName, DWORD dwProviderId,
                                                              LPCSTR szProvider);

FREERDP_API LONG WINAPI Emulate_SCardSetCardTypeProviderNameW(SmartcardEmulationContext* smartcard,
                                                              SCARDCONTEXT hContext,
                                                              LPCWSTR szCardName,
                                                              DWORD dwProviderId,
                                                              LPCWSTR szProvider);

FREERDP_API LONG WINAPI Emulate_SCardForgetCardTypeA(SmartcardEmulationContext* smartcard,
                                                     SCARDCONTEXT hContext, LPCSTR szCardName);

FREERDP_API LONG WINAPI Emulate_SCardForgetCardTypeW(SmartcardEmulationContext* smartcard,
                                                     SCARDCONTEXT hContext, LPCWSTR szCardName);

FREERDP_API LONG WINAPI Emulate_SCardFreeMemory(SmartcardEmulationContext* smartcard,
                                                SCARDCONTEXT hContext, LPVOID pvMem);

FREERDP_API HANDLE WINAPI Emulate_SCardAccessStartedEvent(SmartcardEmulationContext* smartcard);

FREERDP_API void WINAPI Emulate_SCardReleaseStartedEvent(SmartcardEmulationContext* smartcard);

FREERDP_API LONG WINAPI Emulate_SCardLocateCardsA(SmartcardEmulationContext* smartcard,
                                                  SCARDCONTEXT hContext, LPCSTR mszCards,
                                                  LPSCARD_READERSTATEA rgReaderStates,
                                                  DWORD cReaders);

FREERDP_API LONG WINAPI Emulate_SCardLocateCardsW(SmartcardEmulationContext* smartcard,
                                                  SCARDCONTEXT hContext, LPCWSTR mszCards,
                                                  LPSCARD_READERSTATEW rgReaderStates,
                                                  DWORD cReaders);

FREERDP_API LONG WINAPI Emulate_SCardLocateCardsByATRA(SmartcardEmulationContext* smartcard,
                                                       SCARDCONTEXT hContext,
                                                       LPSCARD_ATRMASK rgAtrMasks, DWORD cAtrs,
                                                       LPSCARD_READERSTATEA rgReaderStates,
                                                       DWORD cReaders);

FREERDP_API LONG WINAPI Emulate_SCardLocateCardsByATRW(SmartcardEmulationContext* smartcard,
                                                       SCARDCONTEXT hContext,
                                                       LPSCARD_ATRMASK rgAtrMasks, DWORD cAtrs,
                                                       LPSCARD_READERSTATEW rgReaderStates,
                                                       DWORD cReaders);

FREERDP_API LONG WINAPI Emulate_SCardGetStatusChangeA(SmartcardEmulationContext* smartcard,
                                                      SCARDCONTEXT hContext, DWORD dwTimeout,
                                                      LPSCARD_READERSTATEA rgReaderStates,
                                                      DWORD cReaders);

FREERDP_API LONG WINAPI Emulate_SCardGetStatusChangeW(SmartcardEmulationContext* smartcard,
                                                      SCARDCONTEXT hContext, DWORD dwTimeout,
                                                      LPSCARD_READERSTATEW rgReaderStates,
                                                      DWORD cReaders);

FREERDP_API LONG WINAPI Emulate_SCardCancel(SmartcardEmulationContext* smartcard,
                                            SCARDCONTEXT hContext);

FREERDP_API LONG WINAPI Emulate_SCardConnectA(SmartcardEmulationContext* smartcard,
                                              SCARDCONTEXT hContext, LPCSTR szReader,
                                              DWORD dwShareMode, DWORD dwPreferredProtocols,
                                              LPSCARDHANDLE phCard, LPDWORD pdwActiveProtocol);

FREERDP_API LONG WINAPI Emulate_SCardConnectW(SmartcardEmulationContext* smartcard,
                                              SCARDCONTEXT hContext, LPCWSTR szReader,
                                              DWORD dwShareMode, DWORD dwPreferredProtocols,
                                              LPSCARDHANDLE phCard, LPDWORD pdwActiveProtocol);

FREERDP_API LONG WINAPI Emulate_SCardReconnect(SmartcardEmulationContext* smartcard,
                                               SCARDHANDLE hCard, DWORD dwShareMode,
                                               DWORD dwPreferredProtocols, DWORD dwInitialization,
                                               LPDWORD pdwActiveProtocol);

FREERDP_API LONG WINAPI Emulate_SCardDisconnect(SmartcardEmulationContext* smartcard,
                                                SCARDHANDLE hCard, DWORD dwDisposition);

FREERDP_API LONG WINAPI Emulate_SCardBeginTransaction(SmartcardEmulationContext* smartcard,
                                                      SCARDHANDLE hCard);

FREERDP_API LONG WINAPI Emulate_SCardEndTransaction(SmartcardEmulationContext* smartcard,
                                                    SCARDHANDLE hCard, DWORD dwDisposition);

FREERDP_API LONG WINAPI Emulate_SCardCancelTransaction(SmartcardEmulationContext* smartcard,
                                                       SCARDHANDLE hCard);

FREERDP_API LONG WINAPI Emulate_SCardState(SmartcardEmulationContext* smartcard, SCARDHANDLE hCard,
                                           LPDWORD pdwState, LPDWORD pdwProtocol, LPBYTE pbAtr,
                                           LPDWORD pcbAtrLen);

FREERDP_API LONG WINAPI Emulate_SCardStatusA(SmartcardEmulationContext* smartcard,
                                             SCARDHANDLE hCard, LPSTR mszReaderNames,
                                             LPDWORD pcchReaderLen, LPDWORD pdwState,
                                             LPDWORD pdwProtocol, LPBYTE pbAtr, LPDWORD pcbAtrLen);

FREERDP_API LONG WINAPI Emulate_SCardStatusW(SmartcardEmulationContext* smartcard,
                                             SCARDHANDLE hCard, LPWSTR mszReaderNames,
                                             LPDWORD pcchReaderLen, LPDWORD pdwState,
                                             LPDWORD pdwProtocol, LPBYTE pbAtr, LPDWORD pcbAtrLen);

FREERDP_API LONG WINAPI Emulate_SCardTransmit(SmartcardEmulationContext* smartcard,
                                              SCARDHANDLE hCard, LPCSCARD_IO_REQUEST pioSendPci,
                                              LPCBYTE pbSendBuffer, DWORD cbSendLength,
                                              LPSCARD_IO_REQUEST pioRecvPci, LPBYTE pbRecvBuffer,
                                              LPDWORD pcbRecvLength);

FREERDP_API LONG WINAPI Emulate_SCardGetTransmitCount(SmartcardEmulationContext* smartcard,
                                                      SCARDHANDLE hCard, LPDWORD pcTransmitCount);

FREERDP_API LONG WINAPI Emulate_SCardControl(SmartcardEmulationContext* smartcard,
                                             SCARDHANDLE hCard, DWORD dwControlCode,
                                             LPCVOID lpInBuffer, DWORD cbInBufferSize,
                                             LPVOID lpOutBuffer, DWORD cbOutBufferSize,
                                             LPDWORD lpBytesReturned);

FREERDP_API LONG WINAPI Emulate_SCardGetAttrib(SmartcardEmulationContext* smartcard,
                                               SCARDHANDLE hCard, DWORD dwAttrId, LPBYTE pbAttr,
                                               LPDWORD pcbAttrLen);

FREERDP_API LONG WINAPI Emulate_SCardSetAttrib(SmartcardEmulationContext* smartcard,
                                               SCARDHANDLE hCard, DWORD dwAttrId, LPCBYTE pbAttr,
                                               DWORD cbAttrLen);

FREERDP_API LONG WINAPI Emulate_SCardUIDlgSelectCardA(SmartcardEmulationContext* smartcard,
                                                      LPOPENCARDNAMEA_EX pDlgStruc);

FREERDP_API LONG WINAPI Emulate_SCardUIDlgSelectCardW(SmartcardEmulationContext* smartcard,
                                                      LPOPENCARDNAMEW_EX pDlgStruc);

FREERDP_API LONG WINAPI Emulate_GetOpenCardNameA(SmartcardEmulationContext* smartcard,
                                                 LPOPENCARDNAMEA pDlgStruc);

FREERDP_API LONG WINAPI Emulate_GetOpenCardNameW(SmartcardEmulationContext* smartcard,
                                                 LPOPENCARDNAMEW pDlgStruc);

FREERDP_API LONG WINAPI Emulate_SCardDlgExtendedError(SmartcardEmulationContext* smartcard);

FREERDP_API LONG WINAPI Emulate_SCardReadCacheA(SmartcardEmulationContext* smartcard,
                                                SCARDCONTEXT hContext, UUID* CardIdentifier,
                                                DWORD FreshnessCounter, LPSTR LookupName,
                                                PBYTE Data, DWORD* DataLen);

FREERDP_API LONG WINAPI Emulate_SCardReadCacheW(SmartcardEmulationContext* smartcard,
                                                SCARDCONTEXT hContext, UUID* CardIdentifier,
                                                DWORD FreshnessCounter, LPWSTR LookupName,
                                                PBYTE Data, DWORD* DataLen);

FREERDP_API LONG WINAPI Emulate_SCardWriteCacheA(SmartcardEmulationContext* smartcard,
                                                 SCARDCONTEXT hContext, UUID* CardIdentifier,
                                                 DWORD FreshnessCounter, LPSTR LookupName,
                                                 PBYTE Data, DWORD DataLen);

FREERDP_API LONG WINAPI Emulate_SCardWriteCacheW(SmartcardEmulationContext* smartcard,
                                                 SCARDCONTEXT hContext, UUID* CardIdentifier,
                                                 DWORD FreshnessCounter, LPWSTR LookupName,
                                                 PBYTE Data, DWORD DataLen);

FREERDP_API LONG WINAPI Emulate_SCardGetReaderIconA(SmartcardEmulationContext* smartcard,
                                                    SCARDCONTEXT hContext, LPCSTR szReaderName,
                                                    LPBYTE pbIcon, LPDWORD pcbIcon);

FREERDP_API LONG WINAPI Emulate_SCardGetReaderIconW(SmartcardEmulationContext* smartcard,
                                                    SCARDCONTEXT hContext, LPCWSTR szReaderName,
                                                    LPBYTE pbIcon, LPDWORD pcbIcon);

FREERDP_API LONG WINAPI Emulate_SCardGetDeviceTypeIdA(SmartcardEmulationContext* smartcard,
                                                      SCARDCONTEXT hContext, LPCSTR szReaderName,
                                                      LPDWORD pdwDeviceTypeId);
FREERDP_API LONG WINAPI Emulate_SCardGetDeviceTypeIdW(SmartcardEmulationContext* smartcard,
                                                      SCARDCONTEXT hContext, LPCWSTR szReaderName,
                                                      LPDWORD pdwDeviceTypeId);

FREERDP_API LONG WINAPI Emulate_SCardGetReaderDeviceInstanceIdA(
    SmartcardEmulationContext* smartcard, SCARDCONTEXT hContext, LPCSTR szReaderName,
    LPSTR szDeviceInstanceId, LPDWORD pcchDeviceInstanceId);

FREERDP_API LONG WINAPI Emulate_SCardGetReaderDeviceInstanceIdW(
    SmartcardEmulationContext* smartcard, SCARDCONTEXT hContext, LPCWSTR szReaderName,
    LPWSTR szDeviceInstanceId, LPDWORD pcchDeviceInstanceId);
FREERDP_API LONG WINAPI Emulate_SCardListReadersWithDeviceInstanceIdA(
    SmartcardEmulationContext* smartcard, SCARDCONTEXT hContext, LPCSTR szDeviceInstanceId,
    LPSTR mszReaders, LPDWORD pcchReaders);
FREERDP_API LONG WINAPI Emulate_SCardListReadersWithDeviceInstanceIdW(
    SmartcardEmulationContext* smartcard, SCARDCONTEXT hContext, LPCWSTR szDeviceInstanceId,
    LPWSTR mszReaders, LPDWORD pcchReaders);
FREERDP_API LONG WINAPI Emulate_SCardAudit(SmartcardEmulationContext* smartcard,
                                           SCARDCONTEXT hContext, DWORD dwEvent);

#endif /* WINPR_SMARTCARD_EMULATE_PRIVATE_H */
