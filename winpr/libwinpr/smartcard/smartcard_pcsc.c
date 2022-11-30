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

#ifndef _WIN32

#ifdef __APPLE__
#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/synch.h>
#include <winpr/library.h>
#include <winpr/smartcard.h>
#include <winpr/collections.h>
#include <winpr/environment.h>

#include "smartcard_pcsc.h"

#include "../log.h"
#define TAG WINPR_TAG("smartcard")

/**
 * PC/SC transactions:
 * http://developersblog.wwpass.com/?p=180
 */

/**
 * Smart Card Logon on Windows Vista:
 * http://blogs.msdn.com/b/shivaram/archive/2007/02/26/smart-card-logon-on-windows-vista.aspx
 */

/**
 * The Smart Card Cryptographic Service Provider Cookbook:
 * http://msdn.microsoft.com/en-us/library/ms953432.aspx
 *
 * SCARDCONTEXT
 *
 * The context is a communication channel with the smart card resource manager and
 * all calls to the resource manager must go through this link.
 *
 * All functions that take a context as a parameter or a card handle as parameter,
 * which is indirectly associated with a particular context, may be blocking calls.
 * Examples of these are SCardGetStatusChange and SCardBeginTransaction, which takes
 * a card handle as a parameter. If such a function blocks then all operations wanting
 * to use the context are blocked as well. So, it is recommended that a CSP using
 * monitoring establishes at least two contexts with the resource manager; one for
 * monitoring (with SCardGetStatusChange) and one for other operations.
 *
 * If multiple cards are present, it is recommended that a separate context or pair
 * of contexts be established for each card to prevent operations on one card from
 * blocking operations on another.
 *
 * Example one
 *
 * The example below shows what can happen if a CSP using SCardGetStatusChange for
 * monitoring does not establish two contexts with the resource manager.
 * The context becomes unusable until SCardGetStatusChange unblocks.
 *
 * In this example, there is one process running called P1.
 * P1 calls SCardEstablishContext, which returns the context hCtx.
 * P1 calls SCardConnect (with the hCtx context) which returns a handle to the card, hCard.
 * P1 calls SCardGetStatusChange (with the hCtx context) which blocks because
 * there are no status changes to report.
 * Until the thread running SCardGetStatusChange unblocks, another thread in P1 trying to
 * perform an operation using the context hCtx (or the card hCard) will also be blocked.
 *
 * Example two
 *
 * The example below shows how transaction control ensures that operations meant to be
 * performed without interruption can do so safely within a transaction.
 *
 * In this example, there are two different processes running; P1 and P2.
 * P1 calls SCardEstablishContext, which returns the context hCtx1.
 * P2 calls SCardEstablishContext, which returns the context hCtx2.
 * P1 calls SCardConnect (with the hCtx1 context) which returns a handle to the card, hCard1.
 * P2 calls SCardConnect (with the hCtx2 context) which returns a handle to the same card, hCard2.
 * P1 calls SCardBeginTransaction (with the hCard 1 context).
 * Until P1 calls SCardEndTransaction (with the hCard1 context),
 * any operation using hCard2 will be blocked.
 * Once an operation using hCard2 is blocked and until it's returning,
 * any operation using hCtx2 (and hCard2) will also be blocked.
 */

//#define DISABLE_PCSC_SCARD_AUTOALLOCATE
#include "smartcard_pcsc.h"

#define PCSC_SCARD_PCI_T0 (&g_PCSC_rgSCardT0Pci)
#define PCSC_SCARD_PCI_T1 (&g_PCSC_rgSCardT1Pci)
#define PCSC_SCARD_PCI_RAW (&g_PCSC_rgSCardRawPci)

typedef PCSC_LONG (*fnPCSCSCardEstablishContext)(PCSC_DWORD dwScope, LPCVOID pvReserved1,
                                                 LPCVOID pvReserved2, LPSCARDCONTEXT phContext);
typedef PCSC_LONG (*fnPCSCSCardReleaseContext)(SCARDCONTEXT hContext);
typedef PCSC_LONG (*fnPCSCSCardIsValidContext)(SCARDCONTEXT hContext);
typedef PCSC_LONG (*fnPCSCSCardConnect)(SCARDCONTEXT hContext, LPCSTR szReader,
                                        PCSC_DWORD dwShareMode, PCSC_DWORD dwPreferredProtocols,
                                        LPSCARDHANDLE phCard, PCSC_LPDWORD pdwActiveProtocol);
typedef PCSC_LONG (*fnPCSCSCardReconnect)(SCARDHANDLE hCard, PCSC_DWORD dwShareMode,
                                          PCSC_DWORD dwPreferredProtocols,
                                          PCSC_DWORD dwInitialization,
                                          PCSC_LPDWORD pdwActiveProtocol);
typedef PCSC_LONG (*fnPCSCSCardDisconnect)(SCARDHANDLE hCard, PCSC_DWORD dwDisposition);
typedef PCSC_LONG (*fnPCSCSCardBeginTransaction)(SCARDHANDLE hCard);
typedef PCSC_LONG (*fnPCSCSCardEndTransaction)(SCARDHANDLE hCard, PCSC_DWORD dwDisposition);
typedef PCSC_LONG (*fnPCSCSCardStatus)(SCARDHANDLE hCard, LPSTR mszReaderName,
                                       PCSC_LPDWORD pcchReaderLen, PCSC_LPDWORD pdwState,
                                       PCSC_LPDWORD pdwProtocol, LPBYTE pbAtr,
                                       PCSC_LPDWORD pcbAtrLen);
typedef PCSC_LONG (*fnPCSCSCardGetStatusChange)(SCARDCONTEXT hContext, PCSC_DWORD dwTimeout,
                                                PCSC_SCARD_READERSTATE* rgReaderStates,
                                                PCSC_DWORD cReaders);
typedef PCSC_LONG (*fnPCSCSCardControl)(SCARDHANDLE hCard, PCSC_DWORD dwControlCode,
                                        LPCVOID pbSendBuffer, PCSC_DWORD cbSendLength,
                                        LPVOID pbRecvBuffer, PCSC_DWORD cbRecvLength,
                                        PCSC_LPDWORD lpBytesReturned);
typedef PCSC_LONG (*fnPCSCSCardTransmit)(SCARDHANDLE hCard, const PCSC_SCARD_IO_REQUEST* pioSendPci,
                                         LPCBYTE pbSendBuffer, PCSC_DWORD cbSendLength,
                                         PCSC_SCARD_IO_REQUEST* pioRecvPci, LPBYTE pbRecvBuffer,
                                         PCSC_LPDWORD pcbRecvLength);
typedef PCSC_LONG (*fnPCSCSCardListReaderGroups)(SCARDCONTEXT hContext, LPSTR mszGroups,
                                                 PCSC_LPDWORD pcchGroups);
typedef PCSC_LONG (*fnPCSCSCardListReaders)(SCARDCONTEXT hContext, LPCSTR mszGroups,
                                            LPSTR mszReaders, PCSC_LPDWORD pcchReaders);
typedef PCSC_LONG (*fnPCSCSCardFreeMemory)(SCARDCONTEXT hContext, LPCVOID pvMem);
typedef PCSC_LONG (*fnPCSCSCardCancel)(SCARDCONTEXT hContext);
typedef PCSC_LONG (*fnPCSCSCardGetAttrib)(SCARDHANDLE hCard, PCSC_DWORD dwAttrId, LPBYTE pbAttr,
                                          PCSC_LPDWORD pcbAttrLen);
typedef PCSC_LONG (*fnPCSCSCardSetAttrib)(SCARDHANDLE hCard, PCSC_DWORD dwAttrId, LPCBYTE pbAttr,
                                          PCSC_DWORD cbAttrLen);

typedef struct
{
	fnPCSCSCardEstablishContext pfnSCardEstablishContext;
	fnPCSCSCardReleaseContext pfnSCardReleaseContext;
	fnPCSCSCardIsValidContext pfnSCardIsValidContext;
	fnPCSCSCardConnect pfnSCardConnect;
	fnPCSCSCardReconnect pfnSCardReconnect;
	fnPCSCSCardDisconnect pfnSCardDisconnect;
	fnPCSCSCardBeginTransaction pfnSCardBeginTransaction;
	fnPCSCSCardEndTransaction pfnSCardEndTransaction;
	fnPCSCSCardStatus pfnSCardStatus;
	fnPCSCSCardGetStatusChange pfnSCardGetStatusChange;
	fnPCSCSCardControl pfnSCardControl;
	fnPCSCSCardTransmit pfnSCardTransmit;
	fnPCSCSCardListReaderGroups pfnSCardListReaderGroups;
	fnPCSCSCardListReaders pfnSCardListReaders;
	fnPCSCSCardFreeMemory pfnSCardFreeMemory;
	fnPCSCSCardCancel pfnSCardCancel;
	fnPCSCSCardGetAttrib pfnSCardGetAttrib;
	fnPCSCSCardSetAttrib pfnSCardSetAttrib;
} PCSCFunctionTable;

typedef struct
{
	DWORD len;
	DWORD freshness;
	BYTE* data;
} PCSC_CACHE_ITEM;

typedef struct
{
	SCARDHANDLE owner;
	CRITICAL_SECTION lock;
	SCARDCONTEXT hContext;
	DWORD dwCardHandleCount;
	BOOL isTransactionLocked;
	wHashTable* cache;
} PCSC_SCARDCONTEXT;

typedef struct
{
	BOOL shared;
	SCARDCONTEXT hSharedContext;
} PCSC_SCARDHANDLE;

static HMODULE g_PCSCModule = NULL;
static PCSCFunctionTable g_PCSC = { 0 };

static HANDLE g_StartedEvent = NULL;
static int g_StartedEventRefCount = 0;

static BOOL g_SCardAutoAllocate = FALSE;
static BOOL g_PnP_Notification = TRUE;

#ifdef __MACOSX__
static unsigned int OSXVersion = 0;
#endif

static wListDictionary* g_CardHandles = NULL;
static wListDictionary* g_CardContexts = NULL;
static wListDictionary* g_MemoryBlocks = NULL;

static const char SMARTCARD_PNP_NOTIFICATION_A[] = "\\\\?PnP?\\Notification";

static const PCSC_SCARD_IO_REQUEST g_PCSC_rgSCardT0Pci = { SCARD_PROTOCOL_T0,
	                                                       sizeof(PCSC_SCARD_IO_REQUEST) };
static const PCSC_SCARD_IO_REQUEST g_PCSC_rgSCardT1Pci = { SCARD_PROTOCOL_T1,
	                                                       sizeof(PCSC_SCARD_IO_REQUEST) };
static const PCSC_SCARD_IO_REQUEST g_PCSC_rgSCardRawPci = { PCSC_SCARD_PROTOCOL_RAW,
	                                                        sizeof(PCSC_SCARD_IO_REQUEST) };

static LONG WINAPI PCSC_SCardFreeMemory_Internal(SCARDCONTEXT hContext, LPVOID pvMem);
static LONG WINAPI PCSC_SCardEstablishContext_Internal(DWORD dwScope, LPCVOID pvReserved1,
                                                       LPCVOID pvReserved2,
                                                       LPSCARDCONTEXT phContext);
static LONG WINAPI PCSC_SCardReleaseContext_Internal(SCARDCONTEXT hContext);

static LONG PCSC_SCard_LogError(const char* what)
{
	WLog_WARN(TAG, "Missing function pointer %s=NULL", what);
	return SCARD_E_UNSUPPORTED_FEATURE;
}

static LONG PCSC_MapErrorCodeToWinSCard(PCSC_LONG errorCode)
{
	/**
	 * pcsc-lite returns SCARD_E_UNEXPECTED when it
	 * should return SCARD_E_UNSUPPORTED_FEATURE.
	 *
	 * Additionally, the pcsc-lite headers incorrectly
	 * define SCARD_E_UNSUPPORTED_FEATURE to 0x8010001F,
	 * when the real value should be 0x80100022.
	 */
	if (errorCode != SCARD_S_SUCCESS)
	{
		if (errorCode == SCARD_E_UNEXPECTED)
			errorCode = SCARD_E_UNSUPPORTED_FEATURE;
	}

	return (LONG)errorCode;
}

static DWORD PCSC_ConvertCardStateToWinSCard(DWORD dwCardState, PCSC_LONG status)
{
	/**
	 * pcsc-lite's SCardStatus returns a bit-field, not an enumerated value.
	 *
	 *         State             WinSCard           pcsc-lite
	 *
	 *      SCARD_UNKNOWN           0                0x0001
	 *      SCARD_ABSENT            1                0x0002
	 *      SCARD_PRESENT           2                0x0004
	 *      SCARD_SWALLOWED         3                0x0008
	 *      SCARD_POWERED           4                0x0010
	 *      SCARD_NEGOTIABLE        5                0x0020
	 *      SCARD_SPECIFIC          6                0x0040
	 *
	 * pcsc-lite also never sets SCARD_SPECIFIC,
	 * which is expected by some windows applications.
	 */
	if (status == SCARD_S_SUCCESS)
	{
		if ((dwCardState & PCSC_SCARD_NEGOTIABLE) || (dwCardState & PCSC_SCARD_SPECIFIC))
			return SCARD_SPECIFIC;
	}

	if (dwCardState & PCSC_SCARD_POWERED)
		return SCARD_POWERED;

	if (dwCardState & PCSC_SCARD_NEGOTIABLE)
		return SCARD_NEGOTIABLE;

	if (dwCardState & PCSC_SCARD_SPECIFIC)
		return SCARD_SPECIFIC;

	if (dwCardState & PCSC_SCARD_ABSENT)
		return SCARD_ABSENT;

	if (dwCardState & PCSC_SCARD_PRESENT)
		return SCARD_PRESENT;

	if (dwCardState & PCSC_SCARD_SWALLOWED)
		return SCARD_SWALLOWED;

	if (dwCardState & PCSC_SCARD_UNKNOWN)
		return SCARD_UNKNOWN;

	return SCARD_UNKNOWN;
}

static DWORD PCSC_ConvertProtocolsToWinSCard(PCSC_DWORD dwProtocols)
{
	/**
	 * pcsc-lite uses a different value for SCARD_PROTOCOL_RAW,
	 * and also has SCARD_PROTOCOL_T15 which is not in WinSCard.
	 */
	if (dwProtocols & PCSC_SCARD_PROTOCOL_RAW)
	{
		dwProtocols &= ~PCSC_SCARD_PROTOCOL_RAW;
		dwProtocols |= SCARD_PROTOCOL_RAW;
	}

	if (dwProtocols & PCSC_SCARD_PROTOCOL_T15)
	{
		dwProtocols &= ~PCSC_SCARD_PROTOCOL_T15;
	}

	return (DWORD)dwProtocols;
}

static DWORD PCSC_ConvertProtocolsFromWinSCard(DWORD dwProtocols)
{
	/**
	 * pcsc-lite uses a different value for SCARD_PROTOCOL_RAW,
	 * and it does not define WinSCard's SCARD_PROTOCOL_DEFAULT.
	 */
	if (dwProtocols & SCARD_PROTOCOL_RAW)
	{
		dwProtocols &= ~SCARD_PROTOCOL_RAW;
		dwProtocols |= PCSC_SCARD_PROTOCOL_RAW;
	}

	if (dwProtocols & SCARD_PROTOCOL_DEFAULT)
	{
		dwProtocols &= ~SCARD_PROTOCOL_DEFAULT;
	}

	if (dwProtocols == SCARD_PROTOCOL_UNDEFINED)
	{
		dwProtocols = SCARD_PROTOCOL_Tx;
	}

	return dwProtocols;
}

static PCSC_SCARDCONTEXT* PCSC_GetCardContextData(SCARDCONTEXT hContext)
{
	PCSC_SCARDCONTEXT* pContext;

	if (!g_CardContexts)
		return NULL;

	pContext = (PCSC_SCARDCONTEXT*)ListDictionary_GetItemValue(g_CardContexts, (void*)hContext);

	if (!pContext)
		return NULL;

	return pContext;
}

static void pcsc_cache_item_free(void* ptr)
{
	PCSC_CACHE_ITEM* data = ptr;
	if (data)
		free(data->data);
	free(data);
}

