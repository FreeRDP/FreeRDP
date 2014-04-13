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

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/library.h>
#include <winpr/smartcard.h>
#include <winpr/collections.h>

#include "smartcard_pcsc.h"

//#define DISABLE_PCSC_SCARD_AUTOALLOCATE

struct _PCSC_SCARDCONTEXT
{
	SCARDCONTEXT hContext;
	SCARDHANDLE hCard;
	CRITICAL_SECTION lock;
};
typedef struct _PCSC_SCARDCONTEXT PCSC_SCARDCONTEXT;

struct _PCSC_READER
{
	char* namePCSC;
	char* nameWinSCard;
};
typedef struct _PCSC_READER PCSC_READER;

static HMODULE g_PCSCModule = NULL;
static PCSCFunctionTable g_PCSC = { 0 };

static BOOL g_SCardAutoAllocate = FALSE;

static wArrayList* g_Readers = NULL;
static wListDictionary* g_CardHandles = NULL;
static wListDictionary* g_CardContexts = NULL;
static wListDictionary* g_MemoryBlocks = NULL;

WINSCARDAPI LONG WINAPI PCSC_SCardFreeMemory_Internal(SCARDCONTEXT hContext, LPCVOID pvMem);

LONG PCSC_MapErrorCodeToWinSCard(LONG errorCode)
{
	/**
	 * pcsc-lite returns SCARD_E_UNEXPECTED when it
	 * should return SCARD_E_UNSUPPORTED_FEATURE.
	 *
	 * Additionally, the pcsc-lite headers incorrectly
	 * define SCARD_E_UNSUPPORTED_FEATURE to 0x8010001F,
	 * when the real value should be 0x80100022.
	 */

	if (errorCode == SCARD_E_UNEXPECTED)
		errorCode = SCARD_E_UNSUPPORTED_FEATURE;

	return errorCode;
}

DWORD PCSC_ConvertCardStateToWinSCard(DWORD dwCardState, LONG status)
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

DWORD PCSC_ConvertProtocolsToWinSCard(DWORD dwProtocols)
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

	return dwProtocols;
}

DWORD PCSC_ConvertProtocolsFromWinSCard(DWORD dwProtocols)
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

	return dwProtocols;
}

void PCSC_ReaderAliasFree(PCSC_READER* reader)
{
	if (!reader)
		return;

	free(reader->namePCSC);
	free(reader->nameWinSCard);
}

PCSC_SCARDCONTEXT* PCSC_GetCardContextData(SCARDCONTEXT hContext)
{
	PCSC_SCARDCONTEXT* pContext;

	if (!g_CardContexts)
		return 0;

	pContext = (PCSC_SCARDCONTEXT*) ListDictionary_GetItemValue(g_CardContexts, (void*) hContext);

	if (!pContext)
		return 0;

	return pContext;
}

PCSC_SCARDCONTEXT* PCSC_EstablishCardContext(SCARDCONTEXT hContext)
{
	PCSC_SCARDCONTEXT* pContext;

	pContext = (PCSC_SCARDCONTEXT*) calloc(1, sizeof(PCSC_SCARDCONTEXT));

	if (!pContext)
		return NULL;

	pContext->hContext = hContext;

	InitializeCriticalSectionAndSpinCount(&(pContext->lock), 4000);

	if (!g_CardContexts)
		g_CardContexts = ListDictionary_New(TRUE);

	if (!g_Readers)
	{
		g_Readers = ArrayList_New(TRUE);
		ArrayList_Object(g_Readers)->fnObjectFree = (OBJECT_FREE_FN) PCSC_ReaderAliasFree;
	}

	ListDictionary_Add(g_CardContexts, (void*) hContext, (void*) pContext);

	return pContext;
}

void PCSC_ReleaseCardContext(SCARDCONTEXT hContext)
{
	PCSC_SCARDCONTEXT* pContext;

	pContext = PCSC_GetCardContextData(hContext);

	if (!pContext)
		return;

	DeleteCriticalSection(&(pContext->lock));

	free(pContext);

	if (!g_CardContexts)
		return;

	ListDictionary_Remove(g_CardContexts, (void*) hContext);
}

void PCSC_LockCardContext(SCARDCONTEXT hContext)
{
	PCSC_SCARDCONTEXT* pContext;

	pContext = PCSC_GetCardContextData(hContext);

	if (!pContext)
	{
		fprintf(stderr, "PCSC_LockCardContext: invalid context (%p)\n", (void*) hContext);
		return;
	}

	EnterCriticalSection(&(pContext->lock));
}

void PCSC_UnlockCardContext(SCARDCONTEXT hContext)
{
	PCSC_SCARDCONTEXT* pContext;

	pContext = PCSC_GetCardContextData(hContext);

	if (!pContext)
	{
		fprintf(stderr, "PCSC_UnlockCardContext: invalid context (%p)\n", (void*) hContext);
		return;
	}

	LeaveCriticalSection(&(pContext->lock));
}

char* PCSC_GetReaderNameFromAlias(char* nameWinSCard)
{
	int index;
	int count;
	PCSC_READER* reader;
	char* namePCSC = NULL;

	ArrayList_Lock(g_Readers);

	count = ArrayList_Count(g_Readers);

	for (index = 0; index < count; index++)
	{
		reader = ArrayList_GetItem(g_Readers, index);

		if (strcmp(nameWinSCard, reader->nameWinSCard) == 0)
		{
			namePCSC = reader->namePCSC;
			break;
		}
	}

	ArrayList_Unlock(g_Readers);

	return namePCSC;
}

BOOL PCSC_AddReaderNameAlias(char* namePCSC, char* nameWinSCard)
{
	PCSC_READER* reader;

	if (PCSC_GetReaderNameFromAlias(nameWinSCard))
		return TRUE;

	reader = (PCSC_READER*) calloc(1, sizeof(PCSC_READER));

	if (!reader)
		return FALSE;

	reader->namePCSC = _strdup(namePCSC);
	reader->nameWinSCard = _strdup(nameWinSCard);

	ArrayList_Add(g_Readers, reader);

	return TRUE;
}

