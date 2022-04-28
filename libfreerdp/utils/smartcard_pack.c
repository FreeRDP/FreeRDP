/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Smart Card Structure Packing
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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

#include <freerdp/config.h>

#include <winpr/crt.h>
#include <winpr/print.h>

#include <freerdp/channels/scard.h>
#include <freerdp/utils/smartcard_pack.h>

#include <freerdp/log.h>
#define TAG FREERDP_TAG("scard.pack")

static const DWORD g_LogLevel = WLOG_DEBUG;

#define smartcard_unpack_redir_scard_context(s, context, index) \
	smartcard_unpack_redir_scard_context_((s), (context), (index), __FILE__, __FUNCTION__, __LINE__)
#define smartcard_unpack_redir_scard_handle(s, context, index) \
	smartcard_unpack_redir_scard_handle_((s), (context), (index), __FILE__, __FUNCTION__, __LINE__)

static LONG smartcard_unpack_redir_scard_context_(wStream* s, REDIR_SCARDCONTEXT* context,
                                                  UINT32* index, const char* file,
                                                  const char* function, int line);
static LONG smartcard_pack_redir_scard_context(wStream* s, const REDIR_SCARDCONTEXT* context,
                                               DWORD* index);
static LONG smartcard_unpack_redir_scard_handle_(wStream* s, REDIR_SCARDHANDLE* handle,
                                                 UINT32* index, const char* file,
                                                 const char* function, int line);
static LONG smartcard_pack_redir_scard_handle(wStream* s, const REDIR_SCARDHANDLE* handle,
                                              DWORD* index);
static LONG smartcard_unpack_redir_scard_context_ref(wStream* s, REDIR_SCARDCONTEXT* context);
static LONG smartcard_pack_redir_scard_context_ref(wStream* s, const REDIR_SCARDCONTEXT* context);

static LONG smartcard_unpack_redir_scard_handle_ref(wStream* s, REDIR_SCARDHANDLE* handle);
static LONG smartcard_pack_redir_scard_handle_ref(wStream* s, const REDIR_SCARDHANDLE* handle);

typedef enum
{
	NDR_PTR_FULL,
	NDR_PTR_SIMPLE,
	NDR_PTR_FIXED
} ndr_ptr_t;

/* Reads a NDR pointer and checks if the value read has the expected relative
 * addressing */
#define smartcard_ndr_pointer_read(s, index, ptr) \
	smartcard_ndr_pointer_read_((s), (index), (ptr), __FILE__, __FUNCTION__, __LINE__)
static BOOL smartcard_ndr_pointer_read_(wStream* s, UINT32* index, UINT32* ptr, const char* file,
                                        const char* fkt, int line)
{
	const UINT32 expect = 0x20000 + (*index) * 4;
	UINT32 ndrPtr;
	WINPR_UNUSED(file);
	if (!s)
		return FALSE;
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT32(s, ndrPtr); /* mszGroupsNdrPtr (4 bytes) */
	if (ptr)
		*ptr = ndrPtr;
	if (expect != ndrPtr)
	{
		/* Allow NULL pointer if we read the result */
		if (ptr && (ndrPtr == 0))
			return TRUE;
		WLog_WARN(TAG, "[%s:%d] Read context pointer 0x%08" PRIx32 ", expected 0x%08" PRIx32, fkt,
		          line, ndrPtr, expect);
		return FALSE;
	}

	(*index) = (*index) + 1;
	return TRUE;
}

static LONG smartcard_ndr_read(wStream* s, BYTE** data, size_t min, size_t elementSize,
                               ndr_ptr_t type)
{
	size_t len = 0, offset, len2;
	void* r;
	size_t required = 0;

	switch (type)
	{
		case NDR_PTR_FULL:
			required = 12;
			break;
		case NDR_PTR_SIMPLE:
			required = 4;
			break;
		case NDR_PTR_FIXED:
			required = min;
			break;
	}

	if (!Stream_CheckAndLogRequiredLength(TAG, s, required))
		return STATUS_BUFFER_TOO_SMALL;

	switch (type)
	{
		case NDR_PTR_FULL:
			Stream_Read_UINT32(s, len);
			Stream_Read_UINT32(s, offset);
			Stream_Read_UINT32(s, len2);
			if (len != offset + len2)
			{
				WLog_ERR(TAG,
				         "Invalid data when reading full NDR pointer: total=%" PRIu32
				         ", offset=%" PRIu32 ", remaining=%" PRIu32,
				         len, offset, len2);
				return STATUS_BUFFER_TOO_SMALL;
			}
			break;
		case NDR_PTR_SIMPLE:
			Stream_Read_UINT32(s, len);

			if ((len != min) && (min > 0))
			{
				WLog_ERR(TAG,
				         "Invalid data when reading simple NDR pointer: total=%" PRIu32
				         ", expected=%" PRIu32,
				         len, min);
				return STATUS_BUFFER_TOO_SMALL;
			}
			break;
		case NDR_PTR_FIXED:
			len = (UINT32)min;
			break;
	}

	if (min > len)
	{
		WLog_ERR(TAG, "Invalid length read from NDR pointer, minimum %" PRIu32 ", got %" PRIu32,
		         min, len);
		return STATUS_DATA_ERROR;
	}

	if (len > SIZE_MAX / 2)
		return STATUS_BUFFER_TOO_SMALL;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, len * elementSize * 1ull))
		return STATUS_BUFFER_TOO_SMALL;

	len *= elementSize;

	r = calloc(len + 1, sizeof(CHAR));
	if (!r)
		return SCARD_E_NO_MEMORY;
	Stream_Read(s, r, len);
	smartcard_unpack_read_size_align(s, len, 4);
	*data = r;
	return STATUS_SUCCESS;
}

static BOOL smartcard_ndr_pointer_write(wStream* s, UINT32* index, DWORD length)
{
	const UINT32 ndrPtr = 0x20000 + (*index) * 4;

	if (!s)
		return FALSE;
	if (!Stream_EnsureRemainingCapacity(s, 4))
		return FALSE;

	if (length > 0)
	{
		Stream_Write_UINT32(s, ndrPtr); /* mszGroupsNdrPtr (4 bytes) */
		(*index) = (*index) + 1;
	}
	else
		Stream_Write_UINT32(s, 0);
	return TRUE;
}

static LONG smartcard_ndr_write(wStream* s, const BYTE* data, UINT32 size, UINT32 elementSize,
                                ndr_ptr_t type)
{
	const UINT32 offset = 0;
	const UINT32 len = size;
	const UINT32 dataLen = size * elementSize;
	size_t required;

	if (size == 0)
		return SCARD_S_SUCCESS;

	switch (type)
	{
		case NDR_PTR_FULL:
			required = 12;
			break;
		case NDR_PTR_SIMPLE:
			required = 4;
			break;
		case NDR_PTR_FIXED:
			required = 0;
			break;
	}

	if (!Stream_EnsureRemainingCapacity(s, required + dataLen + 4))
		return STATUS_BUFFER_TOO_SMALL;

	switch (type)
	{
		case NDR_PTR_FULL:
			Stream_Write_UINT32(s, len);
			Stream_Write_UINT32(s, offset);
			Stream_Write_UINT32(s, len);
			break;
		case NDR_PTR_SIMPLE:
			Stream_Write_UINT32(s, len);
			break;
		case NDR_PTR_FIXED:
			break;
	}

	if (data)
		Stream_Write(s, data, dataLen);
	else
		Stream_Zero(s, dataLen);
	return smartcard_pack_write_size_align(s, len, 4);
}

static LONG smartcard_ndr_write_state(wStream* s, const ReaderState_Return* data, UINT32 size,
                                      ndr_ptr_t type)
{
	union
	{
		const ReaderState_Return* reader;
		const BYTE* data;
	} cnv;

	cnv.reader = data;
	return smartcard_ndr_write(s, cnv.data, size, sizeof(ReaderState_Return), type);
}

static LONG smartcard_ndr_read_atrmask(wStream* s, LocateCards_ATRMask** data, size_t min,
                                       ndr_ptr_t type)
{
	union
	{
		LocateCards_ATRMask** ppc;
		BYTE** ppv;
	} u;
	u.ppc = data;
	return smartcard_ndr_read(s, u.ppv, min, sizeof(LocateCards_ATRMask), type);
}

static LONG smartcard_ndr_read_fixed_string_a(wStream* s, CHAR** data, size_t min, ndr_ptr_t type)
{
	union
	{
		CHAR** ppc;
		BYTE** ppv;
	} u;
	u.ppc = data;
	return smartcard_ndr_read(s, u.ppv, min, sizeof(CHAR), type);
}

static LONG smartcard_ndr_read_fixed_string_w(wStream* s, WCHAR** data, size_t min, ndr_ptr_t type)
{
	union
	{
		WCHAR** ppc;
		BYTE** ppv;
	} u;
	u.ppc = data;
	return smartcard_ndr_read(s, u.ppv, min, sizeof(WCHAR), type);
}

static LONG smartcard_ndr_read_a(wStream* s, CHAR** data, ndr_ptr_t type)
{
	union
	{
		CHAR** ppc;
		BYTE** ppv;
	} u;
	u.ppc = data;
	return smartcard_ndr_read(s, u.ppv, 0, sizeof(CHAR), type);
}

static LONG smartcard_ndr_read_w(wStream* s, WCHAR** data, ndr_ptr_t type)
{
	union
	{
		WCHAR** ppc;
		BYTE** ppv;
	} u;
	u.ppc = data;
	return smartcard_ndr_read(s, u.ppv, 0, sizeof(WCHAR), type);
}

static LONG smartcard_ndr_read_u(wStream* s, UUID** data)
{
	union
	{
		UUID** ppc;
		BYTE** ppv;
	} u;
	u.ppc = data;
	return smartcard_ndr_read(s, u.ppv, 1, sizeof(UUID), NDR_PTR_FIXED);
}