static PCSC_SCARDCONTEXT* PCSC_EstablishCardContext(SCARDCONTEXT hContext)
{
	PCSC_SCARDCONTEXT* pContext;
	pContext = (PCSC_SCARDCONTEXT*)calloc(1, sizeof(PCSC_SCARDCONTEXT));

	if (!pContext)
		return NULL;

	pContext->hContext = hContext;

	if (!InitializeCriticalSectionAndSpinCount(&(pContext->lock), 4000))
		goto error_spinlock;

	pContext->cache = HashTable_New(FALSE);
	if (!pContext->cache)
		goto errors;
	if (!HashTable_SetupForStringData(pContext->cache, FALSE))
		goto errors;
	{
		wObject* obj = HashTable_ValueObject(pContext->cache);
		obj->fnObjectFree = pcsc_cache_item_free;
	}

	if (!g_CardContexts)
	{
		g_CardContexts = ListDictionary_New(TRUE);

		if (!g_CardContexts)
			goto errors;
	}

	if (!ListDictionary_Add(g_CardContexts, (void*)hContext, (void*)pContext))
		goto errors;

	return pContext;
errors:
	HashTable_Free(pContext->cache);
	DeleteCriticalSection(&(pContext->lock));
error_spinlock:
	free(pContext);
	return NULL;
}

static void PCSC_ReleaseCardContext(SCARDCONTEXT hContext)
{
	PCSC_SCARDCONTEXT* pContext;
	pContext = PCSC_GetCardContextData(hContext);

	if (!pContext)
	{
		WLog_ERR(TAG, "PCSC_ReleaseCardContext: null pContext!");
		return;
	}

	DeleteCriticalSection(&(pContext->lock));
	HashTable_Free(pContext->cache);
	free(pContext);

	if (!g_CardContexts)
		return;

	ListDictionary_Remove(g_CardContexts, (void*)hContext);
}

static BOOL PCSC_LockCardContext(SCARDCONTEXT hContext)
{
	PCSC_SCARDCONTEXT* pContext;
	pContext = PCSC_GetCardContextData(hContext);

	if (!pContext)
	{
		WLog_ERR(TAG, "PCSC_LockCardContext: invalid context (%p)", (void*)hContext);
		return FALSE;
	}

	EnterCriticalSection(&(pContext->lock));
	return TRUE;
}

static BOOL PCSC_UnlockCardContext(SCARDCONTEXT hContext)
{
	PCSC_SCARDCONTEXT* pContext;
	pContext = PCSC_GetCardContextData(hContext);

	if (!pContext)
	{
		WLog_ERR(TAG, "PCSC_UnlockCardContext: invalid context (%p)", (void*)hContext);
		return FALSE;
	}

	LeaveCriticalSection(&(pContext->lock));
	return TRUE;
}

static PCSC_SCARDHANDLE* PCSC_GetCardHandleData(SCARDHANDLE hCard)
{
	PCSC_SCARDHANDLE* pCard;

	if (!g_CardHandles)
		return NULL;

	pCard = (PCSC_SCARDHANDLE*)ListDictionary_GetItemValue(g_CardHandles, (void*)hCard);

	if (!pCard)
		return NULL;

	return pCard;
}

static SCARDCONTEXT PCSC_GetCardContextFromHandle(SCARDHANDLE hCard)
{
	PCSC_SCARDHANDLE* pCard;
	pCard = PCSC_GetCardHandleData(hCard);

	if (!pCard)
		return 0;

	return pCard->hSharedContext;
}

static BOOL PCSC_WaitForCardAccess(SCARDCONTEXT hContext, SCARDHANDLE hCard, BOOL shared)
{
	BOOL status = TRUE;
	PCSC_SCARDHANDLE* pCard = NULL;
	PCSC_SCARDCONTEXT* pContext = NULL;

	if (!hCard)
	{
		/* SCardConnect */
		pContext = PCSC_GetCardContextData(hContext);

		if (!pContext)
			return FALSE;

		if (!pContext->owner)
			return TRUE;

		/* wait for card ownership */
		return TRUE;
	}

	pCard = PCSC_GetCardHandleData(hCard);

	if (!pCard)
		return FALSE;

	shared = pCard->shared;
	hContext = pCard->hSharedContext;
	pContext = PCSC_GetCardContextData(hContext);

	if (!pContext)
		return FALSE;

	if (!pContext->owner)
	{
		/* card is not owned */
		if (!shared)
			pContext->owner = hCard;

		return TRUE;
	}

	if (pContext->owner == hCard)
	{
		/* already card owner */
	}
	else
	{
		/* wait for card ownership */
	}

	return status;
}

static BOOL PCSC_ReleaseCardAccess(SCARDCONTEXT hContext, SCARDHANDLE hCard)
{
	PCSC_SCARDHANDLE* pCard = NULL;
	PCSC_SCARDCONTEXT* pContext = NULL;

	if (!hCard)
	{
		/* release current owner */
		pContext = PCSC_GetCardContextData(hContext);

		if (!pContext)
			return FALSE;

		hCard = pContext->owner;

		if (!hCard)
			return TRUE;

		pCard = PCSC_GetCardHandleData(hCard);

		if (!pCard)
			return FALSE;

		/* release card ownership */
		pContext->owner = 0;
		return TRUE;
	}

	pCard = PCSC_GetCardHandleData(hCard);

	if (!pCard)
		return FALSE;

	hContext = pCard->hSharedContext;
	pContext = PCSC_GetCardContextData(hContext);

	if (!pContext)
		return FALSE;

	if (pContext->owner == hCard)
	{
		/* release card ownership */
		pContext->owner = 0;
	}

	return TRUE;
}

static PCSC_SCARDHANDLE* PCSC_ConnectCardHandle(SCARDCONTEXT hSharedContext, SCARDHANDLE hCard)
{
	PCSC_SCARDHANDLE* pCard;
	PCSC_SCARDCONTEXT* pContext;
	pContext = PCSC_GetCardContextData(hSharedContext);

	if (!pContext)
	{
		WLog_ERR(TAG, "PCSC_ConnectCardHandle: null pContext!");
		return NULL;
	}

	pCard = (PCSC_SCARDHANDLE*)calloc(1, sizeof(PCSC_SCARDHANDLE));

	if (!pCard)
		return NULL;

	pCard->hSharedContext = hSharedContext;

	if (!g_CardHandles)
	{
		g_CardHandles = ListDictionary_New(TRUE);

		if (!g_CardHandles)
			goto error;
	}

	if (!ListDictionary_Add(g_CardHandles, (void*)hCard, (void*)pCard))
		goto error;

	pContext->dwCardHandleCount++;
	return pCard;
error:
	free(pCard);
	return NULL;
}

static void PCSC_DisconnectCardHandle(SCARDHANDLE hCard)
{
	PCSC_SCARDHANDLE* pCard;
	PCSC_SCARDCONTEXT* pContext;
	pCard = PCSC_GetCardHandleData(hCard);

	if (!pCard)
		return;

	pContext = PCSC_GetCardContextData(pCard->hSharedContext);
	free(pCard);

	if (!g_CardHandles)
		return;

	ListDictionary_Remove(g_CardHandles, (void*)hCard);

	if (!pContext)
	{
		WLog_ERR(TAG, "PCSC_DisconnectCardHandle: null pContext!");
		return;
	}

	pContext->dwCardHandleCount--;
}

static BOOL PCSC_AddMemoryBlock(SCARDCONTEXT hContext, void* pvMem)
{
	if (!g_MemoryBlocks)
	{
		g_MemoryBlocks = ListDictionary_New(TRUE);

		if (!g_MemoryBlocks)
			return FALSE;
	}

	return ListDictionary_Add(g_MemoryBlocks, pvMem, (void*)hContext);
}

static void* PCSC_RemoveMemoryBlock(SCARDCONTEXT hContext, void* pvMem)
{
	WINPR_UNUSED(hContext);

	if (!g_MemoryBlocks)
		return NULL;

	return ListDictionary_Remove(g_MemoryBlocks, pvMem);
}

/**
 * Standard Windows Smart Card API (PCSC)
 */

static LONG WINAPI PCSC_SCardEstablishContext_Internal(DWORD dwScope, LPCVOID pvReserved1,
                                                       LPCVOID pvReserved2,
                                                       LPSCARDCONTEXT phContext)
{
	WINPR_UNUSED(dwScope); /* SCARD_SCOPE_SYSTEM is the only scope supported by pcsc-lite */
	PCSC_LONG status = SCARD_S_SUCCESS;

	if (!g_PCSC.pfnSCardEstablishContext)
		return PCSC_SCard_LogError("g_PCSC.pfnSCardEstablishContext");

	status =
	    g_PCSC.pfnSCardEstablishContext(SCARD_SCOPE_SYSTEM, pvReserved1, pvReserved2, phContext);
	return PCSC_MapErrorCodeToWinSCard(status);
}

static LONG WINAPI PCSC_SCardEstablishContext(DWORD dwScope, LPCVOID pvReserved1,
                                              LPCVOID pvReserved2, LPSCARDCONTEXT phContext)
{
	LONG status;

	status = PCSC_SCardEstablishContext_Internal(dwScope, pvReserved1, pvReserved2, phContext);

	if (status == SCARD_S_SUCCESS)
		PCSC_EstablishCardContext(*phContext);

	return status;
}

static LONG WINAPI PCSC_SCardReleaseContext_Internal(SCARDCONTEXT hContext)
{
	PCSC_LONG status = SCARD_S_SUCCESS;

	if (!g_PCSC.pfnSCardReleaseContext)
		return PCSC_SCard_LogError("g_PCSC.pfnSCardReleaseContext");

	if (!hContext)
	{
		WLog_ERR(TAG, "SCardReleaseContext: null hContext");
		return PCSC_MapErrorCodeToWinSCard(status);
	}

	status = g_PCSC.pfnSCardReleaseContext(hContext);
	return PCSC_MapErrorCodeToWinSCard(status);
}

static LONG WINAPI PCSC_SCardReleaseContext(SCARDCONTEXT hContext)
{
	LONG status = SCARD_S_SUCCESS;

	status = PCSC_SCardReleaseContext_Internal(hContext);

	if (status != SCARD_S_SUCCESS)
		PCSC_ReleaseCardContext(hContext);

	return status;
}

static LONG WINAPI PCSC_SCardIsValidContext(SCARDCONTEXT hContext)
{
	PCSC_LONG status = SCARD_S_SUCCESS;

	if (!g_PCSC.pfnSCardIsValidContext)
		return PCSC_SCard_LogError("g_PCSC.pfnSCardIsValidContext");

	status = g_PCSC.pfnSCardIsValidContext(hContext);
	return PCSC_MapErrorCodeToWinSCard(status);
}

static LONG WINAPI PCSC_SCardListReaderGroups_Internal(SCARDCONTEXT hContext, LPSTR mszGroups,
                                                       LPDWORD pcchGroups)
{
	PCSC_LONG status = SCARD_S_SUCCESS;
	BOOL pcchGroupsAlloc = FALSE;
	PCSC_DWORD pcsc_cchGroups = 0;

	if (!pcchGroups)
		return SCARD_E_INVALID_PARAMETER;

	if (!g_PCSC.pfnSCardListReaderGroups)
		return PCSC_SCard_LogError("g_PCSC.pfnSCardListReaderGroups");

	if (*pcchGroups == SCARD_AUTOALLOCATE)
		pcchGroupsAlloc = TRUE;

	pcsc_cchGroups = pcchGroupsAlloc ? PCSC_SCARD_AUTOALLOCATE : (PCSC_DWORD)*pcchGroups;

	if (pcchGroupsAlloc && !g_SCardAutoAllocate)
	{
		pcsc_cchGroups = 0;
		status = g_PCSC.pfnSCardListReaderGroups(hContext, NULL, &pcsc_cchGroups);

		if (status == SCARD_S_SUCCESS)
		{
			LPSTR tmp = calloc(1, pcsc_cchGroups);

			if (!tmp)
				return SCARD_E_NO_MEMORY;

			status = g_PCSC.pfnSCardListReaderGroups(hContext, tmp, &pcsc_cchGroups);

			if (status != SCARD_S_SUCCESS)
			{
				free(tmp);
				tmp = NULL;
			}
			else
				PCSC_AddMemoryBlock(hContext, tmp);

			*(LPSTR*)mszGroups = tmp;
		}
	}
	else
	{
		status = g_PCSC.pfnSCardListReaderGroups(hContext, mszGroups, &pcsc_cchGroups);
	}

	*pcchGroups = (DWORD)pcsc_cchGroups;
	return PCSC_MapErrorCodeToWinSCard(status);
}

static LONG WINAPI PCSC_SCardListReaderGroupsA(SCARDCONTEXT hContext, LPSTR mszGroups,
                                               LPDWORD pcchGroups)
{
	LONG status = SCARD_S_SUCCESS;

	if (!g_PCSC.pfnSCardListReaderGroups)
		return PCSC_SCard_LogError("g_PCSC.pfnSCardListReaderGroups");

	if (!PCSC_LockCardContext(hContext))
		return SCARD_E_INVALID_HANDLE;

	status = PCSC_SCardListReaderGroups_Internal(hContext, mszGroups, pcchGroups);

	if (!PCSC_UnlockCardContext(hContext))
		return SCARD_E_INVALID_HANDLE;

	return status;
}

static LONG WINAPI PCSC_SCardListReaderGroupsW(SCARDCONTEXT hContext, LPWSTR mszGroups,
                                               LPDWORD pcchGroups)
{
	LPSTR mszGroupsA = NULL;
	LPSTR* pMszGroupsA = &mszGroupsA;
	LONG status = SCARD_S_SUCCESS;

	if (!g_PCSC.pfnSCardListReaderGroups)
		return PCSC_SCard_LogError("g_PCSC.pfnSCardListReaderGroups");

	if (!PCSC_LockCardContext(hContext))
		return SCARD_E_INVALID_HANDLE;

	status = PCSC_SCardListReaderGroups_Internal(hContext, (LPSTR)&mszGroupsA, pcchGroups);

	if (status == SCARD_S_SUCCESS)
	{
		size_t size = 0;
		WCHAR* str = ConvertMszUtf8NToWCharAlloc(*pMszGroupsA, *pcchGroups, &size);
		if (!str)
			return SCARD_E_NO_MEMORY;
		*(WCHAR**)mszGroups = str;
		*pcchGroups = (DWORD)size;
		PCSC_AddMemoryBlock(hContext, str);
		PCSC_SCardFreeMemory_Internal(hContext, *pMszGroupsA);
	}

	if (!PCSC_UnlockCardContext(hContext))
		return SCARD_E_INVALID_HANDLE;

	return status;
}

static LONG WINAPI PCSC_SCardListReaders_Internal(SCARDCONTEXT hContext, LPCSTR mszGroups,
                                                  LPSTR mszReaders, LPDWORD pcchReaders)
{
	PCSC_LONG status = SCARD_S_SUCCESS;
	BOOL pcchReadersAlloc = FALSE;
	PCSC_DWORD pcsc_cchReaders = 0;
	if (!pcchReaders)
		return SCARD_E_INVALID_PARAMETER;

	if (!g_PCSC.pfnSCardListReaders)
		return PCSC_SCard_LogError("g_PCSC.pfnSCardListReaders");

	mszGroups = NULL; /* mszGroups is not supported by pcsc-lite */

	if (*pcchReaders == SCARD_AUTOALLOCATE)
		pcchReadersAlloc = TRUE;

	pcsc_cchReaders = pcchReadersAlloc ? PCSC_SCARD_AUTOALLOCATE : (PCSC_DWORD)*pcchReaders;

	if (pcchReadersAlloc && !g_SCardAutoAllocate)
	{
		pcsc_cchReaders = 0;
		status = g_PCSC.pfnSCardListReaders(hContext, mszGroups, NULL, &pcsc_cchReaders);

		if (status == SCARD_S_SUCCESS)
		{
			char* tmp = calloc(1, pcsc_cchReaders);

			if (!tmp)
				return SCARD_E_NO_MEMORY;

			status = g_PCSC.pfnSCardListReaders(hContext, mszGroups, tmp, &pcsc_cchReaders);

			if (status != SCARD_S_SUCCESS)
			{
				free(tmp);
				tmp = NULL;
			}
			else
				PCSC_AddMemoryBlock(hContext, tmp);

			*(char**)mszReaders = tmp;
		}
	}
	else
	{
		status = g_PCSC.pfnSCardListReaders(hContext, mszGroups, mszReaders, &pcsc_cchReaders);
	}

	*pcchReaders = (DWORD)pcsc_cchReaders;
	return PCSC_MapErrorCodeToWinSCard(status);
}