char* PCSC_ConvertReaderNameToWinSCard(const char* name)
{
	int slot;
	int index;
	int size;
	int length;
	char* nameWinSCard;
	char *na1, *na2 = NULL;
	char *if1, *if2 = NULL;
	char *se1, *se2 = NULL;
	char *in1, *in2 = NULL;
	char *sl1, *sl2 = NULL;

	/**
	 * pcsc-lite reader name format:
	 *
	 * name [interface] (serial) index slot
	 * Athena IDProtect Key v2 [Main Interface] 00 00
	 *
	 * name:      Athena IDProtect Key v2
	 * interface: Main Interface
	 * serial:    N/A
	 * index:     00
	 * slot:      00
	 *
	 * the serial component is optional
	 * the index is a two digit zero-padded integer
	 * the slot is a two digit zero-padded integer
	 */

	length = strlen(name);

	if (length < 10)
		return NULL;

	na1 = (char*) name;
	na2 = (char*) &name[length - 7];

	if1 = strchr(name, '[');

	if (if1)
		if2 = strchr(if1 + 1, ']');

	se1 = strchr(name, '(');

	if (se1)
		se2 = strchr(se1 + 1, ')');

	in1 = (char*) &name[length - 5];
	in2 = (char*) &name[length - 4];

	sl1 = (char*) &name[length - 2];
	sl2 = (char*) &name[length - 1];

	if (!(if1 && if2))
		return NULL;

	if (!(isdigit(*in1) && isdigit(*in2)))
		return NULL;

	if (!(isdigit(*sl1) && isdigit(*sl2)))
		return NULL;

	na2 = if1 - 1;

	while ((*na2 == ' ') && (na2 > na1))
		na2--;
	na2++;

	index = ((*in1 - 0x30) * 10) + (*in2 - 0x30);
	slot = ((*sl1 - 0x30) * 10) + (*sl2 - 0x30);

	length = (na2 - na1);
	size = length + 16;

	nameWinSCard = (char*) malloc(size);

	if (!nameWinSCard)
		return NULL;

	sprintf_s(nameWinSCard, size, "%.*s %d", length, na1, index);

	return nameWinSCard;
}

char* PCSC_ConvertReaderNamesToWinSCard(const char* names)
{
	char *p, *q;
	int plen, qlen;
	int count = 0;
	int total = 0;
	char* nameWinSCard;
	char* namesWinSCard;

	p = (char*) names;
	plen = strlen(p);

	while (plen > 0)
	{
		p += plen + 1;
		total += plen + 1;
		plen = strlen(p);
		count++;
	}
	total++;

	namesWinSCard = (char*) malloc(total * 2);

	if (!namesWinSCard)
		return NULL;

	q = namesWinSCard;

	p = (char*) names;
	plen = strlen(p);

	while (plen > 0)
	{
		nameWinSCard = PCSC_ConvertReaderNameToWinSCard(p);

		if (nameWinSCard)
		{
			PCSC_AddReaderNameAlias(p, nameWinSCard);

			qlen = strlen(nameWinSCard);
			CopyMemory(q, nameWinSCard, qlen);
			free(nameWinSCard);
		}
		else
		{
			qlen = strlen(p);
			CopyMemory(q, p, qlen);
		}

		p += plen + 1;

		q += qlen;
		*q = '\0';
		q++;

		plen = strlen(p);
	}

	*q = '\0';
	q++;

	return namesWinSCard;
}

char* PCSC_ConvertReaderNamesFromWinSCard(const char* names)
{
	char *p, *q;
	int plen, qlen;
	int count = 0;
	int total = 0;
	char* namePCSC;
	char* namesPCSC;

	p = (char*) names;
	plen = strlen(p);

	while (plen > 0)
	{
		p += plen + 1;
		total += plen + 1;
		plen = strlen(p);
		count++;
	}
	total++;

	namesPCSC = (char*) malloc(total);

	if (!namesPCSC)
		return NULL;

	q = namesPCSC;

	p = (char*) names;
	plen = strlen(p);

	while (plen > 0)
	{
		namePCSC = PCSC_GetReaderNameFromAlias(p);

		if (!namePCSC)
			namePCSC = p;

		qlen = strlen(namePCSC);
		CopyMemory(q, namePCSC, qlen);

		p += plen + 1;

		q += qlen;
		*q = '\0';
		q++;

		plen = strlen(p);
	}

	*q = '\0';
	q++;

	return namesPCSC;
}

size_t PCSC_MultiStringLengthA(const char* msz)
{
	char* p = (char*) msz;

	if (!p)
		return 0;

	while ((p[0] != 0) && (p[1] != 0))
		p++;

	return (p - msz) + 1;
}

size_t PCSC_MultiStringLengthW(const WCHAR* msz)
{
	WCHAR* p = (WCHAR*) msz;

	if (!p)
		return 0;

	while ((p[0] != 0) && (p[1] != 0))
		p++;

	return (p - msz) + 1;
}

void PCSC_AddCardHandle(SCARDCONTEXT hContext, SCARDHANDLE hCard)
{
	if (!g_CardHandles)
		g_CardHandles = ListDictionary_New(TRUE);

	ListDictionary_Add(g_CardHandles, (void*) hCard, (void*) hContext);
}

void* PCSC_RemoveCardHandle(SCARDHANDLE hCard)
{
	if (!g_CardHandles)
		return NULL;

	return ListDictionary_Remove(g_CardHandles, (void*) hCard);
}

SCARDCONTEXT PCSC_GetCardContextFromHandle(SCARDHANDLE hCard)
{
	SCARDCONTEXT hContext;

	if (!g_CardHandles)
		return 0;

	hContext = (SCARDCONTEXT) ListDictionary_GetItemValue(g_CardHandles, (void*) hCard);

	return hContext;
}

void PCSC_AddMemoryBlock(SCARDCONTEXT hContext, void* pvMem)
{
	if (!g_MemoryBlocks)
		g_MemoryBlocks = ListDictionary_New(TRUE);

	ListDictionary_Add(g_MemoryBlocks, pvMem, (void*) hContext);
}

void* PCSC_RemoveMemoryBlock(SCARDCONTEXT hContext, void* pvMem)
{
	if (!g_MemoryBlocks)
		return NULL;

	return ListDictionary_Remove(g_MemoryBlocks, pvMem);
}

void* PCSC_SCardAllocMemory(SCARDCONTEXT hContext, size_t size)
{
	void* pvMem;

	pvMem = malloc(size);

	if (!pvMem)
		return NULL;

	PCSC_AddMemoryBlock(hContext, pvMem);

	return pvMem;
}

/**
 * Standard Windows Smart Card API (PCSC)
 */

WINSCARDAPI LONG WINAPI PCSC_SCardEstablishContext(DWORD dwScope,
		LPCVOID pvReserved1, LPCVOID pvReserved2, LPSCARDCONTEXT phContext)
{
	LONG status = SCARD_S_SUCCESS;

	if (g_PCSC.pfnSCardEstablishContext)
	{
		dwScope = SCARD_SCOPE_SYSTEM; /* this is the only scope supported by pcsc-lite */

		status = g_PCSC.pfnSCardEstablishContext(dwScope, pvReserved1, pvReserved2, phContext);
		status = PCSC_MapErrorCodeToWinSCard(status);

		if (!status)
			PCSC_EstablishCardContext(*phContext);
	}

	return status;
}

