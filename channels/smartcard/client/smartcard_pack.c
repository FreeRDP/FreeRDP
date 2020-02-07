/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Smart Card Structure Packing
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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
#include <winpr/print.h>

#include "smartcard_pack.h"

static LONG smartcard_unpack_redir_scard_context(SMARTCARD_DEVICE* smartcard, wStream* s,
                                                 REDIR_SCARDCONTEXT* context);
static LONG smartcard_pack_redir_scard_context(SMARTCARD_DEVICE* smartcard, wStream* s,
                                               const REDIR_SCARDCONTEXT* context);
static LONG smartcard_unpack_redir_scard_handle(SMARTCARD_DEVICE* smartcard, wStream* s,
                                                REDIR_SCARDHANDLE* handle);
static LONG smartcard_pack_redir_scard_handle(SMARTCARD_DEVICE* smartcard, wStream* s,
                                              const REDIR_SCARDHANDLE* handle);
static LONG smartcard_unpack_redir_scard_context_ref(SMARTCARD_DEVICE* smartcard, wStream* s,
                                                     REDIR_SCARDCONTEXT* context);
static LONG smartcard_pack_redir_scard_context_ref(SMARTCARD_DEVICE* smartcard, wStream* s,
                                                   const REDIR_SCARDCONTEXT* context);

static LONG smartcard_unpack_redir_scard_handle_ref(SMARTCARD_DEVICE* smartcard, wStream* s,
                                                    REDIR_SCARDHANDLE* handle);
static LONG smartcard_pack_redir_scard_handle_ref(SMARTCARD_DEVICE* smartcard, wStream* s,
                                                  const REDIR_SCARDHANDLE* handle);