static LONG WINAPI PCSC_SCardListReadersA(SCARDCONTEXT hContext, LPCSTR mszGroups, LPSTR mszReaders,
                                          LPDWORD pcchReaders)
{
	BOOL nullCardContext = FALSE;
	LONG status = SCARD_S_SUCCESS;

	if (!g_PCSC.pfnSCardListReaders)
		return PCSC_SCard_LogError("g_PCSC.pfnSCardListReaders");

	if (!hContext)
	{
		status = PCSC_SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);

		if (status != SCARD_S_SUCCESS)
			return status;

		nullCardContext = TRUE;
	}

	if (!PCSC_LockCardContext(hContext))
		return SCARD_E_INVALID_HANDLE;

	status = PCSC_SCardListReaders_Internal(hContext, mszGroups, mszReaders, pcchReaders);

	if (!PCSC_UnlockCardContext(hContext))
		return SCARD_E_INVALID_HANDLE;

	if (nullCardContext)
	{
		status = PCSC_SCardReleaseContext(hContext);
	}

	return status;
}

static LONG WINAPI PCSC_SCardListReadersW(SCARDCONTEXT hContext, LPCWSTR mszGroups,
                                          LPWSTR mszReaders, LPDWORD pcchReaders)
{
	LPSTR mszGroupsA = NULL;
	LPSTR mszReadersA = NULL;
	LONG status = SCARD_S_SUCCESS;
	BOOL nullCardContext = FALSE;

	if (!g_PCSC.pfnSCardListReaders)
		return PCSC_SCard_LogError("g_PCSC.pfnSCardListReaders");

	if (!hContext)
	{
		status = PCSC_SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);

		if (status != SCARD_S_SUCCESS)
			return status;

		nullCardContext = TRUE;
	}

	if (!PCSC_LockCardContext(hContext))
		return SCARD_E_INVALID_HANDLE;

	mszGroups = NULL; /* mszGroups is not supported by pcsc-lite */

	if (mszGroups)
	{
		mszGroupsA = ConvertWCharToUtf8Alloc(mszGroups, NULL);
		if (!mszGroups)
			return SCARD_E_NO_MEMORY;
	}

	status =
	    PCSC_SCardListReaders_Internal(hContext, mszGroupsA, (LPSTR*)&mszReadersA, pcchReaders);
	if (status == SCARD_S_SUCCESS)
	{
		size_t size = 0;
		WCHAR* str = ConvertMszUtf8NToWCharAlloc(mszReadersA, *pcchReaders, &size);
		PCSC_SCardFreeMemory_Internal(hContext, mszReadersA);
		if (!str || (size > UINT32_MAX))
		{
			free(mszGroupsA);
			return SCARD_E_NO_MEMORY;
		}
		*(LPWSTR*)mszReaders = str;
		*pcchReaders = (DWORD)size;
		PCSC_AddMemoryBlock(hContext, str);
	}

	free(mszGroupsA);

	if (!PCSC_UnlockCardContext(hContext))
		return SCARD_E_INVALID_HANDLE;

	if (nullCardContext)
	{
		status = PCSC_SCardReleaseContext(hContext);
	}

	return status;
}

typedef struct
{
	BYTE atr[64];
	size_t atrLen;
	const char* cardName;
} PcscKnownAtr;

static PcscKnownAtr knownAtrs[] = {
	/* Yubico YubiKey 5 NFC (PKI) */
	{ { 0x3B, 0xFD, 0x13, 0x00, 0x00, 0x81, 0x31, 0xFE, 0x15, 0x80, 0x73, 0xC0,
	    0x21, 0xC0, 0x57, 0x59, 0x75, 0x62, 0x69, 0x4B, 0x65, 0x79, 0x40 },
	  23,
	  "NIST SP 800-73 [PIV]" },
	/* PIVKey C910 PKI Smart Card (eID) */
	{ { 0x3B, 0xFC, 0x18, 0x00, 0x00, 0x81, 0x31, 0x80, 0x45, 0x90, 0x67,
	    0x46, 0x4A, 0x00, 0x64, 0x16, 0x06, 0xF2, 0x72, 0x7E, 0x00, 0xE0 },
	  22,
	  "PIVKey Feitian (E0)" }
};

#ifndef ARRAY_LENGTH
#define ARRAY_LENGTH(a) (sizeof(a) / sizeof(a)[0])
#endif

static const char* findCardByAtr(LPCBYTE pbAtr)
{
	size_t i;

	for (i = 0; i < ARRAY_LENGTH(knownAtrs); i++)
	{
		if (memcmp(knownAtrs[i].atr, pbAtr, knownAtrs[i].atrLen) == 0)
			return knownAtrs[i].cardName;
	}

	return NULL;
}

static LONG WINAPI PCSC_SCardListCardsA(SCARDCONTEXT hContext, LPCBYTE pbAtr,
                                        LPCGUID rgquidInterfaces, DWORD cguidInterfaceCount,
                                        CHAR* mszCards, LPDWORD pcchCards)
{
	const char* cardName = NULL;
	DWORD outputLen = 1;
	CHAR* output = NULL;
	BOOL autoAllocate;

	if (!pbAtr || rgquidInterfaces || cguidInterfaceCount)
		return SCARD_E_UNSUPPORTED_FEATURE;

	if (!pcchCards)
		return SCARD_E_INVALID_PARAMETER;

	autoAllocate = (*pcchCards == SCARD_AUTOALLOCATE);

	cardName = findCardByAtr(pbAtr);
	if (cardName)
		outputLen += strlen(cardName) + 1;

	*pcchCards = outputLen;
	if (autoAllocate)
	{
		output = malloc(outputLen);
		if (!output)
			return SCARD_E_NO_MEMORY;

		*((LPSTR*)mszCards) = output;
	}
	else
	{
		if (!mszCards)
			return SCARD_S_SUCCESS;

		if (*pcchCards < outputLen)
			return SCARD_E_INSUFFICIENT_BUFFER;

		output = mszCards;
	}

	if (cardName)
	{
		size_t toCopy = strlen(cardName) + 1;
		memcpy(output, cardName, toCopy);
		output += toCopy;
	}

	*output = '\0';

	return SCARD_S_SUCCESS;
}

static LONG WINAPI PCSC_SCardListCardsW(SCARDCONTEXT hContext, LPCBYTE pbAtr,
                                        LPCGUID rgquidInterfaces, DWORD cguidInterfaceCount,
                                        WCHAR* mszCards, LPDWORD pcchCards)
{
	const char* cardName = NULL;
	DWORD outputLen = 1;
	WCHAR* output = NULL;
	BOOL autoAllocate;

	if (!pbAtr || rgquidInterfaces || cguidInterfaceCount)
		return SCARD_E_UNSUPPORTED_FEATURE;

	if (!pcchCards)
		return SCARD_E_INVALID_PARAMETER;

	autoAllocate = (*pcchCards == SCARD_AUTOALLOCATE);

	cardName = findCardByAtr(pbAtr);
	if (cardName)
		outputLen += strlen(cardName) + 1;

	*pcchCards = outputLen;
	if (autoAllocate)
	{
		output = malloc(outputLen * 2);
		if (!output)
			return SCARD_E_NO_MEMORY;

		*((LPWSTR*)mszCards) = output;
	}
	else
	{
		if (!mszCards)
			return SCARD_S_SUCCESS;

		if (*pcchCards < outputLen)
			return SCARD_E_INSUFFICIENT_BUFFER;

		output = mszCards;
	}

	if (cardName)
	{
		size_t toCopy = strlen(cardName) + 1;
		if (ConvertUtf8ToWChar(cardName, output, toCopy) < 0)
			return SCARD_F_INTERNAL_ERROR;
		output += toCopy;
	}

	*output = 0;

	return SCARD_S_SUCCESS;
}

static LONG WINAPI PCSC_SCardListInterfacesA(SCARDCONTEXT hContext, LPCSTR szCard,
                                             LPGUID pguidInterfaces, LPDWORD pcguidInterfaces)
{
	WINPR_UNUSED(hContext);
	WINPR_UNUSED(szCard);
	WINPR_UNUSED(pguidInterfaces);
	WINPR_UNUSED(pcguidInterfaces);
	return SCARD_E_UNSUPPORTED_FEATURE;
}

static LONG WINAPI PCSC_SCardListInterfacesW(SCARDCONTEXT hContext, LPCWSTR szCard,
                                             LPGUID pguidInterfaces, LPDWORD pcguidInterfaces)
{
	WINPR_UNUSED(hContext);
	WINPR_UNUSED(szCard);
	WINPR_UNUSED(pguidInterfaces);
	WINPR_UNUSED(pcguidInterfaces);
	return SCARD_E_UNSUPPORTED_FEATURE;
}

static LONG WINAPI PCSC_SCardGetProviderIdA(SCARDCONTEXT hContext, LPCSTR szCard,
                                            LPGUID pguidProviderId)
{
	WINPR_UNUSED(hContext);
	WINPR_UNUSED(szCard);
	WINPR_UNUSED(pguidProviderId);
	return SCARD_E_UNSUPPORTED_FEATURE;
}

static LONG WINAPI PCSC_SCardGetProviderIdW(SCARDCONTEXT hContext, LPCWSTR szCard,
                                            LPGUID pguidProviderId)
{
	WINPR_UNUSED(hContext);
	WINPR_UNUSED(szCard);
	WINPR_UNUSED(pguidProviderId);
	return SCARD_E_UNSUPPORTED_FEATURE;
}

static LONG WINAPI PCSC_SCardGetCardTypeProviderNameA(SCARDCONTEXT hContext, LPCSTR szCardName,
                                                      DWORD dwProviderId, CHAR* szProvider,
                                                      LPDWORD pcchProvider)
{
	WINPR_UNUSED(hContext);
	WINPR_UNUSED(szCardName);
	WINPR_UNUSED(dwProviderId);
	WINPR_UNUSED(szProvider);
	WINPR_UNUSED(pcchProvider);
	return SCARD_E_UNSUPPORTED_FEATURE;
}

static LONG WINAPI PCSC_SCardGetCardTypeProviderNameW(SCARDCONTEXT hContext, LPCWSTR szCardName,
                                                      DWORD dwProviderId, WCHAR* szProvider,
                                                      LPDWORD pcchProvider)
{
	WINPR_UNUSED(hContext);
	WINPR_UNUSED(szCardName);
	WINPR_UNUSED(dwProviderId);
	WINPR_UNUSED(szProvider);
	WINPR_UNUSED(pcchProvider);
	return SCARD_E_UNSUPPORTED_FEATURE;
}

static LONG WINAPI PCSC_SCardIntroduceReaderGroupA(SCARDCONTEXT hContext, LPCSTR szGroupName)
{
	WINPR_UNUSED(hContext);
	WINPR_UNUSED(szGroupName);
	return SCARD_E_UNSUPPORTED_FEATURE;
}

static LONG WINAPI PCSC_SCardIntroduceReaderGroupW(SCARDCONTEXT hContext, LPCWSTR szGroupName)
{
	WINPR_UNUSED(hContext);
	WINPR_UNUSED(szGroupName);
	return SCARD_E_UNSUPPORTED_FEATURE;
}

static LONG WINAPI PCSC_SCardForgetReaderGroupA(SCARDCONTEXT hContext, LPCSTR szGroupName)
{
	WINPR_UNUSED(hContext);
	WINPR_UNUSED(szGroupName);
	return SCARD_E_UNSUPPORTED_FEATURE;
}

static LONG WINAPI PCSC_SCardForgetReaderGroupW(SCARDCONTEXT hContext, LPCWSTR szGroupName)
{
	WINPR_UNUSED(hContext);
	WINPR_UNUSED(szGroupName);
	return SCARD_E_UNSUPPORTED_FEATURE;
}

static LONG WINAPI PCSC_SCardIntroduceReaderA(SCARDCONTEXT hContext, LPCSTR szReaderName,
                                              LPCSTR szDeviceName)
{
	WINPR_UNUSED(hContext);
	WINPR_UNUSED(szReaderName);
	WINPR_UNUSED(szDeviceName);
	return SCARD_E_UNSUPPORTED_FEATURE;
}

static LONG WINAPI PCSC_SCardIntroduceReaderW(SCARDCONTEXT hContext, LPCWSTR szReaderName,
                                              LPCWSTR szDeviceName)
{
	WINPR_UNUSED(hContext);
	WINPR_UNUSED(szReaderName);
	WINPR_UNUSED(szDeviceName);
	return SCARD_E_UNSUPPORTED_FEATURE;
}

static LONG WINAPI PCSC_SCardForgetReaderA(SCARDCONTEXT hContext, LPCSTR szReaderName)
{
	WINPR_UNUSED(hContext);
	WINPR_UNUSED(szReaderName);
	return SCARD_E_UNSUPPORTED_FEATURE;
}

static LONG WINAPI PCSC_SCardForgetReaderW(SCARDCONTEXT hContext, LPCWSTR szReaderName)
{
	WINPR_UNUSED(hContext);
	WINPR_UNUSED(szReaderName);
	return SCARD_E_UNSUPPORTED_FEATURE;
}

static LONG WINAPI PCSC_SCardAddReaderToGroupA(SCARDCONTEXT hContext, LPCSTR szReaderName,
                                               LPCSTR szGroupName)
{
	WINPR_UNUSED(hContext);
	WINPR_UNUSED(szReaderName);
	WINPR_UNUSED(szGroupName);
	return SCARD_E_UNSUPPORTED_FEATURE;
}

static LONG WINAPI PCSC_SCardAddReaderToGroupW(SCARDCONTEXT hContext, LPCWSTR szReaderName,
                                               LPCWSTR szGroupName)
{
	WINPR_UNUSED(hContext);
	WINPR_UNUSED(szReaderName);
	WINPR_UNUSED(szGroupName);
	return SCARD_E_UNSUPPORTED_FEATURE;
}

static LONG WINAPI PCSC_SCardRemoveReaderFromGroupA(SCARDCONTEXT hContext, LPCSTR szReaderName,
                                                    LPCSTR szGroupName)
{
	WINPR_UNUSED(hContext);
	WINPR_UNUSED(szReaderName);
	WINPR_UNUSED(szGroupName);
	return SCARD_E_UNSUPPORTED_FEATURE;
}

static LONG WINAPI PCSC_SCardRemoveReaderFromGroupW(SCARDCONTEXT hContext, LPCWSTR szReaderName,
                                                    LPCWSTR szGroupName)
{
	WINPR_UNUSED(hContext);
	WINPR_UNUSED(szReaderName);
	WINPR_UNUSED(szGroupName);
	return SCARD_E_UNSUPPORTED_FEATURE;
}

static LONG WINAPI PCSC_SCardIntroduceCardTypeA(SCARDCONTEXT hContext, LPCSTR szCardName,
                                                LPCGUID pguidPrimaryProvider,
                                                LPCGUID rgguidInterfaces, DWORD dwInterfaceCount,
                                                LPCBYTE pbAtr, LPCBYTE pbAtrMask, DWORD cbAtrLen)
{
	WINPR_UNUSED(hContext);
	WINPR_UNUSED(szCardName);
	WINPR_UNUSED(pguidPrimaryProvider);
	WINPR_UNUSED(rgguidInterfaces);
	WINPR_UNUSED(dwInterfaceCount);
	WINPR_UNUSED(pbAtr);
	WINPR_UNUSED(pbAtrMask);
	WINPR_UNUSED(cbAtrLen);
	return SCARD_E_UNSUPPORTED_FEATURE;
}