WINSCARDAPI LONG WINAPI PCSC_SCardReleaseContext(SCARDCONTEXT hContext)
{
	LONG status = SCARD_S_SUCCESS;

	if (g_PCSC.pfnSCardReleaseContext)
	{
		status = g_PCSC.pfnSCardReleaseContext(hContext);
		status = PCSC_MapErrorCodeToWinSCard(status);

		if (!status)
			PCSC_ReleaseCardContext(hContext);
	}

	return status;
}

WINSCARDAPI LONG WINAPI PCSC_SCardIsValidContext(SCARDCONTEXT hContext)
{
	LONG status = SCARD_S_SUCCESS;

	if (g_PCSC.pfnSCardIsValidContext)
	{
		status = g_PCSC.pfnSCardIsValidContext(hContext);
		status = PCSC_MapErrorCodeToWinSCard(status);
	}

	return status;
}

WINSCARDAPI LONG WINAPI PCSC_SCardListReaderGroupsA(SCARDCONTEXT hContext,
		LPSTR mszGroups, LPDWORD pcchGroups)
{
	LONG status = SCARD_S_SUCCESS;

	PCSC_LockCardContext(hContext);

	if (g_PCSC.pfnSCardListReaderGroups)
	{
		status = g_PCSC.pfnSCardListReaderGroups(hContext, mszGroups, pcchGroups);
		status = PCSC_MapErrorCodeToWinSCard(status);
	}

	PCSC_UnlockCardContext(hContext);

	return status;
}

WINSCARDAPI LONG WINAPI PCSC_SCardListReaderGroupsW(SCARDCONTEXT hContext,
		LPWSTR mszGroups, LPDWORD pcchGroups)
{
	LONG status = SCARD_S_SUCCESS;

	PCSC_LockCardContext(hContext);

	if (g_PCSC.pfnSCardListReaderGroups)
	{
		mszGroups = NULL;
		pcchGroups = 0;

		/* FIXME: unicode conversion */

		status = g_PCSC.pfnSCardListReaderGroups(hContext, (LPSTR) mszGroups, pcchGroups);
		status = PCSC_MapErrorCodeToWinSCard(status);
	}

	PCSC_UnlockCardContext(hContext);

	return status;
}

WINSCARDAPI LONG WINAPI PCSC_SCardListReaders_Internal(SCARDCONTEXT hContext,
		LPCSTR mszGroups, LPSTR mszReaders, LPDWORD pcchReaders)
{
	LONG status = SCARD_S_SUCCESS;

	if (g_PCSC.pfnSCardListReaders)
	{
		BOOL pcchReadersWrapAlloc = FALSE;
		LPSTR* pMszReaders = (LPSTR*) mszReaders;

		mszGroups = NULL; /* mszGroups is not supported by pcsc-lite */

		if ((*pcchReaders == SCARD_AUTOALLOCATE) && !g_SCardAutoAllocate)
			pcchReadersWrapAlloc = TRUE;

		if (pcchReadersWrapAlloc)
		{
			*pcchReaders = 0;

			status = g_PCSC.pfnSCardListReaders(hContext, mszGroups, NULL, pcchReaders);

			if (!status)
			{
				*pMszReaders = calloc(1, *pcchReaders);

				if (!*pMszReaders)
					return SCARD_E_NO_MEMORY;

				status = g_PCSC.pfnSCardListReaders(hContext, mszGroups, *pMszReaders, pcchReaders);

				if (status)
					free(*pMszReaders);
				else
					PCSC_AddMemoryBlock(hContext, *pMszReaders);
			}
		}
		else
		{
			status = g_PCSC.pfnSCardListReaders(hContext, mszGroups, mszReaders, pcchReaders);
		}

		status = PCSC_MapErrorCodeToWinSCard(status);
	}

	return status;
}

WINSCARDAPI LONG WINAPI PCSC_SCardListReadersA(SCARDCONTEXT hContext,
		LPCSTR mszGroups, LPSTR mszReaders, LPDWORD pcchReaders)
{
	LONG status = SCARD_S_SUCCESS;

	PCSC_LockCardContext(hContext);

	if (g_PCSC.pfnSCardListReaders)
	{
		char* mszReadersWinSCard = NULL;
		LPSTR* pMszReaders = (LPSTR*) mszReaders;

		status = PCSC_SCardListReaders_Internal(hContext, mszGroups, mszReaders, pcchReaders);

		if (!status)
		{
			mszReadersWinSCard = PCSC_ConvertReaderNamesToWinSCard(*pMszReaders);

			if (mszReadersWinSCard)
			{
				PCSC_SCardFreeMemory_Internal(hContext, *pMszReaders);

				*pMszReaders = mszReadersWinSCard;
				*pcchReaders = PCSC_MultiStringLengthA(*pMszReaders) + 2;
				PCSC_AddMemoryBlock(hContext, *pMszReaders);
			}
		}

		status = PCSC_MapErrorCodeToWinSCard(status);
	}

	PCSC_UnlockCardContext(hContext);

	return status;
}