static LONG smartcard_ndr_read(wStream* s, BYTE** data, size_t min)
{
	UINT32 len;
	void* r;

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_ERR(TAG, "Short data while trying to read NDR pointer, expected 4, got %" PRIu32,
		         Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}
	Stream_Read_UINT32(s, len);

	if (min > len)
	{
		WLog_ERR(TAG, "Invalid length read from NDR pointer, minimum %" PRIu32 ", got %" PRIu32,
		         min, len);
		return STATUS_DATA_ERROR;
	}

	if (Stream_GetRemainingLength(s) < len)
	{
		WLog_ERR(TAG,
		         "Short data while trying to read data from NDR pointer, expected %" PRIu32
		         ", got %" PRIu32,
		         len, Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	r = calloc(len + 1, sizeof(CHAR));
	if (!r)
		return SCARD_E_NO_MEMORY;
	Stream_Read(s, r, len);
	*data = r;
	return STATUS_SUCCESS;
}

static LONG smartcard_ndr_read_a(wStream* s, CHAR** data, size_t min)
{
	union {
		CHAR** ppc;
		BYTE** ppv;
	} u;
	u.ppc = data;
	return smartcard_ndr_read(s, u.ppv, min);
}

static LONG smartcard_ndr_read_w(wStream* s, WCHAR** data, size_t min)
{
	union {
		WCHAR** ppc;
		BYTE** ppv;
	} u;
	u.ppc = data;
	return smartcard_ndr_read(s, u.ppv, min);
}

static LONG smartcard_ndr_read_u(wStream* s, UUID** data, size_t min)
{
	union {
		UUID** ppc;
		BYTE** ppv;
	} u;
	u.ppc = data;
	return smartcard_ndr_read(s, u.ppv, min);
}

static char* smartcard_convert_string_list(const void* in, size_t bytes, BOOL unicode)
{
	size_t index, length;
	union {
		const void* pv;
		const char* sz;
		const WCHAR* wz;
	} string;
	char* mszA = NULL;

	string.pv = in;

	if (unicode)
	{
		length = (bytes / 2);
		if (ConvertFromUnicode(CP_UTF8, 0, string.wz, (int)length, &mszA, 0, NULL, NULL) !=
		    (int)length)
		{
			free(mszA);
			return NULL;
		}
	}
	else
	{
		length = bytes;
		mszA = (char*)malloc(length);
		if (!mszA)
			return NULL;
		CopyMemory(mszA, string.sz, length);
	}

	for (index = 0; index < length - 1; index++)
	{
		if (mszA[index] == '\0')
			mszA[index] = ',';
	}

	return mszA;
}

static char* smartcard_msz_dump_a(const char* msz, size_t len, char* buffer, size_t bufferLen)
{
	char* buf = buffer;
	const char* cur = msz;

	while ((len > 0) && cur && cur[0] != '\0' && (bufferLen > 0))
	{
		size_t clen = strnlen(cur, len);
		int rc = _snprintf(buf, bufferLen, "%s", cur);
		bufferLen -= (size_t)rc;
		buf += rc;

		cur += clen;
	}

	return buffer;
}

static char* smartcard_msz_dump_w(const WCHAR* msz, size_t len, char* buffer, size_t bufferLen)
{
	char* sz;
	ConvertFromUnicode(CP_UTF8, 0, msz, (int)len, &sz, 0, NULL, NULL);
	return smartcard_msz_dump_a(sz, len, buffer, bufferLen);
}

static char* smartcard_array_dump(const void* pd, size_t len, char* buffer, size_t bufferLen)
{
	const BYTE* data = pd;
	size_t x;
	int rc;
	char* start = buffer;

	rc = _snprintf(buffer, bufferLen, "{ ");
	buffer += rc;
	bufferLen -= (size_t)rc;

	for (x = 0; x < len; x++)
	{
		rc = _snprintf(buffer, bufferLen, "%02X", data[x]);
		buffer += rc;
		bufferLen -= (size_t)rc;
	}

	rc = _snprintf(buffer, bufferLen, "}");
	buffer += rc;
	bufferLen -= (size_t)rc;

	return start;
}
static void smartcard_log_redir_handle(const char* tag, const REDIR_SCARDHANDLE* pHandle)
{
	char buffer[128];

	WLog_DBG(tag, "hContext: %s",
	         smartcard_array_dump(pHandle->pbHandle, pHandle->cbHandle, buffer, sizeof(buffer)));
}

static void smartcard_log_context(const char* tag, const REDIR_SCARDCONTEXT* phContext)
{
	char buffer[128];
	WLog_DBG(
	    tag, "hContext: %s",
	    smartcard_array_dump(phContext->pbContext, phContext->cbContext, buffer, sizeof(buffer)));
}

static void smartcard_trace_context_and_string_call_a(const char* name,
                                                      const REDIR_SCARDCONTEXT* phContext,
                                                      const CHAR* sz)
{
	if (!WLog_IsLevelActive(WLog_Get(TAG), WLOG_DEBUG))
		return;

	WLog_DBG(TAG, "%s {", name);
	smartcard_log_context(TAG, phContext);
	WLog_DBG(TAG, "  sz=%s", sz);

	WLog_DBG(TAG, "}");
}

static void smartcard_trace_context_and_string_call_w(const char* name,
                                                      const REDIR_SCARDCONTEXT* phContext,
                                                      const WCHAR* sz)
{
	char* tmp;
	if (!WLog_IsLevelActive(WLog_Get(TAG), WLOG_DEBUG))
		return;

	WLog_DBG(TAG, "%s {", name);
	smartcard_log_context(TAG, phContext);
	ConvertFromUnicode(CP_UTF8, 0, sz, -1, &tmp, 0, NULL, NULL);
	WLog_DBG(TAG, "  sz=%s", tmp);
	free(tmp);

	WLog_DBG(TAG, "}");
}

static void smartcard_trace_context_call(SMARTCARD_DEVICE* smartcard, const Context_Call* call,
                                         const char* name)
{
	WINPR_UNUSED(smartcard);

	if (!WLog_IsLevelActive(WLog_Get(TAG), WLOG_DEBUG))
		return;

	WLog_DBG(TAG, "%s_Call {", name);
	smartcard_log_context(TAG, &call->hContext);

	WLog_DBG(TAG, "}");
}

static void smartcard_trace_list_reader_groups_call(SMARTCARD_DEVICE* smartcard,
                                                    const ListReaderGroups_Call* call, BOOL unicode)
{
	WINPR_UNUSED(smartcard);

	if (!WLog_IsLevelActive(WLog_Get(TAG), WLOG_DEBUG))
		return;

	WLog_DBG(TAG, "ListReaderGroups%S_Call {", unicode ? "W" : "A");
	smartcard_log_context(TAG, &call->hContext);

	WLog_DBG(TAG, "fmszGroupsIsNULL: %" PRId32 " cchGroups: 0x%08" PRIx32, call->fmszGroupsIsNULL,
	         call->cchGroups);
	WLog_DBG(TAG, "}");
}

static void smartcard_trace_get_status_change_w_call(SMARTCARD_DEVICE* smartcard,
                                                     const GetStatusChangeW_Call* call)
{
	UINT32 index;
	char* szEventState;
	char* szCurrentState;
	LPSCARD_READERSTATEW readerState;
	WINPR_UNUSED(smartcard);

	if (!WLog_IsLevelActive(WLog_Get(TAG), WLOG_DEBUG))
		return;

	WLog_DBG(TAG, "GetStatusChangeW_Call {");
	smartcard_log_context(TAG, &call->hContext);

	WLog_DBG(TAG, "dwTimeOut: 0x%08" PRIX32 " cReaders: %" PRIu32 "", call->dwTimeOut,
	         call->cReaders);

	for (index = 0; index < call->cReaders; index++)
	{
		char* szReaderA = NULL;
		readerState = &call->rgReaderStates[index];
		ConvertFromUnicode(CP_UTF8, 0, readerState->szReader, -1, &szReaderA, 0, NULL, NULL);
		WLog_DBG(TAG, "\t[%" PRIu32 "]: szReader: %s cbAtr: %" PRIu32 "", index, szReaderA,
		         readerState->cbAtr);
		szCurrentState = SCardGetReaderStateString(readerState->dwCurrentState);
		szEventState = SCardGetReaderStateString(readerState->dwEventState);
		WLog_DBG(TAG, "\t[%" PRIu32 "]: dwCurrentState: %s (0x%08" PRIX32 ")", index,
		         szCurrentState, readerState->dwCurrentState);
		WLog_DBG(TAG, "\t[%" PRIu32 "]: dwEventState: %s (0x%08" PRIX32 ")", index, szEventState,
		         readerState->dwEventState);
		free(szCurrentState);
		free(szEventState);
		free(szReaderA);
	}

	WLog_DBG(TAG, "}");
}

static void smartcard_trace_list_reader_groups_return(SMARTCARD_DEVICE* smartcard,
                                                      const ListReaderGroups_Return* ret,
                                                      BOOL unicode)
{
	char* mszA = NULL;
	WINPR_UNUSED(smartcard);

	if (!WLog_IsLevelActive(WLog_Get(TAG), WLOG_DEBUG))
		return;

	mszA = smartcard_convert_string_list(ret->msz, ret->cBytes, unicode);

	WLog_DBG(TAG, "ListReaderGroups%s_Return {", unicode ? "W" : "A");
	WLog_DBG(TAG, "ReturnCode: %s (0x%08" PRIx32 ")", SCardGetErrorString(ret->ReturnCode),
	         ret->ReturnCode);
	WLog_DBG(TAG, "cBytes: %" PRIu32 " msz: %s", ret->cBytes, mszA);
	WLog_DBG(TAG, "}");
	free(mszA);
}

static void smartcard_trace_list_readers_call(SMARTCARD_DEVICE* smartcard,
                                              const ListReaders_Call* call, BOOL unicode)
{
	char* mszGroupsA = NULL;
	WINPR_UNUSED(smartcard);

	if (!WLog_IsLevelActive(WLog_Get(TAG), WLOG_DEBUG))
		return;

	mszGroupsA = smartcard_convert_string_list(call->mszGroups, call->cBytes, unicode);

	WLog_DBG(TAG, "ListReaders%s_Call {", unicode ? "W" : "A");
	smartcard_log_context(TAG, &call->hContext);

	WLog_DBG(TAG,
	         "cBytes: %" PRIu32 " mszGroups: %s fmszReadersIsNULL: %" PRId32
	         " cchReaders: 0x%08" PRIX32 "",
	         call->cBytes, mszGroupsA, call->fmszReadersIsNULL, call->cchReaders);
	WLog_DBG(TAG, "}");

	free(mszGroupsA);
}

static void smartcard_trace_locate_cards_by_atr_a_call(SMARTCARD_DEVICE* smartcard,
                                                       const LocateCardsByATRA_Call* call)
{
	UINT32 index;
	char* szEventState;
	char* szCurrentState;
	char* rgbAtr;
	LPSCARD_READERSTATEA readerState;
	WINPR_UNUSED(smartcard);

	if (!WLog_IsLevelActive(WLog_Get(TAG), WLOG_DEBUG))
		return;

	WLog_DBG(TAG, "LocateCardsByATRA_Call {");
	smartcard_log_context(TAG, &call->hContext);

	for (index = 0; index < call->cReaders; index++)
	{
		readerState = (LPSCARD_READERSTATEA)&call->rgReaderStates[index];
		WLog_DBG(TAG, "\t[%" PRIu32 "]: szReader: %s cbAtr: %" PRIu32 "", index,
		         readerState->szReader, readerState->cbAtr);
		szCurrentState = SCardGetReaderStateString(readerState->dwCurrentState);
		szEventState = SCardGetReaderStateString(readerState->dwEventState);
		rgbAtr = winpr_BinToHexString((BYTE*)&(readerState->rgbAtr), readerState->cbAtr, FALSE);
		WLog_DBG(TAG, "\t[%" PRIu32 "]: dwCurrentState: %s (0x%08" PRIX32 ")", index,
		         szCurrentState, readerState->dwCurrentState);
		WLog_DBG(TAG, "\t[%" PRIu32 "]: dwEventState: %s (0x%08" PRIX32 ")", index, szEventState,
		         readerState->dwEventState);

		if (rgbAtr)
		{
			WLog_DBG(TAG, "\t[%" PRIu32 "]: cbAtr: %" PRIu32 " rgbAtr: %s", index,
			         readerState->cbAtr, rgbAtr);
		}
		else
		{
			WLog_DBG(TAG, "\t[%" PRIu32 "]: cbAtr: 0 rgbAtr: n/a", index);
		}

		free(szCurrentState);
		free(szEventState);
		free(rgbAtr);
	}

	WLog_DBG(TAG, "}");
}

static void smartcard_trace_locate_cards_a_call(SMARTCARD_DEVICE* smartcard,
                                                const LocateCardsA_Call* call)
{
	char buffer[8192];
	WINPR_UNUSED(smartcard);

	if (!WLog_IsLevelActive(WLog_Get(TAG), WLOG_DEBUG))
		return;

	WLog_DBG(TAG, "LocateCardsA_Call {");
	smartcard_log_context(TAG, &call->hContext);
	WLog_DBG(TAG, " cBytes=%" PRId32, call->cBytes);
	WLog_DBG(TAG, " mszCards=%s",
	         smartcard_msz_dump_a(call->mszCards, call->cBytes, buffer, sizeof(buffer)));
	WLog_DBG(TAG, " cReaders=%" PRId32, call->cReaders);
	// WLog_DBG(TAG, " cReaders=%" PRId32, call->rgReaderStates);

	WLog_DBG(TAG, "}");
}

static void smartcard_trace_locate_cards_return(SMARTCARD_DEVICE* smartcard,
                                                const LocateCards_Return* ret)
{
	WINPR_UNUSED(smartcard);
	if (!WLog_IsLevelActive(WLog_Get(TAG), WLOG_DEBUG))
		return;

	WLog_DBG(TAG, "LocateCards_Return {");
	WLog_DBG(TAG, "ReturnCode: %s (0x%08" PRIX32 ")", SCardGetErrorString(ret->ReturnCode),
	         ret->ReturnCode);

	if (ret->ReturnCode == SCARD_S_SUCCESS)
	{
		WLog_DBG(TAG, " cReaders=%" PRId32, ret->cReaders);
		// WLog_DBG(TAG, " cReaders=%" PRId32, call->rgReaderStates);
	}
	WLog_DBG(TAG, "}");
}

static void smartcard_trace_get_reader_icon_return(SMARTCARD_DEVICE* smartcard,
                                                   const GetReaderIcon_Return* ret)
{
	WINPR_UNUSED(smartcard);
	if (!WLog_IsLevelActive(WLog_Get(TAG), WLOG_DEBUG))
		return;

	WLog_DBG(TAG, "GetReaderIcon_Return {");
	WLog_DBG(TAG, "ReturnCode: %s (0x%08" PRIX32 ")", SCardGetErrorString(ret->ReturnCode),
	         ret->ReturnCode);

	if (ret->ReturnCode == SCARD_S_SUCCESS)
	{
		WLog_DBG(TAG, " cbDataLen=%" PRId32, ret->cbDataLen);
		// WLog_DBG(TAG, " cReaders=%" PRId32, call->pbData);
	}
	WLog_DBG(TAG, "}");
}

static void smartcard_trace_get_transmit_count_return(SMARTCARD_DEVICE* smartcard,
                                                      const GetTransmitCount_Return* ret)
{
	WINPR_UNUSED(smartcard);
	if (!WLog_IsLevelActive(WLog_Get(TAG), WLOG_DEBUG))
		return;

	WLog_DBG(TAG, "GetTransmitCount_Return {");
	WLog_DBG(TAG, "ReturnCode: %s (0x%08" PRIX32 ")", SCardGetErrorString(ret->ReturnCode),
	         ret->ReturnCode);

	WLog_DBG(TAG, " cTransmitCount=%" PRIu32, ret->cTransmitCount);
	WLog_DBG(TAG, "}");
}

static void smartcard_trace_read_cache_return(SMARTCARD_DEVICE* smartcard,
                                              const ReadCache_Return* ret)
{
	WINPR_UNUSED(smartcard);
	if (!WLog_IsLevelActive(WLog_Get(TAG), WLOG_DEBUG))
		return;

	WLog_DBG(TAG, "ReadCache_Return {");
	WLog_DBG(TAG, "ReturnCode: %s (0x%08" PRIX32 ")", SCardGetErrorString(ret->ReturnCode),
	         ret->ReturnCode);

	if (ret->ReturnCode == SCARD_S_SUCCESS)
	{
		WLog_DBG(TAG, " cbDataLen=%" PRId32, ret->cbDataLen);
		// WLog_DBG(TAG, " cReaders=%" PRId32, call->cReaders);
	}
	WLog_DBG(TAG, "}");
}

static void smartcard_trace_locate_cards_w_call(SMARTCARD_DEVICE* smartcard,
                                                const LocateCardsW_Call* call)
{
	char buffer[8192];
	WINPR_UNUSED(smartcard);
	if (!WLog_IsLevelActive(WLog_Get(TAG), WLOG_DEBUG))
		return;

	WLog_DBG(TAG, "LocateCardsW_Call {");
	smartcard_log_context(TAG, &call->hContext);
	WLog_DBG(TAG, " cBytes=%" PRId32, call->cBytes);
	WLog_DBG(TAG, " sz2=%s",
	         smartcard_msz_dump_w(call->mszCards, call->cBytes, buffer, sizeof(buffer)));
	WLog_DBG(TAG, " cReaders=%" PRId32, call->cReaders);
	// WLog_DBG(TAG, " sz2=%s", call->rgReaderStates);
	WLog_DBG(TAG, "}");
}

static void smartcard_trace_list_readers_return(SMARTCARD_DEVICE* smartcard,
                                                const ListReaders_Return* ret, BOOL unicode)
{
	char* mszA = NULL;
	WINPR_UNUSED(smartcard);

	if (!WLog_IsLevelActive(WLog_Get(TAG), WLOG_DEBUG))
		return;

	WLog_DBG(TAG, "ListReaders%s_Return {", unicode ? "W" : "A");
	WLog_DBG(TAG, "ReturnCode: %s (0x%08" PRIX32 ")", SCardGetErrorString(ret->ReturnCode),
	         ret->ReturnCode);

	if (ret->ReturnCode != SCARD_S_SUCCESS)
	{
		WLog_DBG(TAG, "}");
		return;
	}

	mszA = smartcard_convert_string_list(ret->msz, ret->cBytes, unicode);

	WLog_DBG(TAG, "cBytes: %" PRIu32 " msz: %s", ret->cBytes, mszA);
	WLog_DBG(TAG, "}");
	free(mszA);
}

static void smartcard_trace_get_status_change_return(SMARTCARD_DEVICE* smartcard,
                                                     const GetStatusChange_Return* ret,
                                                     BOOL unicode)
{
	UINT32 index;
	char* rgbAtr;
	char* szEventState;
	char* szCurrentState;
	ReaderState_Return* rgReaderState;
	WINPR_UNUSED(smartcard);

	if (!WLog_IsLevelActive(WLog_Get(TAG), WLOG_DEBUG))
		return;

	WLog_DBG(TAG, "GetStatusChange%s_Return {", unicode ? "W" : "A");
	WLog_DBG(TAG, "ReturnCode: %s (0x%08" PRIX32 ")", SCardGetErrorString(ret->ReturnCode),
	         ret->ReturnCode);
	WLog_DBG(TAG, "cReaders: %" PRIu32 "", ret->cReaders);

	for (index = 0; index < ret->cReaders; index++)
	{
		rgReaderState = &(ret->rgReaderStates[index]);
		szCurrentState = SCardGetReaderStateString(rgReaderState->dwCurrentState);
		szEventState = SCardGetReaderStateString(rgReaderState->dwEventState);
		rgbAtr = winpr_BinToHexString((BYTE*)&(rgReaderState->rgbAtr), rgReaderState->cbAtr, FALSE);
		WLog_DBG(TAG, "\t[%" PRIu32 "]: dwCurrentState: %s (0x%08" PRIX32 ")", index,
		         szCurrentState, rgReaderState->dwCurrentState);
		WLog_DBG(TAG, "\t[%" PRIu32 "]: dwEventState: %s (0x%08" PRIX32 ")", index, szEventState,
		         rgReaderState->dwEventState);
		WLog_DBG(TAG, "\t[%" PRIu32 "]: cbAtr: %" PRIu32 " rgbAtr: %s", index, rgReaderState->cbAtr,
		         rgbAtr);
		free(szCurrentState);
		free(szEventState);
		free(rgbAtr);
	}

	WLog_DBG(TAG, "}");
}

static void smartcard_trace_context_and_two_strings_a_call(SMARTCARD_DEVICE* smartcard,
                                                           const ContextAndTwoStringA_Call* call)
{
	WINPR_UNUSED(smartcard);
	if (!WLog_IsLevelActive(WLog_Get(TAG), WLOG_DEBUG))
		return;

	WLog_DBG(TAG, "ContextAndTwoStringW_Call {");
	smartcard_log_context(TAG, &call->hContext);
	WLog_DBG(TAG, " sz1=%s", call->sz1);
	WLog_DBG(TAG, " sz2=%s", call->sz2);
	WLog_DBG(TAG, "}");
}

static void smartcard_trace_context_and_two_strings_w_call(SMARTCARD_DEVICE* smartcard,
                                                           const ContextAndTwoStringW_Call* call)
{
	CHAR* sz1 = NULL;
	CHAR* sz2 = NULL;

	WINPR_UNUSED(smartcard);
	if (!WLog_IsLevelActive(WLog_Get(TAG), WLOG_DEBUG))
		return;

	WLog_DBG(TAG, "ContextAndTwoStringW_Call {");
	smartcard_log_context(TAG, &call->hContext);
	ConvertFromUnicode(CP_UTF8, 0, call->sz1, -1, &sz1, 0, NULL, NULL);
	ConvertFromUnicode(CP_UTF8, 0, call->sz2, -1, &sz2, 0, NULL, NULL);
	WLog_DBG(TAG, " sz1=%s", sz1);
	WLog_DBG(TAG, " sz2=%s", sz2);
	free(sz1);
	free(sz2);

	WLog_DBG(TAG, "}");
}

static void smartcard_trace_get_transmit_count_call(SMARTCARD_DEVICE* smartcard,
                                                    const GetTransmitCount_Call* call)
{
	WINPR_UNUSED(smartcard);
	if (!WLog_IsLevelActive(WLog_Get(TAG), WLOG_DEBUG))
		return;

	WLog_DBG(TAG, "GetTransmitCount_Call {");
	smartcard_log_context(TAG, &call->hContext);
	smartcard_log_redir_handle(TAG, &call->hCard);

	WLog_DBG(TAG, "}");
}

static void smartcard_trace_write_cache_a_call(SMARTCARD_DEVICE* smartcard,
                                               const WriteCacheA_Call* call)
{
	char buffer[1024];
	WINPR_UNUSED(smartcard);
	if (!WLog_IsLevelActive(WLog_Get(TAG), WLOG_DEBUG))
		return;

	WLog_DBG(TAG, "GetTransmitCount_Call {");

	WLog_DBG(TAG, "  szLookupName=%s", call->szLookupName);

	smartcard_log_context(TAG, &call->Common.hContext);
	WLog_DBG(
	    TAG, "..CardIdentifier=%s",
	    smartcard_array_dump(call->Common.CardIdentifier, sizeof(UUID), buffer, sizeof(buffer)));
	WLog_DBG(TAG, "  FreshnessCounter=%" PRIu32, call->Common.FreshnessCounter);
	WLog_DBG(TAG, "  cbDataLen=%" PRIu32, call->Common.cbDataLen);
	WLog_DBG(
	    TAG, "  pbData=%s",
	    smartcard_array_dump(call->Common.pbData, call->Common.cbDataLen, buffer, sizeof(buffer)));
	WLog_DBG(TAG, "}");
}

static void smartcard_trace_write_cache_w_call(SMARTCARD_DEVICE* smartcard,
                                               const WriteCacheW_Call* call)
{
	char* tmp;
	char buffer[1024];
	WINPR_UNUSED(smartcard);
	if (!WLog_IsLevelActive(WLog_Get(TAG), WLOG_DEBUG))
		return;

	WLog_DBG(TAG, "GetTransmitCount_Call {");

	ConvertFromUnicode(CP_UTF8, 0, call->szLookupName, -1, &tmp, 0, NULL, NULL);
	WLog_DBG(TAG, "  szLookupName=%s", tmp);
	free(tmp);
	smartcard_log_context(TAG, &call->Common.hContext);
	WLog_DBG(
	    TAG, "..CardIdentifier=%s",
	    smartcard_array_dump(call->Common.CardIdentifier, sizeof(UUID), buffer, sizeof(buffer)));
	WLog_DBG(TAG, "  FreshnessCounter=%" PRIu32, call->Common.FreshnessCounter);
	WLog_DBG(TAG, "  cbDataLen=%" PRIu32, call->Common.cbDataLen);
	WLog_DBG(
	    TAG, "  pbData=%s",
	    smartcard_array_dump(call->Common.pbData, call->Common.cbDataLen, buffer, sizeof(buffer)));
	WLog_DBG(TAG, "}");
}

static void smartcard_trace_read_cache_a_call(SMARTCARD_DEVICE* smartcard,
                                              const ReadCacheA_Call* call)
{
	char buffer[1024];
	WINPR_UNUSED(smartcard);
	if (!WLog_IsLevelActive(WLog_Get(TAG), WLOG_DEBUG))
		return;

	WLog_DBG(TAG, "GetTransmitCount_Call {");

	WLog_DBG(TAG, "  szLookupName=%s", call->szLookupName);
	smartcard_log_context(TAG, &call->Common.hContext);
	WLog_DBG(
	    TAG, "..CardIdentifier=%s",
	    smartcard_array_dump(call->Common.CardIdentifier, sizeof(UUID), buffer, sizeof(buffer)));
	WLog_DBG(TAG, "  FreshnessCounter=%" PRIu32, call->Common.FreshnessCounter);
	WLog_DBG(TAG, "  fPbDataIsNULL=%" PRId32, call->Common.fPbDataIsNULL);
	WLog_DBG(TAG, "  cbDataLen=%" PRIu32, call->Common.cbDataLen);

	WLog_DBG(TAG, "}");
}

static void smartcard_trace_read_cache_w_call(SMARTCARD_DEVICE* smartcard,
                                              const ReadCacheW_Call* call)
{
	char* tmp;
	char buffer[1024];
	WINPR_UNUSED(smartcard);
	if (!WLog_IsLevelActive(WLog_Get(TAG), WLOG_DEBUG))
		return;

	WLog_DBG(TAG, "GetTransmitCount_Call {");

	ConvertFromUnicode(CP_UTF8, 0, call->szLookupName, -1, &tmp, 0, NULL, NULL);
	WLog_DBG(TAG, "  szLookupName=%s", tmp);
	free(tmp);
	smartcard_log_context(TAG, &call->Common.hContext);
	WLog_DBG(
	    TAG, "..CardIdentifier=%s",
	    smartcard_array_dump(call->Common.CardIdentifier, sizeof(UUID), buffer, sizeof(buffer)));
	WLog_DBG(TAG, "  FreshnessCounter=%" PRIu32, call->Common.FreshnessCounter);
	WLog_DBG(TAG, "  fPbDataIsNULL=%" PRId32, call->Common.fPbDataIsNULL);
	WLog_DBG(TAG, "  cbDataLen=%" PRIu32, call->Common.cbDataLen);

	WLog_DBG(TAG, "}");
}

static void smartcard_trace_transmit_call(SMARTCARD_DEVICE* smartcard, const Transmit_Call* call)
{
	UINT32 cbExtraBytes;
	BYTE* pbExtraBytes;
	WINPR_UNUSED(smartcard);

	if (!WLog_IsLevelActive(WLog_Get(TAG), WLOG_DEBUG))
		return;

	WLog_DBG(TAG, "Transmit_Call {");
	smartcard_log_context(TAG, &call->hContext);
	smartcard_log_redir_handle(TAG, &call->hCard);

	if (call->pioSendPci)
	{
		cbExtraBytes = (UINT32)(call->pioSendPci->cbPciLength - sizeof(SCARD_IO_REQUEST));
		pbExtraBytes = &((BYTE*)call->pioSendPci)[sizeof(SCARD_IO_REQUEST)];
		WLog_DBG(TAG, "pioSendPci: dwProtocol: %" PRIu32 " cbExtraBytes: %" PRIu32 "",
		         call->pioSendPci->dwProtocol, cbExtraBytes);

		if (cbExtraBytes)
		{
			char* szExtraBytes = winpr_BinToHexString(pbExtraBytes, cbExtraBytes, TRUE);
			WLog_DBG(TAG, "pbExtraBytes: %s", szExtraBytes);
			free(szExtraBytes);
		}
	}
	else
	{
		WLog_DBG(TAG, "pioSendPci: null");
	}

	WLog_DBG(TAG, "cbSendLength: %" PRIu32 "", call->cbSendLength);

	if (call->pbSendBuffer)
	{
		char* szSendBuffer = winpr_BinToHexString(call->pbSendBuffer, call->cbSendLength, TRUE);
		WLog_DBG(TAG, "pbSendBuffer: %s", szSendBuffer);
		free(szSendBuffer);
	}
	else
	{
		WLog_DBG(TAG, "pbSendBuffer: null");
	}

	if (call->pioRecvPci)
	{
		cbExtraBytes = (UINT32)(call->pioRecvPci->cbPciLength - sizeof(SCARD_IO_REQUEST));
		pbExtraBytes = &((BYTE*)call->pioRecvPci)[sizeof(SCARD_IO_REQUEST)];
		WLog_DBG(TAG, "pioRecvPci: dwProtocol: %" PRIu32 " cbExtraBytes: %" PRIu32 "",
		         call->pioRecvPci->dwProtocol, cbExtraBytes);

		if (cbExtraBytes)
		{
			char* szExtraBytes = winpr_BinToHexString(pbExtraBytes, cbExtraBytes, TRUE);
			WLog_DBG(TAG, "pbExtraBytes: %s", szExtraBytes);
			free(szExtraBytes);
		}
	}
	else
	{
		WLog_DBG(TAG, "pioRecvPci: null");
	}

	WLog_DBG(TAG, "fpbRecvBufferIsNULL: %" PRId32 " cbRecvLength: %" PRIu32 "",
	         call->fpbRecvBufferIsNULL, call->cbRecvLength);
	WLog_DBG(TAG, "}");
}

static void smartcard_trace_locate_cards_by_atr_w_call(SMARTCARD_DEVICE* smartcard,
                                                       const LocateCardsByATRW_Call* call)
{
	UINT32 index;
	char* szEventState;
	char* szCurrentState;
	char* rgbAtr;
	WINPR_UNUSED(smartcard);

	if (!WLog_IsLevelActive(WLog_Get(TAG), WLOG_DEBUG))
		return;

	WLog_DBG(TAG, "LocateCardsByATRW_Call {");
	smartcard_log_context(TAG, &call->hContext);

	for (index = 0; index < call->cReaders; index++)
	{
		char* tmp = NULL;
		const LPSCARD_READERSTATEW readerState =
		    (const LPSCARD_READERSTATEW)&call->rgReaderStates[index];

		ConvertFromUnicode(CP_UTF8, 0, readerState->szReader, -1, &tmp, 0, NULL, NULL);
		WLog_DBG(TAG, "\t[%" PRIu32 "]: szReader: %s cbAtr: %" PRIu32 "", index, tmp,
		         readerState->cbAtr);
		szCurrentState = SCardGetReaderStateString(readerState->dwCurrentState);
		szEventState = SCardGetReaderStateString(readerState->dwEventState);
		rgbAtr = winpr_BinToHexString((BYTE*)&(readerState->rgbAtr), readerState->cbAtr, FALSE);
		WLog_DBG(TAG, "\t[%" PRIu32 "]: dwCurrentState: %s (0x%08" PRIX32 ")", index,
		         szCurrentState, readerState->dwCurrentState);
		WLog_DBG(TAG, "\t[%" PRIu32 "]: dwEventState: %s (0x%08" PRIX32 ")", index, szEventState,
		         readerState->dwEventState);

		if (rgbAtr)
		{
			WLog_DBG(TAG, "\t[%" PRIu32 "]: cbAtr: %" PRIu32 " rgbAtr: %s", index,
			         readerState->cbAtr, rgbAtr);
		}
		else
		{
			WLog_DBG(TAG, "\t[%" PRIu32 "]: cbAtr: 0 rgbAtr: n/a", index);
		}

		free(szCurrentState);
		free(szEventState);
		free(rgbAtr);
		free(tmp);
	}

	WLog_DBG(TAG, "}");
}

static void smartcard_trace_transmit_return(SMARTCARD_DEVICE* smartcard, const Transmit_Return* ret)
{
	UINT32 cbExtraBytes;
	BYTE* pbExtraBytes;
	WINPR_UNUSED(smartcard);

	if (!WLog_IsLevelActive(WLog_Get(TAG), WLOG_DEBUG))
		return;

	WLog_DBG(TAG, "Transmit_Return {");
	WLog_DBG(TAG, "ReturnCode: %s (0x%08" PRIX32 ")", SCardGetErrorString(ret->ReturnCode),
	         ret->ReturnCode);

	if (ret->pioRecvPci)
	{
		cbExtraBytes = (UINT32)(ret->pioRecvPci->cbPciLength - sizeof(SCARD_IO_REQUEST));
		pbExtraBytes = &((BYTE*)ret->pioRecvPci)[sizeof(SCARD_IO_REQUEST)];
		WLog_DBG(TAG, "pioRecvPci: dwProtocol: %" PRIu32 " cbExtraBytes: %" PRIu32 "",
		         ret->pioRecvPci->dwProtocol, cbExtraBytes);

		if (cbExtraBytes)
		{
			char* szExtraBytes = winpr_BinToHexString(pbExtraBytes, cbExtraBytes, TRUE);
			WLog_DBG(TAG, "pbExtraBytes: %s", szExtraBytes);
			free(szExtraBytes);
		}
	}
	else
	{
		WLog_DBG(TAG, "pioRecvPci: null");
	}

	WLog_DBG(TAG, "cbRecvLength: %" PRIu32 "", ret->cbRecvLength);

	if (ret->pbRecvBuffer)
	{
		char* szRecvBuffer = winpr_BinToHexString(ret->pbRecvBuffer, ret->cbRecvLength, TRUE);
		WLog_DBG(TAG, "pbRecvBuffer: %s", szRecvBuffer);
		free(szRecvBuffer);
	}
	else
	{
		WLog_DBG(TAG, "pbRecvBuffer: null");
	}

	WLog_DBG(TAG, "}");
}

static void smartcard_trace_control_return(SMARTCARD_DEVICE* smartcard, const Control_Return* ret)
{
	WINPR_UNUSED(smartcard);

	if (!WLog_IsLevelActive(WLog_Get(TAG), WLOG_DEBUG))
		return;

	WLog_DBG(TAG, "Control_Return {");
	WLog_DBG(TAG, "ReturnCode: %s (0x%08" PRIX32 ")", SCardGetErrorString(ret->ReturnCode),
	         ret->ReturnCode);
	WLog_DBG(TAG, "cbOutBufferSize: %" PRIu32 "", ret->cbOutBufferSize);

	if (ret->pvOutBuffer)
	{
		char* szOutBuffer = winpr_BinToHexString(ret->pvOutBuffer, ret->cbOutBufferSize, TRUE);
		WLog_DBG(TAG, "pvOutBuffer: %s", szOutBuffer);
		free(szOutBuffer);
	}
	else
	{
		WLog_DBG(TAG, "pvOutBuffer: null");
	}

	WLog_DBG(TAG, "}");
}

static void smartcard_trace_control_call(SMARTCARD_DEVICE* smartcard, const Control_Call* call)
{
	WINPR_UNUSED(smartcard);

	if (!WLog_IsLevelActive(WLog_Get(TAG), WLOG_DEBUG))
		return;

	WLog_DBG(TAG, "Control_Call {");
	smartcard_log_context(TAG, &call->hContext);
	smartcard_log_redir_handle(TAG, &call->hCard);

	WLog_DBG(TAG,
	         "dwControlCode: 0x%08" PRIX32 " cbInBufferSize: %" PRIu32
	         " fpvOutBufferIsNULL: %" PRId32 " cbOutBufferSize: %" PRIu32 "",
	         call->dwControlCode, call->cbInBufferSize, call->fpvOutBufferIsNULL,
	         call->cbOutBufferSize);

	if (call->pvInBuffer)
	{
		char* szInBuffer = winpr_BinToHexString(call->pvInBuffer, call->cbInBufferSize, TRUE);
		WLog_DBG(TAG, "pbInBuffer: %s", szInBuffer);
		free(szInBuffer);
	}
	else
	{
		WLog_DBG(TAG, "pvInBuffer: null");
	}

	WLog_DBG(TAG, "}");
}

static void smartcard_trace_set_attrib_call(SMARTCARD_DEVICE* smartcard, const SetAttrib_Call* call)
{
	char buffer[8192];
	WINPR_UNUSED(smartcard);

	if (!WLog_IsLevelActive(WLog_Get(TAG), WLOG_DEBUG))
		return;

	WLog_DBG(TAG, "GetAttrib_Call {");
	smartcard_log_context(TAG, &call->hContext);
	smartcard_log_redir_handle(TAG, &call->hCard);
	WLog_DBG(TAG, "dwAttrId: 0x%08" PRIX32, call->dwAttrId);
	WLog_DBG(TAG, "cbAttrLen: 0x%08" PRId32, call->cbAttrLen);
	WLog_DBG(TAG, "pbAttr: %s",
	         smartcard_array_dump(call->pbAttr, call->cbAttrLen, buffer, sizeof(buffer)));
	WLog_DBG(TAG, "}");
}

static void smartcard_trace_get_attrib_return(SMARTCARD_DEVICE* smartcard,
                                              const GetAttrib_Return* ret, DWORD dwAttrId)
{
	WINPR_UNUSED(smartcard);

	if (!WLog_IsLevelActive(WLog_Get(TAG), WLOG_DEBUG))
		return;

	WLog_DBG(TAG, "GetAttrib_Return {");
	WLog_DBG(TAG, "ReturnCode: %s (0x%08" PRIX32 ")", SCardGetErrorString(ret->ReturnCode),
	         ret->ReturnCode);
	WLog_DBG(TAG, "dwAttrId: %s (0x%08" PRIX32 ") cbAttrLen: 0x%08" PRIX32 "",
	         SCardGetAttributeString(dwAttrId), dwAttrId, ret->cbAttrLen);

	if (dwAttrId == SCARD_ATTR_VENDOR_NAME)
	{
		WLog_DBG(TAG, "pbAttr: %.*s", ret->cbAttrLen, (char*)ret->pbAttr);
	}
	else if (dwAttrId == SCARD_ATTR_CURRENT_PROTOCOL_TYPE)
	{
		UINT32 dwProtocolType = *((UINT32*)ret->pbAttr);
		WLog_DBG(TAG, "dwProtocolType: %s (0x%08" PRIX32 ")",
		         SCardGetProtocolString(dwProtocolType), dwProtocolType);
	}

	WLog_DBG(TAG, "}");
}

static void smartcard_trace_get_attrib_call(SMARTCARD_DEVICE* smartcard, const GetAttrib_Call* call)
{
	WINPR_UNUSED(smartcard);

	if (!WLog_IsLevelActive(WLog_Get(TAG), WLOG_DEBUG))
		return;

	WLog_DBG(TAG, "GetAttrib_Call {");
	smartcard_log_context(TAG, &call->hContext);
	smartcard_log_redir_handle(TAG, &call->hCard);

	WLog_DBG(TAG,
	         "dwAttrId: %s (0x%08" PRIX32 ") fpbAttrIsNULL: %" PRId32 " cbAttrLen: 0x%08" PRIX32 "",
	         SCardGetAttributeString(call->dwAttrId), call->dwAttrId, call->fpbAttrIsNULL,
	         call->cbAttrLen);
	WLog_DBG(TAG, "}");
}

static void smartcard_trace_status_call(SMARTCARD_DEVICE* smartcard, const Status_Call* call,
                                        BOOL unicode)
{
	WINPR_UNUSED(smartcard);

	if (!WLog_IsLevelActive(WLog_Get(TAG), WLOG_DEBUG))
		return;

	WLog_DBG(TAG, "Status%s_Call {", unicode ? "W" : "A");
	smartcard_log_context(TAG, &call->hContext);
	smartcard_log_redir_handle(TAG, &call->hCard);

	WLog_DBG(TAG,
	         "fmszReaderNamesIsNULL: %" PRId32 " cchReaderLen: %" PRIu32 " cbAtrLen: %" PRIu32 "",
	         call->fmszReaderNamesIsNULL, call->cchReaderLen, call->cbAtrLen);
	WLog_DBG(TAG, "}");
}

static void smartcard_trace_status_return(SMARTCARD_DEVICE* smartcard, const Status_Return* ret,
                                          BOOL unicode)
{
	char* pbAtr = NULL;
	char* mszReaderNamesA = NULL;
	WINPR_UNUSED(smartcard);

	if (!WLog_IsLevelActive(WLog_Get(TAG), WLOG_DEBUG))
		return;

	mszReaderNamesA = smartcard_convert_string_list(ret->mszReaderNames, ret->cBytes, unicode);

	pbAtr = winpr_BinToHexString(ret->pbAtr, ret->cbAtrLen, FALSE);
	WLog_DBG(TAG, "Status%s_Return {", unicode ? "W" : "A");
	WLog_DBG(TAG, "ReturnCode: %s (0x%08" PRIX32 ")", SCardGetErrorString(ret->ReturnCode),
	         ret->ReturnCode);
	WLog_DBG(TAG, "dwState: %s (0x%08" PRIX32 ") dwProtocol: %s (0x%08" PRIX32 ")",
	         SCardGetCardStateString(ret->dwState), ret->dwState,
	         SCardGetProtocolString(ret->dwProtocol), ret->dwProtocol);

	WLog_DBG(TAG, "cBytes: %" PRIu32 " mszReaderNames: %s", ret->cBytes, mszReaderNamesA);

	WLog_DBG(TAG, "cbAtrLen: %" PRIu32 " pbAtr: %s", ret->cbAtrLen, pbAtr);
	WLog_DBG(TAG, "}");
	free(mszReaderNamesA);
	free(pbAtr);
}

static void smartcard_trace_reconnect_return(SMARTCARD_DEVICE* smartcard,
                                             const Reconnect_Return* ret)
{
	WINPR_UNUSED(smartcard);

	if (!WLog_IsLevelActive(WLog_Get(TAG), WLOG_DEBUG))
		return;

	WLog_DBG(TAG, "Reconnect_Return {");
	WLog_DBG(TAG, "ReturnCode: %s (0x%08" PRIX32 ")", SCardGetErrorString(ret->ReturnCode),
	         ret->ReturnCode);
	WLog_DBG(TAG, "dwActiveProtocol: %s (0x%08" PRIX32 ")",
	         SCardGetProtocolString(ret->dwActiveProtocol), ret->dwActiveProtocol);
	WLog_DBG(TAG, "}");
}

static void smartcard_trace_connect_a_call(SMARTCARD_DEVICE* smartcard, const ConnectA_Call* call)
{
	WINPR_UNUSED(smartcard);

	if (!WLog_IsLevelActive(WLog_Get(TAG), WLOG_DEBUG))
		return;

	WLog_DBG(TAG, "ConnectA_Call {");
	smartcard_log_context(TAG, &call->Common.hContext);

	WLog_DBG(TAG,
	         "szReader: %s dwShareMode: %s (0x%08" PRIX32 ") dwPreferredProtocols: %s (0x%08" PRIX32
	         ")",
	         call->szReader, SCardGetShareModeString(call->Common.dwShareMode),
	         call->Common.dwShareMode, SCardGetProtocolString(call->Common.dwPreferredProtocols),
	         call->Common.dwPreferredProtocols);
	WLog_DBG(TAG, "}");
}

static void smartcard_trace_connect_w_call(SMARTCARD_DEVICE* smartcard, const ConnectW_Call* call)
{
	char* szReaderA = NULL;
	WINPR_UNUSED(smartcard);

	if (!WLog_IsLevelActive(WLog_Get(TAG), WLOG_DEBUG))
		return;

	ConvertFromUnicode(CP_UTF8, 0, call->szReader, -1, &szReaderA, 0, NULL, NULL);
	WLog_DBG(TAG, "ConnectW_Call {");
	smartcard_log_context(TAG, &call->Common.hContext);

	WLog_DBG(TAG,
	         "szReader: %s dwShareMode: %s (0x%08" PRIX32 ") dwPreferredProtocols: %s (0x%08" PRIX32
	         ")",
	         szReaderA, SCardGetShareModeString(call->Common.dwShareMode), call->Common.dwShareMode,
	         SCardGetProtocolString(call->Common.dwPreferredProtocols),
	         call->Common.dwPreferredProtocols);
	WLog_DBG(TAG, "}");
	free(szReaderA);
}

static void smartcard_trace_hcard_and_disposition_call(SMARTCARD_DEVICE* smartcard,
                                                       const HCardAndDisposition_Call* call,
                                                       const char* name)
{
	WINPR_UNUSED(smartcard);

	if (!WLog_IsLevelActive(WLog_Get(TAG), WLOG_DEBUG))
		return;

	WLog_DBG(TAG, "%s_Call {", name);
	smartcard_log_context(TAG, &call->hContext);
	smartcard_log_redir_handle(TAG, &call->hCard);

	WLog_DBG(TAG, "dwDisposition: %s (0x%08" PRIX32 ")",
	         SCardGetDispositionString(call->dwDisposition), call->dwDisposition);
	WLog_DBG(TAG, "}");
}

static void smartcard_trace_establish_context_call(SMARTCARD_DEVICE* smartcard,
                                                   const EstablishContext_Call* call)
{
	WINPR_UNUSED(smartcard);

	if (!WLog_IsLevelActive(WLog_Get(TAG), WLOG_DEBUG))
		return;

	WLog_DBG(TAG, "EstablishContext_Call {");
	WLog_DBG(TAG, "dwScope: %s (0x%08" PRIX32 ")", SCardGetScopeString(call->dwScope),
	         call->dwScope);
	WLog_DBG(TAG, "}");
}

static void smartcard_trace_establish_context_return(SMARTCARD_DEVICE* smartcard,
                                                     const EstablishContext_Return* ret)
{
	WINPR_UNUSED(smartcard);

	if (!WLog_IsLevelActive(WLog_Get(TAG), WLOG_DEBUG))
		return;

	WLog_DBG(TAG, "EstablishContext_Return {");
	WLog_DBG(TAG, "ReturnCode: %s (0x%08" PRIX32 ")", SCardGetErrorString(ret->ReturnCode),
	         ret->ReturnCode);
	smartcard_log_context(TAG, &ret->hContext);

	WLog_DBG(TAG, "}");
}

void smartcard_trace_long_return(SMARTCARD_DEVICE* smartcard, const Long_Return* ret,
                                 const char* name)
{
	WINPR_UNUSED(smartcard);

	if (!WLog_IsLevelActive(WLog_Get(TAG), WLOG_DEBUG))
		return;

	WLog_DBG(TAG, "%s_Return {", name);
	WLog_DBG(TAG, "ReturnCode: %s (0x%08" PRIX32 ")", SCardGetErrorString(ret->ReturnCode),
	         ret->ReturnCode);
	WLog_DBG(TAG, "}");
}

void smartcard_trace_connect_return(SMARTCARD_DEVICE* smartcard, const Connect_Return* ret)
{
	WINPR_UNUSED(smartcard);

	if (!WLog_IsLevelActive(WLog_Get(TAG), WLOG_DEBUG))
		return;

	WLog_DBG(TAG, "Connect_Return {");
	WLog_DBG(TAG, "ReturnCode: %s (0x%08" PRIX32 ")", SCardGetErrorString(ret->ReturnCode),
	         ret->ReturnCode);
	smartcard_log_context(TAG, &ret->hContext);
	smartcard_log_redir_handle(TAG, &ret->hCard);

	WLog_DBG(TAG, "dwActiveProtocol: %s (0x%08" PRIX32 ")",
	         SCardGetProtocolString(ret->dwActiveProtocol), ret->dwActiveProtocol);
	WLog_DBG(TAG, "}");
}

void smartcard_trace_reconnect_call(SMARTCARD_DEVICE* smartcard, const Reconnect_Call* call)
{
	WINPR_UNUSED(smartcard);

	if (!WLog_IsLevelActive(WLog_Get(TAG), WLOG_DEBUG))
		return;

	WLog_DBG(TAG, "Reconnect_Call {");
	smartcard_log_context(TAG, &call->hContext);
	smartcard_log_redir_handle(TAG, &call->hCard);

	WLog_DBG(TAG,
	         "dwShareMode: %s (0x%08" PRIX32 ") dwPreferredProtocols: %s (0x%08" PRIX32
	         ") dwInitialization: %s (0x%08" PRIX32 ")",
	         SCardGetShareModeString(call->dwShareMode), call->dwShareMode,
	         SCardGetProtocolString(call->dwPreferredProtocols), call->dwPreferredProtocols,
	         SCardGetDispositionString(call->dwInitialization), call->dwInitialization);
	WLog_DBG(TAG, "}");
}

static void smartcard_trace_device_type_id_return(SMARTCARD_DEVICE* smartcard,
                                                  const GetDeviceTypeId_Return* ret)
{
	WINPR_UNUSED(smartcard);

	if (!WLog_IsLevelActive(WLog_Get(TAG), WLOG_DEBUG))
		return;

	WLog_DBG(TAG, "GetDeviceTypeId_Return {");
	WLog_DBG(TAG, "  ReturnCode: %s (0x%08" PRIX32 ")", SCardGetErrorString(ret->ReturnCode),
	         ret->ReturnCode);
	WLog_DBG(TAG, "  dwDeviceId=%08" PRIx32, ret->dwDeviceId);

	WLog_DBG(TAG, "}");
}

static LONG smartcard_unpack_common_context_and_string_a(SMARTCARD_DEVICE* smartcard, wStream* s,
                                                         REDIR_SCARDCONTEXT* phContext,
                                                         CHAR** pszReaderName)
{
	LONG status;
	size_t len;
	status = smartcard_unpack_redir_scard_context(smartcard, s, phContext);
	if (status)
		return status;

	len = Stream_GetRemainingLength(s);

	*pszReaderName = (CHAR*)calloc((len + 1), 1);

	if (!*pszReaderName)
	{
		WLog_WARN(TAG, "GetDeviceTypeId_Call out of memory error (call->szReaderName)");
		return STATUS_NO_MEMORY;
	}

	Stream_Read(s, *pszReaderName, len);

	smartcard_trace_context_and_string_call_a(__FUNCTION__, phContext, *pszReaderName);
	return SCARD_S_SUCCESS;
}

static LONG smartcard_unpack_common_context_and_string_w(SMARTCARD_DEVICE* smartcard, wStream* s,
                                                         REDIR_SCARDCONTEXT* phContext,
                                                         WCHAR** pszReaderName)
{
	LONG status;
	size_t len;
	status = smartcard_unpack_redir_scard_context(smartcard, s, phContext);
	if (status)
		return status;

	len = Stream_GetRemainingLength(s);

	*pszReaderName = (WCHAR*)calloc((len + 1), 1);

	if (!*pszReaderName)
	{
		WLog_WARN(TAG, "GetDeviceTypeId_Call out of memory error (call->szReaderName)");
		return STATUS_NO_MEMORY;
	}

	Stream_Read_UTF16_String(s, *pszReaderName, len / 2);

	smartcard_trace_context_and_string_call_w(__FUNCTION__, phContext, *pszReaderName);
	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_common_type_header(SMARTCARD_DEVICE* smartcard, wStream* s)
{
	UINT8 version;
	UINT32 filler;
	UINT8 endianness;
	UINT16 commonHeaderLength;
	WINPR_UNUSED(smartcard);

	if (Stream_GetRemainingLength(s) < 8)
	{
		WLog_WARN(TAG, "CommonTypeHeader is too short: %" PRIuz "", Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	/* Process CommonTypeHeader */
	Stream_Read_UINT8(s, version);             /* Version (1 byte) */
	Stream_Read_UINT8(s, endianness);          /* Endianness (1 byte) */
	Stream_Read_UINT16(s, commonHeaderLength); /* CommonHeaderLength (2 bytes) */
	Stream_Read_UINT32(s, filler);             /* Filler (4 bytes), should be 0xCCCCCCCC */

	if (version != 1)
	{
		WLog_WARN(TAG, "Unsupported CommonTypeHeader Version %" PRIu8 "", version);
		return STATUS_INVALID_PARAMETER;
	}

	if (endianness != 0x10)
	{
		WLog_WARN(TAG, "Unsupported CommonTypeHeader Endianness %" PRIu8 "", endianness);
		return STATUS_INVALID_PARAMETER;
	}

	if (commonHeaderLength != 8)
	{
		WLog_WARN(TAG, "Unsupported CommonTypeHeader CommonHeaderLength %" PRIu16 "",
		          commonHeaderLength);
		return STATUS_INVALID_PARAMETER;
	}

	if (filler != 0xCCCCCCCC)
	{
		WLog_WARN(TAG, "Unexpected CommonTypeHeader Filler 0x%08" PRIX32 "", filler);
		return STATUS_INVALID_PARAMETER;
	}

	return SCARD_S_SUCCESS;
}

void smartcard_pack_common_type_header(SMARTCARD_DEVICE* smartcard, wStream* s)
{
	WINPR_UNUSED(smartcard);
	Stream_Write_UINT8(s, 1);           /* Version (1 byte) */
	Stream_Write_UINT8(s, 0x10);        /* Endianness (1 byte) */
	Stream_Write_UINT16(s, 8);          /* CommonHeaderLength (2 bytes) */
	Stream_Write_UINT32(s, 0xCCCCCCCC); /* Filler (4 bytes), should be 0xCCCCCCCC */
}

LONG smartcard_unpack_private_type_header(SMARTCARD_DEVICE* smartcard, wStream* s)
{
	UINT32 filler;
	UINT32 objectBufferLength;
	WINPR_UNUSED(smartcard);

	if (Stream_GetRemainingLength(s) < 8)
	{
		WLog_WARN(TAG, "PrivateTypeHeader is too short: %" PRIuz "", Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Read_UINT32(s, objectBufferLength); /* ObjectBufferLength (4 bytes) */
	Stream_Read_UINT32(s, filler);             /* Filler (4 bytes), should be 0x00000000 */

	if (filler != 0x00000000)
	{
		WLog_WARN(TAG, "Unexpected PrivateTypeHeader Filler 0x%08" PRIX32 "", filler);
		return STATUS_INVALID_PARAMETER;
	}

	if (objectBufferLength != Stream_GetRemainingLength(s))
	{
		WLog_WARN(TAG,
		          "PrivateTypeHeader ObjectBufferLength mismatch: Actual: %" PRIu32
		          ", Expected: %" PRIuz "",
		          objectBufferLength, Stream_GetRemainingLength(s));
		return STATUS_INVALID_PARAMETER;
	}

	return SCARD_S_SUCCESS;
}

void smartcard_pack_private_type_header(SMARTCARD_DEVICE* smartcard, wStream* s,
                                        UINT32 objectBufferLength)
{
	WINPR_UNUSED(smartcard);
	Stream_Write_UINT32(s, objectBufferLength); /* ObjectBufferLength (4 bytes) */
	Stream_Write_UINT32(s, 0x00000000);         /* Filler (4 bytes), should be 0x00000000 */
}

LONG smartcard_unpack_read_size_align(SMARTCARD_DEVICE* smartcard, wStream* s, UINT32 size,
                                      UINT32 alignment)
{
	UINT32 pad;
	WINPR_UNUSED(smartcard);
	pad = size;
	size = (size + alignment - 1) & ~(alignment - 1);
	pad = size - pad;

	if (pad)
		Stream_Seek(s, pad);

	return (LONG)pad;
}

LONG smartcard_pack_write_size_align(SMARTCARD_DEVICE* smartcard, wStream* s, UINT32 size,
                                     UINT32 alignment)
{
	UINT32 pad;
	WINPR_UNUSED(smartcard);
	pad = size;
	size = (size + alignment - 1) & ~(alignment - 1);
	pad = size - pad;

	if (pad)
	{
		if (!Stream_EnsureRemainingCapacity(s, pad))
		{
			WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
			return SCARD_F_INTERNAL_ERROR;
		}

		Stream_Zero(s, pad);
	}

	return SCARD_S_SUCCESS;
}

SCARDCONTEXT smartcard_scard_context_native_from_redir(SMARTCARD_DEVICE* smartcard,
                                                       REDIR_SCARDCONTEXT* context)
{
	SCARDCONTEXT hContext = { 0 };
	WINPR_UNUSED(smartcard);

	if ((context->cbContext != sizeof(ULONG_PTR)) && (context->cbContext != 0))
	{
		WLog_WARN(TAG,
		          "REDIR_SCARDCONTEXT does not match native size: Actual: %" PRIu32
		          ", Expected: %" PRIuz "",
		          context->cbContext, sizeof(ULONG_PTR));
		return 0;
	}

	if (context->cbContext)
		CopyMemory(&hContext, &(context->pbContext), context->cbContext);

	return hContext;
}

void smartcard_scard_context_native_to_redir(SMARTCARD_DEVICE* smartcard,
                                             REDIR_SCARDCONTEXT* context, SCARDCONTEXT hContext)
{
	WINPR_UNUSED(smartcard);
	ZeroMemory(context, sizeof(REDIR_SCARDCONTEXT));
	context->cbContext = sizeof(ULONG_PTR);
	CopyMemory(&(context->pbContext), &hContext, context->cbContext);
}

SCARDHANDLE smartcard_scard_handle_native_from_redir(SMARTCARD_DEVICE* smartcard,
                                                     REDIR_SCARDHANDLE* handle)
{
	SCARDHANDLE hCard = 0;
	WINPR_UNUSED(smartcard);

	if (handle->cbHandle != sizeof(ULONG_PTR))
	{
		WLog_WARN(TAG,
		          "REDIR_SCARDHANDLE does not match native size: Actual: %" PRIu32
		          ", Expected: %" PRIuz "",
		          handle->cbHandle, sizeof(ULONG_PTR));
		return 0;
	}

	if (handle->cbHandle)
		CopyMemory(&hCard, &(handle->pbHandle), handle->cbHandle);

	return hCard;
}

void smartcard_scard_handle_native_to_redir(SMARTCARD_DEVICE* smartcard, REDIR_SCARDHANDLE* handle,
                                            SCARDHANDLE hCard)
{
	WINPR_UNUSED(smartcard);
	ZeroMemory(handle, sizeof(REDIR_SCARDHANDLE));
	handle->cbHandle = sizeof(ULONG_PTR);
	CopyMemory(&(handle->pbHandle), &hCard, handle->cbHandle);
}

LONG smartcard_unpack_redir_scard_context(SMARTCARD_DEVICE* smartcard, wStream* s,
                                          REDIR_SCARDCONTEXT* context)
{
	UINT32 pbContextNdrPtr;
	WINPR_UNUSED(smartcard);
	ZeroMemory(context, sizeof(REDIR_SCARDCONTEXT));

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_WARN(TAG, "REDIR_SCARDCONTEXT is too short: %" PRIuz "", Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Read_UINT32(s, context->cbContext); /* cbContext (4 bytes) */

	if (Stream_GetRemainingLength(s) < context->cbContext)
	{
		WLog_WARN(TAG, "REDIR_SCARDCONTEXT is too short: Actual: %" PRIuz ", Expected: %" PRIu32 "",
		          Stream_GetRemainingLength(s), context->cbContext);
		return STATUS_BUFFER_TOO_SMALL;
	}

	if ((context->cbContext != 0) && (context->cbContext != 4) && (context->cbContext != 8))
	{
		WLog_WARN(TAG, "REDIR_SCARDCONTEXT length is not 0, 4 or 8: %" PRIu32 "",
		          context->cbContext);
		return STATUS_INVALID_PARAMETER;
	}

	Stream_Read_UINT32(s, pbContextNdrPtr); /* pbContextNdrPtr (4 bytes) */

	if (((context->cbContext == 0) && pbContextNdrPtr) ||
	    ((context->cbContext != 0) && !pbContextNdrPtr))
	{
		WLog_WARN(TAG,
		          "REDIR_SCARDCONTEXT cbContext (%" PRIu32 ") pbContextNdrPtr (%" PRIu32
		          ") inconsistency",
		          context->cbContext, pbContextNdrPtr);
		return STATUS_INVALID_PARAMETER;
	}

	if (context->cbContext > Stream_GetRemainingLength(s))
	{
		WLog_WARN(TAG, "REDIR_SCARDCONTEXT is too long: Actual: %" PRIuz ", Expected: %" PRIu32 "",
		          Stream_GetRemainingLength(s), context->cbContext);
		return STATUS_INVALID_PARAMETER;
	}

	return SCARD_S_SUCCESS;
}

LONG smartcard_pack_redir_scard_context(SMARTCARD_DEVICE* smartcard, wStream* s,
                                        const REDIR_SCARDCONTEXT* context)
{
	UINT32 pbContextNdrPtr;
	WINPR_UNUSED(smartcard);
	pbContextNdrPtr = (context->cbContext) ? 0x00020001 : 0;
	Stream_Write_UINT32(s, context->cbContext); /* cbContext (4 bytes) */
	Stream_Write_UINT32(s, pbContextNdrPtr);    /* pbContextNdrPtr (4 bytes) */
	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_redir_scard_context_ref(SMARTCARD_DEVICE* smartcard, wStream* s,
                                              REDIR_SCARDCONTEXT* context)
{
	UINT32 length;
	WINPR_UNUSED(smartcard);

	if (context->cbContext == 0)
		return SCARD_S_SUCCESS;

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_WARN(TAG, "REDIR_SCARDCONTEXT is too short: Actual: %" PRIuz ", Expected: 4",
		          Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Read_UINT32(s, length); /* Length (4 bytes) */

	if (length != context->cbContext)
	{
		WLog_WARN(TAG, "REDIR_SCARDCONTEXT length (%" PRIu32 ") cbContext (%" PRIu32 ") mismatch",
		          length, context->cbContext);
		return STATUS_INVALID_PARAMETER;
	}

	if ((context->cbContext != 0) && (context->cbContext != 4) && (context->cbContext != 8))
	{
		WLog_WARN(TAG, "REDIR_SCARDCONTEXT length is not 4 or 8: %" PRIu32 "", context->cbContext);
		return STATUS_INVALID_PARAMETER;
	}

	if (Stream_GetRemainingLength(s) < context->cbContext)
	{
		WLog_WARN(TAG, "REDIR_SCARDCONTEXT is too short: Actual: %" PRIuz ", Expected: %" PRIu32 "",
		          Stream_GetRemainingLength(s), context->cbContext);
		return STATUS_BUFFER_TOO_SMALL;
	}

	if (context->cbContext)
		Stream_Read(s, &(context->pbContext), context->cbContext);
	else
		ZeroMemory(&(context->pbContext), sizeof(context->pbContext));

	return SCARD_S_SUCCESS;
}

LONG smartcard_pack_redir_scard_context_ref(SMARTCARD_DEVICE* smartcard, wStream* s,
                                            const REDIR_SCARDCONTEXT* context)
{
	WINPR_UNUSED(smartcard);
	Stream_Write_UINT32(s, context->cbContext); /* Length (4 bytes) */

	if (context->cbContext)
	{
		Stream_Write(s, &(context->pbContext), context->cbContext);
	}

	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_redir_scard_handle(SMARTCARD_DEVICE* smartcard, wStream* s,
                                         REDIR_SCARDHANDLE* handle)
{
	UINT32 pbHandleNdrPtr;
	WINPR_UNUSED(smartcard);
	ZeroMemory(handle, sizeof(REDIR_SCARDHANDLE));

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_WARN(TAG, "SCARDHANDLE is too short: %" PRIuz "", Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Read_UINT32(s, handle->cbHandle); /* Length (4 bytes) */

	if ((Stream_GetRemainingLength(s) < handle->cbHandle) || (!handle->cbHandle))
	{
		WLog_WARN(TAG, "SCARDHANDLE is too short: Actual: %" PRIuz ", Expected: %" PRIu32 "",
		          Stream_GetRemainingLength(s), handle->cbHandle);
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Read_UINT32(s, pbHandleNdrPtr); /* NdrPtr (4 bytes) */
	return SCARD_S_SUCCESS;
}

LONG smartcard_pack_redir_scard_handle(SMARTCARD_DEVICE* smartcard, wStream* s,
                                       const REDIR_SCARDHANDLE* handle)
{
	UINT32 pbHandleNdrPtr;
	WINPR_UNUSED(smartcard);
	pbHandleNdrPtr = (handle->cbHandle) ? 0x00020002 : 0;
	Stream_Write_UINT32(s, handle->cbHandle); /* cbHandle (4 bytes) */
	Stream_Write_UINT32(s, pbHandleNdrPtr);   /* pbHandleNdrPtr (4 bytes) */
	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_redir_scard_handle_ref(SMARTCARD_DEVICE* smartcard, wStream* s,
                                             REDIR_SCARDHANDLE* handle)
{
	UINT32 length;
	WINPR_UNUSED(smartcard);

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_WARN(TAG, "REDIR_SCARDHANDLE is too short: Actual: %" PRIuz ", Expected: 4",
		          Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Read_UINT32(s, length); /* Length (4 bytes) */

	if (length != handle->cbHandle)
	{
		WLog_WARN(TAG, "REDIR_SCARDHANDLE length (%" PRIu32 ") cbHandle (%" PRIu32 ") mismatch",
		          length, handle->cbHandle);
		return STATUS_INVALID_PARAMETER;
	}

	if ((handle->cbHandle != 4) && (handle->cbHandle != 8))
	{
		WLog_WARN(TAG, "REDIR_SCARDHANDLE length is not 4 or 8: %" PRIu32 "", handle->cbHandle);
		return STATUS_INVALID_PARAMETER;
	}

	if ((Stream_GetRemainingLength(s) < handle->cbHandle) || (!handle->cbHandle))
	{
		WLog_WARN(TAG, "REDIR_SCARDHANDLE is too short: Actual: %" PRIuz ", Expected: %" PRIu32 "",
		          Stream_GetRemainingLength(s), handle->cbHandle);
		return STATUS_BUFFER_TOO_SMALL;
	}

	if (handle->cbHandle)
		Stream_Read(s, &(handle->pbHandle), handle->cbHandle);

	return SCARD_S_SUCCESS;
}

LONG smartcard_pack_redir_scard_handle_ref(SMARTCARD_DEVICE* smartcard, wStream* s,
                                           const REDIR_SCARDHANDLE* handle)
{
	WINPR_UNUSED(smartcard);
	Stream_Write_UINT32(s, handle->cbHandle); /* Length (4 bytes) */

	if (handle->cbHandle)
		Stream_Write(s, &(handle->pbHandle), handle->cbHandle);

	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_establish_context_call(SMARTCARD_DEVICE* smartcard, wStream* s,
                                             EstablishContext_Call* call)
{
	WINPR_UNUSED(smartcard);

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_WARN(TAG, "EstablishContext_Call is too short: Actual: %" PRIuz ", Expected: 4",
		          Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Read_UINT32(s, call->dwScope); /* dwScope (4 bytes) */
	smartcard_trace_establish_context_call(smartcard, call);
	return SCARD_S_SUCCESS;
}

LONG smartcard_pack_establish_context_return(SMARTCARD_DEVICE* smartcard, wStream* s,
                                             const EstablishContext_Return* ret)
{
	LONG status;

	smartcard_trace_establish_context_return(smartcard, ret);
	if ((status = smartcard_pack_redir_scard_context(smartcard, s, &(ret->hContext))))
	{
		WLog_ERR(TAG, "smartcard_pack_redir_scard_context failed with error %" PRId32 "", status);
		return status;
	}

	if ((status = smartcard_pack_redir_scard_context_ref(smartcard, s, &(ret->hContext))))
		WLog_ERR(TAG, "smartcard_pack_redir_scard_context_ref failed with error %" PRId32 "",
		         status);

	return status;
}

LONG smartcard_unpack_context_call(SMARTCARD_DEVICE* smartcard, wStream* s, Context_Call* call,
                                   const char* name)
{
	LONG status;

	status = smartcard_unpack_redir_scard_context(smartcard, s, &(call->hContext));
	if (status)
		return status;

	if ((status = smartcard_unpack_redir_scard_context_ref(smartcard, s, &(call->hContext))))
		WLog_ERR(TAG, "smartcard_unpack_redir_scard_context_ref failed with error %" PRId32 "",
		         status);

	smartcard_trace_context_call(smartcard, call, name);
	return status;
}

LONG smartcard_unpack_list_reader_groups_call(SMARTCARD_DEVICE* smartcard, wStream* s,
                                              ListReaderGroups_Call* call, BOOL unicode)
{
	LONG status;
	status = smartcard_unpack_redir_scard_context(smartcard, s, &(call->hContext));

	if (status)
		return status;

	if (Stream_GetRemainingLength(s) < 8)
	{
		WLog_WARN(TAG, "ListReaderGroups_Call is too short: %d", (int)Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Read_INT32(s, call->fmszGroupsIsNULL); /* fmszGroupsIsNULL (4 bytes) */
	Stream_Read_UINT32(s, call->cchGroups);       /* cchGroups (4 bytes) */
	status = smartcard_unpack_redir_scard_context_ref(smartcard, s, &(call->hContext));

	if (status)
		return status;

	smartcard_trace_list_reader_groups_call(smartcard, call, unicode);
	return SCARD_S_SUCCESS;
}

LONG smartcard_pack_list_reader_groups_return(SMARTCARD_DEVICE* smartcard, wStream* s,
                                              const ListReaderGroups_Return* ret)
{
	UINT32 mszNdrPtr;
	smartcard_trace_list_reader_groups_return(smartcard, ret, FALSE);

	mszNdrPtr = (ret->cBytes) ? 0x00020008 : 0;
	Stream_EnsureRemainingCapacity(s, ret->cBytes + 32);
	Stream_Write_UINT32(s, ret->cBytes); /* cBytes (4 bytes) */
	Stream_Write_UINT32(s, mszNdrPtr);   /* mszNdrPtr (4 bytes) */

	if (mszNdrPtr)
	{
		Stream_Write_UINT32(s, ret->cBytes); /* mszNdrLen (4 bytes) */

		if (ret->msz)
			Stream_Write(s, ret->msz, ret->cBytes);
		else
			Stream_Zero(s, ret->cBytes);

		smartcard_pack_write_size_align(smartcard, s, ret->cBytes, 4);
	}

	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_list_readers_call(SMARTCARD_DEVICE* smartcard, wStream* s,
                                        ListReaders_Call* call, BOOL unicode)
{
	LONG status;
	UINT32 count;
	UINT32 mszGroupsNdrPtr;
	call->mszGroups = NULL;

	status = smartcard_unpack_redir_scard_context(smartcard, s, &(call->hContext));
	if (status)
		return status;

	if (Stream_GetRemainingLength(s) < 16)
	{
		WLog_WARN(TAG, "ListReaders_Call is too short: %" PRIuz "", Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Read_UINT32(s, call->cBytes);           /* cBytes (4 bytes) */
	Stream_Read_UINT32(s, mszGroupsNdrPtr);        /* mszGroupsNdrPtr (4 bytes) */
	Stream_Read_INT32(s, call->fmszReadersIsNULL); /* fmszReadersIsNULL (4 bytes) */
	Stream_Read_UINT32(s, call->cchReaders);       /* cchReaders (4 bytes) */

	if ((status = smartcard_unpack_redir_scard_context_ref(smartcard, s, &(call->hContext))))
	{
		WLog_ERR(TAG, "smartcard_unpack_redir_scard_context_ref failed with error %" PRId32 "",
		         status);
		return status;
	}

	if ((mszGroupsNdrPtr && !call->cBytes) || (!mszGroupsNdrPtr && call->cBytes))
	{
		WLog_WARN(TAG,
		          "ListReaders_Call mszGroupsNdrPtr (0x%08" PRIX32 ") and cBytes (0x%08" PRIX32
		          ") inconsistency",
		          mszGroupsNdrPtr, call->cBytes);
		return STATUS_INVALID_PARAMETER;
	}

	if (mszGroupsNdrPtr)
	{
		Stream_Read_UINT32(s, count); /* NdrCount (4 bytes) */

		if (count != call->cBytes)
		{
			WLog_WARN(TAG,
			          "ListReaders_Call NdrCount (0x%08" PRIX32 ") and cBytes (0x%08" PRIX32
			          ") inconsistency",
			          count, call->cBytes);
			return STATUS_INVALID_PARAMETER;
		}

		if (Stream_GetRemainingLength(s) < call->cBytes)
		{
			WLog_WARN(TAG,
			          "ListReaders_Call is too short: Actual: %" PRIuz ", Expected: %" PRIu32 "",
			          Stream_GetRemainingLength(s), call->cBytes);
			return STATUS_BUFFER_TOO_SMALL;
		}

		call->mszGroups = (BYTE*)calloc(1, call->cBytes + 4);

		if (!call->mszGroups)
		{
			WLog_WARN(TAG, "ListReaders_Call out of memory error (mszGroups)");
			return STATUS_NO_MEMORY;
		}

		Stream_Read(s, call->mszGroups, call->cBytes);
		smartcard_unpack_read_size_align(smartcard, s, call->cBytes, 4);
	}

	smartcard_trace_list_readers_call(smartcard, call, unicode);
	return SCARD_S_SUCCESS;
}

LONG smartcard_pack_list_readers_return(SMARTCARD_DEVICE* smartcard, wStream* s,
                                        const ListReaders_Return* ret, BOOL unicode)
{
	UINT32 mszNdrPtr;
	LONG error;

	smartcard_trace_list_readers_return(smartcard, ret, unicode);
	if (ret->ReturnCode != SCARD_S_SUCCESS)
		return ret->ReturnCode;

	mszNdrPtr = (ret->cBytes) ? 0x00020008 : 0;

	if (!Stream_EnsureRemainingCapacity(s, ret->cBytes + 32))
	{
		WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Write_UINT32(s, ret->cBytes); /* cBytes (4 bytes) */
	Stream_Write_UINT32(s, mszNdrPtr);   /* mszNdrPtr (4 bytes) */

	if (mszNdrPtr)
	{
		Stream_Write_UINT32(s, ret->cBytes); /* mszNdrLen (4 bytes) */

		if (ret->msz)
			Stream_Write(s, ret->msz, ret->cBytes);
		else
			Stream_Zero(s, ret->cBytes);

		if ((error = smartcard_pack_write_size_align(smartcard, s, ret->cBytes, 4)))
		{
			WLog_ERR(TAG, "smartcard_pack_write_size_align failed with error %" PRId32 "", error);
			return error;
		}
	}

	return SCARD_S_SUCCESS;
}

static LONG smartcard_unpack_connect_common(SMARTCARD_DEVICE* smartcard, wStream* s,
                                            Connect_Common* common)
{
	LONG status;

	if (Stream_GetRemainingLength(s) < 8)
	{
		WLog_WARN(TAG, "Connect_Common is too short: %" PRIuz "", Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	status = smartcard_unpack_redir_scard_context(smartcard, s, &(common->hContext));
	if (status)
		return status;

	Stream_Read_UINT32(s, common->dwShareMode);          /* dwShareMode (4 bytes) */
	Stream_Read_UINT32(s, common->dwPreferredProtocols); /* dwPreferredProtocols (4 bytes) */
	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_connect_a_call(SMARTCARD_DEVICE* smartcard, wStream* s, ConnectA_Call* call)
{
	LONG status;
	UINT32 count;
	call->szReader = NULL;

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_WARN(TAG, "ConnectA_Call is too short: %" PRIuz "", Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Seek_UINT32(s); /* szReaderNdrPtr (4 bytes) */

	if ((status = smartcard_unpack_connect_common(smartcard, s, &(call->Common))))
	{
		WLog_ERR(TAG, "smartcard_unpack_connect_common failed with error %" PRId32 "", status);
		return status;
	}

	/* szReader */
	Stream_Seek_UINT32(s);        /* NdrMaxCount (4 bytes) */
	Stream_Seek_UINT32(s);        /* NdrOffset (4 bytes) */
	Stream_Read_UINT32(s, count); /* NdrActualCount (4 bytes) */
	call->szReader = (unsigned char*)malloc(count + 1);

	if (!call->szReader)
	{
		WLog_WARN(TAG, "ConnectA_Call out of memory error (call->szReader)");
		return STATUS_NO_MEMORY;
	}

	Stream_Read(s, call->szReader, count);
	smartcard_unpack_read_size_align(smartcard, s, count, 4);
	call->szReader[count] = '\0';

	if ((status = smartcard_unpack_redir_scard_context_ref(smartcard, s, &(call->Common.hContext))))
		WLog_ERR(TAG, "smartcard_unpack_redir_scard_context_ref failed with error %" PRId32 "",
		         status);

	smartcard_trace_connect_a_call(smartcard, call);
	return status;
}

LONG smartcard_unpack_connect_w_call(SMARTCARD_DEVICE* smartcard, wStream* s, ConnectW_Call* call)
{
	LONG status;
	UINT32 count;
	call->szReader = NULL;

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_WARN(TAG, "ConnectW_Call is too short: %" PRIuz "", Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Seek_UINT32(s); /* szReaderNdrPtr (4 bytes) */

	if ((status = smartcard_unpack_connect_common(smartcard, s, &(call->Common))))
	{
		WLog_ERR(TAG, "smartcard_unpack_connect_common failed with error %" PRId32 "", status);
		return status;
	}

	/* szReader */
	Stream_Seek_UINT32(s);        /* NdrMaxCount (4 bytes) */
	Stream_Seek_UINT32(s);        /* NdrOffset (4 bytes) */
	Stream_Read_UINT32(s, count); /* NdrActualCount (4 bytes) */
	call->szReader = (WCHAR*)calloc((count + 1), 2);

	if (!call->szReader)
	{
		WLog_WARN(TAG, "ConnectW_Call out of memory error (call->szReader)");
		return STATUS_NO_MEMORY;
	}

	Stream_Read(s, call->szReader, (count * 2));
	smartcard_unpack_read_size_align(smartcard, s, (count * 2), 4);
	call->szReader[count] = '\0';

	if ((status = smartcard_unpack_redir_scard_context_ref(smartcard, s, &(call->Common.hContext))))
		WLog_ERR(TAG, "smartcard_unpack_redir_scard_context_ref failed with error %" PRId32 "",
		         status);

	smartcard_trace_connect_w_call(smartcard, call);
	return status;
}

LONG smartcard_pack_connect_return(SMARTCARD_DEVICE* smartcard, wStream* s, Connect_Return* ret)
{
	LONG status;

	if ((status = smartcard_pack_redir_scard_context(smartcard, s, &(ret->hContext))))
	{
		WLog_ERR(TAG, "smartcard_pack_redir_scard_context failed with error %" PRId32 "", status);
		return status;
	}

	if ((status = smartcard_pack_redir_scard_handle(smartcard, s, &(ret->hCard))))
	{
		WLog_ERR(TAG, "smartcard_pack_redir_scard_handle failed with error %" PRId32 "", status);
		return status;
	}

	Stream_Write_UINT32(s, ret->dwActiveProtocol); /* dwActiveProtocol (4 bytes) */

	if ((status = smartcard_pack_redir_scard_context_ref(smartcard, s, &(ret->hContext))))
	{
		WLog_ERR(TAG, "smartcard_pack_redir_scard_context_ref failed with error %" PRId32 "",
		         status);
		return status;
	}

	if ((status = smartcard_pack_redir_scard_handle_ref(smartcard, s, &(ret->hCard))))
		WLog_ERR(TAG, "smartcard_pack_redir_scard_handle_ref failed with error %" PRId32 "",
		         status);

	return status;
}

LONG smartcard_unpack_reconnect_call(SMARTCARD_DEVICE* smartcard, wStream* s, Reconnect_Call* call)
{
	LONG status;

	status = smartcard_unpack_redir_scard_context(smartcard, s, &(call->hContext));
	if (status)
		return status;

	status = smartcard_unpack_redir_scard_handle(smartcard, s, &(call->hCard));
	if (status)
		return status;

	if (Stream_GetRemainingLength(s) < 12)
	{
		WLog_WARN(TAG, "Reconnect_Call is too short: %" PRIuz "", Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Read_UINT32(s, call->dwShareMode);          /* dwShareMode (4 bytes) */
	Stream_Read_UINT32(s, call->dwPreferredProtocols); /* dwPreferredProtocols (4 bytes) */
	Stream_Read_UINT32(s, call->dwInitialization);     /* dwInitialization (4 bytes) */

	if ((status = smartcard_unpack_redir_scard_context_ref(smartcard, s, &(call->hContext))))
	{
		WLog_ERR(TAG, "smartcard_unpack_redir_scard_context_ref failed with error %" PRId32 "",
		         status);
		return status;
	}

	if ((status = smartcard_unpack_redir_scard_handle_ref(smartcard, s, &(call->hCard))))
		WLog_ERR(TAG, "smartcard_unpack_redir_scard_handle_ref failed with error %" PRId32 "",
		         status);

	smartcard_trace_reconnect_call(smartcard, call);
	return status;
}

LONG smartcard_pack_reconnect_return(SMARTCARD_DEVICE* smartcard, wStream* s,
                                     const Reconnect_Return* ret)
{
	WINPR_UNUSED(smartcard);
	smartcard_trace_reconnect_return(smartcard, ret);
	Stream_Write_UINT32(s, ret->dwActiveProtocol); /* dwActiveProtocol (4 bytes) */
	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_hcard_and_disposition_call(SMARTCARD_DEVICE* smartcard, wStream* s,
                                                 HCardAndDisposition_Call* call, const char* name)
{
	LONG status;

	status = smartcard_unpack_redir_scard_context(smartcard, s, &(call->hContext));
	if (status)
		return status;

	status = smartcard_unpack_redir_scard_handle(smartcard, s, &(call->hCard));
	if (status)
		return status;

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_WARN(TAG, "HCardAndDisposition_Call is too short: %" PRIuz "",
		          Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Read_UINT32(s, call->dwDisposition); /* dwDisposition (4 bytes) */

	if ((status = smartcard_unpack_redir_scard_context_ref(smartcard, s, &(call->hContext))))
	{
		WLog_ERR(TAG, "smartcard_unpack_redir_scard_context_ref failed with error %" PRId32 "",
		         status);
		return status;
	}

	if ((status = smartcard_unpack_redir_scard_handle_ref(smartcard, s, &(call->hCard))))
		WLog_ERR(TAG, "smartcard_unpack_redir_scard_handle_ref failed with error %" PRId32 "",
		         status);

	smartcard_trace_hcard_and_disposition_call(smartcard, call, name);
	return status;
}

static void smartcard_trace_get_status_change_a_call(SMARTCARD_DEVICE* smartcard,
                                                     const GetStatusChangeA_Call* call)
{
	UINT32 index;
	char* szEventState;
	char* szCurrentState;
	LPSCARD_READERSTATEA readerState;
	WINPR_UNUSED(smartcard);

	if (!WLog_IsLevelActive(WLog_Get(TAG), WLOG_DEBUG))
		return;

	WLog_DBG(TAG, "GetStatusChangeA_Call {");
	smartcard_log_context(TAG, &call->hContext);

	WLog_DBG(TAG, "dwTimeOut: 0x%08" PRIX32 " cReaders: %" PRIu32 "", call->dwTimeOut,
	         call->cReaders);

	for (index = 0; index < call->cReaders; index++)
	{
		readerState = &call->rgReaderStates[index];
		WLog_DBG(TAG, "\t[%" PRIu32 "]: szReader: %s cbAtr: %" PRIu32 "", index,
		         readerState->szReader, readerState->cbAtr);
		szCurrentState = SCardGetReaderStateString(readerState->dwCurrentState);
		szEventState = SCardGetReaderStateString(readerState->dwEventState);
		WLog_DBG(TAG, "\t[%" PRIu32 "]: dwCurrentState: %s (0x%08" PRIX32 ")", index,
		         szCurrentState, readerState->dwCurrentState);
		WLog_DBG(TAG, "\t[%" PRIu32 "]: dwEventState: %s (0x%08" PRIX32 ")", index, szEventState,
		         readerState->dwEventState);
		free(szCurrentState);
		free(szEventState);
	}

	WLog_DBG(TAG, "}");
}

/******************************************************************************/
/************************************* End Trace Functions ********************/
/******************************************************************************/

LONG smartcard_unpack_get_status_change_a_call(SMARTCARD_DEVICE* smartcard, wStream* s,
                                               GetStatusChangeA_Call* call)
{
	UINT32 index;
	UINT32 count;
	LONG status;
	UINT32 offset;
	UINT32 maxCount;
	UINT32 szReaderNdrPtr;
	UINT32 rgReaderStatesNdrPtr;
	LPSCARD_READERSTATEA readerState;
	call->rgReaderStates = NULL;

	status = smartcard_unpack_redir_scard_context(smartcard, s, &(call->hContext));
	if (status)
		return status;

	if (Stream_GetRemainingLength(s) < 12)
	{
		WLog_WARN(TAG, "GetStatusChangeA_Call is too short: %" PRIuz "",
		          Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Read_UINT32(s, call->dwTimeOut);      /* dwTimeOut (4 bytes) */
	Stream_Read_UINT32(s, call->cReaders);       /* cReaders (4 bytes) */
	Stream_Read_UINT32(s, rgReaderStatesNdrPtr); /* rgReaderStatesNdrPtr (4 bytes) */

	if ((status = smartcard_unpack_redir_scard_context_ref(smartcard, s, &(call->hContext))))
	{
		WLog_ERR(TAG, "smartcard_unpack_redir_scard_context_ref failed with error %" PRId32 "",
		         status);
		return status;
	}

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_WARN(TAG, "GetStatusChangeA_Call is too short: %" PRIuz "",
		          Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Read_UINT32(s, count); /* NdrCount (4 bytes) */

	if (count != call->cReaders)
	{
		WLog_WARN(TAG,
		          "GetStatusChangeA_Call unexpected reader count: Actual: %" PRIu32
		          ", Expected: %" PRIu32 "",
		          count, call->cReaders);
		return STATUS_INVALID_PARAMETER;
	}

	if (call->cReaders > 0)
	{
		call->rgReaderStates =
		    (LPSCARD_READERSTATEA)calloc(call->cReaders, sizeof(SCARD_READERSTATEA));

		if (!call->rgReaderStates)
		{
			WLog_WARN(TAG, "GetStatusChangeA_Call out of memory error (call->rgReaderStates)");
			return STATUS_NO_MEMORY;
		}

		for (index = 0; index < call->cReaders; index++)
		{
			readerState = &call->rgReaderStates[index];

			if (Stream_GetRemainingLength(s) < 52)
			{
				WLog_WARN(TAG, "GetStatusChangeA_Call is too short: %" PRIuz "",
				          Stream_GetRemainingLength(s));
				return STATUS_BUFFER_TOO_SMALL;
			}

			Stream_Read_UINT32(s, szReaderNdrPtr);              /* szReaderNdrPtr (4 bytes) */
			Stream_Read_UINT32(s, readerState->dwCurrentState); /* dwCurrentState (4 bytes) */
			Stream_Read_UINT32(s, readerState->dwEventState);   /* dwEventState (4 bytes) */
			Stream_Read_UINT32(s, readerState->cbAtr);          /* cbAtr (4 bytes) */
			Stream_Read(s, readerState->rgbAtr, 32);            /* rgbAtr [0..32] (32 bytes) */
			Stream_Seek(s, 4);                                  /* rgbAtr [32..36] (4 bytes) */
		}

		for (index = 0; index < call->cReaders; index++)
		{
			readerState = &call->rgReaderStates[index];

			if (Stream_GetRemainingLength(s) < 12)
			{
				WLog_WARN(TAG, "GetStatusChangeA_Call is too short: %" PRIuz "",
				          Stream_GetRemainingLength(s));
				return STATUS_BUFFER_TOO_SMALL;
			}

			Stream_Read_UINT32(s, maxCount); /* NdrMaxCount (4 bytes) */
			Stream_Read_UINT32(s, offset);   /* NdrOffset (4 bytes) */
			Stream_Read_UINT32(s, count);    /* NdrActualCount (4 bytes) */

			if (Stream_GetRemainingLength(s) < count)
			{
				WLog_WARN(TAG, "GetStatusChangeA_Call is too short: %" PRIuz "",
				          Stream_GetRemainingLength(s));
				return STATUS_BUFFER_TOO_SMALL;
			}

			readerState->szReader = (LPSTR)malloc(count + 1);

			if (!readerState->szReader)
			{
				WLog_WARN(TAG, "GetStatusChangeA_Call out of memory error (readerState->szReader)");
				return STATUS_NO_MEMORY;
			}

			Stream_Read(s, (void*)readerState->szReader, count);
			smartcard_unpack_read_size_align(smartcard, s, count, 4);
			((char*)readerState->szReader)[count] = '\0';

			if (!readerState->szReader)
			{
				WLog_WARN(TAG, "GetStatusChangeA_Call null reader name");
				return STATUS_INVALID_PARAMETER;
			}
		}
	}

	smartcard_trace_get_status_change_a_call(smartcard, call);
	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_get_status_change_w_call(SMARTCARD_DEVICE* smartcard, wStream* s,
                                               GetStatusChangeW_Call* call)
{
	UINT32 index;
	UINT32 count;
	LONG status;
	UINT32 offset;
	UINT32 maxCount;
	UINT32 szReaderNdrPtr;
	UINT32 rgReaderStatesNdrPtr;
	LPSCARD_READERSTATEW readerState;
	call->rgReaderStates = NULL;

	status = smartcard_unpack_redir_scard_context(smartcard, s, &(call->hContext));
	if (status)
		return status;

	if (Stream_GetRemainingLength(s) < 12)
	{
		WLog_WARN(TAG, "GetStatusChangeW_Call is too short: %" PRIuz "",
		          Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Read_UINT32(s, call->dwTimeOut);      /* dwTimeOut (4 bytes) */
	Stream_Read_UINT32(s, call->cReaders);       /* cReaders (4 bytes) */
	Stream_Read_UINT32(s, rgReaderStatesNdrPtr); /* rgReaderStatesNdrPtr (4 bytes) */

	if ((status = smartcard_unpack_redir_scard_context_ref(smartcard, s, &(call->hContext))))
	{
		WLog_ERR(TAG, "smartcard_unpack_redir_scard_context_ref failed with error %" PRId32 "",
		         status);
		return status;
	}

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_WARN(TAG, "GetStatusChangeW_Call is too short: %" PRIuz "",
		          Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Seek_UINT32(s); /* NdrConformant (4 bytes) */

	if (call->cReaders > 0)
	{
		call->rgReaderStates =
		    (LPSCARD_READERSTATEW)calloc(call->cReaders, sizeof(SCARD_READERSTATEW));

		if (!call->rgReaderStates)
		{
			WLog_WARN(TAG, "GetStatusChangeW_Call out of memory error (call->rgReaderStates)");
			return STATUS_NO_MEMORY;
		}

		for (index = 0; index < call->cReaders; index++)
		{
			readerState = &call->rgReaderStates[index];

			if (Stream_GetRemainingLength(s) < 52)
			{
				WLog_WARN(TAG, "GetStatusChangeW_Call is too short: %" PRIuz "",
				          Stream_GetRemainingLength(s));
				return STATUS_BUFFER_TOO_SMALL;
			}

			Stream_Read_UINT32(s, szReaderNdrPtr);              /* (4 bytes) */
			Stream_Read_UINT32(s, readerState->dwCurrentState); /* dwCurrentState (4 bytes) */
			Stream_Read_UINT32(s, readerState->dwEventState);   /* dwEventState (4 bytes) */
			Stream_Read_UINT32(s, readerState->cbAtr);          /* cbAtr (4 bytes) */
			Stream_Read(s, readerState->rgbAtr, 32);            /* rgbAtr [0..32] (32 bytes) */
			Stream_Seek(s, 4);                                  /* rgbAtr [32..36] (4 bytes) */
		}

		for (index = 0; index < call->cReaders; index++)
		{
			readerState = &call->rgReaderStates[index];

			if (Stream_GetRemainingLength(s) < 12)
			{
				WLog_WARN(TAG, "GetStatusChangeW_Call is too short: %" PRIuz "",
				          Stream_GetRemainingLength(s));
				return STATUS_BUFFER_TOO_SMALL;
			}

			Stream_Read_UINT32(s, maxCount); /* NdrMaxCount (4 bytes) */
			Stream_Read_UINT32(s, offset);   /* NdrOffset (4 bytes) */
			Stream_Read_UINT32(s, count);    /* NdrActualCount (4 bytes) */

			if (Stream_GetRemainingLength(s) < (count * 2))
			{
				WLog_WARN(TAG, "GetStatusChangeW_Call is too short: %" PRIuz "",
				          Stream_GetRemainingLength(s));
				return STATUS_BUFFER_TOO_SMALL;
			}

			readerState->szReader = (WCHAR*)calloc((count + 1), 2);

			if (!readerState->szReader)
			{
				WLog_WARN(TAG, "GetStatusChangeW_Call out of memory error (readerState->szReader)");
				return STATUS_NO_MEMORY;
			}

			Stream_Read(s, (void*)readerState->szReader, (count * 2));
			smartcard_unpack_read_size_align(smartcard, s, (count * 2), 4);
			((WCHAR*)readerState->szReader)[count] = '\0';

			if (!readerState->szReader)
			{
				WLog_WARN(TAG, "GetStatusChangeW_Call null reader name");
				return STATUS_INVALID_PARAMETER;
			}
		}
	}

	smartcard_trace_get_status_change_w_call(smartcard, call);
	return SCARD_S_SUCCESS;
}

LONG smartcard_pack_get_status_change_return(SMARTCARD_DEVICE* smartcard, wStream* s,
                                             const GetStatusChange_Return* ret, BOOL unicode)
{
	UINT32 index;
	ReaderState_Return* rgReaderState;
	WINPR_UNUSED(smartcard);

	smartcard_trace_get_status_change_return(smartcard, ret, unicode);

	Stream_Write_UINT32(s, ret->cReaders); /* cReaders (4 bytes) */
	Stream_Write_UINT32(s, 0x00020100);    /* rgReaderStatesNdrPtr (4 bytes) */
	Stream_Write_UINT32(s, ret->cReaders); /* rgReaderStatesNdrCount (4 bytes) */

	for (index = 0; index < ret->cReaders; index++)
	{
		rgReaderState = &(ret->rgReaderStates[index]);
		Stream_Write_UINT32(s, rgReaderState->dwCurrentState); /* dwCurrentState (4 bytes) */
		Stream_Write_UINT32(s, rgReaderState->dwEventState);   /* dwEventState (4 bytes) */
		Stream_Write_UINT32(s, rgReaderState->cbAtr);          /* cbAtr (4 bytes) */
		Stream_Write(s, rgReaderState->rgbAtr, 32);            /* rgbAtr [0..32] (32 bytes) */
		Stream_Zero(s, 4);                                     /* rgbAtr [32..36] (32 bytes) */
	}

	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_state_call(SMARTCARD_DEVICE* smartcard, wStream* s, State_Call* call)
{
	LONG status;

	status = smartcard_unpack_redir_scard_context(smartcard, s, &(call->hContext));
	if (status)
		return status;

	status = smartcard_unpack_redir_scard_handle(smartcard, s, &(call->hCard));
	if (status)
		return status;

	if (Stream_GetRemainingLength(s) < 8)
	{
		WLog_WARN(TAG, "State_Call is too short: %" PRIuz "", Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Read_INT32(s, call->fpbAtrIsNULL); /* fpbAtrIsNULL (4 bytes) */
	Stream_Read_UINT32(s, call->cbAtrLen);    /* cbAtrLen (4 bytes) */

	if ((status = smartcard_unpack_redir_scard_context_ref(smartcard, s, &(call->hContext))))
	{
		WLog_ERR(TAG, "smartcard_unpack_redir_scard_context_ref failed with error %" PRId32 "",
		         status);
		return status;
	}

	if ((status = smartcard_unpack_redir_scard_handle_ref(smartcard, s, &(call->hCard))))
		WLog_ERR(TAG, "smartcard_unpack_redir_scard_handle_ref failed with error %" PRId32 "",
		         status);

	return status;
}

LONG smartcard_pack_state_return(SMARTCARD_DEVICE* smartcard, wStream* s, const State_Return* ret)
{
	LONG status;
	Stream_Write_UINT32(s, ret->dwState);       /* dwState (4 bytes) */
	Stream_Write_UINT32(s, ret->dwProtocol);    /* dwProtocol (4 bytes) */
	Stream_Write_UINT32(s, ret->cbAtrLen);      /* cbAtrLen (4 bytes) */
	Stream_Write_UINT32(s, 0x00020020);         /* rgAtrNdrPtr (4 bytes) */
	Stream_Write_UINT32(s, ret->cbAtrLen);      /* rgAtrLength (4 bytes) */
	Stream_Write(s, ret->rgAtr, ret->cbAtrLen); /* rgAtr */

	if ((status = smartcard_pack_write_size_align(smartcard, s, ret->cbAtrLen, 4)))
		WLog_ERR(TAG, "smartcard_pack_write_size_align failed with error %" PRId32 "", status);

	return status;
}

LONG smartcard_unpack_status_call(SMARTCARD_DEVICE* smartcard, wStream* s, Status_Call* call,
                                  BOOL unicode)
{
	LONG status;

	status = smartcard_unpack_redir_scard_context(smartcard, s, &(call->hContext));
	if (status)
		return status;

	status = smartcard_unpack_redir_scard_handle(smartcard, s, &(call->hCard));
	if (status)
		return status;

	if (Stream_GetRemainingLength(s) < 12)
	{
		WLog_WARN(TAG, "Status_Call is too short: %" PRIuz "", Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Read_INT32(s, call->fmszReaderNamesIsNULL); /* fmszReaderNamesIsNULL (4 bytes) */
	Stream_Read_UINT32(s, call->cchReaderLen);         /* cchReaderLen (4 bytes) */
	Stream_Read_UINT32(s, call->cbAtrLen);             /* cbAtrLen (4 bytes) */

	if ((status = smartcard_unpack_redir_scard_context_ref(smartcard, s, &(call->hContext))))
	{
		WLog_ERR(TAG, "smartcard_unpack_redir_scard_context_ref failed with error %" PRId32 "",
		         status);
		return status;
	}

	if ((status = smartcard_unpack_redir_scard_handle_ref(smartcard, s, &(call->hCard))))
		WLog_ERR(TAG, "smartcard_unpack_redir_scard_handle_ref failed with error %" PRId32 "",
		         status);

	smartcard_trace_status_call(smartcard, call, unicode);
	return status;
}

LONG smartcard_pack_status_return(SMARTCARD_DEVICE* smartcard, wStream* s, const Status_Return* ret,
                                  BOOL unicode)
{
	LONG status;

	smartcard_trace_status_return(smartcard, ret, unicode);
	if (!Stream_EnsureRemainingCapacity(s, ret->cBytes + 64))
	{
		WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Write_UINT32(s, ret->cBytes);     /* cBytes (4 bytes) */
	Stream_Write_UINT32(s, 0x00020010);      /* mszReaderNamesNdrPtr (4 bytes) */
	Stream_Write_UINT32(s, ret->dwState);    /* dwState (4 bytes) */
	Stream_Write_UINT32(s, ret->dwProtocol); /* dwProtocol (4 bytes) */
	Stream_Write(s, ret->pbAtr, 32);         /* pbAtr (32 bytes) */
	Stream_Write_UINT32(s, ret->cbAtrLen);   /* cbAtrLen (4 bytes) */
	Stream_Write_UINT32(s, ret->cBytes);     /* mszReaderNamesNdrLen (4 bytes) */

	if (ret->mszReaderNames)
		Stream_Write(s, ret->mszReaderNames, ret->cBytes);
	else
		Stream_Zero(s, ret->cBytes);

	if ((status = smartcard_pack_write_size_align(smartcard, s, ret->cBytes, 4)))
		WLog_ERR(TAG, "smartcard_pack_write_size_align failed with error %" PRId32 "", status);

	return status;
}

LONG smartcard_unpack_get_attrib_call(SMARTCARD_DEVICE* smartcard, wStream* s, GetAttrib_Call* call)
{
	LONG status;

	status = smartcard_unpack_redir_scard_context(smartcard, s, &(call->hContext));
	if (status)
		return status;

	status = smartcard_unpack_redir_scard_handle(smartcard, s, &(call->hCard));
	if (status)
		return status;

	if (Stream_GetRemainingLength(s) < 12)
	{
		WLog_WARN(TAG, "GetAttrib_Call is too short: %" PRIuz "", Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Read_UINT32(s, call->dwAttrId);     /* dwAttrId (4 bytes) */
	Stream_Read_INT32(s, call->fpbAttrIsNULL); /* fpbAttrIsNULL (4 bytes) */
	Stream_Read_UINT32(s, call->cbAttrLen);    /* cbAttrLen (4 bytes) */

	if ((status = smartcard_unpack_redir_scard_context_ref(smartcard, s, &(call->hContext))))
	{
		WLog_ERR(TAG, "smartcard_unpack_redir_scard_context_ref failed with error %" PRId32 "",
		         status);
		return status;
	}

	if ((status = smartcard_unpack_redir_scard_handle_ref(smartcard, s, &(call->hCard))))
		WLog_ERR(TAG, "smartcard_unpack_redir_scard_handle_ref failed with error %" PRId32 "",
		         status);

	smartcard_trace_get_attrib_call(smartcard, call);
	return status;
}

LONG smartcard_pack_get_attrib_return(SMARTCARD_DEVICE* smartcard, wStream* s,
                                      const GetAttrib_Return* ret, DWORD dwAttrId)
{
	LONG status;

	smartcard_trace_get_attrib_return(smartcard, ret, dwAttrId);
	if (!Stream_EnsureRemainingCapacity(s, ret->cbAttrLen + 32))
	{
		WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Write_UINT32(s, ret->cbAttrLen); /* cbAttrLen (4 bytes) */
	Stream_Write_UINT32(s, 0x00020080);     /* pbAttrNdrPtr (4 bytes) */
	Stream_Write_UINT32(s, ret->cbAttrLen); /* pbAttrNdrCount (4 bytes) */

	if (!ret->pbAttr)
		Stream_Zero(s, ret->cbAttrLen); /* pbAttr */
	else
		Stream_Write(s, ret->pbAttr, ret->cbAttrLen); /* pbAttr */

	if ((status = smartcard_pack_write_size_align(smartcard, s, ret->cbAttrLen, 4)))
		WLog_ERR(TAG, "smartcard_pack_write_size_align failed with error %" PRId32 "", status);

	return status;
}

LONG smartcard_unpack_control_call(SMARTCARD_DEVICE* smartcard, wStream* s, Control_Call* call)
{
	LONG status;
	UINT32 length;
	call->pvInBuffer = NULL;

	status = smartcard_unpack_redir_scard_context(smartcard, s, &(call->hContext));
	if (status)
		return status;

	status = smartcard_unpack_redir_scard_handle(smartcard, s, &(call->hCard));
	if (status)
		return status;

	if (Stream_GetRemainingLength(s) < 20)
	{
		WLog_WARN(TAG, "Control_Call is too short: %" PRIuz "", Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Read_UINT32(s, call->dwControlCode);     /* dwControlCode (4 bytes) */
	Stream_Read_UINT32(s, call->cbInBufferSize);    /* cbInBufferSize (4 bytes) */
	Stream_Seek_UINT32(s);                          /* pvInBufferNdrPtr (4 bytes) */
	Stream_Read_INT32(s, call->fpvOutBufferIsNULL); /* fpvOutBufferIsNULL (4 bytes) */
	Stream_Read_UINT32(s, call->cbOutBufferSize);   /* cbOutBufferSize (4 bytes) */

	if ((status = smartcard_unpack_redir_scard_context_ref(smartcard, s, &(call->hContext))))
	{
		WLog_ERR(TAG, "smartcard_unpack_redir_scard_context_ref failed with error %" PRId32 "",
		         status);
		return status;
	}

	if ((status = smartcard_unpack_redir_scard_handle_ref(smartcard, s, &(call->hCard))))
	{
		WLog_ERR(TAG, "smartcard_unpack_redir_scard_context_ref failed with error %" PRId32 "",
		         status);
		return status;
	}

	if (call->cbInBufferSize)
	{
		if (Stream_GetRemainingLength(s) < 4)
		{
			WLog_WARN(TAG, "Control_Call is too short: %" PRIuz "", Stream_GetRemainingLength(s));
			return STATUS_BUFFER_TOO_SMALL;
		}

		Stream_Read_UINT32(s, length); /* Length (4 bytes) */

		if (Stream_GetRemainingLength(s) < length)
		{
			WLog_WARN(TAG, "Control_Call is too short: %" PRIuz "", Stream_GetRemainingLength(s));
			return STATUS_BUFFER_TOO_SMALL;
		}

		call->pvInBuffer = (BYTE*)malloc(length);

		if (!call->pvInBuffer)
		{
			WLog_WARN(TAG, "Control_Call out of memory error (call->pvInBuffer)");
			return STATUS_NO_MEMORY;
		}

		call->cbInBufferSize = length;
		Stream_Read(s, call->pvInBuffer, length);
	}

	smartcard_trace_control_call(smartcard, call);
	return SCARD_S_SUCCESS;
}

LONG smartcard_pack_control_return(SMARTCARD_DEVICE* smartcard, wStream* s,
                                   const Control_Return* ret)
{
	LONG error;

	smartcard_trace_control_return(smartcard, ret);
	if (!Stream_EnsureRemainingCapacity(s, ret->cbOutBufferSize + 32))
	{
		WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Write_UINT32(s, ret->cbOutBufferSize); /* cbOutBufferSize (4 bytes) */
	Stream_Write_UINT32(s, 0x00020040);           /* pvOutBufferPointer (4 bytes) */
	Stream_Write_UINT32(s, ret->cbOutBufferSize); /* pvOutBufferLength (4 bytes) */

	if (ret->cbOutBufferSize > 0)
	{
		Stream_Write(s, ret->pvOutBuffer, ret->cbOutBufferSize); /* pvOutBuffer */

		if ((error = smartcard_pack_write_size_align(smartcard, s, ret->cbOutBufferSize, 4)))
		{
			WLog_ERR(TAG, "smartcard_pack_write_size_align failed with error %" PRId32 "", error);
			return error;
		}
	}

	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_transmit_call(SMARTCARD_DEVICE* smartcard, wStream* s, Transmit_Call* call)
{
	LONG status;
	UINT32 length;
	BYTE* pbExtraBytes;
	UINT32 pbExtraBytesNdrPtr;
	UINT32 pbSendBufferNdrPtr;
	UINT32 pioRecvPciNdrPtr;
	SCardIO_Request ioSendPci;
	SCardIO_Request ioRecvPci;
	call->pioSendPci = NULL;
	call->pioRecvPci = NULL;
	call->pbSendBuffer = NULL;

	status = smartcard_unpack_redir_scard_context(smartcard, s, &(call->hContext));
	if (status)
		return status;

	status = smartcard_unpack_redir_scard_handle(smartcard, s, &(call->hCard));
	if (status)
		return status;

	if (Stream_GetRemainingLength(s) < 32)
	{
		WLog_WARN(TAG, "Transmit_Call is too short: Actual: %" PRIuz ", Expected: 32",
		          Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Read_UINT32(s, ioSendPci.dwProtocol);     /* dwProtocol (4 bytes) */
	Stream_Read_UINT32(s, ioSendPci.cbExtraBytes);   /* cbExtraBytes (4 bytes) */
	Stream_Read_UINT32(s, pbExtraBytesNdrPtr);       /* pbExtraBytesNdrPtr (4 bytes) */
	Stream_Read_UINT32(s, call->cbSendLength);       /* cbSendLength (4 bytes) */
	Stream_Read_UINT32(s, pbSendBufferNdrPtr);       /* pbSendBufferNdrPtr (4 bytes) */
	Stream_Read_UINT32(s, pioRecvPciNdrPtr);         /* pioRecvPciNdrPtr (4 bytes) */
	Stream_Read_INT32(s, call->fpbRecvBufferIsNULL); /* fpbRecvBufferIsNULL (4 bytes) */
	Stream_Read_UINT32(s, call->cbRecvLength);       /* cbRecvLength (4 bytes) */

	if (ioSendPci.cbExtraBytes > 1024)
	{
		WLog_WARN(TAG,
		          "Transmit_Call ioSendPci.cbExtraBytes is out of bounds: %" PRIu32 " (max: 1024)",
		          ioSendPci.cbExtraBytes);
		return STATUS_INVALID_PARAMETER;
	}

	if (call->cbSendLength > 66560)
	{
		WLog_WARN(TAG, "Transmit_Call cbSendLength is out of bounds: %" PRIu32 " (max: 66560)",
		          ioSendPci.cbExtraBytes);
		return STATUS_INVALID_PARAMETER;
	}

	if ((status = smartcard_unpack_redir_scard_context_ref(smartcard, s, &(call->hContext))))
	{
		WLog_ERR(TAG, "smartcard_unpack_redir_scard_context_ref failed with error %" PRId32 "",
		         status);
		return status;
	}

	if ((status = smartcard_unpack_redir_scard_handle_ref(smartcard, s, &(call->hCard))))
	{
		WLog_ERR(TAG, "smartcard_unpack_redir_scard_handle_ref failed with error %" PRId32 "",
		         status);
		return status;
	}

	if (ioSendPci.cbExtraBytes && !pbExtraBytesNdrPtr)
	{
		WLog_WARN(
		    TAG, "Transmit_Call ioSendPci.cbExtraBytes is non-zero but pbExtraBytesNdrPtr is null");
		return STATUS_INVALID_PARAMETER;
	}

	if (pbExtraBytesNdrPtr)
	{
		if (Stream_GetRemainingLength(s) < 4)
		{
			WLog_WARN(TAG, "Transmit_Call is too short: %" PRIuz " (ioSendPci.pbExtraBytes)",
			          Stream_GetRemainingLength(s));
			return STATUS_BUFFER_TOO_SMALL;
		}

		Stream_Read_UINT32(s, length); /* Length (4 bytes) */

		if (Stream_GetRemainingLength(s) < ioSendPci.cbExtraBytes)
		{
			WLog_WARN(TAG,
			          "Transmit_Call is too short: Actual: %" PRIuz ", Expected: %" PRIu32
			          " (ioSendPci.cbExtraBytes)",
			          Stream_GetRemainingLength(s), ioSendPci.cbExtraBytes);
			return STATUS_BUFFER_TOO_SMALL;
		}

		ioSendPci.pbExtraBytes = Stream_Pointer(s);
		call->pioSendPci =
		    (LPSCARD_IO_REQUEST)malloc(sizeof(SCARD_IO_REQUEST) + ioSendPci.cbExtraBytes);

		if (!call->pioSendPci)
		{
			WLog_WARN(TAG, "Transmit_Call out of memory error (pioSendPci)");
			return STATUS_NO_MEMORY;
		}

		call->pioSendPci->dwProtocol = ioSendPci.dwProtocol;
		call->pioSendPci->cbPciLength = (DWORD)(ioSendPci.cbExtraBytes + sizeof(SCARD_IO_REQUEST));
		pbExtraBytes = &((BYTE*)call->pioSendPci)[sizeof(SCARD_IO_REQUEST)];
		Stream_Read(s, pbExtraBytes, ioSendPci.cbExtraBytes);
		smartcard_unpack_read_size_align(smartcard, s, ioSendPci.cbExtraBytes, 4);
	}
	else
	{
		call->pioSendPci = (LPSCARD_IO_REQUEST)calloc(1, sizeof(SCARD_IO_REQUEST));

		if (!call->pioSendPci)
		{
			WLog_WARN(TAG, "Transmit_Call out of memory error (pioSendPci)");
			return STATUS_NO_MEMORY;
		}

		call->pioSendPci->dwProtocol = ioSendPci.dwProtocol;
		call->pioSendPci->cbPciLength = sizeof(SCARD_IO_REQUEST);
	}

	if (pbSendBufferNdrPtr)
	{
		if (Stream_GetRemainingLength(s) < 4)
		{
			WLog_WARN(TAG, "Transmit_Call is too short: %" PRIuz "", Stream_GetRemainingLength(s));
			return STATUS_BUFFER_TOO_SMALL;
		}

		Stream_Read_UINT32(s, length); /* Length (4 bytes) */

		if (length != call->cbSendLength)
		{
			WLog_WARN(TAG,
			          "Transmit_Call unexpected length: Actual: %" PRIu32 ", Expected: %" PRIu32
			          " (cbSendLength)",
			          length, call->cbSendLength);
			return STATUS_INVALID_PARAMETER;
		}

		if (Stream_GetRemainingLength(s) < call->cbSendLength)
		{
			WLog_WARN(TAG,
			          "Transmit_Call is too short: Actual: %" PRIuz ", Expected: %" PRIu32
			          " (cbSendLength)",
			          Stream_GetRemainingLength(s), call->cbSendLength);
			return STATUS_BUFFER_TOO_SMALL;
		}

		call->pbSendBuffer = (BYTE*)malloc(call->cbSendLength);

		if (!call->pbSendBuffer)
		{
			WLog_WARN(TAG, "Transmit_Call out of memory error (pbSendBuffer)");
			return STATUS_NO_MEMORY;
		}

		Stream_Read(s, call->pbSendBuffer, call->cbSendLength);
		smartcard_unpack_read_size_align(smartcard, s, call->cbSendLength, 4);
	}

	if (pioRecvPciNdrPtr)
	{
		if (Stream_GetRemainingLength(s) < 12)
		{
			WLog_WARN(TAG, "Transmit_Call is too short: Actual: %" PRIuz ", Expected: 12",
			          Stream_GetRemainingLength(s));
			return STATUS_BUFFER_TOO_SMALL;
		}

		Stream_Read_UINT32(s, ioRecvPci.dwProtocol);   /* dwProtocol (4 bytes) */
		Stream_Read_UINT32(s, ioRecvPci.cbExtraBytes); /* cbExtraBytes (4 bytes) */
		Stream_Read_UINT32(s, pbExtraBytesNdrPtr);     /* pbExtraBytesNdrPtr (4 bytes) */

		if (ioRecvPci.cbExtraBytes && !pbExtraBytesNdrPtr)
		{
			WLog_WARN(
			    TAG,
			    "Transmit_Call ioRecvPci.cbExtraBytes is non-zero but pbExtraBytesNdrPtr is null");
			return STATUS_INVALID_PARAMETER;
		}

		if (pbExtraBytesNdrPtr)
		{
			if (Stream_GetRemainingLength(s) < 4)
			{
				WLog_WARN(TAG, "Transmit_Call is too short: %" PRIuz " (ioRecvPci.pbExtraBytes)",
				          Stream_GetRemainingLength(s));
				return STATUS_BUFFER_TOO_SMALL;
			}

			Stream_Read_UINT32(s, length); /* Length (4 bytes) */

			if (ioRecvPci.cbExtraBytes > 1024)
			{
				WLog_WARN(TAG,
				          "Transmit_Call ioRecvPci.cbExtraBytes is out of bounds: %" PRIu32
				          " (max: 1024)",
				          ioRecvPci.cbExtraBytes);
				return STATUS_INVALID_PARAMETER;
			}

			if (length != ioRecvPci.cbExtraBytes)
			{
				WLog_WARN(TAG,
				          "Transmit_Call unexpected length: Actual: %" PRIu32 ", Expected: %" PRIu32
				          " (ioRecvPci.cbExtraBytes)",
				          length, ioRecvPci.cbExtraBytes);
				return STATUS_INVALID_PARAMETER;
			}

			if (Stream_GetRemainingLength(s) < ioRecvPci.cbExtraBytes)
			{
				WLog_WARN(TAG,
				          "Transmit_Call is too short: Actual: %" PRIuz ", Expected: %" PRIu32
				          " (ioRecvPci.cbExtraBytes)",
				          Stream_GetRemainingLength(s), ioRecvPci.cbExtraBytes);
				return STATUS_BUFFER_TOO_SMALL;
			}

			ioRecvPci.pbExtraBytes = Stream_Pointer(s);
			call->pioRecvPci =
			    (LPSCARD_IO_REQUEST)malloc(sizeof(SCARD_IO_REQUEST) + ioRecvPci.cbExtraBytes);

			if (!call->pioRecvPci)
			{
				WLog_WARN(TAG, "Transmit_Call out of memory error (pioRecvPci)");
				return STATUS_NO_MEMORY;
			}

			call->pioRecvPci->dwProtocol = ioRecvPci.dwProtocol;
			call->pioRecvPci->cbPciLength =
			    (DWORD)(ioRecvPci.cbExtraBytes + sizeof(SCARD_IO_REQUEST));
			pbExtraBytes = &((BYTE*)call->pioRecvPci)[sizeof(SCARD_IO_REQUEST)];
			Stream_Read(s, pbExtraBytes, ioRecvPci.cbExtraBytes);
			smartcard_unpack_read_size_align(smartcard, s, ioRecvPci.cbExtraBytes, 4);
		}
		else
		{
			call->pioRecvPci = (LPSCARD_IO_REQUEST)calloc(1, sizeof(SCARD_IO_REQUEST));

			if (!call->pioRecvPci)
			{
				WLog_WARN(TAG, "Transmit_Call out of memory error (pioRecvPci)");
				return STATUS_NO_MEMORY;
			}

			call->pioRecvPci->dwProtocol = ioRecvPci.dwProtocol;
			call->pioRecvPci->cbPciLength = sizeof(SCARD_IO_REQUEST);
		}
	}

	smartcard_trace_transmit_call(smartcard, call);
	return SCARD_S_SUCCESS;
}

LONG smartcard_pack_transmit_return(SMARTCARD_DEVICE* smartcard, wStream* s,
                                    const Transmit_Return* ret)
{
	UINT32 cbExtraBytes;
	BYTE* pbExtraBytes;
	UINT32 pioRecvPciNdrPtr;
	UINT32 pbRecvBufferNdrPtr;
	UINT32 pbExtraBytesNdrPtr;
	LONG error;
	UINT32 cbRecvLength = ret->cbRecvLength;

	smartcard_trace_transmit_return(smartcard, ret);
	if (!ret->pbRecvBuffer)
		cbRecvLength = 0;

	pioRecvPciNdrPtr = (ret->pioRecvPci) ? 0x00020000 : 0;
	pbRecvBufferNdrPtr = (ret->pbRecvBuffer) ? 0x00020004 : 0;
	Stream_Write_UINT32(s, pioRecvPciNdrPtr);   /* pioRecvPciNdrPtr (4 bytes) */
	Stream_Write_UINT32(s, cbRecvLength);       /* cbRecvLength (4 bytes) */
	Stream_Write_UINT32(s, pbRecvBufferNdrPtr); /* pbRecvBufferNdrPtr (4 bytes) */

	if (pioRecvPciNdrPtr)
	{
		cbExtraBytes = (UINT32)(ret->pioRecvPci->cbPciLength - sizeof(SCARD_IO_REQUEST));
		pbExtraBytes = &((BYTE*)ret->pioRecvPci)[sizeof(SCARD_IO_REQUEST)];
		pbExtraBytesNdrPtr = cbExtraBytes ? 0x00020008 : 0;

		if (!Stream_EnsureRemainingCapacity(s, cbExtraBytes + 16))
		{
			WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
			return SCARD_F_INTERNAL_ERROR;
		}

		Stream_Write_UINT32(s, ret->pioRecvPci->dwProtocol); /* dwProtocol (4 bytes) */
		Stream_Write_UINT32(s, cbExtraBytes);                /* cbExtraBytes (4 bytes) */
		Stream_Write_UINT32(s, pbExtraBytesNdrPtr);          /* pbExtraBytesNdrPtr (4 bytes) */

		if (pbExtraBytesNdrPtr)
		{
			Stream_Write_UINT32(s, cbExtraBytes); /* Length (4 bytes) */
			Stream_Write(s, pbExtraBytes, cbExtraBytes);

			if ((error = smartcard_pack_write_size_align(smartcard, s, cbExtraBytes, 4)))
			{
				WLog_ERR(TAG, "smartcard_pack_write_size_align failed with error %" PRId32 "!",
				         error);
				return error;
			}
		}
	}

	if (pbRecvBufferNdrPtr)
	{
		if (!Stream_EnsureRemainingCapacity(s, ret->cbRecvLength + 16))
		{
			WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
			return SCARD_F_INTERNAL_ERROR;
		}

		Stream_Write_UINT32(s, ret->cbRecvLength); /* pbRecvBufferNdrLen (4 bytes) */
		Stream_Write(s, ret->pbRecvBuffer, ret->cbRecvLength);

		if ((error = smartcard_pack_write_size_align(smartcard, s, ret->cbRecvLength, 4)))
		{
			WLog_ERR(TAG, "smartcard_pack_write_size_align failed with error %" PRId32 "!", error);
			return error;
		}
	}

	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_locate_cards_by_atr_a_call(SMARTCARD_DEVICE* smartcard, wStream* s,
                                                 LocateCardsByATRA_Call* call)
{
	UINT32 index;
	UINT32 count;
	LONG status;
	UINT32 offset;
	UINT32 maxCount;
	UINT32 szReaderNdrPtr;
	UINT32 rgReaderStatesNdrPtr;
	UINT32 rgAtrMasksNdrPtr;
	call->rgReaderStates = NULL;

	status = smartcard_unpack_redir_scard_context(smartcard, s, &(call->hContext));
	if (status)
		return status;

	if (Stream_GetRemainingLength(s) < 16)
	{
		WLog_WARN(TAG, "LocateCardsByATRA_Call is too short: %" PRIuz "",
		          Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Read_UINT32(s, call->cAtrs);
	Stream_Read_UINT32(s, rgAtrMasksNdrPtr);
	Stream_Read_UINT32(s, call->cReaders);       /* cReaders (4 bytes) */
	Stream_Read_UINT32(s, rgReaderStatesNdrPtr); /* rgReaderStatesNdrPtr (4 bytes) */

	if ((status = smartcard_unpack_redir_scard_context_ref(smartcard, s, &(call->hContext))))
	{
		WLog_ERR(TAG, "smartcard_unpack_redir_scard_context_ref failed with error %" PRId32 "",
		         status);
		return status;
	}

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_WARN(TAG, "LocateCardsByATRA_Call is too short: %" PRIuz "",
		          Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	if ((rgAtrMasksNdrPtr && !call->cAtrs) || (!rgAtrMasksNdrPtr && call->cAtrs))
	{
		WLog_WARN(TAG,
		          "LocateCardsByATRA_Call rgAtrMasksNdrPtr (0x%08" PRIX32
		          ") and cAtrs (0x%08" PRIX32 ") inconsistency",
		          rgAtrMasksNdrPtr, call->cAtrs);
		return STATUS_INVALID_PARAMETER;
	}

	if (rgAtrMasksNdrPtr)
	{
		Stream_Read_UINT32(s, count);

		if (count != call->cAtrs)
		{
			WLog_WARN(TAG,
			          "LocateCardsByATRA_Call NdrCount (0x%08" PRIX32 ") and cAtrs (0x%08" PRIX32
			          ") inconsistency",
			          count, call->cAtrs);
			return STATUS_INVALID_PARAMETER;
		}

		if (Stream_GetRemainingLength(s) < call->cAtrs)
		{
			WLog_WARN(TAG,
			          "LocateCardsByATRA_Call is too short: Actual: %" PRIuz ", Expected: %" PRIu32
			          "",
			          Stream_GetRemainingLength(s), call->cAtrs);
			return STATUS_BUFFER_TOO_SMALL;
		}

		call->rgAtrMasks = (LocateCards_ATRMask*)calloc(call->cAtrs, sizeof(LocateCards_ATRMask));

		if (!call->rgAtrMasks)
		{
			WLog_WARN(TAG, "LocateCardsByATRA_Call out of memory error (call->rgAtrMasks)");
			return STATUS_NO_MEMORY;
		}

		for (index = 0; index < call->cAtrs; index++)
		{
			Stream_Read_UINT32(s, call->rgAtrMasks[index].cbAtr);
			Stream_Read(s, call->rgAtrMasks[index].rgbAtr, 36);
			Stream_Read(s, call->rgAtrMasks[index].rgbMask, 36);
		}
	}

	Stream_Read_UINT32(s, count);

	if (count != call->cReaders)
	{
		WLog_WARN(TAG,
		          "GetStatusChangeA_Call unexpected reader count: Actual: %" PRIu32
		          ", Expected: %" PRIu32 "",
		          count, call->cReaders);
		return STATUS_INVALID_PARAMETER;
	}

	if (call->cReaders > 0)
	{
		call->rgReaderStates =
		    (SCARD_READERSTATEA*)calloc(call->cReaders, sizeof(SCARD_READERSTATEA));

		if (!call->rgReaderStates)
		{
			WLog_WARN(TAG, "LocateCardsByATRA_Call out of memory error (call->rgReaderStates)");
			return STATUS_NO_MEMORY;
		}

		for (index = 0; index < call->cReaders; index++)
		{
			LPSCARD_READERSTATEA readerState = &call->rgReaderStates[index];

			if (Stream_GetRemainingLength(s) < 52)
			{
				WLog_WARN(TAG, "LocateCardsByATRA_Call is too short: %" PRIuz "",
				          Stream_GetRemainingLength(s));
				return STATUS_BUFFER_TOO_SMALL;
			}

			Stream_Read_UINT32(s, szReaderNdrPtr);              /* szReaderNdrPtr (4 bytes) */
			Stream_Read_UINT32(s, readerState->dwCurrentState); /* dwCurrentState (4 bytes) */
			Stream_Read_UINT32(s, readerState->dwEventState);   /* dwEventState (4 bytes) */
			Stream_Read_UINT32(s, readerState->cbAtr);          /* cbAtr (4 bytes) */
			Stream_Read(s, readerState->rgbAtr, 32);            /* rgbAtr [0..32] (32 bytes) */
			Stream_Seek(s, 4);                                  /* rgbAtr [32..36] (4 bytes) */
		}

		for (index = 0; index < call->cReaders; index++)
		{
			LPSCARD_READERSTATEA readerState = &call->rgReaderStates[index];

			if (Stream_GetRemainingLength(s) < 12)
			{
				WLog_WARN(TAG, "GetStatusChangeA_Call is too short: %" PRIuz "",
				          Stream_GetRemainingLength(s));
				return STATUS_BUFFER_TOO_SMALL;
			}

			Stream_Read_UINT32(s, maxCount); /* NdrMaxCount (4 bytes) */
			Stream_Read_UINT32(s, offset);   /* NdrOffset (4 bytes) */
			Stream_Read_UINT32(s, count);    /* NdrActualCount (4 bytes) */

			if (Stream_GetRemainingLength(s) < count)
			{
				WLog_WARN(TAG, "GetStatusChangeA_Call is too short: %" PRIuz "",
				          Stream_GetRemainingLength(s));
				return STATUS_BUFFER_TOO_SMALL;
			}

			readerState->szReader = (LPSTR)malloc(count + 1);

			if (!readerState->szReader)
			{
				WLog_WARN(TAG, "GetStatusChangeA_Call out of memory error (readerState->szReader)");
				return STATUS_NO_MEMORY;
			}

			Stream_Read(s, (void*)readerState->szReader, count);
			smartcard_unpack_read_size_align(smartcard, s, count, 4);
			((char*)readerState->szReader)[count] = '\0';

			if (!readerState->szReader)
			{
				WLog_WARN(TAG, "GetStatusChangeA_Call null reader name");
				return STATUS_INVALID_PARAMETER;
			}
		}
	}

	smartcard_trace_locate_cards_by_atr_a_call(smartcard, call);
	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_context_and_two_strings_a_call(SMARTCARD_DEVICE* smartcard, wStream* s,
                                                     ContextAndTwoStringA_Call* call)
{
	LONG status;
	UINT32 sz1NdrPtr, sz2NdrPtr;

	smartcard_trace_context_and_two_strings_a_call(smartcard, call);

	status = smartcard_unpack_redir_scard_context(smartcard, s, &(call->hContext));
	if (status)
		return status;

	if (Stream_GetRemainingLength(s) < 8)
	{
		WLog_WARN(TAG, "smartcard_unpack_context_and_two_strings_a_call is too short: %" PRIuz "",
		          Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}
	Stream_Read_UINT32(s, sz1NdrPtr);
	Stream_Read_UINT32(s, sz2NdrPtr);
	if (sz1NdrPtr)
	{
		UINT32 len;
		if (Stream_GetRemainingLength(s) < 4)
		{
			WLog_WARN(TAG,
			          "smartcard_unpack_context_and_two_strings_a_call is too short: %" PRIuz "",
			          Stream_GetRemainingLength(s));
			return STATUS_BUFFER_TOO_SMALL;
		}
		Stream_Read_UINT32(s, len);
		if (Stream_GetRemainingLength(s) < len)
		{
			WLog_WARN(TAG,
			          "smartcard_unpack_context_and_two_strings_a_call is too short: %" PRIuz "",
			          Stream_GetRemainingLength(s));
			return STATUS_BUFFER_TOO_SMALL;
		}
		call->sz1 = calloc(len + 1, sizeof(CHAR));
		if (!call->sz1)
			return STATUS_NO_MEMORY;
		Stream_Read(s, call->sz1, len);
	}
	if (sz2NdrPtr)
	{
		UINT32 len;
		if (Stream_GetRemainingLength(s) < 4)
		{
			WLog_WARN(TAG,
			          "smartcard_unpack_context_and_two_strings_a_call is too short: %" PRIuz "",
			          Stream_GetRemainingLength(s));
			return STATUS_BUFFER_TOO_SMALL;
		}
		Stream_Read_UINT32(s, len);
		if (Stream_GetRemainingLength(s) < len)
		{
			WLog_WARN(TAG,
			          "smartcard_unpack_context_and_two_strings_a_call is too short: %" PRIuz "",
			          Stream_GetRemainingLength(s));
			return STATUS_BUFFER_TOO_SMALL;
		}
		call->sz2 = calloc(len + 1, sizeof(CHAR));
		if (!call->sz2)
			return STATUS_NO_MEMORY;
		Stream_Read(s, call->sz2, len);
	}

	return -1;
}

LONG smartcard_unpack_context_and_two_strings_w_call(SMARTCARD_DEVICE* smartcard, wStream* s,
                                                     ContextAndTwoStringW_Call* call)
{
	LONG status;
	UINT32 sz1NdrPtr, sz2NdrPtr;

	smartcard_trace_context_and_two_strings_w_call(smartcard, call);

	status = smartcard_unpack_redir_scard_context(smartcard, s, &(call->hContext));
	if (status)
		return status;

	if (Stream_GetRemainingLength(s) < 8)
	{
		WLog_WARN(TAG, "smartcard_unpack_context_and_two_strings_w_call is too short: %" PRIuz "",
		          Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}
	Stream_Read_UINT32(s, sz1NdrPtr);
	Stream_Read_UINT32(s, sz2NdrPtr);
	if (sz1NdrPtr)
	{
		UINT32 len;
		if (Stream_GetRemainingLength(s) < 4)
		{
			WLog_WARN(TAG,
			          "smartcard_unpack_context_and_two_strings_w_call is too short: %" PRIuz "",
			          Stream_GetRemainingLength(s));
			return STATUS_BUFFER_TOO_SMALL;
		}
		Stream_Read_UINT32(s, len);
		if (Stream_GetRemainingLength(s) < len)
		{
			WLog_WARN(TAG,
			          "smartcard_unpack_context_and_two_strings_w_call is too short: %" PRIuz "",
			          Stream_GetRemainingLength(s));
			return STATUS_BUFFER_TOO_SMALL;
		}
		call->sz1 = calloc(len + 1, sizeof(WCHAR));
		if (!call->sz1)
			return STATUS_NO_MEMORY;
		Stream_Read(s, call->sz1, len);
	}
	if (sz2NdrPtr)
	{
		UINT32 len;
		if (Stream_GetRemainingLength(s) < 4)
		{
			WLog_WARN(TAG,
			          "smartcard_unpack_context_and_two_strings_w_call is too short: %" PRIuz "",
			          Stream_GetRemainingLength(s));
			return STATUS_BUFFER_TOO_SMALL;
		}
		Stream_Read_UINT32(s, len);
		if (Stream_GetRemainingLength(s) < len)
		{
			WLog_WARN(TAG,
			          "smartcard_unpack_context_and_two_strings_w_call is too short: %" PRIuz "",
			          Stream_GetRemainingLength(s));
			return STATUS_BUFFER_TOO_SMALL;
		}
		call->sz2 = calloc(len + 1, sizeof(WCHAR));
		if (!call->sz2)
			return STATUS_NO_MEMORY;
		Stream_Read(s, call->sz2, len);
	}
	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_locate_cards_a_call(SMARTCARD_DEVICE* smartcard, wStream* s,
                                          LocateCardsA_Call* call)
{

	LONG status;
	UINT32 sz1NdrPtr, sz2NdrPtr;

	smartcard_trace_locate_cards_a_call(smartcard, call);

	status = smartcard_unpack_redir_scard_context(smartcard, s, &(call->hContext));
	if (status)
		return status;

	if (Stream_GetRemainingLength(s) < 8)
	{
		WLog_WARN(TAG, "%s is too short: %" PRIuz "", __FUNCTION__, Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}
	Stream_Read_UINT32(s, call->cBytes);
	Stream_Read_UINT32(s, sz1NdrPtr);
	Stream_Read_UINT32(s, call->cReaders);
	Stream_Read_UINT32(s, sz2NdrPtr);
	if (sz1NdrPtr)
	{
		UINT32 len;
		if (Stream_GetRemainingLength(s) < 4)
		{
			WLog_WARN(TAG,
			          "smartcard_unpack_context_and_two_strings_w_call is too short: %" PRIuz "",
			          Stream_GetRemainingLength(s));
			return STATUS_BUFFER_TOO_SMALL;
		}
		Stream_Read_UINT32(s, len);
		if (Stream_GetRemainingLength(s) < len)
		{
			WLog_WARN(TAG,
			          "smartcard_unpack_context_and_two_strings_w_call is too short: %" PRIuz "",
			          Stream_GetRemainingLength(s));
			return STATUS_BUFFER_TOO_SMALL;
		}
		call->mszCards = calloc(len + 1, sizeof(CHAR));
		if (!call->mszCards)
			return STATUS_NO_MEMORY;
		Stream_Read(s, call->mszCards, len);
	}
	if (sz2NdrPtr)
	{
		UINT32 len;
		if (Stream_GetRemainingLength(s) < 4)
		{
			WLog_WARN(TAG, "%s is too short: %" PRIuz "", __FUNCTION__,
			          Stream_GetRemainingLength(s));
			return STATUS_BUFFER_TOO_SMALL;
		}
		Stream_Read_UINT32(s, len);
		if (Stream_GetRemainingLength(s) < len)
		{
			WLog_WARN(TAG, "%s is too short: %" PRIuz "", __FUNCTION__,
			          Stream_GetRemainingLength(s));
			return STATUS_BUFFER_TOO_SMALL;
		}
		call->rgReaderStates =
		    calloc(len / sizeof(SCARD_READERSTATEA) + 1, sizeof(SCARD_READERSTATEA));
		if (!call->rgReaderStates)
			return STATUS_NO_MEMORY;

		Stream_Read(s, call->rgReaderStates, len);
	}
	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_locate_cards_w_call(SMARTCARD_DEVICE* smartcard, wStream* s,
                                          LocateCardsW_Call* call)
{
	LONG status;
	UINT32 sz1NdrPtr, sz2NdrPtr;

	smartcard_trace_locate_cards_w_call(smartcard, call);

	status = smartcard_unpack_redir_scard_context(smartcard, s, &(call->hContext));
	if (status)
		return status;

	if (Stream_GetRemainingLength(s) < 8)
	{
		WLog_WARN(TAG, "%s is too short: %" PRIuz "", __FUNCTION__, Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}
	Stream_Read_UINT32(s, call->cBytes);
	Stream_Read_UINT32(s, sz1NdrPtr);
	Stream_Read_UINT32(s, call->cReaders);
	Stream_Read_UINT32(s, sz2NdrPtr);
	if (sz1NdrPtr)
	{
		UINT32 len;
		if (Stream_GetRemainingLength(s) < 4)
		{
			WLog_WARN(TAG, "%s is too short: %" PRIuz "", __FUNCTION__,
			          Stream_GetRemainingLength(s));
			return STATUS_BUFFER_TOO_SMALL;
		}
		Stream_Read_UINT32(s, len);
		if (Stream_GetRemainingLength(s) < len)
		{
			WLog_WARN(TAG, "%s is too short: %" PRIuz "", __FUNCTION__,
			          Stream_GetRemainingLength(s));
			return STATUS_BUFFER_TOO_SMALL;
		}
		call->mszCards = calloc(len + 1, sizeof(WCHAR));
		if (!call->mszCards)
			return STATUS_NO_MEMORY;
		Stream_Read(s, call->mszCards, len);
	}
	if (sz2NdrPtr)
	{
		UINT32 len;
		if (Stream_GetRemainingLength(s) < 4)
		{
			WLog_WARN(TAG, "%s is too short: %" PRIuz "", __FUNCTION__,
			          Stream_GetRemainingLength(s));
			return STATUS_BUFFER_TOO_SMALL;
		}
		Stream_Read_UINT32(s, len);
		if (Stream_GetRemainingLength(s) < len)
		{
			WLog_WARN(TAG, "%s is too short: %" PRIuz "", __FUNCTION__,
			          Stream_GetRemainingLength(s));
			return STATUS_BUFFER_TOO_SMALL;
		}
		call->rgReaderStates =
		    calloc(len / sizeof(SCARD_READERSTATEW) + 1, sizeof(SCARD_READERSTATEW));
		if (!call->rgReaderStates)
			return STATUS_NO_MEMORY;

		Stream_Read(s, call->rgReaderStates, len);
	}
	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_set_attrib_call(SMARTCARD_DEVICE* smartcard, wStream* s, SetAttrib_Call* call)
{
	LONG status;
	smartcard_trace_set_attrib_call(smartcard, call);

	status = smartcard_unpack_redir_scard_context(smartcard, s, &(call->hContext));
	if (status)
		return status;
	status = smartcard_unpack_redir_scard_handle(smartcard, s, &(call->hCard));
	if (status)
		return status;

	if (Stream_GetRemainingLength(s) < 12)
		return STATUS_BUFFER_TOO_SMALL;
	Stream_Read_UINT32(s, call->dwAttrId);
	Stream_Read_UINT32(s, call->cbAttrLen);
	if (Stream_GetRemainingLength(s) < call->cbAttrLen)
		return STATUS_BUFFER_TOO_SMALL;
	call->pbAttr = malloc(call->cbAttrLen);
	if (!call->pbAttr)
		return STATUS_NO_MEMORY;

	Stream_Read(s, call->pbAttr, call->cbAttrLen);
	return SCARD_S_SUCCESS;
}

LONG smartcard_pack_set_attrib_return(SMARTCARD_DEVICE* smartcard, wStream* s,
                                      const GetAttrib_Return* ret)
{
	LONG error;

	if (!Stream_EnsureRemainingCapacity(s, ret->cbAttrLen + 8))
	{
		WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Write_UINT32(s, ret->cbAttrLen); /* cbOutBufferSize (4 bytes) */
	Stream_Write(s, ret->pbAttr, ret->cbAttrLen);

	if ((error = smartcard_pack_write_size_align(smartcard, s, ret->cbAttrLen, 4)))
	{
		WLog_ERR(TAG, "smartcard_pack_write_size_align failed with error %" PRId32 "", error);
		return error;
	}

	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_locate_cards_by_atr_w_call(SMARTCARD_DEVICE* smartcard, wStream* s,
                                                 LocateCardsByATRW_Call* call)
{
	UINT32 index;
	UINT32 count;
	LONG status;
	UINT32 offset;
	UINT32 maxCount;
	UINT32 szReaderNdrPtr;
	UINT32 rgReaderStatesNdrPtr;
	UINT32 rgAtrMasksNdrPtr;

	call->rgReaderStates = NULL;

	status = smartcard_unpack_redir_scard_context(smartcard, s, &(call->hContext));
	if (status)
		return status;

	if (Stream_GetRemainingLength(s) < 16)
	{
		WLog_WARN(TAG, "LocateCardsByATRW_Call is too short: %" PRIuz "",
		          Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Read_UINT32(s, call->cAtrs);
	Stream_Read_UINT32(s, rgAtrMasksNdrPtr);
	Stream_Read_UINT32(s, call->cReaders);       /* cReaders (4 bytes) */
	Stream_Read_UINT32(s, rgReaderStatesNdrPtr); /* rgReaderStatesNdrPtr (4 bytes) */

	if ((status = smartcard_unpack_redir_scard_context_ref(smartcard, s, &(call->hContext))))
	{
		WLog_ERR(TAG, "smartcard_unpack_redir_scard_context_ref failed with error %" PRId32 "",
		         status);
		return status;
	}

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_WARN(TAG, "LocateCardsByATRW_Call is too short: %" PRIuz "",
		          Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	if ((rgAtrMasksNdrPtr && !call->cAtrs) || (!rgAtrMasksNdrPtr && call->cAtrs))
	{
		WLog_WARN(TAG,
		          "LocateCardsByATRW_Call rgAtrMasksNdrPtr (0x%08" PRIX32
		          ") and cAtrs (0x%08" PRIX32 ") inconsistency",
		          rgAtrMasksNdrPtr, call->cAtrs);
		return STATUS_INVALID_PARAMETER;
	}

	if (rgAtrMasksNdrPtr)
	{
		Stream_Read_UINT32(s, count);

		if (count != call->cAtrs)
		{
			WLog_WARN(TAG,
			          "LocateCardsByATRW_Call NdrCount (0x%08" PRIX32 ") and cAtrs (0x%08" PRIX32
			          ") inconsistency",
			          count, call->cAtrs);
			return STATUS_INVALID_PARAMETER;
		}

		if (Stream_GetRemainingLength(s) < call->cAtrs)
		{
			WLog_WARN(TAG,
			          "LocateCardsByATRW_Call is too short: Actual: %" PRIuz ", Expected: %" PRIu32
			          "",
			          Stream_GetRemainingLength(s), call->cAtrs);
			return STATUS_BUFFER_TOO_SMALL;
		}

		call->rgAtrMasks = (LocateCards_ATRMask*)calloc(call->cAtrs, sizeof(LocateCards_ATRMask));

		if (!call->rgAtrMasks)
		{
			WLog_WARN(TAG, "LocateCardsByATRW_Call out of memory error (call->rgAtrMasks)");
			return STATUS_NO_MEMORY;
		}

		for (index = 0; index < call->cAtrs; index++)
		{
			Stream_Read_UINT32(s, call->rgAtrMasks[index].cbAtr);
			Stream_Read(s, call->rgAtrMasks[index].rgbAtr, 36);
			Stream_Read(s, call->rgAtrMasks[index].rgbMask, 36);
		}
	}

	Stream_Read_UINT32(s, count);

	if (count != call->cReaders)
	{
		WLog_WARN(TAG,
		          "LocateCardsByATRW_Call unexpected reader count: Actual: %" PRIu32
		          ", Expected: %" PRIu32 "",
		          count, call->cReaders);
		return STATUS_INVALID_PARAMETER;
	}

	if (call->cReaders > 0)
	{
		call->rgReaderStates =
		    (SCARD_READERSTATEW*)calloc(call->cReaders, sizeof(SCARD_READERSTATEW));

		if (!call->rgReaderStates)
		{
			WLog_WARN(TAG, "LocateCardsByATRW_Call out of memory error (call->rgReaderStates)");
			return STATUS_NO_MEMORY;
		}

		for (index = 0; index < call->cReaders; index++)
		{
			LPSCARD_READERSTATEW readerState = (LPSCARD_READERSTATEW)&call->rgReaderStates[index];

			if (Stream_GetRemainingLength(s) < 52)
			{
				WLog_WARN(TAG, "LocateCardsByATRW_Call is too short: %" PRIuz "",
				          Stream_GetRemainingLength(s));
				return STATUS_BUFFER_TOO_SMALL;
			}

			Stream_Read_UINT32(s, szReaderNdrPtr);              /* szReaderNdrPtr (4 bytes) */
			Stream_Read_UINT32(s, readerState->dwCurrentState); /* dwCurrentState (4 bytes) */
			Stream_Read_UINT32(s, readerState->dwEventState);   /* dwEventState (4 bytes) */
			Stream_Read_UINT32(s, readerState->cbAtr);          /* cbAtr (4 bytes) */
			Stream_Read(s, readerState->rgbAtr, 32);            /* rgbAtr [0..32] (32 bytes) */
			Stream_Seek(s, 4);                                  /* rgbAtr [32..36] (4 bytes) */
		}

		for (index = 0; index < call->cReaders; index++)
		{
			LPSCARD_READERSTATEW readerState = (LPSCARD_READERSTATEW)&call->rgReaderStates[index];

			if (Stream_GetRemainingLength(s) < 12)
			{
				WLog_WARN(TAG, "LocateCardsByATRW_Call is too short: %" PRIuz "",
				          Stream_GetRemainingLength(s));
				return STATUS_BUFFER_TOO_SMALL;
			}

			Stream_Read_UINT32(s, maxCount); /* NdrMaxCount (4 bytes) */
			Stream_Read_UINT32(s, offset);   /* NdrOffset (4 bytes) */
			Stream_Read_UINT32(s, count);    /* NdrActualCount (4 bytes) */

			if (Stream_GetRemainingLength(s) < count)
			{
				WLog_WARN(TAG, "LocateCardsByATRW_Call is too short: %" PRIuz "",
				          Stream_GetRemainingLength(s));
				return STATUS_BUFFER_TOO_SMALL;
			}

			readerState->szReader = (LPWSTR)calloc(count + 1, sizeof(WCHAR));

			if (!readerState->szReader)
			{
				WLog_WARN(TAG,
				          "LocateCardsByATRW_Call out of memory error (readerState->szReader)");
				return STATUS_NO_MEMORY;
			}

			Stream_Read_UTF16_String(s, readerState->szReader, count);

			smartcard_unpack_read_size_align(smartcard, s, count, 4);

			if (!readerState->szReader)
			{
				WLog_WARN(TAG, "LocateCardsByATRW_Call null reader name");
				return STATUS_INVALID_PARAMETER;
			}
		}
	}

	smartcard_trace_locate_cards_by_atr_w_call(smartcard, call);
	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_read_cache_a_call(SMARTCARD_DEVICE* smartcard, wStream* s,
                                        ReadCacheA_Call* call)
{
	LONG status;
	UINT32 mszNdrPtr;
	UINT32 contextNdrPtr;
	smartcard_trace_read_cache_a_call(smartcard, call);

	if (Stream_GetRemainingLength(s) < 4)
		return STATUS_BUFFER_TOO_SMALL;
	Stream_Read_UINT32(s, mszNdrPtr);

	status = smartcard_unpack_redir_scard_context(smartcard, s, &(call->Common.hContext));
	if (status)
		return status;

	if (Stream_GetRemainingLength(s) < 16)
		return STATUS_BUFFER_TOO_SMALL;
	Stream_Read_UINT32(s, contextNdrPtr);
	Stream_Read_UINT32(s, call->Common.FreshnessCounter);
	Stream_Read_INT32(s, call->Common.fPbDataIsNULL);
	Stream_Read_UINT32(s, call->Common.cbDataLen);

	call->szLookupName = NULL;
	if (mszNdrPtr)
	{
		status = smartcard_ndr_read_a(s, &call->szLookupName, sizeof(CHAR));
		if (status)
			return status;
	}

	if (contextNdrPtr)
	{
		status = smartcard_ndr_read_u(s, &call->Common.CardIdentifier, sizeof(UUID));
		if (status)
			return status;
	}
	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_read_cache_w_call(SMARTCARD_DEVICE* smartcard, wStream* s,
                                        ReadCacheW_Call* call)
{
	LONG status;
	UINT32 mszNdrPtr;
	UINT32 contextNdrPtr;
	smartcard_trace_read_cache_w_call(smartcard, call);

	if (Stream_GetRemainingLength(s) < 4)
		return STATUS_BUFFER_TOO_SMALL;
	Stream_Read_UINT32(s, mszNdrPtr);

	status = smartcard_unpack_redir_scard_context(smartcard, s, &(call->Common.hContext));
	if (status)
		return status;

	if (Stream_GetRemainingLength(s) < 16)
		return STATUS_BUFFER_TOO_SMALL;
	Stream_Read_UINT32(s, contextNdrPtr);
	Stream_Read_UINT32(s, call->Common.FreshnessCounter);
	Stream_Read_INT32(s, call->Common.fPbDataIsNULL);
	Stream_Read_UINT32(s, call->Common.cbDataLen);

	call->szLookupName = NULL;
	if (mszNdrPtr)
	{
		status = smartcard_ndr_read_w(s, &call->szLookupName, sizeof(WCHAR));
		if (status)
			return status;
	}

	if (contextNdrPtr)
	{
		status = smartcard_ndr_read(s, &call->Common.CardIdentifier, sizeof(UUID));
		if (status)
			return status;
	}
	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_write_cache_a_call(SMARTCARD_DEVICE* smartcard, wStream* s,
                                         WriteCacheA_Call* call)
{
	LONG status;
	UINT32 mszNdrPtr;
	UINT32 contextNdrPtr;
	UINT32 pbDataNdrPtr;
	smartcard_trace_write_cache_a_call(smartcard, call);

	if (Stream_GetRemainingLength(s) < 4)
		return STATUS_BUFFER_TOO_SMALL;
	Stream_Read_UINT32(s, mszNdrPtr);

	status = smartcard_unpack_redir_scard_context(smartcard, s, &(call->Common.hContext));
	if (status)
		return status;

	if (Stream_GetRemainingLength(s) < 16)
		return STATUS_BUFFER_TOO_SMALL;
	Stream_Read_UINT32(s, contextNdrPtr);
	Stream_Read_UINT32(s, call->Common.FreshnessCounter);
	Stream_Read_UINT32(s, call->Common.cbDataLen);
	Stream_Read_UINT32(s, pbDataNdrPtr);

	call->szLookupName = NULL;
	if (mszNdrPtr)
	{
		status = smartcard_ndr_read_a(s, &call->szLookupName, sizeof(CHAR));
		if (status)
			return status;
	}

	call->Common.CardIdentifier = NULL;
	if (contextNdrPtr)
	{
		status = smartcard_ndr_read_u(s, &call->Common.CardIdentifier, sizeof(UUID));
		if (status)
			return status;
	}

	call->Common.pbData = NULL;
	if (pbDataNdrPtr)
	{
		status = smartcard_ndr_read(s, &call->Common.pbData, sizeof(CHAR));
		if (status)
			return status;
	}
	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_write_cache_w_call(SMARTCARD_DEVICE* smartcard, wStream* s,
                                         WriteCacheW_Call* call)
{
	LONG status;
	UINT32 mszNdrPtr;
	UINT32 contextNdrPtr;
	UINT32 pbDataNdrPtr;
	smartcard_trace_write_cache_w_call(smartcard, call);

	if (Stream_GetRemainingLength(s) < 4)
		return STATUS_BUFFER_TOO_SMALL;
	Stream_Read_UINT32(s, mszNdrPtr);

	status = smartcard_unpack_redir_scard_context(smartcard, s, &(call->Common.hContext));
	if (status)
		return status;

	if (Stream_GetRemainingLength(s) < 16)
		return STATUS_BUFFER_TOO_SMALL;
	Stream_Read_UINT32(s, contextNdrPtr);
	Stream_Read_UINT32(s, call->Common.FreshnessCounter);
	Stream_Read_UINT32(s, call->Common.cbDataLen);
	Stream_Read_UINT32(s, pbDataNdrPtr);

	call->szLookupName = NULL;
	if (mszNdrPtr)
	{
		status = smartcard_ndr_read_w(s, &call->szLookupName, sizeof(WCHAR));
		if (status)
			return status;
	}

	call->Common.CardIdentifier = NULL;
	if (contextNdrPtr)
	{
		status = smartcard_ndr_read_u(s, &call->Common.CardIdentifier, sizeof(UUID));
		if (status)
			return status;
	}

	call->Common.pbData = NULL;
	if (pbDataNdrPtr)
	{
		status = smartcard_ndr_read(s, &call->Common.pbData, sizeof(CHAR));
		if (status)
			return status;
	}
	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_get_transmit_count_call(SMARTCARD_DEVICE* smartcard, wStream* s,
                                              GetTransmitCount_Call* call)
{
	LONG status;
	smartcard_trace_get_transmit_count_call(smartcard, call);
	status = smartcard_unpack_redir_scard_context(smartcard, s, &(call->hContext));
	if (status)
		return status;

	status = smartcard_unpack_redir_scard_handle(smartcard, s, &(call->hCard));
	if (status)
		return status;

	return status;
}

LONG smartcard_unpack_get_reader_icon_call(SMARTCARD_DEVICE* smartcard, wStream* s,
                                           GetReaderIcon_Call* call)
{
	return smartcard_unpack_common_context_and_string_w(smartcard, s, &call->hContext,
	                                                    &call->szReaderName);
}

LONG smartcard_unpack_context_and_string_a_call(SMARTCARD_DEVICE* smartcard, wStream* s,
                                                ContextAndStringA_Call* call)
{
	return smartcard_unpack_common_context_and_string_a(smartcard, s, &call->hContext, &call->sz);
}

LONG smartcard_unpack_context_and_string_w_call(SMARTCARD_DEVICE* smartcard, wStream* s,
                                                ContextAndStringW_Call* call)
{
	return smartcard_unpack_common_context_and_string_w(smartcard, s, &call->hContext, &call->sz);
}

LONG smartcard_unpack_get_device_type_id_call(SMARTCARD_DEVICE* smartcard, wStream* s,
                                              GetDeviceTypeId_Call* call)
{
	return smartcard_unpack_common_context_and_string_w(smartcard, s, &call->hContext,
	                                                    &call->szReaderName);
}

LONG smartcard_pack_device_type_id_return(SMARTCARD_DEVICE* smartcard, wStream* s,
                                          const GetDeviceTypeId_Return* ret)
{
	WINPR_UNUSED(smartcard);
	smartcard_trace_device_type_id_return(smartcard, ret);

	if (!Stream_EnsureRemainingCapacity(s, 8))
	{
		WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Write_UINT32(s, ret->dwDeviceId); /* cBytes (4 bytes) */

	return SCARD_S_SUCCESS;
}

LONG smartcard_pack_locate_cards_return(SMARTCARD_DEVICE* smartcard, wStream* s,
                                        const LocateCards_Return* ret)
{

	UINT32 pbDataNdrPtr = (ret->rgReaderStates) ? 0x00020000 : 0;
	smartcard_trace_locate_cards_return(smartcard, ret);
	if (!Stream_EnsureRemainingCapacity(s, 8))
	{
		WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Write_UINT32(s, ret->cReaders); /* cBytes (4 cbDataLen) */
	Stream_Write_UINT32(s, pbDataNdrPtr);
	if (ret->rgReaderStates)
	{
		size_t x, size;

		size = sizeof(ReaderState_Return) * ret->cReaders;
		if (!Stream_EnsureRemainingCapacity(s, size + 8))
		{
			WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
			return SCARD_F_INTERNAL_ERROR;
		}

		Stream_Write_UINT32(s, (UINT32)size);
		for (x = 0; x < ret->cReaders; x++)
		{
			ReaderState_Return* reader = &ret->rgReaderStates[x];
			Stream_Write_UINT32(s, reader->dwCurrentState);
			Stream_Write_UINT32(s, reader->dwEventState);
			Stream_Write_UINT32(s, reader->cbAtr);
			Stream_Write(s, reader->rgbAtr, sizeof(reader->rgbAtr));
		}
		smartcard_pack_write_size_align(smartcard, s, (UINT32)size, 4);
	}

	return SCARD_S_SUCCESS;
}

LONG smartcard_pack_get_reader_icon_return(SMARTCARD_DEVICE* smartcard, wStream* s,
                                           const GetReaderIcon_Return* ret)
{
	UINT32 pbDataNdrPtr = (ret->pbData) ? 0x00020000 : 0;
	smartcard_trace_get_reader_icon_return(smartcard, ret);
	if (!Stream_EnsureRemainingCapacity(s, 8))
	{
		WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Write_UINT32(s, ret->cbDataLen); /* cBytes (4 cbDataLen) */
	Stream_Write_UINT32(s, pbDataNdrPtr);
	if (ret->pbData)
	{
		if (!Stream_EnsureRemainingCapacity(s, ret->cbDataLen + 8))
		{
			WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
			return SCARD_F_INTERNAL_ERROR;
		}

		Stream_Write_UINT32(s, ret->cbDataLen);
		Stream_Write(s, ret->pbData, ret->cbDataLen);
		smartcard_pack_write_size_align(smartcard, s, ret->cbDataLen, 4);
	}

	return SCARD_S_SUCCESS;
}

LONG smartcard_pack_get_transmit_count_return(SMARTCARD_DEVICE* smartcard, wStream* s,
                                              const GetTransmitCount_Return* ret)
{

	smartcard_trace_get_transmit_count_return(smartcard, ret);
	if (!Stream_EnsureRemainingCapacity(s, 8))
	{
		WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Write_UINT32(s, ret->cTransmitCount); /* cBytes (4 cbDataLen) */

	return SCARD_S_SUCCESS;
}

LONG smartcard_pack_read_cache_return(SMARTCARD_DEVICE* smartcard, wStream* s,
                                      const ReadCache_Return* ret)
{
	UINT32 pbDataNdrPtr = (ret->pbData) ? 0x00020000 : 0;
	smartcard_trace_read_cache_return(smartcard, ret);
	if (!Stream_EnsureRemainingCapacity(s, 8))
	{
		WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Write_UINT32(s, ret->cbDataLen); /* cBytes (4 cbDataLen) */
	Stream_Write_UINT32(s, pbDataNdrPtr);
	if (ret->pbData)
	{
		if (!Stream_EnsureRemainingCapacity(s, ret->cbDataLen + 8))
		{
			WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
			return SCARD_F_INTERNAL_ERROR;
		}

		Stream_Write_UINT32(s, ret->cbDataLen);
		Stream_Write(s, ret->pbData, ret->cbDataLen);
		smartcard_pack_write_size_align(smartcard, s, ret->cbDataLen, 4);
	}

	return SCARD_S_SUCCESS;
}