static LONG WINAPI PCSC_SCardIntroduceCardTypeW(SCARDCONTEXT hContext, LPCWSTR szCardName,
                                                LPCGUID pguidPrimaryProvider,
                                                LPCGUID rgguidInterfaces, DWORD dwInterfaceCount,
                                                LPCBYTE pbAtr, LPCBYTE pbAtrMask, DWORD cbAtrLen)
{
	WINPR_UNUSED(hContext);
	WINPR_UNUSED(szCardName);
	WINPR_UNUSED(pguidPrimaryProvider);
	WINPR_UNUSED(rgguidInterfaces);
	WINPR_UNUSED(dwInterfaceCount);
	WINPR_UNUSED(pbAtr);
	WINPR_UNUSED(pbAtrMask);
	WINPR_UNUSED(cbAtrLen);
	return SCARD_E_UNSUPPORTED_FEATURE;
}

static LONG WINAPI PCSC_SCardSetCardTypeProviderNameA(SCARDCONTEXT hContext, LPCSTR szCardName,
                                                      DWORD dwProviderId, LPCSTR szProvider)
{
	WINPR_UNUSED(hContext);
	WINPR_UNUSED(szCardName);
	WINPR_UNUSED(dwProviderId);
	WINPR_UNUSED(szProvider);
	return SCARD_E_UNSUPPORTED_FEATURE;
}

static LONG WINAPI PCSC_SCardSetCardTypeProviderNameW(SCARDCONTEXT hContext, LPCWSTR szCardName,
                                                      DWORD dwProviderId, LPCWSTR szProvider)
{
	WINPR_UNUSED(hContext);
	WINPR_UNUSED(szCardName);
	WINPR_UNUSED(dwProviderId);
	WINPR_UNUSED(szProvider);
	return SCARD_E_UNSUPPORTED_FEATURE;
}

static LONG WINAPI PCSC_SCardForgetCardTypeA(SCARDCONTEXT hContext, LPCSTR szCardName)
{
	WINPR_UNUSED(hContext);
	WINPR_UNUSED(szCardName);
	return SCARD_E_UNSUPPORTED_FEATURE;
}

static LONG WINAPI PCSC_SCardForgetCardTypeW(SCARDCONTEXT hContext, LPCWSTR szCardName)
{
	WINPR_UNUSED(hContext);
	WINPR_UNUSED(szCardName);
	return SCARD_E_UNSUPPORTED_FEATURE;
}

static LONG WINAPI PCSC_SCardFreeMemory_Internal(SCARDCONTEXT hContext, LPVOID pvMem)
{
	PCSC_LONG status = SCARD_S_SUCCESS;

	if (PCSC_RemoveMemoryBlock(hContext, pvMem))
	{
		free((void*)pvMem);
		status = SCARD_S_SUCCESS;
	}
	else
	{
		if (g_PCSC.pfnSCardFreeMemory)
		{
			status = g_PCSC.pfnSCardFreeMemory(hContext, pvMem);
		}
	}

	return PCSC_MapErrorCodeToWinSCard(status);
}

static LONG WINAPI PCSC_SCardFreeMemory(SCARDCONTEXT hContext, LPVOID pvMem)
{
	LONG status = SCARD_S_SUCCESS;

	if (hContext)
	{
		if (!PCSC_LockCardContext(hContext))
			return SCARD_E_INVALID_HANDLE;
	}

	status = PCSC_SCardFreeMemory_Internal(hContext, pvMem);

	if (hContext)
	{
		if (!PCSC_UnlockCardContext(hContext))
			return SCARD_E_INVALID_HANDLE;
	}

	return status;
}