WINSCARDAPI LONG WINAPI PCSC_SCardListReadersW(SCARDCONTEXT hContext,
		LPCWSTR mszGroups, LPWSTR mszReaders, LPDWORD pcchReaders)
{
	LONG status = SCARD_S_SUCCESS;

	PCSC_LockCardContext(hContext);

	if (g_PCSC.pfnSCardListReaders)
	{
		LPSTR mszGroupsA = NULL;
		LPSTR mszReadersA = NULL;
		char* mszReadersWinSCard = NULL;
		LPSTR* pMszReadersA = &mszReadersA;

		mszGroups = NULL; /* mszGroups is not supported by pcsc-lite */

		if (mszGroups)
			ConvertFromUnicode(CP_UTF8, 0, mszGroups, -1, (char**) &mszGroupsA, 0, NULL, NULL);

		status = PCSC_SCardListReaders_Internal(hContext, mszGroupsA, (LPTSTR) &mszReadersA, pcchReaders);

		if (!status)
		{
			mszReadersWinSCard = PCSC_ConvertReaderNamesToWinSCard(mszReadersA);

			if (mszReadersWinSCard)
			{
				PCSC_SCardFreeMemory_Internal(hContext, *pMszReadersA);

				*pMszReadersA = mszReadersWinSCard;
				*pcchReaders = PCSC_MultiStringLengthA(*pMszReadersA) + 2;
				PCSC_AddMemoryBlock(hContext, *pMszReadersA);
			}

			*pcchReaders = ConvertToUnicode(CP_UTF8, 0, *pMszReadersA, *pcchReaders, (WCHAR**) mszReaders, 0);
			PCSC_AddMemoryBlock(hContext, mszReaders);

			PCSC_SCardFreeMemory_Internal(hContext, *pMszReadersA);
		}

		status = PCSC_MapErrorCodeToWinSCard(status);

		free(mszGroupsA);
	}

	PCSC_UnlockCardContext(hContext);

	return status;
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

WINSCARDAPI LONG WINAPI PCSC_SCardFreeMemory_Internal(SCARDCONTEXT hContext, LPCVOID pvMem)
{
	LONG status = SCARD_S_SUCCESS;

	if (PCSC_RemoveMemoryBlock(hContext, (void*) pvMem))
	{
		free((void*) pvMem);
		status = SCARD_S_SUCCESS;
	}
	else
	{
		if (g_PCSC.pfnSCardFreeMemory)
		{
			status = g_PCSC.pfnSCardFreeMemory(hContext, pvMem);
			status = PCSC_MapErrorCodeToWinSCard(status);
		}
	}

	return status;
}

WINSCARDAPI LONG WINAPI PCSC_SCardFreeMemory(SCARDCONTEXT hContext, LPCVOID pvMem)
{
	LONG status = SCARD_S_SUCCESS;

	PCSC_LockCardContext(hContext);

	status = PCSC_SCardFreeMemory_Internal(hContext, pvMem);

	PCSC_UnlockCardContext(hContext);

	return status;
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

WINSCARDAPI LONG WINAPI PCSC_SCardGetStatusChange_Internal(SCARDCONTEXT hContext,
		DWORD dwTimeout, LPSCARD_READERSTATEA rgReaderStates, DWORD cReaders)
{
	LONG status = SCARD_S_SUCCESS;

	if (g_PCSC.pfnSCardGetStatusChange)
	{
		DWORD index;
		DWORD dwEventState;
		BOOL stateChanged = FALSE;
		LPSCARD_READERSTATEA states;

		states = (LPSCARD_READERSTATEA) calloc(cReaders, sizeof(SCARD_READERSTATEA));

		if (!states)
			return SCARD_E_NO_MEMORY;

		for (index = 0; index < cReaders; index++)
		{
			states[index].szReader = PCSC_GetReaderNameFromAlias((char*) rgReaderStates[index].szReader);

			if (!states[index].szReader)
				states[index].szReader = rgReaderStates[index].szReader;

			states[index].pvUserData = rgReaderStates[index].pvUserData;
			states[index].dwCurrentState = rgReaderStates[index].dwCurrentState;
			states[index].dwEventState = rgReaderStates[index].dwEventState;
			states[index].cbAtr = rgReaderStates[index].cbAtr;
			CopyMemory(&(states[index].rgbAtr), &(rgReaderStates[index].rgbAtr), 36);
		}

		/**
		 * pcsc-lite interprets dwTimeout value 0 as INFINITE, use value 1 as a workaround
		 */

		status = g_PCSC.pfnSCardGetStatusChange(hContext, dwTimeout ? dwTimeout : 1, states, cReaders);
		status = PCSC_MapErrorCodeToWinSCard(status);

		for (index = 0; index < cReaders; index++)
		{
			rgReaderStates[index].pvUserData = states[index].pvUserData;
			rgReaderStates[index].dwCurrentState = states[index].dwCurrentState;
			rgReaderStates[index].cbAtr = states[index].cbAtr;
			CopyMemory(&(rgReaderStates[index].rgbAtr), &(states[index].rgbAtr), 36);

			/* pcsc-lite puts an event count in the higher bits of dwEventState */
			states[index].dwEventState &= 0xFFFF;

			dwEventState = states[index].dwEventState & ~SCARD_STATE_CHANGED;

			if (dwEventState != rgReaderStates[index].dwCurrentState)
			{
				rgReaderStates[index].dwEventState = states[index].dwEventState;

				if (dwEventState & SCARD_STATE_PRESENT)
				{
					if (!(dwEventState & SCARD_STATE_EXCLUSIVE))
						rgReaderStates[index].dwEventState |= SCARD_STATE_INUSE;
				}

				stateChanged = TRUE;
			}
			else
			{
				rgReaderStates[index].dwEventState = dwEventState;
			}

			if (rgReaderStates[index].dwCurrentState & SCARD_STATE_IGNORE)
				rgReaderStates[index].dwEventState = SCARD_STATE_IGNORE;
		}

		if ((status == SCARD_S_SUCCESS) && !stateChanged)
			status = SCARD_E_TIMEOUT;
		else if ((status == SCARD_E_TIMEOUT) && stateChanged)
			return SCARD_S_SUCCESS;

		free(states);
	}

	return status;
}

WINSCARDAPI LONG WINAPI PCSC_SCardGetStatusChangeA(SCARDCONTEXT hContext,
		DWORD dwTimeout, LPSCARD_READERSTATEA rgReaderStates, DWORD cReaders)
{
	LONG status = SCARD_S_SUCCESS;

	PCSC_LockCardContext(hContext);

	if (g_PCSC.pfnSCardGetStatusChange)
	{
		status = PCSC_SCardGetStatusChange_Internal(hContext, dwTimeout, rgReaderStates, cReaders);
		status = PCSC_MapErrorCodeToWinSCard(status);
	}

	PCSC_UnlockCardContext(hContext);

	return status;
}

WINSCARDAPI LONG WINAPI PCSC_SCardGetStatusChangeW(SCARDCONTEXT hContext,
		DWORD dwTimeout, LPSCARD_READERSTATEW rgReaderStates, DWORD cReaders)
{
	LONG status = SCARD_S_SUCCESS;

	PCSC_LockCardContext(hContext);

	if (g_PCSC.pfnSCardGetStatusChange)
	{
		DWORD index;
		LPSCARD_READERSTATEA states;

		states = (LPSCARD_READERSTATEA) calloc(cReaders, sizeof(SCARD_READERSTATEA));

		if (!states)
		{
			PCSC_UnlockCardContext(hContext);
			return SCARD_E_NO_MEMORY;
		}

		for (index = 0; index < cReaders; index++)
		{
			states[index].szReader = NULL;

			ConvertFromUnicode(CP_UTF8, 0, rgReaderStates[index].szReader, -1,
					(char**) &states[index].szReader, 0, NULL, NULL);

			states[index].pvUserData = rgReaderStates[index].pvUserData;
			states[index].dwCurrentState = rgReaderStates[index].dwCurrentState;
			states[index].dwEventState = rgReaderStates[index].dwEventState;
			states[index].cbAtr = rgReaderStates[index].cbAtr;
			CopyMemory(&(states[index].rgbAtr), &(rgReaderStates[index].rgbAtr), 36);
		}

		status = PCSC_SCardGetStatusChange_Internal(hContext, dwTimeout, states, cReaders);
		status = PCSC_MapErrorCodeToWinSCard(status);

		for (index = 0; index < cReaders; index++)
		{
			free((void*) states[index].szReader);
			rgReaderStates[index].pvUserData = states[index].pvUserData;
			rgReaderStates[index].dwCurrentState = states[index].dwCurrentState;
			rgReaderStates[index].dwEventState = states[index].dwEventState;
			rgReaderStates[index].cbAtr = states[index].cbAtr;
			CopyMemory(&(rgReaderStates[index].rgbAtr), &(states[index].rgbAtr), 36);
		}

		free(states);
	}

	PCSC_UnlockCardContext(hContext);

	return status;
}

WINSCARDAPI LONG WINAPI PCSC_SCardCancel(SCARDCONTEXT hContext)
{
	LONG status = SCARD_S_SUCCESS;

	if (g_PCSC.pfnSCardCancel)
	{
		status = g_PCSC.pfnSCardCancel(hContext);
		status = PCSC_MapErrorCodeToWinSCard(status);
	}

	return status;
}

WINSCARDAPI LONG WINAPI PCSC_SCardConnect_Internal(SCARDCONTEXT hContext,
		LPCSTR szReader, DWORD dwShareMode, DWORD dwPreferredProtocols,
		LPSCARDHANDLE phCard, LPDWORD pdwActiveProtocol)
{
	LONG status = SCARD_S_SUCCESS;

	if (g_PCSC.pfnSCardConnect)
	{
		char* szReaderPCSC;

		szReaderPCSC = PCSC_GetReaderNameFromAlias((char*) szReader);

		if (!szReaderPCSC)
			szReaderPCSC = (char*) szReader;

		dwPreferredProtocols = PCSC_ConvertProtocolsFromWinSCard(dwPreferredProtocols);

		status = g_PCSC.pfnSCardConnect(hContext, szReaderPCSC,
				dwShareMode, dwPreferredProtocols, phCard, pdwActiveProtocol);
		status = PCSC_MapErrorCodeToWinSCard(status);

		if (status == SCARD_S_SUCCESS)
		{
			PCSC_AddCardHandle(hContext, *phCard);
			*pdwActiveProtocol = PCSC_ConvertProtocolsToWinSCard(*pdwActiveProtocol);
		}
	}

	return status;
}

WINSCARDAPI LONG WINAPI PCSC_SCardConnectA(SCARDCONTEXT hContext,
		LPCSTR szReader, DWORD dwShareMode, DWORD dwPreferredProtocols,
		LPSCARDHANDLE phCard, LPDWORD pdwActiveProtocol)
{
	LONG status = SCARD_S_SUCCESS;

	PCSC_LockCardContext(hContext);

	if (g_PCSC.pfnSCardConnect)
	{
		status = PCSC_SCardConnect_Internal(hContext, szReader, dwShareMode,
				dwPreferredProtocols, phCard, pdwActiveProtocol);
		status = PCSC_MapErrorCodeToWinSCard(status);
	}

	PCSC_UnlockCardContext(hContext);

	return status;
}

WINSCARDAPI LONG WINAPI PCSC_SCardConnectW(SCARDCONTEXT hContext,
		LPCWSTR szReader, DWORD dwShareMode, DWORD dwPreferredProtocols,
		LPSCARDHANDLE phCard, LPDWORD pdwActiveProtocol)
{
	LONG status = SCARD_S_SUCCESS;

	PCSC_LockCardContext(hContext);

	if (g_PCSC.pfnSCardConnect)
	{
		LONG status;
		LPSTR szReaderA = NULL;

		if (szReader)
			ConvertFromUnicode(CP_UTF8, 0, szReader, -1, &szReaderA, 0, NULL, NULL);

		status = PCSC_SCardConnect_Internal(hContext, szReaderA, dwShareMode,
				dwPreferredProtocols, phCard, pdwActiveProtocol);
		status = PCSC_MapErrorCodeToWinSCard(status);

		free(szReaderA);
	}

	PCSC_UnlockCardContext(hContext);

	return status;
}

WINSCARDAPI LONG WINAPI PCSC_SCardReconnect(SCARDHANDLE hCard,
		DWORD dwShareMode, DWORD dwPreferredProtocols, DWORD dwInitialization, LPDWORD pdwActiveProtocol)
{
	LONG status = SCARD_S_SUCCESS;

	if (g_PCSC.pfnSCardReconnect)
	{
		dwPreferredProtocols = PCSC_ConvertProtocolsFromWinSCard(dwPreferredProtocols);

		status = g_PCSC.pfnSCardReconnect(hCard, dwShareMode,
				dwPreferredProtocols, dwInitialization, pdwActiveProtocol);
		status = PCSC_MapErrorCodeToWinSCard(status);

		*pdwActiveProtocol = PCSC_ConvertProtocolsToWinSCard(*pdwActiveProtocol);
	}

	return status;
}

WINSCARDAPI LONG WINAPI PCSC_SCardDisconnect(SCARDHANDLE hCard, DWORD dwDisposition)
{
	LONG status = SCARD_S_SUCCESS;

	if (g_PCSC.pfnSCardDisconnect)
	{
		status = g_PCSC.pfnSCardDisconnect(hCard, dwDisposition);
		status = PCSC_MapErrorCodeToWinSCard(status);

		if (status == SCARD_S_SUCCESS)
			PCSC_RemoveCardHandle(hCard);
	}

	return status;
}

WINSCARDAPI LONG WINAPI PCSC_SCardBeginTransaction(SCARDHANDLE hCard)
{
	LONG status = SCARD_S_SUCCESS;

	if (g_PCSC.pfnSCardBeginTransaction)
	{
		status = g_PCSC.pfnSCardBeginTransaction(hCard);
		status = PCSC_MapErrorCodeToWinSCard(status);
	}

	return status;
}

WINSCARDAPI LONG WINAPI PCSC_SCardEndTransaction(SCARDHANDLE hCard, DWORD dwDisposition)
{
	LONG status = SCARD_S_SUCCESS;

	if (g_PCSC.pfnSCardEndTransaction)
	{
		status = g_PCSC.pfnSCardEndTransaction(hCard, dwDisposition);
		status = PCSC_MapErrorCodeToWinSCard(status);
	}

	return status;
}

WINSCARDAPI LONG WINAPI PCSC_SCardCancelTransaction(SCARDHANDLE hCard)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardState(SCARDHANDLE hCard,
		LPDWORD pdwState, LPDWORD pdwProtocol, LPBYTE pbAtr, LPDWORD pcbAtrLen)
{
	LONG status = SCARD_S_SUCCESS;

	if (g_PCSC.pfnSCardStatus)
	{
		SCARDCONTEXT hContext = 0;
		LPSTR mszReaderNames = NULL;
		DWORD cchReaderLen = SCARD_AUTOALLOCATE;

		status = g_PCSC.pfnSCardStatus(hCard, (LPSTR) &mszReaderNames, &cchReaderLen,
				pdwState, pdwProtocol, pbAtr, pcbAtrLen);
		status = PCSC_MapErrorCodeToWinSCard(status);

		if (mszReaderNames)
		{
			hContext = PCSC_GetCardContextFromHandle(hCard);
			PCSC_SCardFreeMemory_Internal(hContext, mszReaderNames);
		}

		*pdwProtocol = PCSC_ConvertProtocolsToWinSCard(*pdwProtocol);
	}

	return status;
}

WINSCARDAPI LONG WINAPI PCSC_SCardStatus_Internal(SCARDHANDLE hCard,
		LPSTR mszReaderNames, LPDWORD pcchReaderLen, LPDWORD pdwState,
		LPDWORD pdwProtocol, LPBYTE pbAtr, LPDWORD pcbAtrLen)
{
	LONG status = SCARD_S_SUCCESS;

	if (g_PCSC.pfnSCardStatus)
	{
		SCARDCONTEXT hContext;
		char* mszReaderNamesPCSC = NULL;
		BOOL pcbAtrLenWrapAlloc = FALSE;
		BOOL pcchReaderLenWrapAlloc = FALSE;
		LPBYTE* pPbAtr = (LPBYTE*) pbAtr;
		LPSTR* pMszReaderNames = (LPSTR*) mszReaderNames;

		hContext = PCSC_GetCardContextFromHandle(hCard);

		if ((*pcbAtrLen == SCARD_AUTOALLOCATE) && !g_SCardAutoAllocate)
			pcbAtrLenWrapAlloc = TRUE;

		if ((*pcchReaderLen == SCARD_AUTOALLOCATE) && !g_SCardAutoAllocate)
			pcchReaderLenWrapAlloc = TRUE;

		if (pcbAtrLenWrapAlloc || pcchReaderLenWrapAlloc)
		{
			if (pcbAtrLenWrapAlloc)
				*pcbAtrLen = 0;

			if (pcchReaderLenWrapAlloc)
				*pcchReaderLen = 0;

			status = g_PCSC.pfnSCardStatus(hCard,
					mszReaderNames, pcchReaderLen,
					pdwState, pdwProtocol, (pcchReaderLenWrapAlloc) ? NULL : pbAtr, pcbAtrLen);

			if (!status)
			{
				if (pcbAtrLenWrapAlloc)
				{
					*pPbAtr = (BYTE*) calloc(1, *pcbAtrLen);

					if (!*pPbAtr)
						return SCARD_E_NO_MEMORY;
				}

				if (pcchReaderLenWrapAlloc)
				{
					*pMszReaderNames = (LPSTR) calloc(1, *pcchReaderLen);

					if (!*pMszReaderNames)
						return SCARD_E_NO_MEMORY;
				}

				status = g_PCSC.pfnSCardStatus(hCard,
						(pcchReaderLenWrapAlloc) ? *pMszReaderNames : mszReaderNames, pcchReaderLen,
						pdwState, pdwProtocol, (pcbAtrLenWrapAlloc) ? *pPbAtr : pbAtr, pcbAtrLen);

				if (status)
				{
					if (pcbAtrLenWrapAlloc)
						free(*pPbAtr);

					if (pcchReaderLenWrapAlloc)
						free(*pMszReaderNames);
				}
				else
				{
					if (pcbAtrLenWrapAlloc)
						PCSC_AddMemoryBlock(hContext, *pPbAtr);

					if (pcchReaderLenWrapAlloc)
						PCSC_AddMemoryBlock(hContext, *pMszReaderNames);
				}
			}
		}
		else
		{
			status = g_PCSC.pfnSCardStatus(hCard, mszReaderNames, pcchReaderLen,
					pdwState, pdwProtocol, pbAtr, pcbAtrLen);
		}

		status = PCSC_MapErrorCodeToWinSCard(status);

		mszReaderNamesPCSC = PCSC_ConvertReaderNamesToWinSCard(*pMszReaderNames);

		if (mszReaderNamesPCSC)
		{
			PCSC_SCardFreeMemory_Internal(hContext, *pMszReaderNames);

			*pMszReaderNames = mszReaderNamesPCSC;
			*pcchReaderLen = PCSC_MultiStringLengthA(*pMszReaderNames) + 2;
			PCSC_AddMemoryBlock(hContext, *pMszReaderNames);
		}

		*pdwState &= 0xFFFF;
		*pdwState = PCSC_ConvertCardStateToWinSCard(*pdwState, status);

		*pdwProtocol = PCSC_ConvertProtocolsToWinSCard(*pdwProtocol);
	}

	return status;
}

WINSCARDAPI LONG WINAPI PCSC_SCardStatusA(SCARDHANDLE hCard,
		LPSTR mszReaderNames, LPDWORD pcchReaderLen, LPDWORD pdwState,
		LPDWORD pdwProtocol, LPBYTE pbAtr, LPDWORD pcbAtrLen)
{
	LONG status = SCARD_S_SUCCESS;

	if (g_PCSC.pfnSCardStatus)
	{
		status = PCSC_SCardStatus_Internal(hCard, mszReaderNames, pcchReaderLen, pdwState, pdwProtocol, pbAtr, pcbAtrLen);
	}

	return status;
}

WINSCARDAPI LONG WINAPI PCSC_SCardStatusW(SCARDHANDLE hCard,
		LPWSTR mszReaderNames, LPDWORD pcchReaderLen, LPDWORD pdwState,
		LPDWORD pdwProtocol, LPBYTE pbAtr, LPDWORD pcbAtrLen)
{
	LONG status = SCARD_S_SUCCESS;

	if (g_PCSC.pfnSCardStatus)
	{
		SCARDCONTEXT hContext = 0;
		LPSTR mszReaderNamesA = NULL;

		hContext = PCSC_GetCardContextFromHandle(hCard);

		status = PCSC_SCardStatus_Internal(hCard, (LPSTR) &mszReaderNamesA, pcchReaderLen, pdwState, pdwProtocol, pbAtr, pcbAtrLen);

		if (mszReaderNamesA)
		{
			*pcchReaderLen = ConvertToUnicode(CP_UTF8, 0, mszReaderNamesA, *pcchReaderLen, (WCHAR**) mszReaderNames, 0);
			PCSC_AddMemoryBlock(hContext, mszReaderNames);

			PCSC_SCardFreeMemory_Internal(hContext, mszReaderNamesA);
		}
	}

	return status;
}

WINSCARDAPI LONG WINAPI PCSC_SCardTransmit(SCARDHANDLE hCard,
		LPCSCARD_IO_REQUEST pioSendPci, LPCBYTE pbSendBuffer, DWORD cbSendLength,
		LPSCARD_IO_REQUEST pioRecvPci, LPBYTE pbRecvBuffer, LPDWORD pcbRecvLength)
{
	LONG status = SCARD_S_SUCCESS;

	if (g_PCSC.pfnSCardTransmit)
	{
		if (!pioSendPci)
		{
			DWORD dwState = 0;
			DWORD cbAtrLen = 0;
			DWORD dwProtocol = 0;
			DWORD cchReaderLen = 0;

			/**
			 * pcsc-lite cannot have a null pioSendPci parameter, unlike WinSCard.
			 * Query the current protocol and use default SCARD_IO_REQUEST for it.
			 */

			status = SCardStatusA(hCard, NULL, &cchReaderLen, &dwState, &dwProtocol, NULL, &cbAtrLen);

			if (status == SCARD_S_SUCCESS)
			{
				if (dwProtocol == SCARD_PROTOCOL_T0)
					pioSendPci = SCARD_PCI_T0;
				else if (dwProtocol == SCARD_PROTOCOL_T1)
					pioSendPci = SCARD_PCI_T1;
				else if (dwProtocol == SCARD_PROTOCOL_RAW)
					pioSendPci = SCARD_PCI_RAW;
			}
		}

		status = g_PCSC.pfnSCardTransmit(hCard, pioSendPci, pbSendBuffer,
				cbSendLength, pioRecvPci, pbRecvBuffer, pcbRecvLength);
		status = PCSC_MapErrorCodeToWinSCard(status);
	}

	return status;
}

WINSCARDAPI LONG WINAPI PCSC_SCardGetTransmitCount(SCARDHANDLE hCard, LPDWORD pcTransmitCount)
{
	return 0;
}

WINSCARDAPI LONG WINAPI PCSC_SCardControl(SCARDHANDLE hCard,
		DWORD dwControlCode, LPCVOID lpInBuffer, DWORD cbInBufferSize,
		LPVOID lpOutBuffer, DWORD cbOutBufferSize, LPDWORD lpBytesReturned)
{
	LONG status = SCARD_S_SUCCESS;

	if (g_PCSC.pfnSCardControl)
	{
		status = g_PCSC.pfnSCardControl(hCard,
				dwControlCode, lpInBuffer, cbInBufferSize,
				lpOutBuffer, cbOutBufferSize, lpBytesReturned);
		status = PCSC_MapErrorCodeToWinSCard(status);
	}

	return status;
}

WINSCARDAPI LONG WINAPI PCSC_SCardGetAttrib_Internal(SCARDHANDLE hCard, DWORD dwAttrId, LPBYTE pbAttr, LPDWORD pcbAttrLen)
{
	LONG status = SCARD_S_SUCCESS;

	if (g_PCSC.pfnSCardGetAttrib)
	{
		SCARDCONTEXT hContext = 0;
		BOOL pcbAttrLenWrapAlloc = FALSE;
		LPBYTE* pPbAttr = (LPBYTE*) pbAttr;

		if ((*pcbAttrLen == SCARD_AUTOALLOCATE) && !g_SCardAutoAllocate)
			pcbAttrLenWrapAlloc = TRUE;

		if (pcbAttrLenWrapAlloc)
		{
			*pcbAttrLen = 0;

			status = g_PCSC.pfnSCardGetAttrib(hCard, dwAttrId, NULL, pcbAttrLen);

			if (!status)
			{
				*pPbAttr = (BYTE*) calloc(1, *pcbAttrLen);

				if (!*pPbAttr)
					return SCARD_E_NO_MEMORY;

				status = g_PCSC.pfnSCardGetAttrib(hCard, dwAttrId, *pPbAttr, pcbAttrLen);

				if (status)
				{
					free(*pPbAttr);
				}
				else
				{
					hContext = PCSC_GetCardContextFromHandle(hCard);
					PCSC_AddMemoryBlock(hContext, *pPbAttr);
				}
			}
		}
		else
		{
			status = g_PCSC.pfnSCardGetAttrib(hCard, dwAttrId, pbAttr, pcbAttrLen);
		}

		status = PCSC_MapErrorCodeToWinSCard(status);
	}

	return status;
}

WINSCARDAPI LONG WINAPI PCSC_SCardGetAttrib(SCARDHANDLE hCard, DWORD dwAttrId, LPBYTE pbAttr, LPDWORD pcbAttrLen)
{
	DWORD cbAttrLen;
	SCARDCONTEXT hContext;
	BOOL attrAutoAlloc = FALSE;
	LONG status = SCARD_S_SUCCESS;
	LPBYTE* pPbAttr = (LPBYTE*) pbAttr;

	cbAttrLen = *pcbAttrLen;

	if (*pcbAttrLen == SCARD_AUTOALLOCATE)
		attrAutoAlloc = TRUE;

	hContext = PCSC_GetCardContextFromHandle(hCard);

	status = PCSC_SCardGetAttrib_Internal(hCard, dwAttrId, pbAttr, pcbAttrLen);

	if (status == SCARD_S_SUCCESS)
	{
		if (dwAttrId == SCARD_ATTR_VENDOR_NAME)
		{
			/**
			 * pcsc-lite adds a null terminator to the vendor name,
			 * while WinSCard doesn't. Strip the null terminator.
			 */

			if (attrAutoAlloc)
				*pcbAttrLen = strlen((char*) *pPbAttr);
			else
				*pcbAttrLen = strlen((char*) pbAttr);
		}
	}
	else if (status == SCARD_E_UNSUPPORTED_FEATURE)
	{
		if (dwAttrId == SCARD_ATTR_DEVICE_FRIENDLY_NAME_A)
		{
			WCHAR* pbAttrW = NULL;

			*pcbAttrLen = SCARD_AUTOALLOCATE;

			status = PCSC_SCardGetAttrib_Internal(hCard, SCARD_ATTR_DEVICE_FRIENDLY_NAME_W,
					(LPBYTE) &pbAttrW, pcbAttrLen);

			if (status)
			{
				int length;
				char* pbAttrA = NULL;

				length = ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) pbAttrW,
						*pcbAttrLen, (char**) &pbAttrA, 0, NULL, NULL);

				PCSC_SCardFreeMemory_Internal(hContext, pbAttrW);

				if (attrAutoAlloc)
				{
					PCSC_AddMemoryBlock(hContext, pbAttrA);
					*pPbAttr = (BYTE*) pbAttrA;
					*pcbAttrLen = length;
				}
				else
				{
					if (length > cbAttrLen)
					{
						free(pbAttrA);
						return SCARD_E_INSUFFICIENT_BUFFER;
					}
					else
					{
						CopyMemory(pbAttr, (BYTE*) pbAttrA, length);
						*pcbAttrLen = length;
					}
				}
			}
		}
		else if (dwAttrId == SCARD_ATTR_DEVICE_FRIENDLY_NAME_W)
		{
			char* pbAttrA = NULL;

			*pcbAttrLen = SCARD_AUTOALLOCATE;

			status = PCSC_SCardGetAttrib_Internal(hCard, SCARD_ATTR_DEVICE_FRIENDLY_NAME_A,
					(LPBYTE) &pbAttrA, pcbAttrLen);

			if (status)
			{
				int length;
				WCHAR* pbAttrW = NULL;

				length = ConvertToUnicode(CP_UTF8, 0, (char*) pbAttr, *pcbAttrLen, &pbAttrW, 0);

				PCSC_SCardFreeMemory_Internal(hContext, pbAttrA);

				if (attrAutoAlloc)
				{
					PCSC_AddMemoryBlock(hContext, pbAttrW);
					*pPbAttr = (BYTE*) pbAttrW;
					*pcbAttrLen = length;
				}
				else
				{
					if (length > cbAttrLen)
					{
						free(pbAttrW);
						return SCARD_E_INSUFFICIENT_BUFFER;
					}
					else
					{
						CopyMemory(pbAttr, (BYTE*) pbAttrW, length);
						*pcbAttrLen = length;
					}
				}
			}
		}
	}

	return status;
}