static char* smartcard_convert_string_list(const void* in, size_t bytes, BOOL unicode)
{
	size_t index, length;
	union
	{
		const void* pv;
		const char* sz;
		const WCHAR* wz;
	} string;
	char* mszA = NULL;

	string.pv = in;

	if (bytes < 1)
		return NULL;

	if (in == NULL)
		return NULL;

	if (unicode)
	{
		length = (bytes / sizeof(WCHAR)) - 1;
		WINPR_ASSERT(length < INT_MAX);

		mszA = (char*)calloc(length + 1, sizeof(char));
		if (!mszA)
			return NULL;
		if (ConvertFromUnicode(CP_UTF8, 0, string.wz, (int)length, &mszA, (int)length + 1, NULL,
		                       NULL) != (int)length)
		{
			free(mszA);
			return NULL;
		}
	}
	else
	{
		length = bytes;
		mszA = (char*)calloc(length, sizeof(char));
		if (!mszA)
			return NULL;
		CopyMemory(mszA, string.sz, length - 1);
		mszA[length - 1] = '\0';
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
	char* sz = NULL;
	ConvertFromUnicode(CP_UTF8, 0, msz, (int)len, &sz, 0, NULL, NULL);
	smartcard_msz_dump_a(sz, len, buffer, bufferLen);
	free(sz);
	return buffer;
}

static char* smartcard_array_dump(const void* pd, size_t len, char* buffer, size_t bufferLen)
{
	const BYTE* data = pd;
	size_t x;
	int rc;
	char* start = buffer;

	/* Ensure '\0' termination */
	if (bufferLen > 0)
	{
		buffer[bufferLen - 1] = '\0';
		bufferLen--;
	}

	rc = _snprintf(buffer, bufferLen, "{ ");
	if ((rc < 0) || ((size_t)rc > bufferLen))
		goto fail;
	buffer += rc;
	bufferLen -= (size_t)rc;

	for (x = 0; x < len; x++)
	{
		rc = _snprintf(buffer, bufferLen, "%02X", data[x]);
		if ((rc < 0) || ((size_t)rc > bufferLen))
			goto fail;
		buffer += rc;
		bufferLen -= (size_t)rc;
	}

	rc = _snprintf(buffer, bufferLen, " }");
	if ((rc < 0) || ((size_t)rc > bufferLen))
		goto fail;

fail:
	return start;
}
static void smartcard_log_redir_handle(const char* tag, const REDIR_SCARDHANDLE* pHandle)
{
	char buffer[128];

	WLog_LVL(tag, g_LogLevel, "  hContext: %s",
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
	if (!WLog_IsLevelActive(WLog_Get(TAG), g_LogLevel))
		return;

	WLog_LVL(TAG, g_LogLevel, "%s {", name);
	smartcard_log_context(TAG, phContext);
	WLog_LVL(TAG, g_LogLevel, "  sz=%s", sz);

	WLog_LVL(TAG, g_LogLevel, "}");
}

static void smartcard_trace_context_and_string_call_w(const char* name,
                                                      const REDIR_SCARDCONTEXT* phContext,
                                                      const WCHAR* sz)
{
	char* tmp = NULL;
	if (!WLog_IsLevelActive(WLog_Get(TAG), g_LogLevel))
		return;

	WLog_LVL(TAG, g_LogLevel, "%s {", name);
	smartcard_log_context(TAG, phContext);
	ConvertFromUnicode(CP_UTF8, 0, sz, -1, &tmp, 0, NULL, NULL);
	WLog_LVL(TAG, g_LogLevel, "  sz=%s", tmp);
	free(tmp);

	WLog_LVL(TAG, g_LogLevel, "}");
}

static void smartcard_trace_context_call(const Context_Call* call, const char* name)
{

	if (!WLog_IsLevelActive(WLog_Get(TAG), g_LogLevel))
		return;

	WLog_LVL(TAG, g_LogLevel, "%s_Call {", name);
	smartcard_log_context(TAG, &call->handles.hContext);

	WLog_LVL(TAG, g_LogLevel, "}");
}

static void smartcard_trace_list_reader_groups_call(const ListReaderGroups_Call* call, BOOL unicode)
{

	if (!WLog_IsLevelActive(WLog_Get(TAG), g_LogLevel))
		return;

	WLog_LVL(TAG, g_LogLevel, "ListReaderGroups%S_Call {", unicode ? "W" : "A");
	smartcard_log_context(TAG, &call->handles.hContext);

	WLog_LVL(TAG, g_LogLevel, "fmszGroupsIsNULL: %" PRId32 " cchGroups: 0x%08" PRIx32,
	         call->fmszGroupsIsNULL, call->cchGroups);
	WLog_LVL(TAG, g_LogLevel, "}");
}

static void smartcard_trace_get_status_change_w_call(const GetStatusChangeW_Call* call)
{
	UINT32 index;
	char* szEventState;
	char* szCurrentState;
	LPSCARD_READERSTATEW readerState;

	if (!WLog_IsLevelActive(WLog_Get(TAG), g_LogLevel))
		return;

	WLog_LVL(TAG, g_LogLevel, "GetStatusChangeW_Call {");
	smartcard_log_context(TAG, &call->handles.hContext);

	WLog_LVL(TAG, g_LogLevel, "dwTimeOut: 0x%08" PRIX32 " cReaders: %" PRIu32 "", call->dwTimeOut,
	         call->cReaders);

	for (index = 0; index < call->cReaders; index++)
	{
		char* szReaderA = NULL;
		readerState = &call->rgReaderStates[index];
		ConvertFromUnicode(CP_UTF8, 0, readerState->szReader, -1, &szReaderA, 0, NULL, NULL);
		WLog_LVL(TAG, g_LogLevel, "\t[%" PRIu32 "]: szReader: %s cbAtr: %" PRIu32 "", index,
		         szReaderA, readerState->cbAtr);
		szCurrentState = SCardGetReaderStateString(readerState->dwCurrentState);
		szEventState = SCardGetReaderStateString(readerState->dwEventState);
		WLog_LVL(TAG, g_LogLevel, "\t[%" PRIu32 "]: dwCurrentState: %s (0x%08" PRIX32 ")", index,
		         szCurrentState, readerState->dwCurrentState);
		WLog_LVL(TAG, g_LogLevel, "\t[%" PRIu32 "]: dwEventState: %s (0x%08" PRIX32 ")", index,
		         szEventState, readerState->dwEventState);
		free(szCurrentState);
		free(szEventState);
		free(szReaderA);
	}

	WLog_LVL(TAG, g_LogLevel, "}");
}

static void smartcard_trace_list_reader_groups_return(const ListReaderGroups_Return* ret,
                                                      BOOL unicode)
{
	char* mszA = NULL;

	if (!WLog_IsLevelActive(WLog_Get(TAG), g_LogLevel))
		return;

	mszA = smartcard_convert_string_list(ret->msz, ret->cBytes, unicode);

	WLog_LVL(TAG, g_LogLevel, "ListReaderGroups%s_Return {", unicode ? "W" : "A");
	WLog_LVL(TAG, g_LogLevel, "  ReturnCode: %s (0x%08" PRIx32 ")",
	         SCardGetErrorString(ret->ReturnCode), ret->ReturnCode);
	WLog_LVL(TAG, g_LogLevel, "  cBytes: %" PRIu32 " msz: %s", ret->cBytes, mszA);
	WLog_LVL(TAG, g_LogLevel, "}");
	free(mszA);
}

static void smartcard_trace_list_readers_call(const ListReaders_Call* call, BOOL unicode)
{
	char* mszGroupsA = NULL;

	if (!WLog_IsLevelActive(WLog_Get(TAG), g_LogLevel))
		return;

	mszGroupsA = smartcard_convert_string_list(call->mszGroups, call->cBytes, unicode);

	WLog_LVL(TAG, g_LogLevel, "ListReaders%s_Call {", unicode ? "W" : "A");
	smartcard_log_context(TAG, &call->handles.hContext);

	WLog_LVL(TAG, g_LogLevel,
	         "cBytes: %" PRIu32 " mszGroups: %s fmszReadersIsNULL: %" PRId32
	         " cchReaders: 0x%08" PRIX32 "",
	         call->cBytes, mszGroupsA, call->fmszReadersIsNULL, call->cchReaders);
	WLog_LVL(TAG, g_LogLevel, "}");

	free(mszGroupsA);
}

static void smartcard_trace_locate_cards_by_atr_a_call(const LocateCardsByATRA_Call* call)
{
	UINT32 index;
	char* szEventState;
	char* szCurrentState;

	if (!WLog_IsLevelActive(WLog_Get(TAG), g_LogLevel))
		return;

	WLog_LVL(TAG, g_LogLevel, "LocateCardsByATRA_Call {");
	smartcard_log_context(TAG, &call->handles.hContext);

	for (index = 0; index < call->cReaders; index++)
	{
		char buffer[1024];
		const LPSCARD_READERSTATEA readerState = &call->rgReaderStates[index];

		WLog_LVL(TAG, g_LogLevel, "\t[%" PRIu32 "]: szReader: %s cbAtr: %" PRIu32 "", index,
		         readerState->szReader, readerState->cbAtr);
		szCurrentState = SCardGetReaderStateString(readerState->dwCurrentState);
		szEventState = SCardGetReaderStateString(readerState->dwEventState);
		WLog_LVL(TAG, g_LogLevel, "\t[%" PRIu32 "]: dwCurrentState: %s (0x%08" PRIX32 ")", index,
		         szCurrentState, readerState->dwCurrentState);
		WLog_LVL(TAG, g_LogLevel, "\t[%" PRIu32 "]: dwEventState: %s (0x%08" PRIX32 ")", index,
		         szEventState, readerState->dwEventState);

		WLog_DBG(
		    TAG, "\t[%" PRIu32 "]: cbAtr: %" PRIu32 " rgbAtr: %s", index, readerState->cbAtr,
		    smartcard_array_dump(readerState->rgbAtr, readerState->cbAtr, buffer, sizeof(buffer)));

		free(szCurrentState);
		free(szEventState);
	}

	WLog_LVL(TAG, g_LogLevel, "}");
}

static void smartcard_trace_locate_cards_a_call(const LocateCardsA_Call* call)
{
	char buffer[8192];

	if (!WLog_IsLevelActive(WLog_Get(TAG), g_LogLevel))
		return;

	WLog_LVL(TAG, g_LogLevel, "LocateCardsA_Call {");
	smartcard_log_context(TAG, &call->handles.hContext);
	WLog_LVL(TAG, g_LogLevel, " cBytes=%" PRId32, call->cBytes);
	WLog_LVL(TAG, g_LogLevel, " mszCards=%s",
	         smartcard_msz_dump_a(call->mszCards, call->cBytes, buffer, sizeof(buffer)));
	WLog_LVL(TAG, g_LogLevel, " cReaders=%" PRId32, call->cReaders);
	// WLog_LVL(TAG, g_LogLevel, " cReaders=%" PRId32, call->rgReaderStates);

	WLog_LVL(TAG, g_LogLevel, "}");
}

static void smartcard_trace_locate_cards_return(const LocateCards_Return* ret)
{

	if (!WLog_IsLevelActive(WLog_Get(TAG), g_LogLevel))
		return;

	WLog_LVL(TAG, g_LogLevel, "LocateCards_Return {");
	WLog_LVL(TAG, g_LogLevel, "  ReturnCode: %s (0x%08" PRIX32 ")",
	         SCardGetErrorString(ret->ReturnCode), ret->ReturnCode);

	if (ret->ReturnCode == SCARD_S_SUCCESS)
	{
		WLog_LVL(TAG, g_LogLevel, "  cReaders=%" PRId32, ret->cReaders);
	}
	WLog_LVL(TAG, g_LogLevel, "}");
}

static void smartcard_trace_get_reader_icon_return(const GetReaderIcon_Return* ret)
{

	if (!WLog_IsLevelActive(WLog_Get(TAG), g_LogLevel))
		return;

	WLog_LVL(TAG, g_LogLevel, "GetReaderIcon_Return {");
	WLog_LVL(TAG, g_LogLevel, "  ReturnCode: %s (0x%08" PRIX32 ")",
	         SCardGetErrorString(ret->ReturnCode), ret->ReturnCode);

	if (ret->ReturnCode == SCARD_S_SUCCESS)
	{
		WLog_LVL(TAG, g_LogLevel, "  cbDataLen=%" PRId32, ret->cbDataLen);
	}
	WLog_LVL(TAG, g_LogLevel, "}");
}

static void smartcard_trace_get_transmit_count_return(const GetTransmitCount_Return* ret)
{

	if (!WLog_IsLevelActive(WLog_Get(TAG), g_LogLevel))
		return;

	WLog_LVL(TAG, g_LogLevel, "GetTransmitCount_Return {");
	WLog_LVL(TAG, g_LogLevel, "  ReturnCode: %s (0x%08" PRIX32 ")",
	         SCardGetErrorString(ret->ReturnCode), ret->ReturnCode);

	WLog_LVL(TAG, g_LogLevel, "  cTransmitCount=%" PRIu32, ret->cTransmitCount);
	WLog_LVL(TAG, g_LogLevel, "}");
}

static void smartcard_trace_read_cache_return(const ReadCache_Return* ret)
{

	if (!WLog_IsLevelActive(WLog_Get(TAG), g_LogLevel))
		return;

	WLog_LVL(TAG, g_LogLevel, "ReadCache_Return {");
	WLog_LVL(TAG, g_LogLevel, "  ReturnCode: %s (0x%08" PRIX32 ")",
	         SCardGetErrorString(ret->ReturnCode), ret->ReturnCode);

	if (ret->ReturnCode == SCARD_S_SUCCESS)
	{
		char buffer[1024];
		WLog_LVL(TAG, g_LogLevel, " cbDataLen=%" PRId32, ret->cbDataLen);
		WLog_LVL(TAG, g_LogLevel, "  cbData: %s",
		         smartcard_array_dump(ret->pbData, ret->cbDataLen, buffer, sizeof(buffer)));
	}
	WLog_LVL(TAG, g_LogLevel, "}");
}

static void smartcard_trace_locate_cards_w_call(const LocateCardsW_Call* call)
{
	char buffer[8192];

	if (!WLog_IsLevelActive(WLog_Get(TAG), g_LogLevel))
		return;

	WLog_LVL(TAG, g_LogLevel, "LocateCardsW_Call {");
	smartcard_log_context(TAG, &call->handles.hContext);
	WLog_LVL(TAG, g_LogLevel, " cBytes=%" PRId32, call->cBytes);
	WLog_LVL(TAG, g_LogLevel, " sz2=%s",
	         smartcard_msz_dump_w(call->mszCards, call->cBytes, buffer, sizeof(buffer)));
	WLog_LVL(TAG, g_LogLevel, " cReaders=%" PRId32, call->cReaders);
	// WLog_LVL(TAG, g_LogLevel, " sz2=%s", call->rgReaderStates);
	WLog_LVL(TAG, g_LogLevel, "}");
}

static void smartcard_trace_list_readers_return(const ListReaders_Return* ret, BOOL unicode)
{
	char* mszA = NULL;

	if (!WLog_IsLevelActive(WLog_Get(TAG), g_LogLevel))
		return;

	WLog_LVL(TAG, g_LogLevel, "ListReaders%s_Return {", unicode ? "W" : "A");
	WLog_LVL(TAG, g_LogLevel, "  ReturnCode: %s (0x%08" PRIX32 ")",
	         SCardGetErrorString(ret->ReturnCode), ret->ReturnCode);

	if (ret->ReturnCode != SCARD_S_SUCCESS)
	{
		WLog_LVL(TAG, g_LogLevel, "}");
		return;
	}

	mszA = smartcard_convert_string_list(ret->msz, ret->cBytes, unicode);

	WLog_LVL(TAG, g_LogLevel, "  cBytes: %" PRIu32 " msz: %s", ret->cBytes, mszA);
	WLog_LVL(TAG, g_LogLevel, "}");
	free(mszA);
}

static void smartcard_trace_get_status_change_return(const GetStatusChange_Return* ret,
                                                     BOOL unicode)
{
	UINT32 index;
	char* szEventState;
	char* szCurrentState;

	if (!WLog_IsLevelActive(WLog_Get(TAG), g_LogLevel))
		return;

	WLog_LVL(TAG, g_LogLevel, "GetStatusChange%s_Return {", unicode ? "W" : "A");
	WLog_LVL(TAG, g_LogLevel, "  ReturnCode: %s (0x%08" PRIX32 ")",
	         SCardGetErrorString(ret->ReturnCode), ret->ReturnCode);
	WLog_LVL(TAG, g_LogLevel, "  cReaders: %" PRIu32 "", ret->cReaders);

	for (index = 0; index < ret->cReaders; index++)
	{
		char buffer[1024];
		const ReaderState_Return* rgReaderState = &(ret->rgReaderStates[index]);
		szCurrentState = SCardGetReaderStateString(rgReaderState->dwCurrentState);
		szEventState = SCardGetReaderStateString(rgReaderState->dwEventState);
		WLog_LVL(TAG, g_LogLevel, "    [%" PRIu32 "]: dwCurrentState: %s (0x%08" PRIX32 ")", index,
		         szCurrentState, rgReaderState->dwCurrentState);
		WLog_LVL(TAG, g_LogLevel, "    [%" PRIu32 "]: dwEventState: %s (0x%08" PRIX32 ")", index,
		         szEventState, rgReaderState->dwEventState);
		WLog_LVL(TAG, g_LogLevel, "    [%" PRIu32 "]: cbAtr: %" PRIu32 " rgbAtr: %s", index,
		         rgReaderState->cbAtr,
		         smartcard_array_dump(rgReaderState->rgbAtr, rgReaderState->cbAtr, buffer,
		                              sizeof(buffer)));
		free(szCurrentState);
		free(szEventState);
	}

	WLog_LVL(TAG, g_LogLevel, "}");
}

static void smartcard_trace_context_and_two_strings_a_call(const ContextAndTwoStringA_Call* call)
{
	if (!WLog_IsLevelActive(WLog_Get(TAG), g_LogLevel))
		return;

	WLog_LVL(TAG, g_LogLevel, "ContextAndTwoStringW_Call {");
	smartcard_log_context(TAG, &call->handles.hContext);
	WLog_LVL(TAG, g_LogLevel, " sz1=%s", call->sz1);
	WLog_LVL(TAG, g_LogLevel, " sz2=%s", call->sz2);
	WLog_LVL(TAG, g_LogLevel, "}");
}

static void smartcard_trace_context_and_two_strings_w_call(const ContextAndTwoStringW_Call* call)
{
	CHAR* sz1 = NULL;
	CHAR* sz2 = NULL;

	if (!WLog_IsLevelActive(WLog_Get(TAG), g_LogLevel))
		return;

	WLog_LVL(TAG, g_LogLevel, "ContextAndTwoStringW_Call {");
	smartcard_log_context(TAG, &call->handles.hContext);
	ConvertFromUnicode(CP_UTF8, 0, call->sz1, -1, &sz1, 0, NULL, NULL);
	ConvertFromUnicode(CP_UTF8, 0, call->sz2, -1, &sz2, 0, NULL, NULL);
	WLog_LVL(TAG, g_LogLevel, " sz1=%s", sz1);
	WLog_LVL(TAG, g_LogLevel, " sz2=%s", sz2);
	free(sz1);
	free(sz2);

	WLog_LVL(TAG, g_LogLevel, "}");
}

static void smartcard_trace_get_transmit_count_call(const GetTransmitCount_Call* call)
{
	if (!WLog_IsLevelActive(WLog_Get(TAG), g_LogLevel))
		return;

	WLog_LVL(TAG, g_LogLevel, "GetTransmitCount_Call {");
	smartcard_log_context(TAG, &call->handles.hContext);
	smartcard_log_redir_handle(TAG, &call->handles.hCard);

	WLog_LVL(TAG, g_LogLevel, "}");
}

static void smartcard_trace_write_cache_a_call(const WriteCacheA_Call* call)
{
	char buffer[1024];

	if (!WLog_IsLevelActive(WLog_Get(TAG), g_LogLevel))
		return;

	WLog_LVL(TAG, g_LogLevel, "GetTransmitCount_Call {");

	WLog_LVL(TAG, g_LogLevel, "  szLookupName=%s", call->szLookupName);

	smartcard_log_context(TAG, &call->Common.handles.hContext);
	WLog_DBG(
	    TAG, "..CardIdentifier=%s",
	    smartcard_array_dump(call->Common.CardIdentifier, sizeof(UUID), buffer, sizeof(buffer)));
	WLog_LVL(TAG, g_LogLevel, "  FreshnessCounter=%" PRIu32, call->Common.FreshnessCounter);
	WLog_LVL(TAG, g_LogLevel, "  cbDataLen=%" PRIu32, call->Common.cbDataLen);
	WLog_DBG(
	    TAG, "  pbData=%s",
	    smartcard_array_dump(call->Common.pbData, call->Common.cbDataLen, buffer, sizeof(buffer)));
	WLog_LVL(TAG, g_LogLevel, "}");
}

static void smartcard_trace_write_cache_w_call(const WriteCacheW_Call* call)
{
	char* tmp = NULL;
	char buffer[1024];

	if (!WLog_IsLevelActive(WLog_Get(TAG), g_LogLevel))
		return;

	WLog_LVL(TAG, g_LogLevel, "GetTransmitCount_Call {");

	ConvertFromUnicode(CP_UTF8, 0, call->szLookupName, -1, &tmp, 0, NULL, NULL);
	WLog_LVL(TAG, g_LogLevel, "  szLookupName=%s", tmp);
	free(tmp);
	smartcard_log_context(TAG, &call->Common.handles.hContext);
	WLog_DBG(
	    TAG, "..CardIdentifier=%s",
	    smartcard_array_dump(call->Common.CardIdentifier, sizeof(UUID), buffer, sizeof(buffer)));
	WLog_LVL(TAG, g_LogLevel, "  FreshnessCounter=%" PRIu32, call->Common.FreshnessCounter);
	WLog_LVL(TAG, g_LogLevel, "  cbDataLen=%" PRIu32, call->Common.cbDataLen);
	WLog_DBG(
	    TAG, "  pbData=%s",
	    smartcard_array_dump(call->Common.pbData, call->Common.cbDataLen, buffer, sizeof(buffer)));
	WLog_LVL(TAG, g_LogLevel, "}");
}

static void smartcard_trace_read_cache_a_call(const ReadCacheA_Call* call)
{
	char buffer[1024];

	if (!WLog_IsLevelActive(WLog_Get(TAG), g_LogLevel))
		return;

	WLog_LVL(TAG, g_LogLevel, "GetTransmitCount_Call {");

	WLog_LVL(TAG, g_LogLevel, "  szLookupName=%s", call->szLookupName);
	smartcard_log_context(TAG, &call->Common.handles.hContext);
	WLog_DBG(
	    TAG, "..CardIdentifier=%s",
	    smartcard_array_dump(call->Common.CardIdentifier, sizeof(UUID), buffer, sizeof(buffer)));
	WLog_LVL(TAG, g_LogLevel, "  FreshnessCounter=%" PRIu32, call->Common.FreshnessCounter);
	WLog_LVL(TAG, g_LogLevel, "  fPbDataIsNULL=%" PRId32, call->Common.fPbDataIsNULL);
	WLog_LVL(TAG, g_LogLevel, "  cbDataLen=%" PRIu32, call->Common.cbDataLen);

	WLog_LVL(TAG, g_LogLevel, "}");
}

static void smartcard_trace_read_cache_w_call(const ReadCacheW_Call* call)
{
	char* tmp = NULL;
	char buffer[1024];

	if (!WLog_IsLevelActive(WLog_Get(TAG), g_LogLevel))
		return;

	WLog_LVL(TAG, g_LogLevel, "GetTransmitCount_Call {");

	ConvertFromUnicode(CP_UTF8, 0, call->szLookupName, -1, &tmp, 0, NULL, NULL);
	WLog_LVL(TAG, g_LogLevel, "  szLookupName=%s", tmp);
	free(tmp);
	smartcard_log_context(TAG, &call->Common.handles.hContext);
	WLog_DBG(
	    TAG, "..CardIdentifier=%s",
	    smartcard_array_dump(call->Common.CardIdentifier, sizeof(UUID), buffer, sizeof(buffer)));
	WLog_LVL(TAG, g_LogLevel, "  FreshnessCounter=%" PRIu32, call->Common.FreshnessCounter);
	WLog_LVL(TAG, g_LogLevel, "  fPbDataIsNULL=%" PRId32, call->Common.fPbDataIsNULL);
	WLog_LVL(TAG, g_LogLevel, "  cbDataLen=%" PRIu32, call->Common.cbDataLen);

	WLog_LVL(TAG, g_LogLevel, "}");
}

static void smartcard_trace_transmit_call(const Transmit_Call* call)
{
	UINT32 cbExtraBytes;
	BYTE* pbExtraBytes;

	if (!WLog_IsLevelActive(WLog_Get(TAG), g_LogLevel))
		return;

	WLog_LVL(TAG, g_LogLevel, "Transmit_Call {");
	smartcard_log_context(TAG, &call->handles.hContext);
	smartcard_log_redir_handle(TAG, &call->handles.hCard);

	if (call->pioSendPci)
	{
		cbExtraBytes = (UINT32)(call->pioSendPci->cbPciLength - sizeof(SCARD_IO_REQUEST));
		pbExtraBytes = &((BYTE*)call->pioSendPci)[sizeof(SCARD_IO_REQUEST)];
		WLog_LVL(TAG, g_LogLevel, "pioSendPci: dwProtocol: %" PRIu32 " cbExtraBytes: %" PRIu32 "",
		         call->pioSendPci->dwProtocol, cbExtraBytes);

		if (cbExtraBytes)
		{
			char buffer[1024];
			WLog_LVL(TAG, g_LogLevel, "pbExtraBytes: %s",
			         smartcard_array_dump(pbExtraBytes, cbExtraBytes, buffer, sizeof(buffer)));
		}
	}
	else
	{
		WLog_LVL(TAG, g_LogLevel, "pioSendPci: null");
	}

	WLog_LVL(TAG, g_LogLevel, "cbSendLength: %" PRIu32 "", call->cbSendLength);

	if (call->pbSendBuffer)
	{
		char buffer[1024];
		WLog_DBG(
		    TAG, "pbSendBuffer: %s",
		    smartcard_array_dump(call->pbSendBuffer, call->cbSendLength, buffer, sizeof(buffer)));
	}
	else
	{
		WLog_LVL(TAG, g_LogLevel, "pbSendBuffer: null");
	}

	if (call->pioRecvPci)
	{
		cbExtraBytes = (UINT32)(call->pioRecvPci->cbPciLength - sizeof(SCARD_IO_REQUEST));
		pbExtraBytes = &((BYTE*)call->pioRecvPci)[sizeof(SCARD_IO_REQUEST)];
		WLog_LVL(TAG, g_LogLevel, "pioRecvPci: dwProtocol: %" PRIu32 " cbExtraBytes: %" PRIu32 "",
		         call->pioRecvPci->dwProtocol, cbExtraBytes);

		if (cbExtraBytes)
		{
			char buffer[1024];
			WLog_LVL(TAG, g_LogLevel, "pbExtraBytes: %s",
			         smartcard_array_dump(pbExtraBytes, cbExtraBytes, buffer, sizeof(buffer)));
		}
	}
	else
	{
		WLog_LVL(TAG, g_LogLevel, "pioRecvPci: null");
	}

	WLog_LVL(TAG, g_LogLevel, "fpbRecvBufferIsNULL: %" PRId32 " cbRecvLength: %" PRIu32 "",
	         call->fpbRecvBufferIsNULL, call->cbRecvLength);
	WLog_LVL(TAG, g_LogLevel, "}");
}

static void smartcard_trace_locate_cards_by_atr_w_call(const LocateCardsByATRW_Call* call)
{
	UINT32 index;
	char* szEventState;
	char* szCurrentState;

	if (!WLog_IsLevelActive(WLog_Get(TAG), g_LogLevel))
		return;

	WLog_LVL(TAG, g_LogLevel, "LocateCardsByATRW_Call {");
	smartcard_log_context(TAG, &call->handles.hContext);

	for (index = 0; index < call->cReaders; index++)
	{
		char buffer[1024];
		char* tmp = NULL;
		const LPSCARD_READERSTATEW readerState =
		    (const LPSCARD_READERSTATEW)&call->rgReaderStates[index];

		ConvertFromUnicode(CP_UTF8, 0, readerState->szReader, -1, &tmp, 0, NULL, NULL);
		WLog_LVL(TAG, g_LogLevel, "\t[%" PRIu32 "]: szReader: %s cbAtr: %" PRIu32 "", index, tmp,
		         readerState->cbAtr);
		szCurrentState = SCardGetReaderStateString(readerState->dwCurrentState);
		szEventState = SCardGetReaderStateString(readerState->dwEventState);
		WLog_LVL(TAG, g_LogLevel, "\t[%" PRIu32 "]: dwCurrentState: %s (0x%08" PRIX32 ")", index,
		         szCurrentState, readerState->dwCurrentState);
		WLog_LVL(TAG, g_LogLevel, "\t[%" PRIu32 "]: dwEventState: %s (0x%08" PRIX32 ")", index,
		         szEventState, readerState->dwEventState);

		WLog_DBG(
		    TAG, "\t[%" PRIu32 "]: cbAtr: %" PRIu32 " rgbAtr: %s", index, readerState->cbAtr,
		    smartcard_array_dump(readerState->rgbAtr, readerState->cbAtr, buffer, sizeof(buffer)));

		free(szCurrentState);
		free(szEventState);
		free(tmp);
	}

	WLog_LVL(TAG, g_LogLevel, "}");
}

static void smartcard_trace_transmit_return(const Transmit_Return* ret)
{
	UINT32 cbExtraBytes;
	BYTE* pbExtraBytes;

	if (!WLog_IsLevelActive(WLog_Get(TAG), g_LogLevel))
		return;

	WLog_LVL(TAG, g_LogLevel, "Transmit_Return {");
	WLog_LVL(TAG, g_LogLevel, "  ReturnCode: %s (0x%08" PRIX32 ")",
	         SCardGetErrorString(ret->ReturnCode), ret->ReturnCode);

	if (ret->pioRecvPci)
	{
		cbExtraBytes = (UINT32)(ret->pioRecvPci->cbPciLength - sizeof(SCARD_IO_REQUEST));
		pbExtraBytes = &((BYTE*)ret->pioRecvPci)[sizeof(SCARD_IO_REQUEST)];
		WLog_LVL(TAG, g_LogLevel, "  pioRecvPci: dwProtocol: %" PRIu32 " cbExtraBytes: %" PRIu32 "",
		         ret->pioRecvPci->dwProtocol, cbExtraBytes);

		if (cbExtraBytes)
		{
			char buffer[1024];
			WLog_LVL(TAG, g_LogLevel, "  pbExtraBytes: %s",
			         smartcard_array_dump(pbExtraBytes, cbExtraBytes, buffer, sizeof(buffer)));
		}
	}
	else
	{
		WLog_LVL(TAG, g_LogLevel, "  pioRecvPci: null");
	}

	WLog_LVL(TAG, g_LogLevel, "  cbRecvLength: %" PRIu32 "", ret->cbRecvLength);

	if (ret->pbRecvBuffer)
	{
		char buffer[1024];
		WLog_DBG(
		    TAG, "  pbRecvBuffer: %s",
		    smartcard_array_dump(ret->pbRecvBuffer, ret->cbRecvLength, buffer, sizeof(buffer)));
	}
	else
	{
		WLog_LVL(TAG, g_LogLevel, "  pbRecvBuffer: null");
	}

	WLog_LVL(TAG, g_LogLevel, "}");
}

static void smartcard_trace_control_return(const Control_Return* ret)
{
	if (!WLog_IsLevelActive(WLog_Get(TAG), g_LogLevel))
		return;

	WLog_LVL(TAG, g_LogLevel, "Control_Return {");
	WLog_LVL(TAG, g_LogLevel, "  ReturnCode: %s (0x%08" PRIX32 ")",
	         SCardGetErrorString(ret->ReturnCode), ret->ReturnCode);
	WLog_LVL(TAG, g_LogLevel, "  cbOutBufferSize: %" PRIu32 "", ret->cbOutBufferSize);

	if (ret->pvOutBuffer)
	{
		char buffer[1024];
		WLog_DBG(
		    TAG, "pvOutBuffer: %s",
		    smartcard_array_dump(ret->pvOutBuffer, ret->cbOutBufferSize, buffer, sizeof(buffer)));
	}
	else
	{
		WLog_LVL(TAG, g_LogLevel, "pvOutBuffer: null");
	}

	WLog_LVL(TAG, g_LogLevel, "}");
}

static void smartcard_trace_control_call(const Control_Call* call)
{
	if (!WLog_IsLevelActive(WLog_Get(TAG), g_LogLevel))
		return;

	WLog_LVL(TAG, g_LogLevel, "Control_Call {");
	smartcard_log_context(TAG, &call->handles.hContext);
	smartcard_log_redir_handle(TAG, &call->handles.hCard);

	WLog_LVL(TAG, g_LogLevel,
	         "dwControlCode: 0x%08" PRIX32 " cbInBufferSize: %" PRIu32
	         " fpvOutBufferIsNULL: %" PRId32 " cbOutBufferSize: %" PRIu32 "",
	         call->dwControlCode, call->cbInBufferSize, call->fpvOutBufferIsNULL,
	         call->cbOutBufferSize);

	if (call->pvInBuffer)
	{
		char buffer[1024];
		WLog_DBG(
		    TAG, "pbInBuffer: %s",
		    smartcard_array_dump(call->pvInBuffer, call->cbInBufferSize, buffer, sizeof(buffer)));
	}
	else
	{
		WLog_LVL(TAG, g_LogLevel, "pvInBuffer: null");
	}

	WLog_LVL(TAG, g_LogLevel, "}");
}

static void smartcard_trace_set_attrib_call(const SetAttrib_Call* call)
{
	char buffer[8192];

	if (!WLog_IsLevelActive(WLog_Get(TAG), g_LogLevel))
		return;

	WLog_LVL(TAG, g_LogLevel, "GetAttrib_Call {");
	smartcard_log_context(TAG, &call->handles.hContext);
	smartcard_log_redir_handle(TAG, &call->handles.hCard);
	WLog_LVL(TAG, g_LogLevel, "dwAttrId: 0x%08" PRIX32, call->dwAttrId);
	WLog_LVL(TAG, g_LogLevel, "cbAttrLen: 0x%08" PRId32, call->cbAttrLen);
	WLog_LVL(TAG, g_LogLevel, "pbAttr: %s",
	         smartcard_array_dump(call->pbAttr, call->cbAttrLen, buffer, sizeof(buffer)));
	WLog_LVL(TAG, g_LogLevel, "}");
}

static void smartcard_trace_get_attrib_return(const GetAttrib_Return* ret, DWORD dwAttrId)
{

	if (!WLog_IsLevelActive(WLog_Get(TAG), g_LogLevel))
		return;

	WLog_LVL(TAG, g_LogLevel, "GetAttrib_Return {");
	WLog_LVL(TAG, g_LogLevel, "  ReturnCode: %s (0x%08" PRIX32 ")",
	         SCardGetErrorString(ret->ReturnCode), ret->ReturnCode);
	WLog_LVL(TAG, g_LogLevel, "  dwAttrId: %s (0x%08" PRIX32 ") cbAttrLen: 0x%08" PRIX32 "",
	         SCardGetAttributeString(dwAttrId), dwAttrId, ret->cbAttrLen);

	if (dwAttrId == SCARD_ATTR_VENDOR_NAME)
	{
		WLog_LVL(TAG, g_LogLevel, "  pbAttr: %.*s", ret->cbAttrLen, (char*)ret->pbAttr);
	}
	else if (dwAttrId == SCARD_ATTR_CURRENT_PROTOCOL_TYPE)
	{
		union
		{
			BYTE* pb;
			DWORD* pd;
		} attr;
		attr.pb = ret->pbAttr;
		WLog_LVL(TAG, g_LogLevel, "  dwProtocolType: %s (0x%08" PRIX32 ")",
		         SCardGetProtocolString(*attr.pd), *attr.pd);
	}

	WLog_LVL(TAG, g_LogLevel, "}");
}

static void smartcard_trace_get_attrib_call(const GetAttrib_Call* call)
{
	if (!WLog_IsLevelActive(WLog_Get(TAG), g_LogLevel))
		return;

	WLog_LVL(TAG, g_LogLevel, "GetAttrib_Call {");
	smartcard_log_context(TAG, &call->handles.hContext);
	smartcard_log_redir_handle(TAG, &call->handles.hCard);

	WLog_LVL(TAG, g_LogLevel,
	         "dwAttrId: %s (0x%08" PRIX32 ") fpbAttrIsNULL: %" PRId32 " cbAttrLen: 0x%08" PRIX32 "",
	         SCardGetAttributeString(call->dwAttrId), call->dwAttrId, call->fpbAttrIsNULL,
	         call->cbAttrLen);
	WLog_LVL(TAG, g_LogLevel, "}");
}

static void smartcard_trace_status_call(const Status_Call* call, BOOL unicode)
{
	if (!WLog_IsLevelActive(WLog_Get(TAG), g_LogLevel))
		return;

	WLog_LVL(TAG, g_LogLevel, "Status%s_Call {", unicode ? "W" : "A");
	smartcard_log_context(TAG, &call->handles.hContext);
	smartcard_log_redir_handle(TAG, &call->handles.hCard);

	WLog_LVL(TAG, g_LogLevel,
	         "fmszReaderNamesIsNULL: %" PRId32 " cchReaderLen: %" PRIu32 " cbAtrLen: %" PRIu32 "",
	         call->fmszReaderNamesIsNULL, call->cchReaderLen, call->cbAtrLen);
	WLog_LVL(TAG, g_LogLevel, "}");
}

static void smartcard_trace_status_return(const Status_Return* ret, BOOL unicode)
{
	char* mszReaderNamesA = NULL;
	char buffer[1024];
	DWORD cBytes;

	if (!WLog_IsLevelActive(WLog_Get(TAG), g_LogLevel))
		return;
	cBytes = ret->cBytes;
	if (ret->ReturnCode != SCARD_S_SUCCESS)
		cBytes = 0;
	if (cBytes == SCARD_AUTOALLOCATE)
		cBytes = 0;
	mszReaderNamesA = smartcard_convert_string_list(ret->mszReaderNames, cBytes, unicode);

	WLog_LVL(TAG, g_LogLevel, "Status%s_Return {", unicode ? "W" : "A");
	WLog_LVL(TAG, g_LogLevel, "  ReturnCode: %s (0x%08" PRIX32 ")",
	         SCardGetErrorString(ret->ReturnCode), ret->ReturnCode);
	WLog_LVL(TAG, g_LogLevel, "  dwState: %s (0x%08" PRIX32 ") dwProtocol: %s (0x%08" PRIX32 ")",
	         SCardGetCardStateString(ret->dwState), ret->dwState,
	         SCardGetProtocolString(ret->dwProtocol), ret->dwProtocol);

	WLog_LVL(TAG, g_LogLevel, "  cBytes: %" PRIu32 " mszReaderNames: %s", ret->cBytes,
	         mszReaderNamesA);

	WLog_LVL(TAG, g_LogLevel, "  cbAtrLen: %" PRIu32 " pbAtr: %s", ret->cbAtrLen,
	         smartcard_array_dump(ret->pbAtr, ret->cbAtrLen, buffer, sizeof(buffer)));
	WLog_LVL(TAG, g_LogLevel, "}");
	free(mszReaderNamesA);
}

static void smartcard_trace_state_return(const State_Return* ret)
{
	char buffer[1024];
	char* state;

	if (!WLog_IsLevelActive(WLog_Get(TAG), g_LogLevel))
		return;

	state = SCardGetReaderStateString(ret->dwState);
	WLog_LVL(TAG, g_LogLevel, "Reconnect_Return {");
	WLog_LVL(TAG, g_LogLevel, "  ReturnCode: %s (0x%08" PRIX32 ")",
	         SCardGetErrorString(ret->ReturnCode), ret->ReturnCode);
	WLog_LVL(TAG, g_LogLevel, "  dwState:    %s (0x%08" PRIX32 ")", state, ret->dwState);
	WLog_LVL(TAG, g_LogLevel, "  dwProtocol: %s (0x%08" PRIX32 ")",
	         SCardGetProtocolString(ret->dwProtocol), ret->dwProtocol);
	WLog_LVL(TAG, g_LogLevel, "  cbAtrLen:      (0x%08" PRIX32 ")", ret->cbAtrLen);
	WLog_LVL(TAG, g_LogLevel, "  rgAtr:      %s",
	         smartcard_array_dump(ret->rgAtr, sizeof(ret->rgAtr), buffer, sizeof(buffer)));
	WLog_LVL(TAG, g_LogLevel, "}");
	free(state);
}

static void smartcard_trace_reconnect_return(const Reconnect_Return* ret)
{

	if (!WLog_IsLevelActive(WLog_Get(TAG), g_LogLevel))
		return;

	WLog_LVL(TAG, g_LogLevel, "Reconnect_Return {");
	WLog_LVL(TAG, g_LogLevel, "  ReturnCode: %s (0x%08" PRIX32 ")",
	         SCardGetErrorString(ret->ReturnCode), ret->ReturnCode);
	WLog_LVL(TAG, g_LogLevel, "  dwActiveProtocol: %s (0x%08" PRIX32 ")",
	         SCardGetProtocolString(ret->dwActiveProtocol), ret->dwActiveProtocol);
	WLog_LVL(TAG, g_LogLevel, "}");
}

static void smartcard_trace_connect_a_call(const ConnectA_Call* call)
{

	if (!WLog_IsLevelActive(WLog_Get(TAG), g_LogLevel))
		return;

	WLog_LVL(TAG, g_LogLevel, "ConnectA_Call {");
	smartcard_log_context(TAG, &call->Common.handles.hContext);

	WLog_LVL(TAG, g_LogLevel,
	         "szReader: %s dwShareMode: %s (0x%08" PRIX32 ") dwPreferredProtocols: %s (0x%08" PRIX32
	         ")",
	         call->szReader, SCardGetShareModeString(call->Common.dwShareMode),
	         call->Common.dwShareMode, SCardGetProtocolString(call->Common.dwPreferredProtocols),
	         call->Common.dwPreferredProtocols);
	WLog_LVL(TAG, g_LogLevel, "}");
}

static void smartcard_trace_connect_w_call(const ConnectW_Call* call)
{
	char* szReaderA = NULL;

	if (!WLog_IsLevelActive(WLog_Get(TAG), g_LogLevel))
		return;

	ConvertFromUnicode(CP_UTF8, 0, call->szReader, -1, &szReaderA, 0, NULL, NULL);
	WLog_LVL(TAG, g_LogLevel, "ConnectW_Call {");
	smartcard_log_context(TAG, &call->Common.handles.hContext);

	WLog_LVL(TAG, g_LogLevel,
	         "szReader: %s dwShareMode: %s (0x%08" PRIX32 ") dwPreferredProtocols: %s (0x%08" PRIX32
	         ")",
	         szReaderA, SCardGetShareModeString(call->Common.dwShareMode), call->Common.dwShareMode,
	         SCardGetProtocolString(call->Common.dwPreferredProtocols),
	         call->Common.dwPreferredProtocols);
	WLog_LVL(TAG, g_LogLevel, "}");
	free(szReaderA);
}

static void smartcard_trace_hcard_and_disposition_call(const HCardAndDisposition_Call* call,
                                                       const char* name)
{
	if (!WLog_IsLevelActive(WLog_Get(TAG), g_LogLevel))
		return;

	WLog_LVL(TAG, g_LogLevel, "%s_Call {", name);
	smartcard_log_context(TAG, &call->handles.hContext);
	smartcard_log_redir_handle(TAG, &call->handles.hCard);

	WLog_LVL(TAG, g_LogLevel, "dwDisposition: %s (0x%08" PRIX32 ")",
	         SCardGetDispositionString(call->dwDisposition), call->dwDisposition);
	WLog_LVL(TAG, g_LogLevel, "}");
}

static void smartcard_trace_establish_context_call(const EstablishContext_Call* call)
{

	if (!WLog_IsLevelActive(WLog_Get(TAG), g_LogLevel))
		return;

	WLog_LVL(TAG, g_LogLevel, "EstablishContext_Call {");
	WLog_LVL(TAG, g_LogLevel, "dwScope: %s (0x%08" PRIX32 ")", SCardGetScopeString(call->dwScope),
	         call->dwScope);
	WLog_LVL(TAG, g_LogLevel, "}");
}

static void smartcard_trace_establish_context_return(const EstablishContext_Return* ret)
{

	if (!WLog_IsLevelActive(WLog_Get(TAG), g_LogLevel))
		return;

	WLog_LVL(TAG, g_LogLevel, "EstablishContext_Return {");
	WLog_LVL(TAG, g_LogLevel, "  ReturnCode: %s (0x%08" PRIX32 ")",
	         SCardGetErrorString(ret->ReturnCode), ret->ReturnCode);
	smartcard_log_context(TAG, &ret->hContext);

	WLog_LVL(TAG, g_LogLevel, "}");
}

void smartcard_trace_long_return(const Long_Return* ret, const char* name)
{
	if (!WLog_IsLevelActive(WLog_Get(TAG), g_LogLevel))
		return;

	WLog_LVL(TAG, g_LogLevel, "%s_Return {", name);
	WLog_LVL(TAG, g_LogLevel, "  ReturnCode: %s (0x%08" PRIX32 ")",
	         SCardGetErrorString(ret->ReturnCode), ret->ReturnCode);
	WLog_LVL(TAG, g_LogLevel, "}");
}

static void smartcard_trace_connect_return(const Connect_Return* ret)
{
	if (!WLog_IsLevelActive(WLog_Get(TAG), g_LogLevel))
		return;

	WLog_LVL(TAG, g_LogLevel, "Connect_Return {");
	WLog_LVL(TAG, g_LogLevel, "  ReturnCode: %s (0x%08" PRIX32 ")",
	         SCardGetErrorString(ret->ReturnCode), ret->ReturnCode);
	smartcard_log_context(TAG, &ret->hContext);
	smartcard_log_redir_handle(TAG, &ret->hCard);

	WLog_LVL(TAG, g_LogLevel, "  dwActiveProtocol: %s (0x%08" PRIX32 ")",
	         SCardGetProtocolString(ret->dwActiveProtocol), ret->dwActiveProtocol);
	WLog_LVL(TAG, g_LogLevel, "}");
}

static void smartcard_trace_reconnect_call(const Reconnect_Call* call)
{
	if (!WLog_IsLevelActive(WLog_Get(TAG), g_LogLevel))
		return;

	WLog_LVL(TAG, g_LogLevel, "Reconnect_Call {");
	smartcard_log_context(TAG, &call->handles.hContext);
	smartcard_log_redir_handle(TAG, &call->handles.hCard);

	WLog_LVL(TAG, g_LogLevel,
	         "dwShareMode: %s (0x%08" PRIX32 ") dwPreferredProtocols: %s (0x%08" PRIX32
	         ") dwInitialization: %s (0x%08" PRIX32 ")",
	         SCardGetShareModeString(call->dwShareMode), call->dwShareMode,
	         SCardGetProtocolString(call->dwPreferredProtocols), call->dwPreferredProtocols,
	         SCardGetDispositionString(call->dwInitialization), call->dwInitialization);
	WLog_LVL(TAG, g_LogLevel, "}");
}

static void smartcard_trace_device_type_id_return(const GetDeviceTypeId_Return* ret)
{
	if (!WLog_IsLevelActive(WLog_Get(TAG), g_LogLevel))
		return;

	WLog_LVL(TAG, g_LogLevel, "GetDeviceTypeId_Return {");
	WLog_LVL(TAG, g_LogLevel, "  ReturnCode: %s (0x%08" PRIX32 ")",
	         SCardGetErrorString(ret->ReturnCode), ret->ReturnCode);
	WLog_LVL(TAG, g_LogLevel, "  dwDeviceId=%08" PRIx32, ret->dwDeviceId);

	WLog_LVL(TAG, g_LogLevel, "}");
}

static LONG smartcard_unpack_common_context_and_string_a(wStream* s, REDIR_SCARDCONTEXT* phContext,
                                                         CHAR** pszReaderName)
{
	LONG status;
	UINT32 index = 0;
	status = smartcard_unpack_redir_scard_context(s, phContext, &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!smartcard_ndr_pointer_read(s, &index, NULL))
		return ERROR_INVALID_DATA;

	status = smartcard_unpack_redir_scard_context_ref(s, phContext);
	if (status != SCARD_S_SUCCESS)
		return status;

	status = smartcard_ndr_read_a(s, pszReaderName, NDR_PTR_FULL);
	if (status != SCARD_S_SUCCESS)
		return status;

	smartcard_trace_context_and_string_call_a(__FUNCTION__, phContext, *pszReaderName);
	return SCARD_S_SUCCESS;
}

static LONG smartcard_unpack_common_context_and_string_w(wStream* s, REDIR_SCARDCONTEXT* phContext,
                                                         WCHAR** pszReaderName)
{
	LONG status;
	UINT32 index = 0;

	status = smartcard_unpack_redir_scard_context(s, phContext, &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!smartcard_ndr_pointer_read(s, &index, NULL))
		return ERROR_INVALID_DATA;

	status = smartcard_unpack_redir_scard_context_ref(s, phContext);
	if (status != SCARD_S_SUCCESS)
		return status;

	status = smartcard_ndr_read_w(s, pszReaderName, NDR_PTR_FULL);
	if (status != SCARD_S_SUCCESS)
		return status;

	smartcard_trace_context_and_string_call_w(__FUNCTION__, phContext, *pszReaderName);
	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_common_type_header(wStream* s)
{
	UINT8 version;
	UINT32 filler;
	UINT8 endianness;
	UINT16 commonHeaderLength;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return STATUS_BUFFER_TOO_SMALL;

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

void smartcard_pack_common_type_header(wStream* s)
{
	Stream_Write_UINT8(s, 1);           /* Version (1 byte) */
	Stream_Write_UINT8(s, 0x10);        /* Endianness (1 byte) */
	Stream_Write_UINT16(s, 8);          /* CommonHeaderLength (2 bytes) */
	Stream_Write_UINT32(s, 0xCCCCCCCC); /* Filler (4 bytes), should be 0xCCCCCCCC */
}

LONG smartcard_unpack_private_type_header(wStream* s)
{
	UINT32 filler;
	UINT32 objectBufferLength;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return STATUS_BUFFER_TOO_SMALL;

	Stream_Read_UINT32(s, objectBufferLength); /* ObjectBufferLength (4 bytes) */
	Stream_Read_UINT32(s, filler);             /* Filler (4 bytes), should be 0x00000000 */

	if (filler != 0x00000000)
	{
		WLog_WARN(TAG, "Unexpected PrivateTypeHeader Filler 0x%08" PRIX32 "", filler);
		return STATUS_INVALID_PARAMETER;
	}

	if (!Stream_CheckAndLogRequiredLength(TAG, s, objectBufferLength))
		return STATUS_INVALID_PARAMETER;

	return SCARD_S_SUCCESS;
}

void smartcard_pack_private_type_header(wStream* s, UINT32 objectBufferLength)
{
	Stream_Write_UINT32(s, objectBufferLength); /* ObjectBufferLength (4 bytes) */
	Stream_Write_UINT32(s, 0x00000000);         /* Filler (4 bytes), should be 0x00000000 */
}

LONG smartcard_unpack_read_size_align(wStream* s, size_t size, UINT32 alignment)
{
	size_t pad;

	pad = size;
	size = (size + alignment - 1) & ~(alignment - 1);
	pad = size - pad;

	if (pad)
		Stream_Seek(s, pad);

	return (LONG)pad;
}

LONG smartcard_pack_write_size_align(wStream* s, size_t size, UINT32 alignment)
{
	size_t pad;

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

SCARDCONTEXT smartcard_scard_context_native_from_redir(REDIR_SCARDCONTEXT* context)
{
	SCARDCONTEXT hContext = { 0 };

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

void smartcard_scard_context_native_to_redir(REDIR_SCARDCONTEXT* context, SCARDCONTEXT hContext)
{
	ZeroMemory(context, sizeof(REDIR_SCARDCONTEXT));
	context->cbContext = sizeof(ULONG_PTR);
	CopyMemory(&(context->pbContext), &hContext, context->cbContext);
}

SCARDHANDLE smartcard_scard_handle_native_from_redir(REDIR_SCARDHANDLE* handle)
{
	SCARDHANDLE hCard = 0;

	if (handle->cbHandle == 0)
		return hCard;

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

void smartcard_scard_handle_native_to_redir(REDIR_SCARDHANDLE* handle, SCARDHANDLE hCard)
{
	ZeroMemory(handle, sizeof(REDIR_SCARDHANDLE));
	handle->cbHandle = sizeof(ULONG_PTR);
	CopyMemory(&(handle->pbHandle), &hCard, handle->cbHandle);
}

LONG smartcard_unpack_redir_scard_context_(wStream* s, REDIR_SCARDCONTEXT* context, UINT32* index,
                                           const char* file, const char* function, int line)
{
	UINT32 pbContextNdrPtr;

	WINPR_UNUSED(file);

	ZeroMemory(context, sizeof(REDIR_SCARDCONTEXT));

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return STATUS_BUFFER_TOO_SMALL;

	Stream_Read_UINT32(s, context->cbContext); /* cbContext (4 bytes) */

	if (!Stream_CheckAndLogRequiredLength(TAG, s, context->cbContext))
		return STATUS_BUFFER_TOO_SMALL;

	if ((context->cbContext != 0) && (context->cbContext != 4) && (context->cbContext != 8))
	{
		WLog_WARN(TAG, "REDIR_SCARDCONTEXT length is not 0, 4 or 8: %" PRIu32 "",
		          context->cbContext);
		return STATUS_INVALID_PARAMETER;
	}

	if (!smartcard_ndr_pointer_read_(s, index, &pbContextNdrPtr, file, function, line))
		return ERROR_INVALID_DATA;

	if (((context->cbContext == 0) && pbContextNdrPtr) ||
	    ((context->cbContext != 0) && !pbContextNdrPtr))
	{
		WLog_WARN(TAG,
		          "REDIR_SCARDCONTEXT cbContext (%" PRIu32 ") pbContextNdrPtr (%" PRIu32
		          ") inconsistency",
		          context->cbContext, pbContextNdrPtr);
		return STATUS_INVALID_PARAMETER;
	}

	if (!Stream_CheckAndLogRequiredLength(TAG, s, context->cbContext))
		return STATUS_INVALID_PARAMETER;

	return SCARD_S_SUCCESS;
}

LONG smartcard_pack_redir_scard_context(wStream* s, const REDIR_SCARDCONTEXT* context, DWORD* index)
{
	const UINT32 pbContextNdrPtr = 0x00020000 + *index * 4;

	if (context->cbContext != 0)
	{
		Stream_Write_UINT32(s, context->cbContext); /* cbContext (4 bytes) */
		Stream_Write_UINT32(s, pbContextNdrPtr);    /* pbContextNdrPtr (4 bytes) */
		*index = *index + 1;
	}
	else
		Stream_Zero(s, 8);

	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_redir_scard_context_ref(wStream* s, REDIR_SCARDCONTEXT* context)
{
	UINT32 length;

	if (context->cbContext == 0)
		return SCARD_S_SUCCESS;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return STATUS_BUFFER_TOO_SMALL;

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

	if (!Stream_CheckAndLogRequiredLength(TAG, s, context->cbContext))
		return STATUS_BUFFER_TOO_SMALL;

	if (context->cbContext)
		Stream_Read(s, &(context->pbContext), context->cbContext);
	else
		ZeroMemory(&(context->pbContext), sizeof(context->pbContext));

	return SCARD_S_SUCCESS;
}

LONG smartcard_pack_redir_scard_context_ref(wStream* s, const REDIR_SCARDCONTEXT* context)
{

	Stream_Write_UINT32(s, context->cbContext); /* Length (4 bytes) */

	if (context->cbContext)
	{
		Stream_Write(s, &(context->pbContext), context->cbContext);
	}

	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_redir_scard_handle_(wStream* s, REDIR_SCARDHANDLE* handle, UINT32* index,
                                          const char* file, const char* function, int line)
{
	ZeroMemory(handle, sizeof(REDIR_SCARDHANDLE));

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return STATUS_BUFFER_TOO_SMALL;

	Stream_Read_UINT32(s, handle->cbHandle); /* Length (4 bytes) */

	if (!Stream_CheckAndLogRequiredLength(TAG, s, handle->cbHandle))
		return STATUS_BUFFER_TOO_SMALL;

	if (!smartcard_ndr_pointer_read_(s, index, NULL, file, function, line))
		return ERROR_INVALID_DATA;

	return SCARD_S_SUCCESS;
}

LONG smartcard_pack_redir_scard_handle(wStream* s, const REDIR_SCARDHANDLE* handle, DWORD* index)
{
	const UINT32 pbContextNdrPtr = 0x00020000 + *index * 4;

	if (handle->cbHandle != 0)
	{
		Stream_Write_UINT32(s, handle->cbHandle); /* cbContext (4 bytes) */
		Stream_Write_UINT32(s, pbContextNdrPtr);  /* pbContextNdrPtr (4 bytes) */
		*index = *index + 1;
	}
	else
		Stream_Zero(s, 8);
	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_redir_scard_handle_ref(wStream* s, REDIR_SCARDHANDLE* handle)
{
	UINT32 length;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return STATUS_BUFFER_TOO_SMALL;

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

	if (!Stream_CheckAndLogRequiredLength(TAG, s, handle->cbHandle))
		return STATUS_BUFFER_TOO_SMALL;

	if (handle->cbHandle)
		Stream_Read(s, &(handle->pbHandle), handle->cbHandle);

	return SCARD_S_SUCCESS;
}

LONG smartcard_pack_redir_scard_handle_ref(wStream* s, const REDIR_SCARDHANDLE* handle)
{

	Stream_Write_UINT32(s, handle->cbHandle); /* Length (4 bytes) */

	if (handle->cbHandle)
		Stream_Write(s, &(handle->pbHandle), handle->cbHandle);

	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_establish_context_call(wStream* s, EstablishContext_Call* call)
{
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return STATUS_BUFFER_TOO_SMALL;

	Stream_Read_UINT32(s, call->dwScope); /* dwScope (4 bytes) */
	smartcard_trace_establish_context_call(call);
	return SCARD_S_SUCCESS;
}

LONG smartcard_pack_establish_context_return(wStream* s, const EstablishContext_Return* ret)
{
	LONG status;
	DWORD index = 0;

	smartcard_trace_establish_context_return(ret);
	if (ret->ReturnCode != SCARD_S_SUCCESS)
		return ret->ReturnCode;

	if ((status = smartcard_pack_redir_scard_context(s, &(ret->hContext), &index)))
		return status;

	return smartcard_pack_redir_scard_context_ref(s, &(ret->hContext));
}

LONG smartcard_unpack_context_call(wStream* s, Context_Call* call, const char* name)
{
	LONG status;
	UINT32 index = 0;

	status = smartcard_unpack_redir_scard_context(s, &(call->handles.hContext), &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	if ((status = smartcard_unpack_redir_scard_context_ref(s, &(call->handles.hContext))))
		WLog_ERR(TAG, "smartcard_unpack_redir_scard_context_ref failed with error %" PRId32 "",
		         status);

	smartcard_trace_context_call(call, name);
	return status;
}

LONG smartcard_unpack_list_reader_groups_call(wStream* s, ListReaderGroups_Call* call, BOOL unicode)
{
	LONG status;
	UINT32 index = 0;
	status = smartcard_unpack_redir_scard_context(s, &(call->handles.hContext), &index);

	if (status != SCARD_S_SUCCESS)
		return status;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return STATUS_BUFFER_TOO_SMALL;

	Stream_Read_INT32(s, call->fmszGroupsIsNULL); /* fmszGroupsIsNULL (4 bytes) */
	Stream_Read_UINT32(s, call->cchGroups);       /* cchGroups (4 bytes) */
	status = smartcard_unpack_redir_scard_context_ref(s, &(call->handles.hContext));

	if (status != SCARD_S_SUCCESS)
		return status;

	smartcard_trace_list_reader_groups_call(call, unicode);
	return SCARD_S_SUCCESS;
}

LONG smartcard_pack_list_reader_groups_return(wStream* s, const ListReaderGroups_Return* ret,
                                              BOOL unicode)
{
	LONG status;
	DWORD cBytes = ret->cBytes;
	UINT32 index = 0;

	smartcard_trace_list_reader_groups_return(ret, unicode);
	if (ret->ReturnCode != SCARD_S_SUCCESS)
		cBytes = 0;
	if (cBytes == SCARD_AUTOALLOCATE)
		cBytes = 0;

	if (!Stream_EnsureRemainingCapacity(s, 4))
		return SCARD_E_NO_MEMORY;

	Stream_Write_UINT32(s, cBytes); /* cBytes (4 bytes) */
	if (!smartcard_ndr_pointer_write(s, &index, cBytes))
		return SCARD_E_NO_MEMORY;

	status = smartcard_ndr_write(s, ret->msz, cBytes, 1, NDR_PTR_SIMPLE);
	if (status != SCARD_S_SUCCESS)
		return status;
	return ret->ReturnCode;
}

LONG smartcard_unpack_list_readers_call(wStream* s, ListReaders_Call* call, BOOL unicode)
{
	LONG status;
	UINT32 index = 0;
	UINT32 mszGroupsNdrPtr;
	call->mszGroups = NULL;

	status = smartcard_unpack_redir_scard_context(s, &(call->handles.hContext), &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 16))
		return STATUS_BUFFER_TOO_SMALL;

	Stream_Read_UINT32(s, call->cBytes); /* cBytes (4 bytes) */
	if (!smartcard_ndr_pointer_read(s, &index, &mszGroupsNdrPtr))
		return ERROR_INVALID_DATA;
	Stream_Read_INT32(s, call->fmszReadersIsNULL); /* fmszReadersIsNULL (4 bytes) */
	Stream_Read_UINT32(s, call->cchReaders);       /* cchReaders (4 bytes) */

	if ((status = smartcard_unpack_redir_scard_context_ref(s, &(call->handles.hContext))))
		return status;

	if (mszGroupsNdrPtr)
	{
		status = smartcard_ndr_read(s, &call->mszGroups, call->cBytes, 1, NDR_PTR_SIMPLE);
		if (status != SCARD_S_SUCCESS)
			return status;
	}

	smartcard_trace_list_readers_call(call, unicode);
	return SCARD_S_SUCCESS;
}

LONG smartcard_pack_list_readers_return(wStream* s, const ListReaders_Return* ret, BOOL unicode)
{
	LONG status;
	UINT32 index = 0;
	UINT32 size = ret->cBytes;

	smartcard_trace_list_readers_return(ret, unicode);
	if (ret->ReturnCode != SCARD_S_SUCCESS)
		size = 0;

	if (!Stream_EnsureRemainingCapacity(s, 4))
	{
		WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Write_UINT32(s, size); /* cBytes (4 bytes) */
	if (!smartcard_ndr_pointer_write(s, &index, size))
		return SCARD_E_NO_MEMORY;

	status = smartcard_ndr_write(s, ret->msz, size, 1, NDR_PTR_SIMPLE);
	if (status != SCARD_S_SUCCESS)
		return status;
	return ret->ReturnCode;
}

static LONG smartcard_unpack_connect_common(wStream* s, Connect_Common_Call* common, UINT32* index)
{
	LONG status;

	status = smartcard_unpack_redir_scard_context(s, &(common->handles.hContext), index);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return STATUS_BUFFER_TOO_SMALL;

	Stream_Read_UINT32(s, common->dwShareMode);          /* dwShareMode (4 bytes) */
	Stream_Read_UINT32(s, common->dwPreferredProtocols); /* dwPreferredProtocols (4 bytes) */
	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_connect_a_call(wStream* s, ConnectA_Call* call)
{
	LONG status;
	UINT32 index = 0;
	call->szReader = NULL;

	if (!smartcard_ndr_pointer_read(s, &index, NULL))
		return ERROR_INVALID_DATA;

	if ((status = smartcard_unpack_connect_common(s, &(call->Common), &index)))
	{
		WLog_ERR(TAG, "smartcard_unpack_connect_common failed with error %" PRId32 "", status);
		return status;
	}

	status = smartcard_ndr_read_a(s, &call->szReader, NDR_PTR_FULL);
	if (status != SCARD_S_SUCCESS)
		return status;

	if ((status = smartcard_unpack_redir_scard_context_ref(s, &(call->Common.handles.hContext))))
		WLog_ERR(TAG, "smartcard_unpack_redir_scard_context_ref failed with error %" PRId32 "",
		         status);

	smartcard_trace_connect_a_call(call);
	return status;
}

LONG smartcard_unpack_connect_w_call(wStream* s, ConnectW_Call* call)
{
	LONG status;
	UINT32 index = 0;

	call->szReader = NULL;

	if (!smartcard_ndr_pointer_read(s, &index, NULL))
		return ERROR_INVALID_DATA;

	if ((status = smartcard_unpack_connect_common(s, &(call->Common), &index)))
	{
		WLog_ERR(TAG, "smartcard_unpack_connect_common failed with error %" PRId32 "", status);
		return status;
	}

	status = smartcard_ndr_read_w(s, &call->szReader, NDR_PTR_FULL);
	if (status != SCARD_S_SUCCESS)
		return status;

	if ((status = smartcard_unpack_redir_scard_context_ref(s, &(call->Common.handles.hContext))))
		WLog_ERR(TAG, "smartcard_unpack_redir_scard_context_ref failed with error %" PRId32 "",
		         status);

	smartcard_trace_connect_w_call(call);
	return status;
}

LONG smartcard_pack_connect_return(wStream* s, const Connect_Return* ret)
{
	LONG status;
	DWORD index = 0;

	smartcard_trace_connect_return(ret);

	status = smartcard_pack_redir_scard_context(s, &ret->hContext, &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	status = smartcard_pack_redir_scard_handle(s, &ret->hCard, &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!Stream_EnsureRemainingCapacity(s, 4))
		return SCARD_E_NO_MEMORY;

	Stream_Write_UINT32(s, ret->dwActiveProtocol); /* dwActiveProtocol (4 bytes) */
	status = smartcard_pack_redir_scard_context_ref(s, &ret->hContext);
	if (status != SCARD_S_SUCCESS)
		return status;
	return smartcard_pack_redir_scard_handle_ref(s, &(ret->hCard));
}

LONG smartcard_unpack_reconnect_call(wStream* s, Reconnect_Call* call)
{
	LONG status;
	UINT32 index = 0;

	status = smartcard_unpack_redir_scard_context(s, &(call->handles.hContext), &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	status = smartcard_unpack_redir_scard_handle(s, &(call->handles.hCard), &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 12))
		return STATUS_BUFFER_TOO_SMALL;

	Stream_Read_UINT32(s, call->dwShareMode);          /* dwShareMode (4 bytes) */
	Stream_Read_UINT32(s, call->dwPreferredProtocols); /* dwPreferredProtocols (4 bytes) */
	Stream_Read_UINT32(s, call->dwInitialization);     /* dwInitialization (4 bytes) */

	if ((status = smartcard_unpack_redir_scard_context_ref(s, &(call->handles.hContext))))
	{
		WLog_ERR(TAG, "smartcard_unpack_redir_scard_context_ref failed with error %" PRId32 "",
		         status);
		return status;
	}

	if ((status = smartcard_unpack_redir_scard_handle_ref(s, &(call->handles.hCard))))
		WLog_ERR(TAG, "smartcard_unpack_redir_scard_handle_ref failed with error %" PRId32 "",
		         status);

	smartcard_trace_reconnect_call(call);
	return status;
}

LONG smartcard_pack_reconnect_return(wStream* s, const Reconnect_Return* ret)
{
	smartcard_trace_reconnect_return(ret);

	if (!Stream_EnsureRemainingCapacity(s, 4))
		return SCARD_E_NO_MEMORY;
	Stream_Write_UINT32(s, ret->dwActiveProtocol); /* dwActiveProtocol (4 bytes) */
	return ret->ReturnCode;
}

LONG smartcard_unpack_hcard_and_disposition_call(wStream* s, HCardAndDisposition_Call* call,
                                                 const char* name)
{
	LONG status;
	UINT32 index = 0;

	status = smartcard_unpack_redir_scard_context(s, &(call->handles.hContext), &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	status = smartcard_unpack_redir_scard_handle(s, &(call->handles.hCard), &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return STATUS_BUFFER_TOO_SMALL;

	Stream_Read_UINT32(s, call->dwDisposition); /* dwDisposition (4 bytes) */

	if ((status = smartcard_unpack_redir_scard_context_ref(s, &(call->handles.hContext))))
		return status;

	if ((status = smartcard_unpack_redir_scard_handle_ref(s, &(call->handles.hCard))))
		return status;

	smartcard_trace_hcard_and_disposition_call(call, name);
	return status;
}

static void smartcard_trace_get_status_change_a_call(const GetStatusChangeA_Call* call)
{
	UINT32 index;
	char* szEventState;
	char* szCurrentState;
	LPSCARD_READERSTATEA readerState;

	if (!WLog_IsLevelActive(WLog_Get(TAG), g_LogLevel))
		return;

	WLog_LVL(TAG, g_LogLevel, "GetStatusChangeA_Call {");
	smartcard_log_context(TAG, &call->handles.hContext);

	WLog_LVL(TAG, g_LogLevel, "dwTimeOut: 0x%08" PRIX32 " cReaders: %" PRIu32 "", call->dwTimeOut,
	         call->cReaders);

	for (index = 0; index < call->cReaders; index++)
	{
		readerState = &call->rgReaderStates[index];
		WLog_LVL(TAG, g_LogLevel, "\t[%" PRIu32 "]: szReader: %s cbAtr: %" PRIu32 "", index,
		         readerState->szReader, readerState->cbAtr);
		szCurrentState = SCardGetReaderStateString(readerState->dwCurrentState);
		szEventState = SCardGetReaderStateString(readerState->dwEventState);
		WLog_LVL(TAG, g_LogLevel, "\t[%" PRIu32 "]: dwCurrentState: %s (0x%08" PRIX32 ")", index,
		         szCurrentState, readerState->dwCurrentState);
		WLog_LVL(TAG, g_LogLevel, "\t[%" PRIu32 "]: dwEventState: %s (0x%08" PRIX32 ")", index,
		         szEventState, readerState->dwEventState);
		free(szCurrentState);
		free(szEventState);
	}

	WLog_LVL(TAG, g_LogLevel, "}");
}

static LONG smartcard_unpack_reader_state_a(wStream* s, LPSCARD_READERSTATEA* ppcReaders,
                                            UINT32 cReaders, UINT32* ptrIndex)
{
	UINT32 index, len;
	LONG status = SCARD_E_NO_MEMORY;
	LPSCARD_READERSTATEA rgReaderStates;
	BOOL* states;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return status;

	Stream_Read_UINT32(s, len);
	if (len != cReaders)
	{
		WLog_ERR(TAG, "Count mismatch when reading LPSCARD_READERSTATEA");
		return status;
	}
	rgReaderStates = (LPSCARD_READERSTATEA)calloc(cReaders, sizeof(SCARD_READERSTATEA));
	states = calloc(cReaders, sizeof(BOOL));
	if (!rgReaderStates || !states)
		goto fail;
	status = ERROR_INVALID_DATA;

	for (index = 0; index < cReaders; index++)
	{
		UINT32 ptr = UINT32_MAX;
		LPSCARD_READERSTATEA readerState = &rgReaderStates[index];

		if (!Stream_CheckAndLogRequiredLength(TAG, s, 52))
			goto fail;

		if (!smartcard_ndr_pointer_read(s, ptrIndex, &ptr))
		{
			if (ptr != 0)
				goto fail;
		}
		/* Ignore NULL length strings */
		states[index] = ptr != 0;
		Stream_Read_UINT32(s, readerState->dwCurrentState); /* dwCurrentState (4 bytes) */
		Stream_Read_UINT32(s, readerState->dwEventState);   /* dwEventState (4 bytes) */
		Stream_Read_UINT32(s, readerState->cbAtr);          /* cbAtr (4 bytes) */
		Stream_Read(s, readerState->rgbAtr, 36);            /* rgbAtr [0..36] (36 bytes) */
	}

	for (index = 0; index < cReaders; index++)
	{
		LPSCARD_READERSTATEA readerState = &rgReaderStates[index];

		/* Ignore empty strings */
		if (!states[index])
			continue;
		status = smartcard_ndr_read_a(s, &readerState->szReader, NDR_PTR_FULL);
		if (status != SCARD_S_SUCCESS)
			goto fail;
	}

	*ppcReaders = rgReaderStates;
	free(states);
	return SCARD_S_SUCCESS;
fail:
	if (rgReaderStates)
	{
		for (index = 0; index < cReaders; index++)
		{
			LPSCARD_READERSTATEA readerState = &rgReaderStates[index];
			free(readerState->szReader);
		}
	}
	free(rgReaderStates);
	free(states);
	return status;
}

static LONG smartcard_unpack_reader_state_w(wStream* s, LPSCARD_READERSTATEW* ppcReaders,
                                            UINT32 cReaders, UINT32* ptrIndex)
{
	UINT32 index, len;
	LONG status = SCARD_E_NO_MEMORY;
	LPSCARD_READERSTATEW rgReaderStates;
	BOOL* states;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return status;

	Stream_Read_UINT32(s, len);
	if (len != cReaders)
	{
		WLog_ERR(TAG, "Count mismatch when reading LPSCARD_READERSTATEW");
		return status;
	}

	rgReaderStates = (LPSCARD_READERSTATEW)calloc(cReaders, sizeof(SCARD_READERSTATEW));
	states = calloc(cReaders, sizeof(BOOL));

	if (!rgReaderStates || !states)
		goto fail;

	status = ERROR_INVALID_DATA;
	for (index = 0; index < cReaders; index++)
	{
		UINT32 ptr = UINT32_MAX;
		LPSCARD_READERSTATEW readerState = &rgReaderStates[index];

		if (!Stream_CheckAndLogRequiredLength(TAG, s, 52))
			goto fail;

		if (!smartcard_ndr_pointer_read(s, ptrIndex, &ptr))
		{
			if (ptr != 0)
				goto fail;
		}
		/* Ignore NULL length strings */
		states[index] = ptr != 0;
		Stream_Read_UINT32(s, readerState->dwCurrentState); /* dwCurrentState (4 bytes) */
		Stream_Read_UINT32(s, readerState->dwEventState);   /* dwEventState (4 bytes) */
		Stream_Read_UINT32(s, readerState->cbAtr);          /* cbAtr (4 bytes) */
		Stream_Read(s, readerState->rgbAtr, 36);            /* rgbAtr [0..36] (36 bytes) */
	}

	for (index = 0; index < cReaders; index++)
	{
		LPSCARD_READERSTATEW readerState = &rgReaderStates[index];

		/* Skip NULL pointers */
		if (!states[index])
			continue;

		status = smartcard_ndr_read_w(s, &readerState->szReader, NDR_PTR_FULL);
		if (status != SCARD_S_SUCCESS)
			goto fail;
	}

	*ppcReaders = rgReaderStates;
	free(states);
	return SCARD_S_SUCCESS;
fail:
	if (rgReaderStates)
	{
		for (index = 0; index < cReaders; index++)
		{
			LPSCARD_READERSTATEW readerState = &rgReaderStates[index];
			free(readerState->szReader);
		}
	}
	free(rgReaderStates);
	free(states);
	return status;
}

/******************************************************************************/
/************************************* End Trace Functions ********************/
/******************************************************************************/

LONG smartcard_unpack_get_status_change_a_call(wStream* s, GetStatusChangeA_Call* call)
{
	LONG status;
	UINT32 ndrPtr;
	UINT32 index = 0;
	call->rgReaderStates = NULL;

	status = smartcard_unpack_redir_scard_context(s, &(call->handles.hContext), &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 12))
		return STATUS_BUFFER_TOO_SMALL;

	Stream_Read_UINT32(s, call->dwTimeOut); /* dwTimeOut (4 bytes) */
	Stream_Read_UINT32(s, call->cReaders);  /* cReaders (4 bytes) */
	if (!smartcard_ndr_pointer_read(s, &index, &ndrPtr))
		return ERROR_INVALID_DATA;

	if ((status = smartcard_unpack_redir_scard_context_ref(s, &(call->handles.hContext))))
		return status;

	if (ndrPtr)
	{
		status = smartcard_unpack_reader_state_a(s, &call->rgReaderStates, call->cReaders, &index);
		if (status != SCARD_S_SUCCESS)
			return status;
	}

	smartcard_trace_get_status_change_a_call(call);
	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_get_status_change_w_call(wStream* s, GetStatusChangeW_Call* call)
{
	UINT32 ndrPtr;
	LONG status;
	UINT32 index = 0;

	call->rgReaderStates = NULL;

	status = smartcard_unpack_redir_scard_context(s, &(call->handles.hContext), &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 12))
		return STATUS_BUFFER_TOO_SMALL;

	Stream_Read_UINT32(s, call->dwTimeOut); /* dwTimeOut (4 bytes) */
	Stream_Read_UINT32(s, call->cReaders);  /* cReaders (4 bytes) */
	if (!smartcard_ndr_pointer_read(s, &index, &ndrPtr))
		return ERROR_INVALID_DATA;

	if ((status = smartcard_unpack_redir_scard_context_ref(s, &(call->handles.hContext))))
		return status;

	if (ndrPtr)
	{
		status = smartcard_unpack_reader_state_w(s, &call->rgReaderStates, call->cReaders, &index);
		if (status != SCARD_S_SUCCESS)
			return status;
	}

	smartcard_trace_get_status_change_w_call(call);
	return SCARD_S_SUCCESS;
}

LONG smartcard_pack_get_status_change_return(wStream* s, const GetStatusChange_Return* ret,
                                             BOOL unicode)
{
	LONG status;
	DWORD cReaders = ret->cReaders;
	UINT32 index = 0;

	smartcard_trace_get_status_change_return(ret, unicode);
	if (ret->ReturnCode != SCARD_S_SUCCESS)
		cReaders = 0;
	if (cReaders == SCARD_AUTOALLOCATE)
		cReaders = 0;

	if (!Stream_EnsureRemainingCapacity(s, 4))
		return SCARD_E_NO_MEMORY;

	Stream_Write_UINT32(s, cReaders); /* cReaders (4 bytes) */
	if (!smartcard_ndr_pointer_write(s, &index, cReaders))
		return SCARD_E_NO_MEMORY;
	status = smartcard_ndr_write_state(s, ret->rgReaderStates, cReaders, NDR_PTR_SIMPLE);
	if (status != SCARD_S_SUCCESS)
		return status;
	return ret->ReturnCode;
}

LONG smartcard_unpack_state_call(wStream* s, State_Call* call)
{
	LONG status;
	UINT32 index = 0;

	status = smartcard_unpack_redir_scard_context(s, &(call->handles.hContext), &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	status = smartcard_unpack_redir_scard_handle(s, &(call->handles.hCard), &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return STATUS_BUFFER_TOO_SMALL;

	Stream_Read_INT32(s, call->fpbAtrIsNULL); /* fpbAtrIsNULL (4 bytes) */
	Stream_Read_UINT32(s, call->cbAtrLen);    /* cbAtrLen (4 bytes) */

	if ((status = smartcard_unpack_redir_scard_context_ref(s, &(call->handles.hContext))))
		return status;

	if ((status = smartcard_unpack_redir_scard_handle_ref(s, &(call->handles.hCard))))
		return status;

	return status;
}

LONG smartcard_pack_state_return(wStream* s, const State_Return* ret)
{
	LONG status;
	DWORD cbAtrLen = ret->cbAtrLen;
	UINT32 index = 0;

	smartcard_trace_state_return(ret);
	if (ret->ReturnCode != SCARD_S_SUCCESS)
		cbAtrLen = 0;
	if (cbAtrLen == SCARD_AUTOALLOCATE)
		cbAtrLen = 0;

	Stream_Write_UINT32(s, ret->dwState);    /* dwState (4 bytes) */
	Stream_Write_UINT32(s, ret->dwProtocol); /* dwProtocol (4 bytes) */
	Stream_Write_UINT32(s, cbAtrLen);        /* cbAtrLen (4 bytes) */
	if (!smartcard_ndr_pointer_write(s, &index, cbAtrLen))
		return SCARD_E_NO_MEMORY;
	status = smartcard_ndr_write(s, ret->rgAtr, cbAtrLen, 1, NDR_PTR_SIMPLE);
	if (status != SCARD_S_SUCCESS)
		return status;
	return ret->ReturnCode;
}

LONG smartcard_unpack_status_call(wStream* s, Status_Call* call, BOOL unicode)
{
	LONG status;
	UINT32 index = 0;
	status = smartcard_unpack_redir_scard_context(s, &(call->handles.hContext), &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	status = smartcard_unpack_redir_scard_handle(s, &(call->handles.hCard), &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 12))
		return STATUS_BUFFER_TOO_SMALL;

	Stream_Read_INT32(s, call->fmszReaderNamesIsNULL); /* fmszReaderNamesIsNULL (4 bytes) */
	Stream_Read_UINT32(s, call->cchReaderLen);         /* cchReaderLen (4 bytes) */
	Stream_Read_UINT32(s, call->cbAtrLen);             /* cbAtrLen (4 bytes) */

	if ((status = smartcard_unpack_redir_scard_context_ref(s, &(call->handles.hContext))))
		return status;

	if ((status = smartcard_unpack_redir_scard_handle_ref(s, &(call->handles.hCard))))
		return status;

	smartcard_trace_status_call(call, unicode);
	return status;
}

LONG smartcard_pack_status_return(wStream* s, const Status_Return* ret, BOOL unicode)
{
	LONG status;
	UINT32 index = 0;
	DWORD cBytes = ret->cBytes;

	smartcard_trace_status_return(ret, unicode);
	if (ret->ReturnCode != SCARD_S_SUCCESS)
		cBytes = 0;
	if (cBytes == SCARD_AUTOALLOCATE)
		cBytes = 0;

	if (!Stream_EnsureRemainingCapacity(s, 4))
		return SCARD_F_INTERNAL_ERROR;

	Stream_Write_UINT32(s, cBytes); /* cBytes (4 bytes) */
	if (!smartcard_ndr_pointer_write(s, &index, cBytes))
		return SCARD_E_NO_MEMORY;

	if (!Stream_EnsureRemainingCapacity(s, 44))
		return SCARD_F_INTERNAL_ERROR;

	Stream_Write_UINT32(s, ret->dwState);            /* dwState (4 bytes) */
	Stream_Write_UINT32(s, ret->dwProtocol);         /* dwProtocol (4 bytes) */
	Stream_Write(s, ret->pbAtr, sizeof(ret->pbAtr)); /* pbAtr (32 bytes) */
	Stream_Write_UINT32(s, ret->cbAtrLen);           /* cbAtrLen (4 bytes) */
	status = smartcard_ndr_write(s, ret->mszReaderNames, cBytes, 1, NDR_PTR_SIMPLE);
	if (status != SCARD_S_SUCCESS)
		return status;
	return ret->ReturnCode;
}

LONG smartcard_unpack_get_attrib_call(wStream* s, GetAttrib_Call* call)
{
	LONG status;
	UINT32 index = 0;

	status = smartcard_unpack_redir_scard_context(s, &(call->handles.hContext), &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	status = smartcard_unpack_redir_scard_handle(s, &(call->handles.hCard), &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 12))
		return STATUS_BUFFER_TOO_SMALL;

	Stream_Read_UINT32(s, call->dwAttrId);     /* dwAttrId (4 bytes) */
	Stream_Read_INT32(s, call->fpbAttrIsNULL); /* fpbAttrIsNULL (4 bytes) */
	Stream_Read_UINT32(s, call->cbAttrLen);    /* cbAttrLen (4 bytes) */

	if ((status = smartcard_unpack_redir_scard_context_ref(s, &(call->handles.hContext))))
		return status;

	if ((status = smartcard_unpack_redir_scard_handle_ref(s, &(call->handles.hCard))))
		return status;

	smartcard_trace_get_attrib_call(call);
	return status;
}

LONG smartcard_pack_get_attrib_return(wStream* s, const GetAttrib_Return* ret, DWORD dwAttrId,
                                      DWORD cbAttrCallLen)
{
	LONG status;
	DWORD cbAttrLen;
	UINT32 index = 0;
	smartcard_trace_get_attrib_return(ret, dwAttrId);

	if (!Stream_EnsureRemainingCapacity(s, 4))
		return SCARD_F_INTERNAL_ERROR;

	cbAttrLen = ret->cbAttrLen;
	if (ret->ReturnCode != SCARD_S_SUCCESS)
		cbAttrLen = 0;
	if (cbAttrLen == SCARD_AUTOALLOCATE)
		cbAttrLen = 0;

	if (ret->pbAttr)
	{
		if (cbAttrCallLen < cbAttrLen)
			cbAttrLen = cbAttrCallLen;
	}
	Stream_Write_UINT32(s, cbAttrLen); /* cbAttrLen (4 bytes) */
	if (!smartcard_ndr_pointer_write(s, &index, cbAttrLen))
		return SCARD_E_NO_MEMORY;

	status = smartcard_ndr_write(s, ret->pbAttr, cbAttrLen, 1, NDR_PTR_SIMPLE);
	if (status != SCARD_S_SUCCESS)
		return status;
	return ret->ReturnCode;
}

LONG smartcard_unpack_control_call(wStream* s, Control_Call* call)
{
	LONG status;
	UINT32 index = 0;
	UINT32 pvInBufferNdrPtr;

	call->pvInBuffer = NULL;

	status = smartcard_unpack_redir_scard_context(s, &(call->handles.hContext), &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	status = smartcard_unpack_redir_scard_handle(s, &(call->handles.hCard), &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 20))
		return STATUS_BUFFER_TOO_SMALL;

	Stream_Read_UINT32(s, call->dwControlCode);                    /* dwControlCode (4 bytes) */
	Stream_Read_UINT32(s, call->cbInBufferSize);                   /* cbInBufferSize (4 bytes) */
	if (!smartcard_ndr_pointer_read(s, &index, &pvInBufferNdrPtr)) /* pvInBufferNdrPtr (4 bytes) */
		return ERROR_INVALID_DATA;
	Stream_Read_INT32(s, call->fpvOutBufferIsNULL); /* fpvOutBufferIsNULL (4 bytes) */
	Stream_Read_UINT32(s, call->cbOutBufferSize);   /* cbOutBufferSize (4 bytes) */

	if ((status = smartcard_unpack_redir_scard_context_ref(s, &(call->handles.hContext))))
		return status;

	if ((status = smartcard_unpack_redir_scard_handle_ref(s, &(call->handles.hCard))))
		return status;

	if (pvInBufferNdrPtr)
	{
		status = smartcard_ndr_read(s, &call->pvInBuffer, call->cbInBufferSize, 1, NDR_PTR_SIMPLE);
		if (status != SCARD_S_SUCCESS)
			return status;
	}

	smartcard_trace_control_call(call);
	return SCARD_S_SUCCESS;
}

LONG smartcard_pack_control_return(wStream* s, const Control_Return* ret)
{
	LONG status;
	DWORD cbDataLen = ret->cbOutBufferSize;
	UINT32 index = 0;

	smartcard_trace_control_return(ret);
	if (ret->ReturnCode != SCARD_S_SUCCESS)
		cbDataLen = 0;
	if (cbDataLen == SCARD_AUTOALLOCATE)
		cbDataLen = 0;

	if (!Stream_EnsureRemainingCapacity(s, 4))
		return SCARD_F_INTERNAL_ERROR;

	Stream_Write_UINT32(s, cbDataLen); /* cbOutBufferSize (4 bytes) */
	if (!smartcard_ndr_pointer_write(s, &index, cbDataLen))
		return SCARD_E_NO_MEMORY;

	status = smartcard_ndr_write(s, ret->pvOutBuffer, cbDataLen, 1, NDR_PTR_SIMPLE);
	if (status != SCARD_S_SUCCESS)
		return status;
	return ret->ReturnCode;
}

LONG smartcard_unpack_transmit_call(wStream* s, Transmit_Call* call)
{
	LONG status;
	UINT32 length;
	BYTE* pbExtraBytes;
	UINT32 pbExtraBytesNdrPtr;
	UINT32 pbSendBufferNdrPtr;
	UINT32 pioRecvPciNdrPtr;
	SCardIO_Request ioSendPci;
	SCardIO_Request ioRecvPci;
	UINT32 index = 0;
	call->pioSendPci = NULL;
	call->pioRecvPci = NULL;
	call->pbSendBuffer = NULL;

	status = smartcard_unpack_redir_scard_context(s, &(call->handles.hContext), &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	status = smartcard_unpack_redir_scard_handle(s, &(call->handles.hCard), &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 32))
		return STATUS_BUFFER_TOO_SMALL;

	Stream_Read_UINT32(s, ioSendPci.dwProtocol);   /* dwProtocol (4 bytes) */
	Stream_Read_UINT32(s, ioSendPci.cbExtraBytes); /* cbExtraBytes (4 bytes) */
	if (!smartcard_ndr_pointer_read(s, &index,
	                                &pbExtraBytesNdrPtr)) /* pbExtraBytesNdrPtr (4 bytes) */
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, call->cbSendLength); /* cbSendLength (4 bytes) */
	if (!smartcard_ndr_pointer_read(s, &index,
	                                &pbSendBufferNdrPtr)) /* pbSendBufferNdrPtr (4 bytes) */
		return ERROR_INVALID_DATA;

	if (!smartcard_ndr_pointer_read(s, &index, &pioRecvPciNdrPtr)) /* pioRecvPciNdrPtr (4 bytes) */
		return ERROR_INVALID_DATA;

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

	if ((status = smartcard_unpack_redir_scard_context_ref(s, &(call->handles.hContext))))
		return status;

	if ((status = smartcard_unpack_redir_scard_handle_ref(s, &(call->handles.hCard))))
		return status;

	if (ioSendPci.cbExtraBytes && !pbExtraBytesNdrPtr)
	{
		WLog_WARN(
		    TAG, "Transmit_Call ioSendPci.cbExtraBytes is non-zero but pbExtraBytesNdrPtr is null");
		return STATUS_INVALID_PARAMETER;
	}

	if (pbExtraBytesNdrPtr)
	{
		// TODO: Use unified pointer reading
		if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
			return STATUS_BUFFER_TOO_SMALL;

		Stream_Read_UINT32(s, length); /* Length (4 bytes) */

		if (!Stream_CheckAndLogRequiredLength(TAG, s, ioSendPci.cbExtraBytes))
			return STATUS_BUFFER_TOO_SMALL;

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
		smartcard_unpack_read_size_align(s, ioSendPci.cbExtraBytes, 4);
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
		status = smartcard_ndr_read(s, &call->pbSendBuffer, call->cbSendLength, 1, NDR_PTR_SIMPLE);
		if (status != SCARD_S_SUCCESS)
			return status;
	}

	if (pioRecvPciNdrPtr)
	{
		if (!Stream_CheckAndLogRequiredLength(TAG, s, 12))
			return STATUS_BUFFER_TOO_SMALL;

		Stream_Read_UINT32(s, ioRecvPci.dwProtocol);   /* dwProtocol (4 bytes) */
		Stream_Read_UINT32(s, ioRecvPci.cbExtraBytes); /* cbExtraBytes (4 bytes) */
		if (!smartcard_ndr_pointer_read(s, &index,
		                                &pbExtraBytesNdrPtr)) /* pbExtraBytesNdrPtr (4 bytes) */
			return ERROR_INVALID_DATA;

		if (ioRecvPci.cbExtraBytes && !pbExtraBytesNdrPtr)
		{
			WLog_WARN(
			    TAG,
			    "Transmit_Call ioRecvPci.cbExtraBytes is non-zero but pbExtraBytesNdrPtr is null");
			return STATUS_INVALID_PARAMETER;
		}

		if (pbExtraBytesNdrPtr)
		{
			// TODO: Unify ndr pointer reading
			if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
				return STATUS_BUFFER_TOO_SMALL;

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

			if (!Stream_CheckAndLogRequiredLength(TAG, s, ioRecvPci.cbExtraBytes))
				return STATUS_BUFFER_TOO_SMALL;

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
			smartcard_unpack_read_size_align(s, ioRecvPci.cbExtraBytes, 4);
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

	smartcard_trace_transmit_call(call);
	return SCARD_S_SUCCESS;
}

LONG smartcard_pack_transmit_return(wStream* s, const Transmit_Return* ret)
{
	LONG status;
	UINT32 index = 0;
	LONG error;
	UINT32 cbRecvLength = ret->cbRecvLength;
	UINT32 cbRecvPci = ret->pioRecvPci ? ret->pioRecvPci->cbPciLength : 0;

	smartcard_trace_transmit_return(ret);

	if (!ret->pbRecvBuffer)
		cbRecvLength = 0;

	if (!smartcard_ndr_pointer_write(s, &index, cbRecvPci))
		return SCARD_E_NO_MEMORY;
	if (!Stream_EnsureRemainingCapacity(s, 4))
		return SCARD_E_NO_MEMORY;
	Stream_Write_UINT32(s, cbRecvLength); /* cbRecvLength (4 bytes) */
	if (!smartcard_ndr_pointer_write(s, &index, cbRecvLength))
		return SCARD_E_NO_MEMORY;

	if (ret->pioRecvPci)
	{
		UINT32 cbExtraBytes = (UINT32)(ret->pioRecvPci->cbPciLength - sizeof(SCARD_IO_REQUEST));
		BYTE* pbExtraBytes = &((BYTE*)ret->pioRecvPci)[sizeof(SCARD_IO_REQUEST)];

		if (!Stream_EnsureRemainingCapacity(s, cbExtraBytes + 16))
		{
			WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
			return SCARD_F_INTERNAL_ERROR;
		}

		Stream_Write_UINT32(s, ret->pioRecvPci->dwProtocol); /* dwProtocol (4 bytes) */
		Stream_Write_UINT32(s, cbExtraBytes);                /* cbExtraBytes (4 bytes) */
		if (!smartcard_ndr_pointer_write(s, &index, cbExtraBytes))
			return SCARD_E_NO_MEMORY;
		error = smartcard_ndr_write(s, pbExtraBytes, cbExtraBytes, 1, NDR_PTR_SIMPLE);
		if (error)
			return error;
	}

	status = smartcard_ndr_write(s, ret->pbRecvBuffer, ret->cbRecvLength, 1, NDR_PTR_SIMPLE);
	if (status != SCARD_S_SUCCESS)
		return status;
	return ret->ReturnCode;
}

LONG smartcard_unpack_locate_cards_by_atr_a_call(wStream* s, LocateCardsByATRA_Call* call)
{
	LONG status;
	UINT32 rgReaderStatesNdrPtr;
	UINT32 rgAtrMasksNdrPtr;
	UINT32 index = 0;
	call->rgReaderStates = NULL;

	status = smartcard_unpack_redir_scard_context(s, &(call->handles.hContext), &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 16))
		return STATUS_BUFFER_TOO_SMALL;

	Stream_Read_UINT32(s, call->cAtrs);
	if (!smartcard_ndr_pointer_read(s, &index, &rgAtrMasksNdrPtr))
		return ERROR_INVALID_DATA;
	Stream_Read_UINT32(s, call->cReaders); /* cReaders (4 bytes) */
	if (!smartcard_ndr_pointer_read(s, &index, &rgReaderStatesNdrPtr))
		return ERROR_INVALID_DATA;

	if ((status = smartcard_unpack_redir_scard_context_ref(s, &(call->handles.hContext))))
		return status;

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
		status = smartcard_ndr_read_atrmask(s, &call->rgAtrMasks, call->cAtrs, NDR_PTR_SIMPLE);
		if (status != SCARD_S_SUCCESS)
			return status;
	}

	if (rgReaderStatesNdrPtr)
	{
		status = smartcard_unpack_reader_state_a(s, &call->rgReaderStates, call->cReaders, &index);
		if (status != SCARD_S_SUCCESS)
			return status;
	}

	smartcard_trace_locate_cards_by_atr_a_call(call);
	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_context_and_two_strings_a_call(wStream* s, ContextAndTwoStringA_Call* call)
{
	LONG status;
	UINT32 sz1NdrPtr, sz2NdrPtr;
	UINT32 index = 0;

	status = smartcard_unpack_redir_scard_context(s, &(call->handles.hContext), &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!smartcard_ndr_pointer_read(s, &index, &sz1NdrPtr))
		return ERROR_INVALID_DATA;
	if (!smartcard_ndr_pointer_read(s, &index, &sz2NdrPtr))
		return ERROR_INVALID_DATA;

	status = smartcard_unpack_redir_scard_context_ref(s, &call->handles.hContext);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (sz1NdrPtr)
	{
		status = smartcard_ndr_read_a(s, &call->sz1, NDR_PTR_FULL);
		if (status != SCARD_S_SUCCESS)
			return status;
	}
	if (sz2NdrPtr)
	{
		status = smartcard_ndr_read_a(s, &call->sz2, NDR_PTR_FULL);
		if (status != SCARD_S_SUCCESS)
			return status;
	}
	smartcard_trace_context_and_two_strings_a_call(call);
	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_context_and_two_strings_w_call(wStream* s, ContextAndTwoStringW_Call* call)
{
	LONG status;
	UINT32 sz1NdrPtr, sz2NdrPtr;
	UINT32 index = 0;
	status = smartcard_unpack_redir_scard_context(s, &(call->handles.hContext), &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!smartcard_ndr_pointer_read(s, &index, &sz1NdrPtr))
		return ERROR_INVALID_DATA;
	if (!smartcard_ndr_pointer_read(s, &index, &sz2NdrPtr))
		return ERROR_INVALID_DATA;

	status = smartcard_unpack_redir_scard_context_ref(s, &call->handles.hContext);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (sz1NdrPtr)
	{
		status = smartcard_ndr_read_w(s, &call->sz1, NDR_PTR_FULL);
		if (status != SCARD_S_SUCCESS)
			return status;
	}
	if (sz2NdrPtr)
	{
		status = smartcard_ndr_read_w(s, &call->sz2, NDR_PTR_FULL);
		if (status != SCARD_S_SUCCESS)
			return status;
	}
	smartcard_trace_context_and_two_strings_w_call(call);
	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_locate_cards_a_call(wStream* s, LocateCardsA_Call* call)
{
	LONG status;
	UINT32 sz1NdrPtr, sz2NdrPtr;
	UINT32 index = 0;
	status = smartcard_unpack_redir_scard_context(s, &(call->handles.hContext), &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 16))
		return STATUS_BUFFER_TOO_SMALL;

	Stream_Read_UINT32(s, call->cBytes);
	if (!smartcard_ndr_pointer_read(s, &index, &sz1NdrPtr))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, call->cReaders);
	if (!smartcard_ndr_pointer_read(s, &index, &sz2NdrPtr))
		return ERROR_INVALID_DATA;

	if (sz1NdrPtr)
	{
		status =
		    smartcard_ndr_read_fixed_string_a(s, &call->mszCards, call->cBytes, NDR_PTR_SIMPLE);
		if (status != SCARD_S_SUCCESS)
			return status;
	}
	if (sz2NdrPtr)
	{
		status = smartcard_unpack_reader_state_a(s, &call->rgReaderStates, call->cReaders, &index);
		if (status != SCARD_S_SUCCESS)
			return status;
	}
	smartcard_trace_locate_cards_a_call(call);
	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_locate_cards_w_call(wStream* s, LocateCardsW_Call* call)
{
	LONG status;
	UINT32 sz1NdrPtr, sz2NdrPtr;
	UINT32 index = 0;

	status = smartcard_unpack_redir_scard_context(s, &(call->handles.hContext), &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 16))
		return STATUS_BUFFER_TOO_SMALL;

	Stream_Read_UINT32(s, call->cBytes);
	if (!smartcard_ndr_pointer_read(s, &index, &sz1NdrPtr))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, call->cReaders);
	if (!smartcard_ndr_pointer_read(s, &index, &sz2NdrPtr))
		return ERROR_INVALID_DATA;

	if (sz1NdrPtr)
	{
		status =
		    smartcard_ndr_read_fixed_string_w(s, &call->mszCards, call->cBytes, NDR_PTR_SIMPLE);
		if (status != SCARD_S_SUCCESS)
			return status;
	}
	if (sz2NdrPtr)
	{
		status = smartcard_unpack_reader_state_w(s, &call->rgReaderStates, call->cReaders, &index);
		if (status != SCARD_S_SUCCESS)
			return status;
	}
	smartcard_trace_locate_cards_w_call(call);
	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_set_attrib_call(wStream* s, SetAttrib_Call* call)
{
	LONG status;
	UINT32 index = 0;
	UINT32 ndrPtr;

	status = smartcard_unpack_redir_scard_context(s, &(call->handles.hContext), &index);
	if (status != SCARD_S_SUCCESS)
		return status;
	status = smartcard_unpack_redir_scard_handle(s, &(call->handles.hCard), &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 12))
		return STATUS_BUFFER_TOO_SMALL;
	Stream_Read_UINT32(s, call->dwAttrId);
	Stream_Read_UINT32(s, call->cbAttrLen);

	if (!smartcard_ndr_pointer_read(s, &index, &ndrPtr))
		return ERROR_INVALID_DATA;

	if ((status = smartcard_unpack_redir_scard_context_ref(s, &(call->handles.hContext))))
		return status;

	if ((status = smartcard_unpack_redir_scard_handle_ref(s, &(call->handles.hCard))))
		return status;

	if (ndrPtr)
	{
		// TODO: call->cbAttrLen was larger than the pointer value.
		// TODO: Maybe need to refine the checks?
		status = smartcard_ndr_read(s, &call->pbAttr, 0, 1, NDR_PTR_SIMPLE);
		if (status != SCARD_S_SUCCESS)
			return status;
	}
	smartcard_trace_set_attrib_call(call);
	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_locate_cards_by_atr_w_call(wStream* s, LocateCardsByATRW_Call* call)
{
	LONG status;
	UINT32 rgReaderStatesNdrPtr;
	UINT32 rgAtrMasksNdrPtr;
	UINT32 index = 0;
	call->rgReaderStates = NULL;

	status = smartcard_unpack_redir_scard_context(s, &(call->handles.hContext), &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 16))
		return STATUS_BUFFER_TOO_SMALL;

	Stream_Read_UINT32(s, call->cAtrs);
	if (!smartcard_ndr_pointer_read(s, &index, &rgAtrMasksNdrPtr))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, call->cReaders); /* cReaders (4 bytes) */
	if (!smartcard_ndr_pointer_read(s, &index, &rgReaderStatesNdrPtr))
		return ERROR_INVALID_DATA;

	if ((status = smartcard_unpack_redir_scard_context_ref(s, &(call->handles.hContext))))
		return status;

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
		status = smartcard_ndr_read_atrmask(s, &call->rgAtrMasks, call->cAtrs, NDR_PTR_SIMPLE);
		if (status != SCARD_S_SUCCESS)
			return status;
	}

	if (rgReaderStatesNdrPtr)
	{
		status = smartcard_unpack_reader_state_w(s, &call->rgReaderStates, call->cReaders, &index);
		if (status != SCARD_S_SUCCESS)
			return status;
	}

	smartcard_trace_locate_cards_by_atr_w_call(call);
	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_read_cache_a_call(wStream* s, ReadCacheA_Call* call)
{
	LONG status;
	UINT32 mszNdrPtr;
	UINT32 contextNdrPtr;
	UINT32 index = 0;

	if (!smartcard_ndr_pointer_read(s, &index, &mszNdrPtr))
		return ERROR_INVALID_DATA;

	status = smartcard_unpack_redir_scard_context(s, &(call->Common.handles.hContext), &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!smartcard_ndr_pointer_read(s, &index, &contextNdrPtr))
		return ERROR_INVALID_DATA;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 12))
		return STATUS_BUFFER_TOO_SMALL;
	Stream_Read_UINT32(s, call->Common.FreshnessCounter);
	Stream_Read_INT32(s, call->Common.fPbDataIsNULL);
	Stream_Read_UINT32(s, call->Common.cbDataLen);

	call->szLookupName = NULL;
	if (mszNdrPtr)
	{
		status = smartcard_ndr_read_a(s, &call->szLookupName, NDR_PTR_FULL);
		if (status != SCARD_S_SUCCESS)
			return status;
	}

	status = smartcard_unpack_redir_scard_context_ref(s, &call->Common.handles.hContext);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (contextNdrPtr)
	{
		status = smartcard_ndr_read_u(s, &call->Common.CardIdentifier);
		if (status != SCARD_S_SUCCESS)
			return status;
	}
	smartcard_trace_read_cache_a_call(call);
	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_read_cache_w_call(wStream* s, ReadCacheW_Call* call)
{
	LONG status;
	UINT32 mszNdrPtr;
	UINT32 contextNdrPtr;
	UINT32 index = 0;

	if (!smartcard_ndr_pointer_read(s, &index, &mszNdrPtr))
		return ERROR_INVALID_DATA;

	status = smartcard_unpack_redir_scard_context(s, &(call->Common.handles.hContext), &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!smartcard_ndr_pointer_read(s, &index, &contextNdrPtr))
		return ERROR_INVALID_DATA;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 12))
		return STATUS_BUFFER_TOO_SMALL;
	Stream_Read_UINT32(s, call->Common.FreshnessCounter);
	Stream_Read_INT32(s, call->Common.fPbDataIsNULL);
	Stream_Read_UINT32(s, call->Common.cbDataLen);

	call->szLookupName = NULL;
	if (mszNdrPtr)
	{
		status = smartcard_ndr_read_w(s, &call->szLookupName, NDR_PTR_FULL);
		if (status != SCARD_S_SUCCESS)
			return status;
	}

	status = smartcard_unpack_redir_scard_context_ref(s, &call->Common.handles.hContext);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (contextNdrPtr)
	{
		status = smartcard_ndr_read_u(s, &call->Common.CardIdentifier);
		if (status != SCARD_S_SUCCESS)
			return status;
	}
	smartcard_trace_read_cache_w_call(call);
	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_write_cache_a_call(wStream* s, WriteCacheA_Call* call)
{
	LONG status;
	UINT32 mszNdrPtr;
	UINT32 contextNdrPtr;
	UINT32 pbDataNdrPtr;
	UINT32 index = 0;

	if (!smartcard_ndr_pointer_read(s, &index, &mszNdrPtr))
		return ERROR_INVALID_DATA;

	status = smartcard_unpack_redir_scard_context(s, &(call->Common.handles.hContext), &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!smartcard_ndr_pointer_read(s, &index, &contextNdrPtr))
		return ERROR_INVALID_DATA;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return STATUS_BUFFER_TOO_SMALL;

	Stream_Read_UINT32(s, call->Common.FreshnessCounter);
	Stream_Read_UINT32(s, call->Common.cbDataLen);

	if (!smartcard_ndr_pointer_read(s, &index, &pbDataNdrPtr))
		return ERROR_INVALID_DATA;

	call->szLookupName = NULL;
	if (mszNdrPtr)
	{
		status = smartcard_ndr_read_a(s, &call->szLookupName, NDR_PTR_FULL);
		if (status != SCARD_S_SUCCESS)
			return status;
	}

	status = smartcard_unpack_redir_scard_context_ref(s, &call->Common.handles.hContext);
	if (status != SCARD_S_SUCCESS)
		return status;

	call->Common.CardIdentifier = NULL;
	if (contextNdrPtr)
	{
		status = smartcard_ndr_read_u(s, &call->Common.CardIdentifier);
		if (status != SCARD_S_SUCCESS)
			return status;
	}

	call->Common.pbData = NULL;
	if (pbDataNdrPtr)
	{
		status =
		    smartcard_ndr_read(s, &call->Common.pbData, call->Common.cbDataLen, 1, NDR_PTR_SIMPLE);
		if (status != SCARD_S_SUCCESS)
			return status;
	}
	smartcard_trace_write_cache_a_call(call);
	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_write_cache_w_call(wStream* s, WriteCacheW_Call* call)
{
	LONG status;
	UINT32 mszNdrPtr;
	UINT32 contextNdrPtr;
	UINT32 pbDataNdrPtr;
	UINT32 index = 0;

	if (!smartcard_ndr_pointer_read(s, &index, &mszNdrPtr))
		return ERROR_INVALID_DATA;

	status = smartcard_unpack_redir_scard_context(s, &(call->Common.handles.hContext), &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!smartcard_ndr_pointer_read(s, &index, &contextNdrPtr))
		return ERROR_INVALID_DATA;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return STATUS_BUFFER_TOO_SMALL;
	Stream_Read_UINT32(s, call->Common.FreshnessCounter);
	Stream_Read_UINT32(s, call->Common.cbDataLen);

	if (!smartcard_ndr_pointer_read(s, &index, &pbDataNdrPtr))
		return ERROR_INVALID_DATA;

	call->szLookupName = NULL;
	if (mszNdrPtr)
	{
		status = smartcard_ndr_read_w(s, &call->szLookupName, NDR_PTR_FULL);
		if (status != SCARD_S_SUCCESS)
			return status;
	}

	status = smartcard_unpack_redir_scard_context_ref(s, &call->Common.handles.hContext);
	if (status != SCARD_S_SUCCESS)
		return status;

	call->Common.CardIdentifier = NULL;
	if (contextNdrPtr)
	{
		status = smartcard_ndr_read_u(s, &call->Common.CardIdentifier);
		if (status != SCARD_S_SUCCESS)
			return status;
	}

	call->Common.pbData = NULL;
	if (pbDataNdrPtr)
	{
		status =
		    smartcard_ndr_read(s, &call->Common.pbData, call->Common.cbDataLen, 1, NDR_PTR_SIMPLE);
		if (status != SCARD_S_SUCCESS)
			return status;
	}
	smartcard_trace_write_cache_w_call(call);
	return status;
}

LONG smartcard_unpack_get_transmit_count_call(wStream* s, GetTransmitCount_Call* call)
{
	LONG status;
	UINT32 index = 0;

	status = smartcard_unpack_redir_scard_context(s, &(call->handles.hContext), &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	status = smartcard_unpack_redir_scard_handle(s, &(call->handles.hCard), &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	if ((status = smartcard_unpack_redir_scard_context_ref(s, &(call->handles.hContext))))
	{
		WLog_ERR(TAG, "smartcard_unpack_redir_scard_context_ref failed with error %" PRId32 "",
		         status);
		return status;
	}

	if ((status = smartcard_unpack_redir_scard_handle_ref(s, &(call->handles.hCard))))
		WLog_ERR(TAG, "smartcard_unpack_redir_scard_handle_ref failed with error %" PRId32 "",
		         status);

	smartcard_trace_get_transmit_count_call(call);
	return status;
}

LONG smartcard_unpack_get_reader_icon_call(wStream* s, GetReaderIcon_Call* call)
{
	return smartcard_unpack_common_context_and_string_w(s, &call->handles.hContext,
	                                                    &call->szReaderName);
}

LONG smartcard_unpack_context_and_string_a_call(wStream* s, ContextAndStringA_Call* call)
{
	return smartcard_unpack_common_context_and_string_a(s, &call->handles.hContext, &call->sz);
}

LONG smartcard_unpack_context_and_string_w_call(wStream* s, ContextAndStringW_Call* call)
{
	return smartcard_unpack_common_context_and_string_w(s, &call->handles.hContext, &call->sz);
}

LONG smartcard_unpack_get_device_type_id_call(wStream* s, GetDeviceTypeId_Call* call)
{
	return smartcard_unpack_common_context_and_string_w(s, &call->handles.hContext,
	                                                    &call->szReaderName);
}

LONG smartcard_pack_device_type_id_return(wStream* s, const GetDeviceTypeId_Return* ret)
{
	smartcard_trace_device_type_id_return(ret);

	if (!Stream_EnsureRemainingCapacity(s, 4))
	{
		WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Write_UINT32(s, ret->dwDeviceId); /* cBytes (4 bytes) */

	return ret->ReturnCode;
}

LONG smartcard_pack_locate_cards_return(wStream* s, const LocateCards_Return* ret)
{
	LONG status;
	DWORD cbDataLen = ret->cReaders;
	UINT32 index = 0;

	smartcard_trace_locate_cards_return(ret);
	if (ret->ReturnCode != SCARD_S_SUCCESS)
		cbDataLen = 0;
	if (cbDataLen == SCARD_AUTOALLOCATE)
		cbDataLen = 0;

	if (!Stream_EnsureRemainingCapacity(s, 4))
	{
		WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Write_UINT32(s, cbDataLen); /* cBytes (4 cbDataLen) */
	if (!smartcard_ndr_pointer_write(s, &index, cbDataLen))
		return SCARD_E_NO_MEMORY;

	status = smartcard_ndr_write_state(s, ret->rgReaderStates, cbDataLen, NDR_PTR_SIMPLE);
	if (status != SCARD_S_SUCCESS)
		return status;
	return ret->ReturnCode;
}

LONG smartcard_pack_get_reader_icon_return(wStream* s, const GetReaderIcon_Return* ret)
{
	LONG status;
	UINT32 index = 0;
	DWORD cbDataLen = ret->cbDataLen;
	smartcard_trace_get_reader_icon_return(ret);
	if (ret->ReturnCode != SCARD_S_SUCCESS)
		cbDataLen = 0;
	if (cbDataLen == SCARD_AUTOALLOCATE)
		cbDataLen = 0;

	if (!Stream_EnsureRemainingCapacity(s, 4))
	{
		WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Write_UINT32(s, cbDataLen); /* cBytes (4 cbDataLen) */
	if (!smartcard_ndr_pointer_write(s, &index, cbDataLen))
		return SCARD_E_NO_MEMORY;

	status = smartcard_ndr_write(s, ret->pbData, cbDataLen, 1, NDR_PTR_SIMPLE);
	if (status != SCARD_S_SUCCESS)
		return status;
	return ret->ReturnCode;
}

LONG smartcard_pack_get_transmit_count_return(wStream* s, const GetTransmitCount_Return* ret)
{
	smartcard_trace_get_transmit_count_return(ret);

	if (!Stream_EnsureRemainingCapacity(s, 4))
	{
		WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Write_UINT32(s, ret->cTransmitCount); /* cBytes (4 cbDataLen) */

	return ret->ReturnCode;
}

LONG smartcard_pack_read_cache_return(wStream* s, const ReadCache_Return* ret)
{
	LONG status;
	UINT32 index = 0;
	DWORD cbDataLen = ret->cbDataLen;
	smartcard_trace_read_cache_return(ret);
	if (ret->ReturnCode != SCARD_S_SUCCESS)
		cbDataLen = 0;

	if (cbDataLen == SCARD_AUTOALLOCATE)
		cbDataLen = 0;

	if (!Stream_EnsureRemainingCapacity(s, 4))
	{
		WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Write_UINT32(s, cbDataLen); /* cBytes (4 cbDataLen) */
	if (!smartcard_ndr_pointer_write(s, &index, cbDataLen))
		return SCARD_E_NO_MEMORY;

	status = smartcard_ndr_write(s, ret->pbData, cbDataLen, 1, NDR_PTR_SIMPLE);
	if (status != SCARD_S_SUCCESS)
		return status;
	return ret->ReturnCode;
}