static HANDLE WINAPI PCSC_SCardAccessStartedEvent(void)
{
	LONG status = 0;
	SCARDCONTEXT hContext = 0;

	status = PCSC_SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);

	if (status != SCARD_S_SUCCESS)
		return NULL;

	status = PCSC_SCardReleaseContext(hContext);

	if (status != SCARD_S_SUCCESS)
		return NULL;

	if (!g_StartedEvent)
	{
		if (!(g_StartedEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
			return NULL;

		if (!SetEvent(g_StartedEvent))
		{
			CloseHandle(g_StartedEvent);
			return NULL;
		}
	}

	g_StartedEventRefCount++;
	return g_StartedEvent;
}

static void WINAPI PCSC_SCardReleaseStartedEvent(void)
{
	g_StartedEventRefCount--;

	if (g_StartedEventRefCount == 0)
	{
		if (g_StartedEvent)
		{
			CloseHandle(g_StartedEvent);
			g_StartedEvent = NULL;
		}
	}
}

static LONG WINAPI PCSC_SCardLocateCardsA(SCARDCONTEXT hContext, LPCSTR mszCards,
                                          LPSCARD_READERSTATEA rgReaderStates, DWORD cReaders)
{
	WINPR_UNUSED(hContext);
	WINPR_UNUSED(mszCards);
	WINPR_UNUSED(rgReaderStates);
	WINPR_UNUSED(cReaders);
	return SCARD_E_UNSUPPORTED_FEATURE;
}

static LONG WINAPI PCSC_SCardLocateCardsW(SCARDCONTEXT hContext, LPCWSTR mszCards,
                                          LPSCARD_READERSTATEW rgReaderStates, DWORD cReaders)
{
	WINPR_UNUSED(hContext);
	WINPR_UNUSED(mszCards);
	WINPR_UNUSED(rgReaderStates);
	WINPR_UNUSED(cReaders);
	return SCARD_E_UNSUPPORTED_FEATURE;
}

static LONG WINAPI PCSC_SCardLocateCardsByATRA(SCARDCONTEXT hContext, LPSCARD_ATRMASK rgAtrMasks,
                                               DWORD cAtrs, LPSCARD_READERSTATEA rgReaderStates,
                                               DWORD cReaders)
{
	WINPR_UNUSED(hContext);
	WINPR_UNUSED(rgAtrMasks);
	WINPR_UNUSED(cAtrs);
	WINPR_UNUSED(rgReaderStates);
	WINPR_UNUSED(cReaders);
	return SCARD_E_UNSUPPORTED_FEATURE;
}

static LONG WINAPI PCSC_SCardLocateCardsByATRW(SCARDCONTEXT hContext, LPSCARD_ATRMASK rgAtrMasks,
                                               DWORD cAtrs, LPSCARD_READERSTATEW rgReaderStates,
                                               DWORD cReaders)
{
	WINPR_UNUSED(hContext);
	WINPR_UNUSED(rgAtrMasks);
	WINPR_UNUSED(cAtrs);
	WINPR_UNUSED(rgReaderStates);
	WINPR_UNUSED(cReaders);
	return SCARD_E_UNSUPPORTED_FEATURE;
}

static LONG WINAPI PCSC_SCardGetStatusChange_Internal(SCARDCONTEXT hContext, DWORD dwTimeout,
                                                      LPSCARD_READERSTATEA rgReaderStates,
                                                      DWORD cReaders)
{
	PCSC_DWORD i, j;
	INT64* map;
	PCSC_DWORD cMappedReaders;
	PCSC_SCARD_READERSTATE* states;
	PCSC_LONG status = SCARD_S_SUCCESS;
	PCSC_DWORD pcsc_dwTimeout = (PCSC_DWORD)dwTimeout;
	PCSC_DWORD pcsc_cReaders = (PCSC_DWORD)cReaders;

	if (!g_PCSC.pfnSCardGetStatusChange)
		return PCSC_SCard_LogError("g_PCSC.pfnSCardGetStatusChange");

	if (!cReaders)
		return SCARD_S_SUCCESS;

	/* pcsc-lite interprets value 0 as INFINITE, work around the problem by using value 1 */
	pcsc_dwTimeout = pcsc_dwTimeout ? pcsc_dwTimeout : 1;
	/**
	 * Apple's SmartCard Services (not vanilla pcsc-lite) appears to have trouble with the
	 * "\\\\?PnP?\\Notification" reader name. I am always getting EXC_BAD_ACCESS with it.
	 *
	 * The SmartCard Services tarballs can be found here:
	 * http://opensource.apple.com/tarballs/SmartCardServices/
	 *
	 * The "\\\\?PnP?\\Notification" string cannot be found anywhere in the sources,
	 * while this string is present in the vanilla pcsc-lite sources.
	 *
	 * To work around this apparent lack of "\\\\?PnP?\\Notification" support,
	 * we have to filter rgReaderStates to exclude the special PnP reader name.
	 */
	map = (INT64*)calloc(pcsc_cReaders, sizeof(INT64));

	if (!map)
		return SCARD_E_NO_MEMORY;

	states = (PCSC_SCARD_READERSTATE*)calloc(pcsc_cReaders, sizeof(PCSC_SCARD_READERSTATE));

	if (!states)
	{
		free(map);
		return SCARD_E_NO_MEMORY;
	}

	for (i = j = 0; i < pcsc_cReaders; i++)
	{
		if (!g_PnP_Notification)
		{
			LPSCARD_READERSTATEA reader = &rgReaderStates[i];
			if (!reader->szReader)
				continue;
			if (0 == _stricmp(reader->szReader, SMARTCARD_PNP_NOTIFICATION_A))
			{
				map[i] = -1; /* unmapped */
				continue;
			}
		}

		map[i] = (INT64)j;
		states[j].szReader = rgReaderStates[i].szReader;
		states[j].dwCurrentState = rgReaderStates[i].dwCurrentState;
		states[j].pvUserData = rgReaderStates[i].pvUserData;
		states[j].dwEventState = rgReaderStates[i].dwEventState;
		states[j].cbAtr = rgReaderStates[i].cbAtr;
		CopyMemory(&(states[j].rgbAtr), &(rgReaderStates[i].rgbAtr), PCSC_MAX_ATR_SIZE);
		j++;
	}

	cMappedReaders = j;

	if (cMappedReaders > 0)
	{
		status = g_PCSC.pfnSCardGetStatusChange(hContext, pcsc_dwTimeout, states, cMappedReaders);
	}
	else
	{
		status = SCARD_S_SUCCESS;
	}

	for (i = 0; i < pcsc_cReaders; i++)
	{
		if (map[i] < 0)
			continue; /* unmapped */

		j = (PCSC_DWORD)map[i];
		rgReaderStates[i].dwCurrentState = (DWORD)states[j].dwCurrentState;
		rgReaderStates[i].cbAtr = (DWORD)states[j].cbAtr;
		CopyMemory(&(rgReaderStates[i].rgbAtr), &(states[j].rgbAtr), PCSC_MAX_ATR_SIZE);
		rgReaderStates[i].dwEventState = (DWORD)states[j].dwEventState;
	}

	free(map);
	free(states);
	return PCSC_MapErrorCodeToWinSCard(status);
}

static LONG WINAPI PCSC_SCardGetStatusChangeA(SCARDCONTEXT hContext, DWORD dwTimeout,
                                              LPSCARD_READERSTATEA rgReaderStates, DWORD cReaders)
{
	LONG status = SCARD_S_SUCCESS;

	if (!PCSC_LockCardContext(hContext))
		return SCARD_E_INVALID_HANDLE;

	status = PCSC_SCardGetStatusChange_Internal(hContext, dwTimeout, rgReaderStates, cReaders);

	if (!PCSC_UnlockCardContext(hContext))
		return SCARD_E_INVALID_HANDLE;

	return status;
}

static LONG WINAPI PCSC_SCardGetStatusChangeW(SCARDCONTEXT hContext, DWORD dwTimeout,
                                              LPSCARD_READERSTATEW rgReaderStates, DWORD cReaders)
{
	DWORD index;
	LPSCARD_READERSTATEA states;
	LONG status = SCARD_S_SUCCESS;

	if (!g_PCSC.pfnSCardGetStatusChange)
		return PCSC_SCard_LogError("g_PCSC.pfnSCardGetStatusChange");

	if (!PCSC_LockCardContext(hContext))
		return SCARD_E_INVALID_HANDLE;

	states = (LPSCARD_READERSTATEA)calloc(cReaders, sizeof(SCARD_READERSTATEA));

	if (!states)
	{
		PCSC_UnlockCardContext(hContext);
		return SCARD_E_NO_MEMORY;
	}

	for (index = 0; index < cReaders; index++)
	{
		const LPSCARD_READERSTATEW curReader = &rgReaderStates[index];
		LPSCARD_READERSTATEA cur = &states[index];

		cur->szReader = ConvertWCharToUtf8Alloc(curReader->szReader, NULL);
		cur->pvUserData = curReader->pvUserData;
		cur->dwCurrentState = curReader->dwCurrentState;
		cur->dwEventState = curReader->dwEventState;
		cur->cbAtr = curReader->cbAtr;
		CopyMemory(&(cur->rgbAtr), &(curReader->rgbAtr), ARRAYSIZE(cur->rgbAtr));
	}

	status = PCSC_SCardGetStatusChange_Internal(hContext, dwTimeout, states, cReaders);

	for (index = 0; index < cReaders; index++)
	{
		free((void*)states[index].szReader);
		rgReaderStates[index].pvUserData = states[index].pvUserData;
		rgReaderStates[index].dwCurrentState = states[index].dwCurrentState;
		rgReaderStates[index].dwEventState = states[index].dwEventState;
		rgReaderStates[index].cbAtr = states[index].cbAtr;
		CopyMemory(&(rgReaderStates[index].rgbAtr), &(states[index].rgbAtr), 36);
	}

	free(states);

	if (!PCSC_UnlockCardContext(hContext))
		return SCARD_E_INVALID_HANDLE;

	return status;
}

static LONG WINAPI PCSC_SCardCancel(SCARDCONTEXT hContext)
{
	PCSC_LONG status = SCARD_S_SUCCESS;

	if (!g_PCSC.pfnSCardCancel)
		return PCSC_SCard_LogError("g_PCSC.pfnSCardCancel");

	status = g_PCSC.pfnSCardCancel(hContext);
	return PCSC_MapErrorCodeToWinSCard(status);
}

static LONG WINAPI PCSC_SCardConnect_Internal(SCARDCONTEXT hContext, LPCSTR szReader,
                                              DWORD dwShareMode, DWORD dwPreferredProtocols,
                                              LPSCARDHANDLE phCard, LPDWORD pdwActiveProtocol)
{
	BOOL shared;
	const char* szReaderPCSC;
	PCSC_LONG status = SCARD_S_SUCCESS;
	PCSC_SCARDHANDLE* pCard = NULL;
	PCSC_DWORD pcsc_dwShareMode = (PCSC_DWORD)dwShareMode;
	PCSC_DWORD pcsc_dwPreferredProtocols = 0;
	PCSC_DWORD pcsc_dwActiveProtocol = 0;

	if (!g_PCSC.pfnSCardConnect)
		return PCSC_SCard_LogError("g_PCSC.pfnSCardConnect");

	shared = (dwShareMode == SCARD_SHARE_DIRECT) ? TRUE : FALSE;
	PCSC_WaitForCardAccess(hContext, 0, shared);
	szReaderPCSC = szReader;

	/**
	 * As stated here :
	 * https://pcsclite.alioth.debian.org/api/group__API.html#ga4e515829752e0a8dbc4d630696a8d6a5
	 * SCARD_PROTOCOL_UNDEFINED is valid for dwPreferredProtocols (only) if dwShareMode ==
	 * SCARD_SHARE_DIRECT and allows to send control commands to the reader (with SCardControl())
	 * even if a card is not present in the reader
	 */
	if (pcsc_dwShareMode == SCARD_SHARE_DIRECT && dwPreferredProtocols == SCARD_PROTOCOL_UNDEFINED)
		pcsc_dwPreferredProtocols = SCARD_PROTOCOL_UNDEFINED;
	else
		pcsc_dwPreferredProtocols =
		    (PCSC_DWORD)PCSC_ConvertProtocolsFromWinSCard(dwPreferredProtocols);

	status = g_PCSC.pfnSCardConnect(hContext, szReaderPCSC, pcsc_dwShareMode,
	                                pcsc_dwPreferredProtocols, phCard, &pcsc_dwActiveProtocol);

	if (status == SCARD_S_SUCCESS)
	{
		pCard = PCSC_ConnectCardHandle(hContext, *phCard);
		*pdwActiveProtocol = PCSC_ConvertProtocolsToWinSCard((DWORD)pcsc_dwActiveProtocol);
		pCard->shared = shared;
		PCSC_WaitForCardAccess(hContext, pCard->hSharedContext, shared);
	}

	return PCSC_MapErrorCodeToWinSCard(status);
}

static LONG WINAPI PCSC_SCardConnectA(SCARDCONTEXT hContext, LPCSTR szReader, DWORD dwShareMode,
                                      DWORD dwPreferredProtocols, LPSCARDHANDLE phCard,
                                      LPDWORD pdwActiveProtocol)
{
	LONG status = SCARD_S_SUCCESS;

	if (!PCSC_LockCardContext(hContext))
		return SCARD_E_INVALID_HANDLE;

	status = PCSC_SCardConnect_Internal(hContext, szReader, dwShareMode, dwPreferredProtocols,
	                                    phCard, pdwActiveProtocol);

	if (!PCSC_UnlockCardContext(hContext))
		return SCARD_E_INVALID_HANDLE;

	return status;
}

static LONG WINAPI PCSC_SCardConnectW(SCARDCONTEXT hContext, LPCWSTR szReader, DWORD dwShareMode,
                                      DWORD dwPreferredProtocols, LPSCARDHANDLE phCard,
                                      LPDWORD pdwActiveProtocol)
{
	LPSTR szReaderA = NULL;
	LONG status = SCARD_S_SUCCESS;

	if (!PCSC_LockCardContext(hContext))
		return SCARD_E_INVALID_HANDLE;

	if (szReader)
	{
		szReaderA = ConvertWCharToUtf8Alloc(szReader, NULL);
		if (!szReaderA)
			return SCARD_E_INSUFFICIENT_BUFFER;
	}

	status = PCSC_SCardConnect_Internal(hContext, szReaderA, dwShareMode, dwPreferredProtocols,
	                                    phCard, pdwActiveProtocol);
	free(szReaderA);

	if (!PCSC_UnlockCardContext(hContext))
		return SCARD_E_INVALID_HANDLE;

	return status;
}

static LONG WINAPI PCSC_SCardReconnect(SCARDHANDLE hCard, DWORD dwShareMode,
                                       DWORD dwPreferredProtocols, DWORD dwInitialization,
                                       LPDWORD pdwActiveProtocol)
{
	BOOL shared;
	PCSC_LONG status = SCARD_S_SUCCESS;
	PCSC_DWORD pcsc_dwShareMode = (PCSC_DWORD)dwShareMode;
	PCSC_DWORD pcsc_dwPreferredProtocols = 0;
	PCSC_DWORD pcsc_dwInitialization = (PCSC_DWORD)dwInitialization;
	PCSC_DWORD pcsc_dwActiveProtocol = 0;

	if (!g_PCSC.pfnSCardReconnect)
		return PCSC_SCard_LogError("g_PCSC.pfnSCardReconnect");

	shared = (dwShareMode == SCARD_SHARE_DIRECT) ? TRUE : FALSE;
	PCSC_WaitForCardAccess(0, hCard, shared);
	pcsc_dwPreferredProtocols = (PCSC_DWORD)PCSC_ConvertProtocolsFromWinSCard(dwPreferredProtocols);
	status = g_PCSC.pfnSCardReconnect(hCard, pcsc_dwShareMode, pcsc_dwPreferredProtocols,
	                                  pcsc_dwInitialization, &pcsc_dwActiveProtocol);

	*pdwActiveProtocol = PCSC_ConvertProtocolsToWinSCard((DWORD)pcsc_dwActiveProtocol);
	return PCSC_MapErrorCodeToWinSCard(status);
}

static LONG WINAPI PCSC_SCardDisconnect(SCARDHANDLE hCard, DWORD dwDisposition)
{
	PCSC_LONG status = SCARD_S_SUCCESS;
	PCSC_DWORD pcsc_dwDisposition = (PCSC_DWORD)dwDisposition;

	if (!g_PCSC.pfnSCardDisconnect)
		return PCSC_SCard_LogError("g_PCSC.pfnSCardDisconnect");

	status = g_PCSC.pfnSCardDisconnect(hCard, pcsc_dwDisposition);

	if (status == SCARD_S_SUCCESS)
	{
		PCSC_DisconnectCardHandle(hCard);
	}

	PCSC_ReleaseCardAccess(0, hCard);
	return PCSC_MapErrorCodeToWinSCard(status);
}

static LONG WINAPI PCSC_SCardBeginTransaction(SCARDHANDLE hCard)
{
	PCSC_LONG status = SCARD_S_SUCCESS;
	PCSC_SCARDHANDLE* pCard = NULL;
	PCSC_SCARDCONTEXT* pContext = NULL;

	if (!g_PCSC.pfnSCardBeginTransaction)
		return PCSC_SCard_LogError("g_PCSC.pfnSCardBeginTransaction");

	pCard = PCSC_GetCardHandleData(hCard);

	if (!pCard)
		return SCARD_E_INVALID_HANDLE;

	pContext = PCSC_GetCardContextData(pCard->hSharedContext);

	if (!pContext)
		return SCARD_E_INVALID_HANDLE;

	if (pContext->isTransactionLocked)
		return SCARD_S_SUCCESS; /* disable nested transactions */

	status = g_PCSC.pfnSCardBeginTransaction(hCard);

	pContext->isTransactionLocked = TRUE;
	return PCSC_MapErrorCodeToWinSCard(status);
}

static LONG WINAPI PCSC_SCardEndTransaction(SCARDHANDLE hCard, DWORD dwDisposition)
{
	PCSC_LONG status = SCARD_S_SUCCESS;
	PCSC_SCARDHANDLE* pCard = NULL;
	PCSC_SCARDCONTEXT* pContext = NULL;
	PCSC_DWORD pcsc_dwDisposition = (PCSC_DWORD)dwDisposition;

	if (!g_PCSC.pfnSCardEndTransaction)
		return PCSC_SCard_LogError("g_PCSC.pfnSCardEndTransaction");

	pCard = PCSC_GetCardHandleData(hCard);

	if (!pCard)
		return SCARD_E_INVALID_HANDLE;

	pContext = PCSC_GetCardContextData(pCard->hSharedContext);

	if (!pContext)
		return SCARD_E_INVALID_HANDLE;

	PCSC_ReleaseCardAccess(0, hCard);

	if (!pContext->isTransactionLocked)
		return SCARD_S_SUCCESS; /* disable nested transactions */

	status = g_PCSC.pfnSCardEndTransaction(hCard, pcsc_dwDisposition);

	pContext->isTransactionLocked = FALSE;
	return PCSC_MapErrorCodeToWinSCard(status);
}

static LONG WINAPI PCSC_SCardCancelTransaction(SCARDHANDLE hCard)
{
	WINPR_UNUSED(hCard);
	return SCARD_S_SUCCESS;
}

/*
 * PCSC returns a string but Windows SCardStatus requires the return to be a multi string.
 * Therefore extra length checks and additional buffer allocation is required
 */
static LONG WINAPI PCSC_SCardStatus_Internal(SCARDHANDLE hCard, LPSTR mszReaderNames,
                                             LPDWORD pcchReaderLen, LPDWORD pdwState,
                                             LPDWORD pdwProtocol, LPBYTE pbAtr, LPDWORD pcbAtrLen,
                                             BOOL unicode)
{
	PCSC_SCARDHANDLE* pCard = NULL;
	SCARDCONTEXT hContext;
	PCSC_LONG status;
	PCSC_DWORD pcsc_cchReaderLen = 0;
	PCSC_DWORD pcsc_cbAtrLen = 0;
	PCSC_DWORD pcsc_dwState = 0;
	PCSC_DWORD pcsc_dwProtocol = 0;
	BOOL allocateReader = FALSE;
	BOOL allocateAtr = FALSE;
	LPSTR readerNames = mszReaderNames;
	LPBYTE atr = pbAtr;
	LPSTR tReader = NULL;
	LPBYTE tATR = NULL;

	if (!g_PCSC.pfnSCardStatus)
		return PCSC_SCard_LogError("g_PCSC.pfnSCardStatus");

	pCard = PCSC_GetCardHandleData(hCard);

	if (!pCard)
		return SCARD_E_INVALID_VALUE;

	PCSC_WaitForCardAccess(0, hCard, pCard->shared);
	hContext = PCSC_GetCardContextFromHandle(hCard);

	if (!hContext)
		return SCARD_E_INVALID_VALUE;

	status =
	    g_PCSC.pfnSCardStatus(hCard, NULL, &pcsc_cchReaderLen, NULL, NULL, NULL, &pcsc_cbAtrLen);

	if (status != STATUS_SUCCESS)
		return PCSC_MapErrorCodeToWinSCard(status);

	pcsc_cchReaderLen++;

	if (unicode)
		pcsc_cchReaderLen *= 2;

	if (pcchReaderLen)
	{
		if (*pcchReaderLen == SCARD_AUTOALLOCATE)
			allocateReader = TRUE;
		else if (mszReaderNames && (*pcchReaderLen < pcsc_cchReaderLen))
			return SCARD_E_INSUFFICIENT_BUFFER;
		else
			pcsc_cchReaderLen = *pcchReaderLen;
	}

	if (pcbAtrLen)
	{
		if (*pcbAtrLen == SCARD_AUTOALLOCATE)
			allocateAtr = TRUE;
		else if (pbAtr && (*pcbAtrLen < pcsc_cbAtrLen))
			return SCARD_E_INSUFFICIENT_BUFFER;
		else
			pcsc_cbAtrLen = *pcbAtrLen;
	}

	if (allocateReader && pcsc_cchReaderLen > 0 && mszReaderNames)
	{
#ifdef __MACOSX__

		/**
		 * Workaround for SCardStatus Bug in MAC OS X Yosemite
		 */
		if (OSXVersion == 0x10100000)
			pcsc_cchReaderLen++;

#endif
		tReader = calloc(sizeof(WCHAR), pcsc_cchReaderLen);

		if (!tReader)
		{
			status = ERROR_NOT_ENOUGH_MEMORY;
			goto out_fail;
		}

		readerNames = tReader;
	}

	if (allocateAtr && pcsc_cbAtrLen > 0 && pbAtr)
	{
		tATR = calloc(1, pcsc_cbAtrLen);

		if (!tATR)
		{
			status = ERROR_NOT_ENOUGH_MEMORY;
			goto out_fail;
		}

		atr = tATR;
	}

	status = g_PCSC.pfnSCardStatus(hCard, readerNames, &pcsc_cchReaderLen, &pcsc_dwState,
	                               &pcsc_dwProtocol, atr, &pcsc_cbAtrLen);

	if (status != STATUS_SUCCESS)
		goto out_fail;

	if (tATR)
	{
		PCSC_AddMemoryBlock(hContext, tATR);
		*(BYTE**)pbAtr = tATR;
	}

	if (tReader)
	{
		if (unicode)
		{
			size_t size = 0;
			WCHAR* tmp = ConvertMszUtf8NToWCharAlloc(tReader, pcsc_cchReaderLen + 1, &size);

			if (tmp == NULL)
			{
				status = ERROR_NOT_ENOUGH_MEMORY;
				goto out_fail;
			}

			free(tReader);

			PCSC_AddMemoryBlock(hContext, tmp);
			*(WCHAR**)mszReaderNames = tmp;
		}
		else
		{
			tReader[pcsc_cchReaderLen - 1] = '\0';
			PCSC_AddMemoryBlock(hContext, tReader);
			*(char**)mszReaderNames = tReader;
		}
	}

	pcsc_dwState &= 0xFFFF;

	if (pdwState)
		*pdwState = PCSC_ConvertCardStateToWinSCard((DWORD)pcsc_dwState, status);

	if (pdwProtocol)
		*pdwProtocol = PCSC_ConvertProtocolsToWinSCard((DWORD)pcsc_dwProtocol);

	if (pcbAtrLen)
		*pcbAtrLen = (DWORD)pcsc_cbAtrLen;

	if (pcchReaderLen)
	{
		WINPR_ASSERT(pcsc_cchReaderLen < UINT32_MAX);
		*pcchReaderLen = (DWORD)pcsc_cchReaderLen + 1u;
	}

	return (LONG)status;
out_fail:
	free(tReader);
	free(tATR);
	return (LONG)status;
}

static LONG WINAPI PCSC_SCardState(SCARDHANDLE hCard, LPDWORD pdwState, LPDWORD pdwProtocol,
                                   LPBYTE pbAtr, LPDWORD pcbAtrLen)
{
	DWORD cchReaderLen;
	SCARDCONTEXT hContext = 0;
	LPSTR mszReaderNames = NULL;
	PCSC_LONG status = SCARD_S_SUCCESS;
	PCSC_SCARDHANDLE* pCard = NULL;
	DWORD pcsc_dwState = 0;
	DWORD pcsc_dwProtocol = 0;
	DWORD pcsc_cbAtrLen = 0;

	if (pcbAtrLen)
		pcsc_cbAtrLen = (DWORD)*pcbAtrLen;

	if (!g_PCSC.pfnSCardStatus)
		return PCSC_SCard_LogError("g_PCSC.pfnSCardStatus");

	pCard = PCSC_GetCardHandleData(hCard);

	if (!pCard)
		return SCARD_E_INVALID_VALUE;

	PCSC_WaitForCardAccess(0, hCard, pCard->shared);
	hContext = PCSC_GetCardContextFromHandle(hCard);

	if (!hContext)
		return SCARD_E_INVALID_VALUE;

	cchReaderLen = SCARD_AUTOALLOCATE;
	status = PCSC_SCardStatus_Internal(hCard, (LPSTR)&mszReaderNames, &cchReaderLen, &pcsc_dwState,
	                                   &pcsc_dwProtocol, pbAtr, &pcsc_cbAtrLen, FALSE);

	if (mszReaderNames)
		PCSC_SCardFreeMemory_Internal(hContext, mszReaderNames);

	*pdwState = (DWORD)pcsc_dwState;
	*pdwProtocol = PCSC_ConvertProtocolsToWinSCard((DWORD)pcsc_dwProtocol);
	if (pcbAtrLen)
		*pcbAtrLen = (DWORD)pcsc_cbAtrLen;
	return PCSC_MapErrorCodeToWinSCard(status);
}

static LONG WINAPI PCSC_SCardStatusA(SCARDHANDLE hCard, LPSTR mszReaderNames, LPDWORD pcchReaderLen,
                                     LPDWORD pdwState, LPDWORD pdwProtocol, LPBYTE pbAtr,
                                     LPDWORD pcbAtrLen)
{

	return PCSC_SCardStatus_Internal(hCard, mszReaderNames, pcchReaderLen, pdwState, pdwProtocol,
	                                 pbAtr, pcbAtrLen, FALSE);
}

static LONG WINAPI PCSC_SCardStatusW(SCARDHANDLE hCard, LPWSTR mszReaderNames,
                                     LPDWORD pcchReaderLen, LPDWORD pdwState, LPDWORD pdwProtocol,
                                     LPBYTE pbAtr, LPDWORD pcbAtrLen)
{

	return PCSC_SCardStatus_Internal(hCard, (LPSTR)mszReaderNames, pcchReaderLen, pdwState,
	                                 pdwProtocol, pbAtr, pcbAtrLen, TRUE);
}

static LONG WINAPI PCSC_SCardTransmit(SCARDHANDLE hCard, LPCSCARD_IO_REQUEST pioSendPci,
                                      LPCBYTE pbSendBuffer, DWORD cbSendLength,
                                      LPSCARD_IO_REQUEST pioRecvPci, LPBYTE pbRecvBuffer,
                                      LPDWORD pcbRecvLength)
{
	PCSC_LONG status = SCARD_S_SUCCESS;
	PCSC_SCARDHANDLE* pCard = NULL;
	PCSC_DWORD cbExtraBytes = 0;
	BYTE* pbExtraBytes = NULL;
	BYTE* pcsc_pbExtraBytes = NULL;
	PCSC_DWORD pcsc_cbSendLength = (PCSC_DWORD)cbSendLength;
	PCSC_DWORD pcsc_cbRecvLength = 0;
	union
	{
		const PCSC_SCARD_IO_REQUEST* pcs;
		PCSC_SCARD_IO_REQUEST* ps;
		LPSCARD_IO_REQUEST lps;
		LPCSCARD_IO_REQUEST lpcs;
		BYTE* pb;
	} sendPci, recvPci, inRecvPci, inSendPci;

	sendPci.ps = NULL;
	recvPci.ps = NULL;
	inRecvPci.lps = pioRecvPci;
	inSendPci.lpcs = pioSendPci;

	if (!g_PCSC.pfnSCardTransmit)
		return PCSC_SCard_LogError("g_PCSC.pfnSCardTransmit");

	pCard = PCSC_GetCardHandleData(hCard);

	if (!pCard)
		return SCARD_E_INVALID_VALUE;

	PCSC_WaitForCardAccess(0, hCard, pCard->shared);

	if (!pcbRecvLength)
		return SCARD_E_INVALID_PARAMETER;

	if (*pcbRecvLength == SCARD_AUTOALLOCATE)
		return SCARD_E_INVALID_PARAMETER;

	pcsc_cbRecvLength = (PCSC_DWORD)*pcbRecvLength;

	if (!inSendPci.lpcs)
	{
		PCSC_DWORD dwState = 0;
		PCSC_DWORD cbAtrLen = 0;
		PCSC_DWORD dwProtocol = 0;
		PCSC_DWORD cchReaderLen = 0;
		/**
		 * pcsc-lite cannot have a null pioSendPci parameter, unlike WinSCard.
		 * Query the current protocol and use default SCARD_IO_REQUEST for it.
		 */
		status = g_PCSC.pfnSCardStatus(hCard, NULL, &cchReaderLen, &dwState, &dwProtocol, NULL,
		                               &cbAtrLen);

		if (status == SCARD_S_SUCCESS)
		{
			if (dwProtocol == SCARD_PROTOCOL_T0)
				sendPci.pcs = PCSC_SCARD_PCI_T0;
			else if (dwProtocol == SCARD_PROTOCOL_T1)
				sendPci.pcs = PCSC_SCARD_PCI_T1;
			else if (dwProtocol == PCSC_SCARD_PROTOCOL_RAW)
				sendPci.pcs = PCSC_SCARD_PCI_RAW;
		}
	}
	else
	{
		cbExtraBytes = inSendPci.lpcs->cbPciLength - sizeof(SCARD_IO_REQUEST);
		sendPci.ps = (PCSC_SCARD_IO_REQUEST*)malloc(sizeof(PCSC_SCARD_IO_REQUEST) + cbExtraBytes);

		if (!sendPci.ps)
			return SCARD_E_NO_MEMORY;

		sendPci.ps->dwProtocol = (PCSC_DWORD)inSendPci.lpcs->dwProtocol;
		sendPci.ps->cbPciLength = sizeof(PCSC_SCARD_IO_REQUEST) + cbExtraBytes;
		pbExtraBytes = &(inSendPci.pb)[sizeof(SCARD_IO_REQUEST)];
		pcsc_pbExtraBytes = &(sendPci.pb)[sizeof(PCSC_SCARD_IO_REQUEST)];
		CopyMemory(pcsc_pbExtraBytes, pbExtraBytes, cbExtraBytes);
	}

	if (inRecvPci.lps)
	{
		cbExtraBytes = inRecvPci.lps->cbPciLength - sizeof(SCARD_IO_REQUEST);
		recvPci.ps = (PCSC_SCARD_IO_REQUEST*)malloc(sizeof(PCSC_SCARD_IO_REQUEST) + cbExtraBytes);

		if (!recvPci.ps)
		{
			if (inSendPci.lpcs)
				free(sendPci.ps);

			return SCARD_E_NO_MEMORY;
		}

		recvPci.ps->dwProtocol = (PCSC_DWORD)inRecvPci.lps->dwProtocol;
		recvPci.ps->cbPciLength = sizeof(PCSC_SCARD_IO_REQUEST) + cbExtraBytes;
		pbExtraBytes = &(inRecvPci.pb)[sizeof(SCARD_IO_REQUEST)];
		pcsc_pbExtraBytes = &(recvPci.pb)[sizeof(PCSC_SCARD_IO_REQUEST)];
		CopyMemory(pcsc_pbExtraBytes, pbExtraBytes, cbExtraBytes);
	}

	status = g_PCSC.pfnSCardTransmit(hCard, sendPci.ps, pbSendBuffer, pcsc_cbSendLength, recvPci.ps,
	                                 pbRecvBuffer, &pcsc_cbRecvLength);

	*pcbRecvLength = (DWORD)pcsc_cbRecvLength;

	if (inSendPci.lpcs)
		free(sendPci.ps); /* pcsc_pioSendPci is dynamically allocated only when pioSendPci is
		                          non null */

	if (inRecvPci.lps)
	{
		cbExtraBytes = inRecvPci.lps->cbPciLength - sizeof(SCARD_IO_REQUEST);
		pbExtraBytes = &(inRecvPci.pb)[sizeof(SCARD_IO_REQUEST)];
		pcsc_pbExtraBytes = &(recvPci.pb)[sizeof(PCSC_SCARD_IO_REQUEST)];
		CopyMemory(pbExtraBytes, pcsc_pbExtraBytes, cbExtraBytes); /* copy extra bytes */
		free(recvPci.ps); /* pcsc_pioRecvPci is dynamically allocated only when pioRecvPci is
		                          non null */
	}

	return PCSC_MapErrorCodeToWinSCard(status);
}

static LONG WINAPI PCSC_SCardGetTransmitCount(SCARDHANDLE hCard, LPDWORD pcTransmitCount)
{
	WINPR_UNUSED(pcTransmitCount);
	PCSC_SCARDHANDLE* pCard = NULL;

	pCard = PCSC_GetCardHandleData(hCard);

	if (!pCard)
		return SCARD_E_INVALID_VALUE;

	PCSC_WaitForCardAccess(0, hCard, pCard->shared);
	return SCARD_S_SUCCESS;
}

static LONG WINAPI PCSC_SCardControl(SCARDHANDLE hCard, DWORD dwControlCode, LPCVOID lpInBuffer,
                                     DWORD cbInBufferSize, LPVOID lpOutBuffer,
                                     DWORD cbOutBufferSize, LPDWORD lpBytesReturned)
{
	DWORD IoCtlFunction = 0;
	DWORD IoCtlDeviceType = 0;
	BOOL getFeatureRequest = FALSE;
	PCSC_LONG status = SCARD_S_SUCCESS;
	PCSC_SCARDHANDLE* pCard = NULL;
	PCSC_DWORD pcsc_dwControlCode = 0;
	PCSC_DWORD pcsc_cbInBufferSize = (PCSC_DWORD)cbInBufferSize;
	PCSC_DWORD pcsc_cbOutBufferSize = (PCSC_DWORD)cbOutBufferSize;
	PCSC_DWORD pcsc_BytesReturned = 0;

	if (!g_PCSC.pfnSCardControl)
		return PCSC_SCard_LogError("g_PCSC.pfnSCardControl");

	pCard = PCSC_GetCardHandleData(hCard);

	if (!pCard)
		return SCARD_E_INVALID_VALUE;

	PCSC_WaitForCardAccess(0, hCard, pCard->shared);
	/**
	 * PCSCv2 Part 10:
	 * http://www.pcscworkgroup.com/specifications/files/pcsc10_v2.02.09.pdf
	 *
	 * Smart Card Driver IOCTLs:
	 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff548988/
	 *
	 * Converting Windows Feature Request IOCTL code to the pcsc-lite control code:
	 * http://musclecard.996296.n3.nabble.com/Converting-Windows-Feature-Request-IOCTL-code-to-the-pcsc-lite-control-code-td4906.html
	 */
	IoCtlFunction = FUNCTION_FROM_CTL_CODE(dwControlCode);
	IoCtlDeviceType = DEVICE_TYPE_FROM_CTL_CODE(dwControlCode);

	if (dwControlCode == IOCTL_SMARTCARD_GET_FEATURE_REQUEST)
		getFeatureRequest = TRUE;

	if (IoCtlDeviceType == FILE_DEVICE_SMARTCARD)
		dwControlCode = PCSC_SCARD_CTL_CODE(IoCtlFunction);

	pcsc_dwControlCode = (PCSC_DWORD)dwControlCode;
	status = g_PCSC.pfnSCardControl(hCard, pcsc_dwControlCode, lpInBuffer, pcsc_cbInBufferSize,
	                                lpOutBuffer, pcsc_cbOutBufferSize, &pcsc_BytesReturned);

	*lpBytesReturned = (DWORD)pcsc_BytesReturned;

	if (getFeatureRequest)
	{
		UINT32 index;
		UINT32 count;
		PCSC_TLV_STRUCTURE* tlv = (PCSC_TLV_STRUCTURE*)lpOutBuffer;

		if ((*lpBytesReturned % sizeof(PCSC_TLV_STRUCTURE)) != 0)
			return SCARD_E_UNEXPECTED;

		count = *lpBytesReturned / sizeof(PCSC_TLV_STRUCTURE);

		for (index = 0; index < count; index++)
		{
			if (tlv[index].length != 4)
				return SCARD_E_UNEXPECTED;
		}
	}

	return PCSC_MapErrorCodeToWinSCard(status);
}

static LONG WINAPI PCSC_SCardGetAttrib_Internal(SCARDHANDLE hCard, DWORD dwAttrId, LPBYTE pbAttr,
                                                LPDWORD pcbAttrLen)
{
	SCARDCONTEXT hContext = 0;
	BOOL pcbAttrLenAlloc = FALSE;
	PCSC_LONG status = SCARD_S_SUCCESS;
	PCSC_SCARDHANDLE* pCard = NULL;
	PCSC_DWORD pcsc_dwAttrId = (PCSC_DWORD)dwAttrId;
	PCSC_DWORD pcsc_cbAttrLen = 0;

	if (!g_PCSC.pfnSCardGetAttrib)
		return PCSC_SCard_LogError("g_PCSC.pfnSCardGetAttrib");

	pCard = PCSC_GetCardHandleData(hCard);

	if (!pCard)
		return SCARD_E_INVALID_VALUE;

	PCSC_WaitForCardAccess(0, hCard, pCard->shared);
	hContext = PCSC_GetCardContextFromHandle(hCard);

	if (!hContext)
		return SCARD_E_INVALID_HANDLE;

	if (!pcbAttrLen)
		return SCARD_E_INVALID_PARAMETER;

	if (*pcbAttrLen == SCARD_AUTOALLOCATE)
	{
		if (!pbAttr)
			return SCARD_E_INVALID_PARAMETER;
		pcbAttrLenAlloc = TRUE;
	}

	pcsc_cbAttrLen = pcbAttrLenAlloc ? PCSC_SCARD_AUTOALLOCATE : (PCSC_DWORD)*pcbAttrLen;

	if (pcbAttrLenAlloc && !g_SCardAutoAllocate)
	{
		pcsc_cbAttrLen = 0;
		status = g_PCSC.pfnSCardGetAttrib(hCard, pcsc_dwAttrId, NULL, &pcsc_cbAttrLen);

		if (status == SCARD_S_SUCCESS)
		{
			BYTE* tmp = (BYTE*)calloc(1, pcsc_cbAttrLen);

			if (!tmp)
				return SCARD_E_NO_MEMORY;

			status = g_PCSC.pfnSCardGetAttrib(hCard, pcsc_dwAttrId, tmp, &pcsc_cbAttrLen);

			if (status != SCARD_S_SUCCESS)
				free(tmp);
			else
				PCSC_AddMemoryBlock(hContext, tmp);
			*(BYTE**)pbAttr = tmp;
		}
	}
	else
	{
		status = g_PCSC.pfnSCardGetAttrib(hCard, pcsc_dwAttrId, pbAttr, &pcsc_cbAttrLen);
	}

	if (status == SCARD_S_SUCCESS)
		*pcbAttrLen = (DWORD)pcsc_cbAttrLen;
	return PCSC_MapErrorCodeToWinSCard(status);
}

static LONG WINAPI PCSC_SCardGetAttrib_FriendlyName(SCARDHANDLE hCard, DWORD dwAttrId,
                                                    LPBYTE pbAttr, LPDWORD pcbAttrLen)
{
	size_t length = 0;
	char* namePCSC = NULL;
	char* pbAttrA = NULL;
	DWORD cbAttrLen = 0;
	WCHAR* pbAttrW = NULL;
	SCARDCONTEXT hContext;
	LONG status = SCARD_S_SUCCESS;

	hContext = PCSC_GetCardContextFromHandle(hCard);

	if (!hContext)
		return SCARD_E_INVALID_HANDLE;

	if (!pcbAttrLen)
		return SCARD_E_INVALID_PARAMETER;
	cbAttrLen = *pcbAttrLen;
	*pcbAttrLen = SCARD_AUTOALLOCATE;
	status = PCSC_SCardGetAttrib_Internal(hCard, SCARD_ATTR_DEVICE_FRIENDLY_NAME_A,
	                                      (LPBYTE)&pbAttrA, pcbAttrLen);

	if (status != SCARD_S_SUCCESS)
	{
		*pcbAttrLen = SCARD_AUTOALLOCATE;
		status = PCSC_SCardGetAttrib_Internal(hCard, SCARD_ATTR_DEVICE_FRIENDLY_NAME_W,
		                                      (LPBYTE)&pbAttrW, pcbAttrLen);

		if (status != SCARD_S_SUCCESS)
			return status;

		namePCSC = ConvertMszWCharNToUtf8Alloc(pbAttrW, *pcbAttrLen, NULL);
		PCSC_SCardFreeMemory_Internal(hContext, pbAttrW);
	}
	else
	{
		namePCSC = _strdup(pbAttrA);

		if (!namePCSC)
			return SCARD_E_NO_MEMORY;

		PCSC_SCardFreeMemory_Internal(hContext, pbAttrA);
	}

	length = strlen(namePCSC);

	if (dwAttrId == SCARD_ATTR_DEVICE_FRIENDLY_NAME_W)
	{
		size_t size = 0;
		WCHAR* friendlyNameW = ConvertUtf8ToWCharAlloc(namePCSC, &size);
		/* length here includes null terminator */

		if (!friendlyNameW)
			status = SCARD_E_NO_MEMORY;
		else
		{
			length = size;

			if (cbAttrLen == SCARD_AUTOALLOCATE)
			{
				WINPR_ASSERT(length <= UINT32_MAX / sizeof(WCHAR));
				*(WCHAR**)pbAttr = friendlyNameW;
				*pcbAttrLen = (UINT32)length * sizeof(WCHAR);
				PCSC_AddMemoryBlock(hContext, friendlyNameW);
			}
			else
			{
				if ((length * 2) > cbAttrLen)
					status = SCARD_E_INSUFFICIENT_BUFFER;
				else
				{
					WINPR_ASSERT(length <= UINT32_MAX / sizeof(WCHAR));
					CopyMemory(pbAttr, (BYTE*)friendlyNameW, (length * sizeof(WCHAR)));
					*pcbAttrLen = (UINT32)length * sizeof(WCHAR);
				}
				free(friendlyNameW);
			}
		}
		free(namePCSC);
	}
	else
	{
		if (cbAttrLen == SCARD_AUTOALLOCATE)
		{
			*(CHAR**)pbAttr = namePCSC;
			WINPR_ASSERT(length <= UINT32_MAX);
			*pcbAttrLen = (UINT32)length;
			PCSC_AddMemoryBlock(hContext, namePCSC);
		}
		else
		{
			if ((length + 1) > cbAttrLen)
				status = SCARD_E_INSUFFICIENT_BUFFER;
			else
			{
				CopyMemory(pbAttr, namePCSC, length + 1);
				WINPR_ASSERT(length <= UINT32_MAX);
				*pcbAttrLen = (UINT32)length;
			}
			free(namePCSC);
		}
	}

	return status;
}

static LONG WINAPI PCSC_SCardGetAttrib(SCARDHANDLE hCard, DWORD dwAttrId, LPBYTE pbAttr,
                                       LPDWORD pcbAttrLen)
{
	DWORD cbAttrLen;
	SCARDCONTEXT hContext;
	BOOL pcbAttrLenAlloc = FALSE;
	LONG status = SCARD_S_SUCCESS;

	if (NULL == pcbAttrLen)
		return SCARD_E_INVALID_PARAMETER;

	cbAttrLen = *pcbAttrLen;

	if (*pcbAttrLen == SCARD_AUTOALLOCATE)
	{
		if (NULL == pbAttr)
			return SCARD_E_INVALID_PARAMETER;

		pcbAttrLenAlloc = TRUE;
		*(BYTE**)pbAttr = NULL;
	}
	else
	{
		/**
		 * pcsc-lite returns SCARD_E_INSUFFICIENT_BUFFER if the given
		 * buffer size is larger than PCSC_MAX_BUFFER_SIZE (264)
		 */
		if (*pcbAttrLen > PCSC_MAX_BUFFER_SIZE)
			*pcbAttrLen = PCSC_MAX_BUFFER_SIZE;
	}

	hContext = PCSC_GetCardContextFromHandle(hCard);

	if (!hContext)
		return SCARD_E_INVALID_HANDLE;

	if ((dwAttrId == SCARD_ATTR_DEVICE_FRIENDLY_NAME_A) ||
	    (dwAttrId == SCARD_ATTR_DEVICE_FRIENDLY_NAME_W))
	{
		status = PCSC_SCardGetAttrib_FriendlyName(hCard, dwAttrId, pbAttr, pcbAttrLen);
		return status;
	}

	status = PCSC_SCardGetAttrib_Internal(hCard, dwAttrId, pbAttr, pcbAttrLen);

	if (status == SCARD_S_SUCCESS)
	{
		if (dwAttrId == SCARD_ATTR_VENDOR_NAME)
		{
			if (pbAttr)
			{
				const char* vendorName;

				/**
				 * pcsc-lite adds a null terminator to the vendor name,
				 * while WinSCard doesn't. Strip the null terminator.
				 */

				if (pcbAttrLenAlloc)
					vendorName = (char*)*(BYTE**)pbAttr;
				else
					vendorName = (char*)pbAttr;

				if (vendorName)
				{
					size_t len = strnlen(vendorName, *pcbAttrLen);
					WINPR_ASSERT(len <= UINT32_MAX);
					*pcbAttrLen = (DWORD)len;
				}
				else
					*pcbAttrLen = 0;
			}
		}
	}
	else
	{

		if (dwAttrId == SCARD_ATTR_CURRENT_PROTOCOL_TYPE)
		{
			if (!pcbAttrLenAlloc)
			{
				PCSC_DWORD dwState = 0;
				PCSC_DWORD cbAtrLen = 0;
				PCSC_DWORD dwProtocol = 0;
				PCSC_DWORD cchReaderLen = 0;
				status = (LONG)g_PCSC.pfnSCardStatus(hCard, NULL, &cchReaderLen, &dwState,
				                                     &dwProtocol, NULL, &cbAtrLen);

				if (status == SCARD_S_SUCCESS)
				{
					if (cbAttrLen < sizeof(DWORD))
						return SCARD_E_INSUFFICIENT_BUFFER;

					*(DWORD*)pbAttr = PCSC_ConvertProtocolsToWinSCard(dwProtocol);
					*pcbAttrLen = sizeof(DWORD);
				}
			}
		}
		else if (dwAttrId == SCARD_ATTR_CHANNEL_ID)
		{
			if (!pcbAttrLenAlloc)
			{
				UINT32 channelType = 0x20; /* USB */
				UINT32 channelNumber = 0;

				if (cbAttrLen < sizeof(DWORD))
					return SCARD_E_INSUFFICIENT_BUFFER;

				status = SCARD_S_SUCCESS;
				*(DWORD*)pbAttr = (channelType << 16u) | channelNumber;
				*pcbAttrLen = sizeof(DWORD);
			}
		}
		else if (dwAttrId == SCARD_ATTR_VENDOR_IFD_TYPE)
		{
		}
		else if (dwAttrId == SCARD_ATTR_DEFAULT_CLK)
		{
		}
		else if (dwAttrId == SCARD_ATTR_DEFAULT_DATA_RATE)
		{
		}
		else if (dwAttrId == SCARD_ATTR_MAX_CLK)
		{
		}
		else if (dwAttrId == SCARD_ATTR_MAX_DATA_RATE)
		{
		}
		else if (dwAttrId == SCARD_ATTR_MAX_IFSD)
		{
		}
		else if (dwAttrId == SCARD_ATTR_CHARACTERISTICS)
		{
		}
		else if (dwAttrId == SCARD_ATTR_DEVICE_SYSTEM_NAME_A)
		{
		}
		else if (dwAttrId == SCARD_ATTR_DEVICE_UNIT)
		{
		}
		else if (dwAttrId == SCARD_ATTR_POWER_MGMT_SUPPORT)
		{
		}
		else if (dwAttrId == SCARD_ATTR_CURRENT_CLK)
		{
		}
		else if (dwAttrId == SCARD_ATTR_CURRENT_F)
		{
		}
		else if (dwAttrId == SCARD_ATTR_CURRENT_D)
		{
		}
		else if (dwAttrId == SCARD_ATTR_CURRENT_N)
		{
		}
		else if (dwAttrId == SCARD_ATTR_CURRENT_CWT)
		{
		}
		else if (dwAttrId == SCARD_ATTR_CURRENT_BWT)
		{
		}
		else if (dwAttrId == SCARD_ATTR_CURRENT_IFSC)
		{
		}
		else if (dwAttrId == SCARD_ATTR_CURRENT_EBC_ENCODING)
		{
		}
		else if (dwAttrId == SCARD_ATTR_CURRENT_IFSD)
		{
		}
		else if (dwAttrId == SCARD_ATTR_ICC_TYPE_PER_ATR)
		{
		}
	}

	return status;
}

static LONG WINAPI PCSC_SCardSetAttrib(SCARDHANDLE hCard, DWORD dwAttrId, LPCBYTE pbAttr,
                                       DWORD cbAttrLen)
{
	PCSC_LONG status = SCARD_S_SUCCESS;
	PCSC_SCARDHANDLE* pCard = NULL;
	PCSC_DWORD pcsc_dwAttrId = (PCSC_DWORD)dwAttrId;
	PCSC_DWORD pcsc_cbAttrLen = (PCSC_DWORD)cbAttrLen;

	if (!g_PCSC.pfnSCardSetAttrib)
		return PCSC_SCard_LogError("g_PCSC.pfnSCardSetAttrib");

	pCard = PCSC_GetCardHandleData(hCard);

	if (!pCard)
		return SCARD_E_INVALID_VALUE;

	PCSC_WaitForCardAccess(0, hCard, pCard->shared);
	status = g_PCSC.pfnSCardSetAttrib(hCard, pcsc_dwAttrId, pbAttr, pcsc_cbAttrLen);
	return PCSC_MapErrorCodeToWinSCard(status);
}

static LONG WINAPI PCSC_SCardUIDlgSelectCardA(LPOPENCARDNAMEA_EX pDlgStruc)
{
	WINPR_UNUSED(pDlgStruc);

	return SCARD_E_UNSUPPORTED_FEATURE;
}

static LONG WINAPI PCSC_SCardUIDlgSelectCardW(LPOPENCARDNAMEW_EX pDlgStruc)
{
	WINPR_UNUSED(pDlgStruc);
	return SCARD_E_UNSUPPORTED_FEATURE;
}

static LONG WINAPI PCSC_GetOpenCardNameA(LPOPENCARDNAMEA pDlgStruc)
{
	WINPR_UNUSED(pDlgStruc);
	return SCARD_E_UNSUPPORTED_FEATURE;
}

static LONG WINAPI PCSC_GetOpenCardNameW(LPOPENCARDNAMEW pDlgStruc)
{
	WINPR_UNUSED(pDlgStruc);
	return SCARD_E_UNSUPPORTED_FEATURE;
}

static LONG WINAPI PCSC_SCardDlgExtendedError(void)
{

	return SCARD_E_UNSUPPORTED_FEATURE;
}

static char* card_id_and_name_a(const UUID* CardIdentifier, LPCSTR LookupName)
{
	WINPR_ASSERT(CardIdentifier);
	WINPR_ASSERT(LookupName);

	size_t len = strlen(LookupName) + 34;
	char* id = malloc(len);
	if (!id)
		return NULL;

	snprintf(id, len, "%08X%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X\\%s", CardIdentifier->Data1,
	         CardIdentifier->Data2, CardIdentifier->Data3, CardIdentifier->Data4[0],
	         CardIdentifier->Data4[1], CardIdentifier->Data4[2], CardIdentifier->Data4[3],
	         CardIdentifier->Data4[4], CardIdentifier->Data4[5], CardIdentifier->Data4[6],
	         CardIdentifier->Data4[7], LookupName);
	return id;
}

static char* card_id_and_name_w(const UUID* CardIdentifier, LPCWSTR LookupName)
{
	char* res;
	char* tmp = ConvertWCharToUtf8Alloc(LookupName, NULL);
	if (!tmp)
		return NULL;
	res = card_id_and_name_a(CardIdentifier, tmp);
	free(tmp);
	return res;
}

static LONG WINAPI PCSC_SCardReadCacheA(SCARDCONTEXT hContext, UUID* CardIdentifier,
                                        DWORD FreshnessCounter, LPSTR LookupName, PBYTE Data,
                                        DWORD* DataLen)
{
	PCSC_CACHE_ITEM* data;
	PCSC_SCARDCONTEXT* ctx = PCSC_GetCardContextData(hContext);
	char* id = card_id_and_name_a(CardIdentifier, LookupName);

	data = HashTable_GetItemValue(ctx->cache, id);
	free(id);
	if (!data)
	{
		*DataLen = 0;
		return SCARD_W_CACHE_ITEM_NOT_FOUND;
	}

	if (FreshnessCounter != data->freshness)
	{
		*DataLen = 0;
		return SCARD_W_CACHE_ITEM_STALE;
	}

	if (*DataLen == SCARD_AUTOALLOCATE)
	{
		BYTE* mem = calloc(1, data->len);
		if (!mem)
			return SCARD_E_NO_MEMORY;

		if (!PCSC_AddMemoryBlock(hContext, mem))
		{
			free(mem);
			return SCARD_E_NO_MEMORY;
		}

		memcpy(mem, data->data, data->len);
		*(BYTE**)Data = mem;
	}
	else
		memcpy(Data, data->data, data->len);
	*DataLen = data->len;
	return SCARD_S_SUCCESS;
}

static LONG WINAPI PCSC_SCardReadCacheW(SCARDCONTEXT hContext, UUID* CardIdentifier,
                                        DWORD FreshnessCounter, LPWSTR LookupName, PBYTE Data,
                                        DWORD* DataLen)
{
	PCSC_CACHE_ITEM* data;
	PCSC_SCARDCONTEXT* ctx = PCSC_GetCardContextData(hContext);
	char* id = card_id_and_name_w(CardIdentifier, LookupName);

	data = HashTable_GetItemValue(ctx->cache, id);
	free(id);

	if (!data)
	{
		*DataLen = 0;
		return SCARD_W_CACHE_ITEM_NOT_FOUND;
	}

	if (FreshnessCounter != data->freshness)
	{
		*DataLen = 0;
		return SCARD_W_CACHE_ITEM_STALE;
	}

	if (*DataLen == SCARD_AUTOALLOCATE)
	{
		BYTE* mem = calloc(1, data->len);
		if (!mem)
			return SCARD_E_NO_MEMORY;

		if (!PCSC_AddMemoryBlock(hContext, mem))
		{
			free(mem);
			return SCARD_E_NO_MEMORY;
		}

		memcpy(mem, data->data, data->len);
		*(BYTE**)Data = mem;
	}
	else
		memcpy(Data, data->data, data->len);
	*DataLen = data->len;
	return SCARD_S_SUCCESS;
}

static LONG WINAPI PCSC_SCardWriteCacheA(SCARDCONTEXT hContext, UUID* CardIdentifier,
                                         DWORD FreshnessCounter, LPSTR LookupName, PBYTE Data,
                                         DWORD DataLen)
{
	PCSC_CACHE_ITEM* data;
	PCSC_SCARDCONTEXT* ctx = PCSC_GetCardContextData(hContext);
	char* id;

	if (!ctx)
		return SCARD_E_FILE_NOT_FOUND;

	id = card_id_and_name_a(CardIdentifier, LookupName);

	if (!id)
		return SCARD_E_NO_MEMORY;

	data = malloc(sizeof(PCSC_CACHE_ITEM));
	if (!data)
	{
		free(id);
		return SCARD_E_NO_MEMORY;
	}
	data->data = malloc(DataLen);
	if (!data->data)
	{
		free(id);
		free(data);
		return SCARD_E_NO_MEMORY;
	}
	data->len = DataLen;
	data->freshness = FreshnessCounter;
	memcpy(data->data, Data, data->len);

	HashTable_Remove(ctx->cache, id);
	const BOOL rc = HashTable_Insert(ctx->cache, id, data);
	free(id);

	if (!rc)
	{
		pcsc_cache_item_free(data);
		return SCARD_E_NO_MEMORY;
	}

	return SCARD_S_SUCCESS;
}

static LONG WINAPI PCSC_SCardWriteCacheW(SCARDCONTEXT hContext, UUID* CardIdentifier,
                                         DWORD FreshnessCounter, LPWSTR LookupName, PBYTE Data,
                                         DWORD DataLen)
{
	PCSC_CACHE_ITEM* data;
	PCSC_SCARDCONTEXT* ctx = PCSC_GetCardContextData(hContext);
	char* id;
	if (!ctx)
		return SCARD_E_FILE_NOT_FOUND;

	id = card_id_and_name_w(CardIdentifier, LookupName);

	if (!id)
		return SCARD_E_NO_MEMORY;

	data = malloc(sizeof(PCSC_CACHE_ITEM));
	if (!data)
	{
		free(id);
		return SCARD_E_NO_MEMORY;
	}
	data->data = malloc(DataLen);
	if (!data->data)
	{
		free(id);
		free(data);
		return SCARD_E_NO_MEMORY;
	}
	data->len = DataLen;
	data->freshness = FreshnessCounter;
	memcpy(data->data, Data, data->len);

	HashTable_Remove(ctx->cache, id);
	const BOOL rc = HashTable_Insert(ctx->cache, id, data);
	free(id);

	if (!rc)
	{
		pcsc_cache_item_free(data);
		return SCARD_E_NO_MEMORY;
	}

	return SCARD_S_SUCCESS;
}

static LONG WINAPI PCSC_SCardGetReaderIconA(SCARDCONTEXT hContext, LPCSTR szReaderName,
                                            LPBYTE pbIcon, LPDWORD pcbIcon)
{
	WINPR_UNUSED(hContext);
	WINPR_UNUSED(szReaderName);
	WINPR_UNUSED(pbIcon);
	WINPR_UNUSED(pcbIcon);
	return SCARD_E_UNSUPPORTED_FEATURE;
}

static LONG WINAPI PCSC_SCardGetReaderIconW(SCARDCONTEXT hContext, LPCWSTR szReaderName,
                                            LPBYTE pbIcon, LPDWORD pcbIcon)
{
	WINPR_UNUSED(hContext);
	WINPR_UNUSED(szReaderName);
	WINPR_UNUSED(pbIcon);
	WINPR_UNUSED(pcbIcon);
	return SCARD_E_UNSUPPORTED_FEATURE;
}

static LONG WINAPI PCSC_SCardGetDeviceTypeIdA(SCARDCONTEXT hContext, LPCSTR szReaderName,
                                              LPDWORD pdwDeviceTypeId)
{
	WINPR_UNUSED(hContext);
	WINPR_UNUSED(szReaderName);
	WINPR_UNUSED(pdwDeviceTypeId);
	if (pdwDeviceTypeId)
		*pdwDeviceTypeId = SCARD_READER_TYPE_USB;
	return SCARD_S_SUCCESS;
}

static LONG WINAPI PCSC_SCardGetDeviceTypeIdW(SCARDCONTEXT hContext, LPCWSTR szReaderName,
                                              LPDWORD pdwDeviceTypeId)
{
	WINPR_UNUSED(hContext);
	WINPR_UNUSED(szReaderName);
	if (pdwDeviceTypeId)
		*pdwDeviceTypeId = SCARD_READER_TYPE_USB;
	return SCARD_S_SUCCESS;
}

static LONG WINAPI PCSC_SCardGetReaderDeviceInstanceIdA(SCARDCONTEXT hContext, LPCSTR szReaderName,
                                                        LPSTR szDeviceInstanceId,
                                                        LPDWORD pcchDeviceInstanceId)
{
	WINPR_UNUSED(hContext);
	WINPR_UNUSED(szReaderName);
	WINPR_UNUSED(szDeviceInstanceId);
	WINPR_UNUSED(pcchDeviceInstanceId);
	return SCARD_E_UNSUPPORTED_FEATURE;
}

static LONG WINAPI PCSC_SCardGetReaderDeviceInstanceIdW(SCARDCONTEXT hContext, LPCWSTR szReaderName,
                                                        LPWSTR szDeviceInstanceId,
                                                        LPDWORD pcchDeviceInstanceId)
{
	WINPR_UNUSED(hContext);
	WINPR_UNUSED(szReaderName);
	WINPR_UNUSED(szDeviceInstanceId);
	WINPR_UNUSED(pcchDeviceInstanceId);
	return SCARD_E_UNSUPPORTED_FEATURE;
}

static LONG WINAPI PCSC_SCardListReadersWithDeviceInstanceIdA(SCARDCONTEXT hContext,
                                                              LPCSTR szDeviceInstanceId,
                                                              LPSTR mszReaders, LPDWORD pcchReaders)
{
	WINPR_UNUSED(hContext);
	WINPR_UNUSED(szDeviceInstanceId);
	WINPR_UNUSED(mszReaders);
	WINPR_UNUSED(pcchReaders);
	return SCARD_E_UNSUPPORTED_FEATURE;
}

static LONG WINAPI PCSC_SCardListReadersWithDeviceInstanceIdW(SCARDCONTEXT hContext,
                                                              LPCWSTR szDeviceInstanceId,
                                                              LPWSTR mszReaders,
                                                              LPDWORD pcchReaders)
{
	WINPR_UNUSED(hContext);
	WINPR_UNUSED(szDeviceInstanceId);
	WINPR_UNUSED(mszReaders);
	WINPR_UNUSED(pcchReaders);
	return SCARD_E_UNSUPPORTED_FEATURE;
}

static LONG WINAPI PCSC_SCardAudit(SCARDCONTEXT hContext, DWORD dwEvent)
{

	WINPR_UNUSED(hContext);
	WINPR_UNUSED(dwEvent);
	return SCARD_E_UNSUPPORTED_FEATURE;
}

#ifdef __MACOSX__
unsigned int determineMacOSXVersion(void)
{
	int mib[2];
	size_t len = 0;
	char* kernelVersion = NULL;
	char* tok = NULL;
	unsigned int version = 0;
	long majorVersion = 0;
	long minorVersion = 0;
	long patchVersion = 0;
	int count = 0;
	char* context = NULL;
	mib[0] = CTL_KERN;
	mib[1] = KERN_OSRELEASE;

	if (sysctl(mib, 2, NULL, &len, NULL, 0) != 0)
		return 0;

	kernelVersion = calloc(len, sizeof(char));

	if (!kernelVersion)
		return 0;

	if (sysctl(mib, 2, kernelVersion, &len, NULL, 0) != 0)
	{
		free(kernelVersion);
		return 0;
	}

	tok = strtok_s(kernelVersion, ".", &context);
	errno = 0;

	while (tok)
	{
		switch (count)
		{
			case 0:
				majorVersion = strtol(tok, NULL, 0);

				if (errno != 0)
					goto fail;

				break;

			case 1:
				minorVersion = strtol(tok, NULL, 0);

				if (errno != 0)
					goto fail;

				break;

			case 2:
				patchVersion = strtol(tok, NULL, 0);

				if (errno != 0)
					goto fail;

				break;
		}

		tok = strtok_s(NULL, ".", &context);
		count++;
	}

	/**
	 * Source : http://en.wikipedia.org/wiki/Darwin_(operating_system)
	 **/
	if (majorVersion < 5)
	{
		if (minorVersion < 4)
			version = 0x10000000;
		else
			version = 0x10010000;
	}
	else
	{
		switch (majorVersion)
		{
			case 5:
				version = 0x10010000;
				break;

			case 6:
				version = 0x10020000;
				break;

			case 7:
				version = 0x10030000;
				break;

			case 8:
				version = 0x10040000;
				break;

			case 9:
				version = 0x10050000;
				break;

			case 10:
				version = 0x10060000;
				break;

			case 11:
				version = 0x10070000;
				break;

			case 12:
				version = 0x10080000;
				break;

			case 13:
				version = 0x10090000;
				break;

			default:
				version = 0x10100000;
				break;
		}

		version |= (minorVersion << 8) | (patchVersion);
	}

fail:
	free(kernelVersion);
	return version;
}
#endif

static const SCardApiFunctionTable PCSC_SCardApiFunctionTable = {
	0, /* dwVersion */
	0, /* dwFlags */

	PCSC_SCardEstablishContext,                 /* SCardEstablishContext */
	PCSC_SCardReleaseContext,                   /* SCardReleaseContext */
	PCSC_SCardIsValidContext,                   /* SCardIsValidContext */
	PCSC_SCardListReaderGroupsA,                /* SCardListReaderGroupsA */
	PCSC_SCardListReaderGroupsW,                /* SCardListReaderGroupsW */
	PCSC_SCardListReadersA,                     /* SCardListReadersA */
	PCSC_SCardListReadersW,                     /* SCardListReadersW */
	PCSC_SCardListCardsA,                       /* SCardListCardsA */
	PCSC_SCardListCardsW,                       /* SCardListCardsW */
	PCSC_SCardListInterfacesA,                  /* SCardListInterfacesA */
	PCSC_SCardListInterfacesW,                  /* SCardListInterfacesW */
	PCSC_SCardGetProviderIdA,                   /* SCardGetProviderIdA */
	PCSC_SCardGetProviderIdW,                   /* SCardGetProviderIdW */
	PCSC_SCardGetCardTypeProviderNameA,         /* SCardGetCardTypeProviderNameA */
	PCSC_SCardGetCardTypeProviderNameW,         /* SCardGetCardTypeProviderNameW */
	PCSC_SCardIntroduceReaderGroupA,            /* SCardIntroduceReaderGroupA */
	PCSC_SCardIntroduceReaderGroupW,            /* SCardIntroduceReaderGroupW */
	PCSC_SCardForgetReaderGroupA,               /* SCardForgetReaderGroupA */
	PCSC_SCardForgetReaderGroupW,               /* SCardForgetReaderGroupW */
	PCSC_SCardIntroduceReaderA,                 /* SCardIntroduceReaderA */
	PCSC_SCardIntroduceReaderW,                 /* SCardIntroduceReaderW */
	PCSC_SCardForgetReaderA,                    /* SCardForgetReaderA */
	PCSC_SCardForgetReaderW,                    /* SCardForgetReaderW */
	PCSC_SCardAddReaderToGroupA,                /* SCardAddReaderToGroupA */
	PCSC_SCardAddReaderToGroupW,                /* SCardAddReaderToGroupW */
	PCSC_SCardRemoveReaderFromGroupA,           /* SCardRemoveReaderFromGroupA */
	PCSC_SCardRemoveReaderFromGroupW,           /* SCardRemoveReaderFromGroupW */
	PCSC_SCardIntroduceCardTypeA,               /* SCardIntroduceCardTypeA */
	PCSC_SCardIntroduceCardTypeW,               /* SCardIntroduceCardTypeW */
	PCSC_SCardSetCardTypeProviderNameA,         /* SCardSetCardTypeProviderNameA */
	PCSC_SCardSetCardTypeProviderNameW,         /* SCardSetCardTypeProviderNameW */
	PCSC_SCardForgetCardTypeA,                  /* SCardForgetCardTypeA */
	PCSC_SCardForgetCardTypeW,                  /* SCardForgetCardTypeW */
	PCSC_SCardFreeMemory,                       /* SCardFreeMemory */
	PCSC_SCardAccessStartedEvent,               /* SCardAccessStartedEvent */
	PCSC_SCardReleaseStartedEvent,              /* SCardReleaseStartedEvent */
	PCSC_SCardLocateCardsA,                     /* SCardLocateCardsA */
	PCSC_SCardLocateCardsW,                     /* SCardLocateCardsW */
	PCSC_SCardLocateCardsByATRA,                /* SCardLocateCardsByATRA */
	PCSC_SCardLocateCardsByATRW,                /* SCardLocateCardsByATRW */
	PCSC_SCardGetStatusChangeA,                 /* SCardGetStatusChangeA */
	PCSC_SCardGetStatusChangeW,                 /* SCardGetStatusChangeW */
	PCSC_SCardCancel,                           /* SCardCancel */
	PCSC_SCardConnectA,                         /* SCardConnectA */
	PCSC_SCardConnectW,                         /* SCardConnectW */
	PCSC_SCardReconnect,                        /* SCardReconnect */
	PCSC_SCardDisconnect,                       /* SCardDisconnect */
	PCSC_SCardBeginTransaction,                 /* SCardBeginTransaction */
	PCSC_SCardEndTransaction,                   /* SCardEndTransaction */
	PCSC_SCardCancelTransaction,                /* SCardCancelTransaction */
	PCSC_SCardState,                            /* SCardState */
	PCSC_SCardStatusA,                          /* SCardStatusA */
	PCSC_SCardStatusW,                          /* SCardStatusW */
	PCSC_SCardTransmit,                         /* SCardTransmit */
	PCSC_SCardGetTransmitCount,                 /* SCardGetTransmitCount */
	PCSC_SCardControl,                          /* SCardControl */
	PCSC_SCardGetAttrib,                        /* SCardGetAttrib */
	PCSC_SCardSetAttrib,                        /* SCardSetAttrib */
	PCSC_SCardUIDlgSelectCardA,                 /* SCardUIDlgSelectCardA */
	PCSC_SCardUIDlgSelectCardW,                 /* SCardUIDlgSelectCardW */
	PCSC_GetOpenCardNameA,                      /* GetOpenCardNameA */
	PCSC_GetOpenCardNameW,                      /* GetOpenCardNameW */
	PCSC_SCardDlgExtendedError,                 /* SCardDlgExtendedError */
	PCSC_SCardReadCacheA,                       /* SCardReadCacheA */
	PCSC_SCardReadCacheW,                       /* SCardReadCacheW */
	PCSC_SCardWriteCacheA,                      /* SCardWriteCacheA */
	PCSC_SCardWriteCacheW,                      /* SCardWriteCacheW */
	PCSC_SCardGetReaderIconA,                   /* SCardGetReaderIconA */
	PCSC_SCardGetReaderIconW,                   /* SCardGetReaderIconW */
	PCSC_SCardGetDeviceTypeIdA,                 /* SCardGetDeviceTypeIdA */
	PCSC_SCardGetDeviceTypeIdW,                 /* SCardGetDeviceTypeIdW */
	PCSC_SCardGetReaderDeviceInstanceIdA,       /* SCardGetReaderDeviceInstanceIdA */
	PCSC_SCardGetReaderDeviceInstanceIdW,       /* SCardGetReaderDeviceInstanceIdW */
	PCSC_SCardListReadersWithDeviceInstanceIdA, /* SCardListReadersWithDeviceInstanceIdA */
	PCSC_SCardListReadersWithDeviceInstanceIdW, /* SCardListReadersWithDeviceInstanceIdW */
	PCSC_SCardAudit                             /* SCardAudit */
};

const SCardApiFunctionTable* PCSC_GetSCardApiFunctionTable(void)
{
	return &PCSC_SCardApiFunctionTable;
}

int PCSC_InitializeSCardApi(void)
{
	/* Disable pcsc-lite's (poor) blocking so we can handle it ourselves */
	SetEnvironmentVariableA("PCSCLITE_NO_BLOCKING", "1");
#ifdef __MACOSX__
	g_PCSCModule = LoadLibraryX("/System/Library/Frameworks/PCSC.framework/PCSC");
	OSXVersion = determineMacOSXVersion();

	if (OSXVersion == 0)
		return -1;

#else
	g_PCSCModule = LoadLibraryA("libpcsclite.so.1");

	if (!g_PCSCModule)
		g_PCSCModule = LoadLibraryA("libpcsclite.so");

#endif

	if (!g_PCSCModule)
		return -1;

	g_PCSC.pfnSCardEstablishContext =
	    (fnPCSCSCardEstablishContext)GetProcAddress(g_PCSCModule, "SCardEstablishContext");
	g_PCSC.pfnSCardReleaseContext =
	    (fnPCSCSCardReleaseContext)GetProcAddress(g_PCSCModule, "SCardReleaseContext");
	g_PCSC.pfnSCardIsValidContext =
	    (fnPCSCSCardIsValidContext)GetProcAddress(g_PCSCModule, "SCardIsValidContext");
	g_PCSC.pfnSCardConnect = (fnPCSCSCardConnect)GetProcAddress(g_PCSCModule, "SCardConnect");
	g_PCSC.pfnSCardReconnect = (fnPCSCSCardReconnect)GetProcAddress(g_PCSCModule, "SCardReconnect");
	g_PCSC.pfnSCardDisconnect =
	    (fnPCSCSCardDisconnect)GetProcAddress(g_PCSCModule, "SCardDisconnect");
	g_PCSC.pfnSCardBeginTransaction =
	    (fnPCSCSCardBeginTransaction)GetProcAddress(g_PCSCModule, "SCardBeginTransaction");
	g_PCSC.pfnSCardEndTransaction =
	    (fnPCSCSCardEndTransaction)GetProcAddress(g_PCSCModule, "SCardEndTransaction");
	g_PCSC.pfnSCardStatus = (fnPCSCSCardStatus)GetProcAddress(g_PCSCModule, "SCardStatus");
	g_PCSC.pfnSCardGetStatusChange =
	    (fnPCSCSCardGetStatusChange)GetProcAddress(g_PCSCModule, "SCardGetStatusChange");
#ifdef __MACOSX__

	if (OSXVersion >= 0x10050600)
		g_PCSC.pfnSCardControl =
		    (fnPCSCSCardControl)GetProcAddress(g_PCSCModule, "SCardControl132");
	else
		g_PCSC.pfnSCardControl = (fnPCSCSCardControl)GetProcAddress(g_PCSCModule, "SCardControl");

#else
	g_PCSC.pfnSCardControl = (fnPCSCSCardControl)GetProcAddress(g_PCSCModule, "SCardControl");
#endif
	g_PCSC.pfnSCardTransmit = (fnPCSCSCardTransmit)GetProcAddress(g_PCSCModule, "SCardTransmit");
	g_PCSC.pfnSCardListReaderGroups =
	    (fnPCSCSCardListReaderGroups)GetProcAddress(g_PCSCModule, "SCardListReaderGroups");
	g_PCSC.pfnSCardListReaders =
	    (fnPCSCSCardListReaders)GetProcAddress(g_PCSCModule, "SCardListReaders");
	g_PCSC.pfnSCardCancel = (fnPCSCSCardCancel)GetProcAddress(g_PCSCModule, "SCardCancel");
	g_PCSC.pfnSCardGetAttrib = (fnPCSCSCardGetAttrib)GetProcAddress(g_PCSCModule, "SCardGetAttrib");
	g_PCSC.pfnSCardSetAttrib = (fnPCSCSCardSetAttrib)GetProcAddress(g_PCSCModule, "SCardSetAttrib");
	g_PCSC.pfnSCardFreeMemory = NULL;
#ifndef __APPLE__
	g_PCSC.pfnSCardFreeMemory =
	    (fnPCSCSCardFreeMemory)GetProcAddress(g_PCSCModule, "SCardFreeMemory");
#endif

	if (g_PCSC.pfnSCardFreeMemory)
		g_SCardAutoAllocate = TRUE;

#ifdef DISABLE_PCSC_SCARD_AUTOALLOCATE
	g_PCSC.pfnSCardFreeMemory = NULL;
	g_SCardAutoAllocate = FALSE;
#endif
#ifdef __APPLE__
	g_PnP_Notification = FALSE;
#endif
	return 1;
}

#endif