WINSCARDAPI LONG WINAPI PCSC_SCardSetAttrib(SCARDHANDLE hCard, DWORD dwAttrId, LPCBYTE pbAttr, DWORD cbAttrLen)
{
	LONG status = SCARD_S_SUCCESS;

	if (g_PCSC.pfnSCardSetAttrib)
	{
		status = g_PCSC.pfnSCardSetAttrib(hCard, dwAttrId, pbAttr, cbAttrLen);
		status = PCSC_MapErrorCodeToWinSCard(status);
	}

	return status;
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

extern PCSCFunctionTable g_PCSC_Link;
extern int PCSC_InitializeSCardApi_Link(void);

int PCSC_InitializeSCardApi(void)
{
#if 0
	if (PCSC_InitializeSCardApi_Link() >= 0)
	{
		g_PCSC.pfnSCardEstablishContext = g_PCSC_Link.pfnSCardEstablishContext;
		g_PCSC.pfnSCardReleaseContext = g_PCSC_Link.pfnSCardReleaseContext;
		g_PCSC.pfnSCardIsValidContext = g_PCSC_Link.pfnSCardIsValidContext;
		g_PCSC.pfnSCardConnect = g_PCSC_Link.pfnSCardConnect;
		g_PCSC.pfnSCardReconnect = g_PCSC_Link.pfnSCardReconnect;
		g_PCSC.pfnSCardDisconnect = g_PCSC_Link.pfnSCardDisconnect;
		g_PCSC.pfnSCardBeginTransaction = g_PCSC_Link.pfnSCardBeginTransaction;
		g_PCSC.pfnSCardEndTransaction = g_PCSC_Link.pfnSCardEndTransaction;
		g_PCSC.pfnSCardStatus = g_PCSC_Link.pfnSCardStatus;
		g_PCSC.pfnSCardGetStatusChange = g_PCSC_Link.pfnSCardGetStatusChange;
		g_PCSC.pfnSCardControl = g_PCSC_Link.pfnSCardControl;
		g_PCSC.pfnSCardTransmit = g_PCSC_Link.pfnSCardTransmit;
		g_PCSC.pfnSCardListReaderGroups = g_PCSC_Link.pfnSCardListReaderGroups;
		g_PCSC.pfnSCardListReaders = g_PCSC_Link.pfnSCardListReaders;
		g_PCSC.pfnSCardFreeMemory = g_PCSC_Link.pfnSCardFreeMemory;
		g_PCSC.pfnSCardCancel = g_PCSC_Link.pfnSCardCancel;
		g_PCSC.pfnSCardGetAttrib = g_PCSC_Link.pfnSCardGetAttrib;
		g_PCSC.pfnSCardSetAttrib = g_PCSC_Link.pfnSCardSetAttrib;
		
		return 1;
	}
#endif
	
#ifdef __MACOSX__
	g_PCSCModule = LoadLibraryA("/System/Library/Frameworks/PCSC.framework/PCSC");
#else
	g_PCSCModule = LoadLibraryA("libpcsclite.so");
#endif
	
	if (!g_PCSCModule)
		return -1;

	g_PCSC.pfnSCardEstablishContext = (void*) GetProcAddress(g_PCSCModule, "SCardEstablishContext");
	g_PCSC.pfnSCardReleaseContext = (void*) GetProcAddress(g_PCSCModule, "SCardReleaseContext");
	g_PCSC.pfnSCardIsValidContext = (void*) GetProcAddress(g_PCSCModule, "SCardIsValidContext");
	g_PCSC.pfnSCardConnect = (void*) GetProcAddress(g_PCSCModule, "SCardConnect");
	g_PCSC.pfnSCardReconnect = (void*) GetProcAddress(g_PCSCModule, "SCardReconnect");
	g_PCSC.pfnSCardDisconnect = (void*) GetProcAddress(g_PCSCModule, "SCardDisconnect");
	g_PCSC.pfnSCardBeginTransaction = (void*) GetProcAddress(g_PCSCModule, "SCardBeginTransaction");
	g_PCSC.pfnSCardEndTransaction = (void*) GetProcAddress(g_PCSCModule, "SCardEndTransaction");
	g_PCSC.pfnSCardStatus = (void*) GetProcAddress(g_PCSCModule, "SCardStatus");
	g_PCSC.pfnSCardGetStatusChange = (void*) GetProcAddress(g_PCSCModule, "SCardGetStatusChange");
	g_PCSC.pfnSCardControl = (void*) GetProcAddress(g_PCSCModule, "SCardControl");
	g_PCSC.pfnSCardTransmit = (void*) GetProcAddress(g_PCSCModule, "SCardTransmit");
	g_PCSC.pfnSCardListReaderGroups = (void*) GetProcAddress(g_PCSCModule, "SCardListReaderGroups");
	g_PCSC.pfnSCardListReaders = (void*) GetProcAddress(g_PCSCModule, "SCardListReaders");
	g_PCSC.pfnSCardCancel = (void*) GetProcAddress(g_PCSCModule, "SCardCancel");
	g_PCSC.pfnSCardGetAttrib = (void*) GetProcAddress(g_PCSCModule, "SCardGetAttrib");
	g_PCSC.pfnSCardSetAttrib = (void*) GetProcAddress(g_PCSCModule, "SCardSetAttrib");
	
	g_PCSC.pfnSCardFreeMemory = NULL;
	
#ifndef __MACOSX__
	g_PCSC.pfnSCardFreeMemory = (void*) GetProcAddress(g_PCSCModule, "SCardFreeMemory");
#endif
	
	if (g_PCSC.pfnSCardFreeMemory)
		g_SCardAutoAllocate = TRUE;

#ifdef DISABLE_PCSC_SCARD_AUTOALLOCATE
	g_PCSC.pfnSCardFreeMemory = NULL;
	g_SCardAutoAllocate = FALSE;
#endif

	return 1;
}
