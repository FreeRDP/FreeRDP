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
#include "smartcard_pack.h"

#include <freerdp/log.h>
#define SCARD_TAG FREERDP_TAG("scard.pack")

static const DWORD g_LogLevel = WLOG_DEBUG;

static wLog* scard_log(void)
{
	static wLog* log = NULL;
	if (!log)
		log = WLog_Get(SCARD_TAG);
	return log;
}

#define smartcard_unpack_redir_scard_context(log, s, context, index, ndr)                  \
	smartcard_unpack_redir_scard_context_((log), (s), (context), (index), (ndr), __FILE__, \
	                                      __func__, __LINE__)
#define smartcard_unpack_redir_scard_handle(log, s, context, index)                          \
	smartcard_unpack_redir_scard_handle_((log), (s), (context), (index), __FILE__, __func__, \
	                                     __LINE__)

static LONG smartcard_unpack_redir_scard_context_(wLog* log, wStream* s,
                                                  REDIR_SCARDCONTEXT* context, UINT32* index,
                                                  UINT32* ppbContextNdrPtr, const char* file,
                                                  const char* function, size_t line);
static LONG smartcard_pack_redir_scard_context(wLog* log, wStream* s,
                                               const REDIR_SCARDCONTEXT* context, DWORD* index);
static LONG smartcard_unpack_redir_scard_handle_(wLog* log, wStream* s, REDIR_SCARDHANDLE* handle,
                                                 UINT32* index, const char* file,
                                                 const char* function, size_t line);
static LONG smartcard_pack_redir_scard_handle(wLog* log, wStream* s,
                                              const REDIR_SCARDHANDLE* handle, DWORD* index);
static LONG smartcard_unpack_redir_scard_context_ref(wLog* log, wStream* s, UINT32 pbContextNdrPtr,
                                                     REDIR_SCARDCONTEXT* context);
static LONG smartcard_pack_redir_scard_context_ref(wLog* log, wStream* s,
                                                   const REDIR_SCARDCONTEXT* context);

static LONG smartcard_unpack_redir_scard_handle_ref(wLog* log, wStream* s,
                                                    REDIR_SCARDHANDLE* handle);
static LONG smartcard_pack_redir_scard_handle_ref(wLog* log, wStream* s,
                                                  const REDIR_SCARDHANDLE* handle);

typedef enum
{
	NDR_PTR_FULL,
	NDR_PTR_SIMPLE,
	NDR_PTR_FIXED
} ndr_ptr_t;

/* Reads a NDR pointer and checks if the value read has the expected relative
 * addressing */
#define smartcard_ndr_pointer_read(log, s, index, ptr) \
	smartcard_ndr_pointer_read_((log), (s), (index), (ptr), __FILE__, __func__, __LINE__)
static BOOL smartcard_ndr_pointer_read_(wLog* log, wStream* s, UINT32* index, UINT32* ptr,
                                        const char* file, const char* fkt, size_t line)
{
	const UINT32 expect = 0x20000 + (*index) * 4;
	UINT32 ndrPtr = 0;
	WINPR_UNUSED(file);
	if (!s)
		return FALSE;
	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 4))
		return FALSE;

	Stream_Read_UINT32(s, ndrPtr); /* mszGroupsNdrPtr (4 bytes) */
	if (ptr)
		*ptr = ndrPtr;
	if (expect != ndrPtr)
	{
		/* Allow NULL pointer if we read the result */
		if (ptr && (ndrPtr == 0))
			return TRUE;
		WLog_Print(log, WLOG_WARN,
		           "[%s:%" PRIuz "] Read context pointer 0x%08" PRIx32 ", expected 0x%08" PRIx32,
		           fkt, line, ndrPtr, expect);
		return FALSE;
	}

	(*index) = (*index) + 1;
	return TRUE;
}

static LONG smartcard_ndr_read_ex(wLog* log, wStream* s, BYTE** data, size_t min,
                                  size_t elementSize, ndr_ptr_t type, size_t* plen)
{
	size_t len = 0;
	size_t offset = 0;
	size_t len2 = 0;
	void* r = NULL;
	size_t required = 0;

	*data = NULL;
	if (plen)
		*plen = 0;

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
		default:
			return STATUS_INVALID_PARAMETER;
	}

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, required))
		return STATUS_BUFFER_TOO_SMALL;

	switch (type)
	{
		case NDR_PTR_FULL:
			Stream_Read_UINT32(s, len);
			Stream_Read_UINT32(s, offset);
			Stream_Read_UINT32(s, len2);
			if (len != offset + len2)
			{
				WLog_Print(log, WLOG_ERROR,
				           "Invalid data when reading full NDR pointer: total=%" PRIuz
				           ", offset=%" PRIuz ", remaining=%" PRIuz,
				           len, offset, len2);
				return STATUS_BUFFER_TOO_SMALL;
			}
			break;
		case NDR_PTR_SIMPLE:
			Stream_Read_UINT32(s, len);

			if ((len != min) && (min > 0))
			{
				WLog_Print(log, WLOG_ERROR,
				           "Invalid data when reading simple NDR pointer: total=%" PRIuz
				           ", expected=%" PRIuz,
				           len, min);
				return STATUS_BUFFER_TOO_SMALL;
			}
			break;
		case NDR_PTR_FIXED:
			len = (UINT32)min;
			break;
		default:
			return STATUS_INVALID_PARAMETER;
	}

	if (min > len)
	{
		WLog_Print(log, WLOG_ERROR,
		           "Invalid length read from NDR pointer, minimum %" PRIuz ", got %" PRIuz, min,
		           len);
		return STATUS_DATA_ERROR;
	}

	if (len > SIZE_MAX / 2)
		return STATUS_BUFFER_TOO_SMALL;

	if (!Stream_CheckAndLogRequiredLengthOfSizeWLog(log, s, len, elementSize))
		return STATUS_BUFFER_TOO_SMALL;

	len *= elementSize;

	/* Ensure proper '\0' termination for all kinds of unicode strings
	 * as we do not know if the data from the wire contains one.
	 */
	r = calloc(len + sizeof(WCHAR), sizeof(CHAR));
	if (!r)
		return SCARD_E_NO_MEMORY;
	Stream_Read(s, r, len);
	const LONG pad = smartcard_unpack_read_size_align(s, len, 4);
	len += (size_t)pad;
	*data = r;
	if (plen)
		*plen = len;
	return STATUS_SUCCESS;
}

static LONG smartcard_ndr_read(wLog* log, wStream* s, BYTE** data, size_t min, size_t elementSize,
                               ndr_ptr_t type)
{
	return smartcard_ndr_read_ex(log, s, data, min, elementSize, type, NULL);
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
	size_t required = 0;

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
		default:
			return SCARD_E_INVALID_PARAMETER;
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
		default:
			return SCARD_E_INVALID_PARAMETER;
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

	WINPR_ASSERT(data || (size == 0));
	cnv.reader = data;
	return smartcard_ndr_write(s, cnv.data, size, sizeof(ReaderState_Return), type);
}

static LONG smartcard_ndr_read_atrmask(wLog* log, wStream* s, LocateCards_ATRMask** data,
                                       size_t min, ndr_ptr_t type)
{
	union
	{
		LocateCards_ATRMask** ppc;
		BYTE** ppv;
	} u;
	u.ppc = data;
	return smartcard_ndr_read(log, s, u.ppv, min, sizeof(LocateCards_ATRMask), type);
}

static LONG smartcard_ndr_read_fixed_string_a(wLog* log, wStream* s, CHAR** data, size_t min,
                                              ndr_ptr_t type)
{
	union
	{
		CHAR** ppc;
		BYTE** ppv;
	} u;
	u.ppc = data;
	return smartcard_ndr_read(log, s, u.ppv, min, sizeof(CHAR), type);
}

static LONG smartcard_ndr_read_fixed_string_w(wLog* log, wStream* s, WCHAR** data, size_t min,
                                              ndr_ptr_t type)
{
	union
	{
		WCHAR** ppc;
		BYTE** ppv;
	} u;
	u.ppc = data;
	return smartcard_ndr_read(log, s, u.ppv, min, sizeof(WCHAR), type);
}

static LONG smartcard_ndr_read_a(wLog* log, wStream* s, CHAR** data, ndr_ptr_t type)
{
	union
	{
		CHAR** ppc;
		BYTE** ppv;
	} u;
	u.ppc = data;
	return smartcard_ndr_read(log, s, u.ppv, 0, sizeof(CHAR), type);
}

static LONG smartcard_ndr_read_w(wLog* log, wStream* s, WCHAR** data, ndr_ptr_t type)
{
	union
	{
		WCHAR** ppc;
		BYTE** ppv;
	} u;
	u.ppc = data;
	return smartcard_ndr_read(log, s, u.ppv, 0, sizeof(WCHAR), type);
}

static LONG smartcard_ndr_read_u(wLog* log, wStream* s, UUID** data)
{
	union
	{
		UUID** ppc;
		BYTE** ppv;
	} u;
	u.ppc = data;
	return smartcard_ndr_read(log, s, u.ppv, 1, sizeof(UUID), NDR_PTR_FIXED);
}

static char* smartcard_convert_string_list(const void* in, size_t bytes, BOOL unicode)
{
	size_t length = 0;
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
		mszA = ConvertMszWCharNToUtf8Alloc(string.wz, bytes / sizeof(WCHAR), &length);
		if (!mszA)
			return NULL;
	}
	else
	{
		mszA = (char*)calloc(bytes, sizeof(char));
		if (!mszA)
			return NULL;
		CopyMemory(mszA, string.sz, bytes - 1);
		length = bytes;
	}

	if (length < 1)
	{
		free(mszA);
		return NULL;
	}
	for (size_t index = 0; index < length - 1; index++)
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
	size_t szlen = 0;
	if (!msz)
		return NULL;
	char* sz = ConvertMszWCharNToUtf8Alloc(msz, len, &szlen);
	if (!sz)
		return NULL;

	smartcard_msz_dump_a(sz, szlen, buffer, bufferLen);
	free(sz);
	return buffer;
}

static char* smartcard_array_dump(const void* pd, size_t len, char* buffer, size_t bufferLen)
{
	const BYTE* data = pd;
	int rc = 0;
	char* start = buffer;

	WINPR_ASSERT(buffer || (bufferLen == 0));

	if (!data && (len > 0))
	{
		(void)_snprintf(buffer, bufferLen, "{ NULL [%" PRIuz "] }", len);
		goto fail;
	}

	rc = _snprintf(buffer, bufferLen, "{ ");
	if ((rc < 0) || ((size_t)rc >= bufferLen))
		goto fail;
	buffer += rc;
	bufferLen -= (size_t)rc;

	for (size_t x = 0; x < len; x++)
	{
		rc = _snprintf(buffer, bufferLen, "%02X", data[x]);
		if ((rc < 0) || ((size_t)rc >= bufferLen))
			goto fail;
		buffer += rc;
		bufferLen -= (size_t)rc;
	}

	rc = _snprintf(buffer, bufferLen, " }");
	if ((rc < 0) || ((size_t)rc >= bufferLen))
		goto fail;

fail:
	return start;
}

static void smartcard_log_redir_handle(wLog* log, const REDIR_SCARDHANDLE* pHandle)
{
	char buffer[128] = { 0 };

	WINPR_ASSERT(pHandle);
	WLog_Print(log, g_LogLevel, "  hContext: %s",
	           smartcard_array_dump(pHandle->pbHandle, pHandle->cbHandle, buffer, sizeof(buffer)));
}

static void smartcard_log_context(wLog* log, const REDIR_SCARDCONTEXT* phContext)
{
	char buffer[128] = { 0 };

	WINPR_ASSERT(phContext);
	WLog_Print(
	    log, g_LogLevel, "hContext: %s",
	    smartcard_array_dump(phContext->pbContext, phContext->cbContext, buffer, sizeof(buffer)));
}

static void smartcard_trace_context_and_string_call_a(wLog* log, const char* name,
                                                      const REDIR_SCARDCONTEXT* phContext,
                                                      const CHAR* sz)
{
	if (!WLog_IsLevelActive(log, g_LogLevel))
		return;

	WLog_Print(log, g_LogLevel, "%s {", name);
	smartcard_log_context(log, phContext);
	WLog_Print(log, g_LogLevel, "  sz=%s", sz);

	WLog_Print(log, g_LogLevel, "}");
}

static void smartcard_trace_context_and_string_call_w(wLog* log, const char* name,
                                                      const REDIR_SCARDCONTEXT* phContext,
                                                      const WCHAR* sz)
{
	char tmp[1024] = { 0 };

	if (!WLog_IsLevelActive(log, g_LogLevel))
		return;

	if (sz)
		(void)ConvertWCharToUtf8(sz, tmp, ARRAYSIZE(tmp));

	WLog_Print(log, g_LogLevel, "%s {", name);
	smartcard_log_context(log, phContext);
	WLog_Print(log, g_LogLevel, "  sz=%s", tmp);
	WLog_Print(log, g_LogLevel, "}");
}

static void smartcard_trace_context_call(wLog* log, const Context_Call* call, const char* name)
{
	WINPR_ASSERT(call);

	if (!WLog_IsLevelActive(log, g_LogLevel))
		return;

	WLog_Print(log, g_LogLevel, "%s_Call {", name);
	smartcard_log_context(log, &call->handles.hContext);

	WLog_Print(log, g_LogLevel, "}");
}

static void smartcard_trace_list_reader_groups_call(wLog* log, const ListReaderGroups_Call* call,
                                                    BOOL unicode)
{
	WINPR_ASSERT(call);

	if (!WLog_IsLevelActive(log, g_LogLevel))
		return;

	WLog_Print(log, g_LogLevel, "ListReaderGroups%s_Call {", unicode ? "W" : "A");
	smartcard_log_context(log, &call->handles.hContext);

	WLog_Print(log, g_LogLevel, "fmszGroupsIsNULL: %" PRId32 " cchGroups: 0x%08" PRIx32,
	           call->fmszGroupsIsNULL, call->cchGroups);
	WLog_Print(log, g_LogLevel, "}");
}

static void dump_reader_states_return(wLog* log, const ReaderState_Return* rgReaderStates,
                                      UINT32 cReaders)
{
	WINPR_ASSERT(rgReaderStates || (cReaders == 0));
	for (UINT32 index = 0; index < cReaders; index++)
	{
		char buffer[1024] = { 0 };

		const ReaderState_Return* readerState = &rgReaderStates[index];
		char* szCurrentState = SCardGetReaderStateString(readerState->dwCurrentState);
		char* szEventState = SCardGetReaderStateString(readerState->dwEventState);
		WLog_Print(log, g_LogLevel, "\t[%" PRIu32 "]: dwCurrentState: %s (0x%08" PRIX32 ")", index,
		           szCurrentState, readerState->dwCurrentState);
		WLog_Print(log, g_LogLevel, "\t[%" PRIu32 "]: dwEventState: %s (0x%08" PRIX32 ")", index,
		           szEventState, readerState->dwEventState);
		free(szCurrentState);
		free(szEventState);

		WLog_Print(
		    log, g_LogLevel, "\t[%" PRIu32 "]: cbAttr: %" PRIu32 " { %s }", index,
		    readerState->cbAtr,
		    smartcard_array_dump(readerState->rgbAtr, readerState->cbAtr, buffer, sizeof(buffer)));
	}
}

static void dump_reader_states_a(wLog* log, const SCARD_READERSTATEA* rgReaderStates,
                                 UINT32 cReaders)
{
	WINPR_ASSERT(rgReaderStates || (cReaders == 0));
	for (UINT32 index = 0; index < cReaders; index++)
	{
		char buffer[1024] = { 0 };

		const SCARD_READERSTATEA* readerState = &rgReaderStates[index];

		WLog_Print(log, g_LogLevel, "\t[%" PRIu32 "]: szReader: %s cbAtr: %" PRIu32 "", index,
		           readerState->szReader, readerState->cbAtr);
		char* szCurrentState = SCardGetReaderStateString(readerState->dwCurrentState);
		char* szEventState = SCardGetReaderStateString(readerState->dwEventState);
		WLog_Print(log, g_LogLevel, "\t[%" PRIu32 "]: dwCurrentState: %s (0x%08" PRIX32 ")", index,
		           szCurrentState, readerState->dwCurrentState);
		WLog_Print(log, g_LogLevel, "\t[%" PRIu32 "]: dwEventState: %s (0x%08" PRIX32 ")", index,
		           szEventState, readerState->dwEventState);
		free(szCurrentState);
		free(szEventState);

		WLog_Print(
		    log, g_LogLevel, "\t[%" PRIu32 "]: cbAttr: %" PRIu32 " { %s }", index,
		    readerState->cbAtr,
		    smartcard_array_dump(readerState->rgbAtr, readerState->cbAtr, buffer, sizeof(buffer)));
	}
}

static void dump_reader_states_w(wLog* log, const SCARD_READERSTATEW* rgReaderStates,
                                 UINT32 cReaders)
{
	WINPR_ASSERT(rgReaderStates || (cReaders == 0));
	for (UINT32 index = 0; index < cReaders; index++)
	{
		char buffer[1024] = { 0 };

		const SCARD_READERSTATEW* readerState = &rgReaderStates[index];
		(void)ConvertWCharToUtf8(readerState->szReader, buffer, sizeof(buffer));
		WLog_Print(log, g_LogLevel, "\t[%" PRIu32 "]: szReader: %s cbAtr: %" PRIu32 "", index,
		           buffer, readerState->cbAtr);
		char* szCurrentState = SCardGetReaderStateString(readerState->dwCurrentState);
		char* szEventState = SCardGetReaderStateString(readerState->dwEventState);
		WLog_Print(log, g_LogLevel, "\t[%" PRIu32 "]: dwCurrentState: %s (0x%08" PRIX32 ")", index,
		           szCurrentState, readerState->dwCurrentState);
		WLog_Print(log, g_LogLevel, "\t[%" PRIu32 "]: dwEventState: %s (0x%08" PRIX32 ")", index,
		           szEventState, readerState->dwEventState);
		free(szCurrentState);
		free(szEventState);

		WLog_Print(
		    log, g_LogLevel, "\t[%" PRIu32 "]: cbAttr: %" PRIu32 " { %s }", index,
		    readerState->cbAtr,
		    smartcard_array_dump(readerState->rgbAtr, readerState->cbAtr, buffer, sizeof(buffer)));
	}
}

static void smartcard_trace_get_status_change_w_call(wLog* log, const GetStatusChangeW_Call* call)
{
	WINPR_ASSERT(call);

	if (!WLog_IsLevelActive(log, g_LogLevel))
		return;

	WLog_Print(log, g_LogLevel, "GetStatusChangeW_Call {");
	smartcard_log_context(log, &call->handles.hContext);

	WLog_Print(log, g_LogLevel, "dwTimeOut: 0x%08" PRIX32 " cReaders: %" PRIu32 "", call->dwTimeOut,
	           call->cReaders);

	dump_reader_states_w(log, call->rgReaderStates, call->cReaders);

	WLog_Print(log, g_LogLevel, "}");
}

static void smartcard_trace_list_reader_groups_return(wLog* log, const ListReaderGroups_Return* ret,
                                                      BOOL unicode)
{
	WINPR_ASSERT(ret);

	if (!WLog_IsLevelActive(log, g_LogLevel))
		return;

	char* mszA = smartcard_convert_string_list(ret->msz, ret->cBytes, unicode);

	WLog_Print(log, g_LogLevel, "ListReaderGroups%s_Return {", unicode ? "W" : "A");
	WLog_Print(log, g_LogLevel, "  ReturnCode: %s (0x%08" PRIx32 ")",
	           SCardGetErrorString(ret->ReturnCode),
	           WINPR_CXX_COMPAT_CAST(UINT32, ret->ReturnCode));
	WLog_Print(log, g_LogLevel, "  cBytes: %" PRIu32 " msz: %s", ret->cBytes, mszA);
	WLog_Print(log, g_LogLevel, "}");
	free(mszA);
}

static void smartcard_trace_list_readers_call(wLog* log, const ListReaders_Call* call, BOOL unicode)
{
	WINPR_ASSERT(call);

	if (!WLog_IsLevelActive(log, g_LogLevel))
		return;

	char* mszGroupsA = smartcard_convert_string_list(call->mszGroups, call->cBytes, unicode);

	WLog_Print(log, g_LogLevel, "ListReaders%s_Call {", unicode ? "W" : "A");
	smartcard_log_context(log, &call->handles.hContext);

	WLog_Print(log, g_LogLevel,
	           "cBytes: %" PRIu32 " mszGroups: %s fmszReadersIsNULL: %" PRId32
	           " cchReaders: 0x%08" PRIX32 "",
	           call->cBytes, mszGroupsA, call->fmszReadersIsNULL, call->cchReaders);
	WLog_Print(log, g_LogLevel, "}");

	free(mszGroupsA);
}

static void smartcard_trace_locate_cards_by_atr_a_call(wLog* log,
                                                       const LocateCardsByATRA_Call* call)
{
	WINPR_ASSERT(call);

	if (!WLog_IsLevelActive(log, g_LogLevel))
		return;

	WLog_Print(log, g_LogLevel, "LocateCardsByATRA_Call {");
	smartcard_log_context(log, &call->handles.hContext);

	dump_reader_states_a(log, call->rgReaderStates, call->cReaders);

	WLog_Print(log, g_LogLevel, "}");
}

static void smartcard_trace_locate_cards_a_call(wLog* log, const LocateCardsA_Call* call)
{
	char buffer[8192] = { 0 };

	WINPR_ASSERT(call);

	if (!WLog_IsLevelActive(log, g_LogLevel))
		return;

	WLog_Print(log, g_LogLevel, "LocateCardsA_Call {");
	smartcard_log_context(log, &call->handles.hContext);
	WLog_Print(log, g_LogLevel, " cBytes=%" PRIu32, call->cBytes);
	WLog_Print(log, g_LogLevel, " mszCards=%s",
	           smartcard_msz_dump_a(call->mszCards, call->cBytes, buffer, sizeof(buffer)));
	WLog_Print(log, g_LogLevel, " cReaders=%" PRIu32, call->cReaders);
	dump_reader_states_a(log, call->rgReaderStates, call->cReaders);

	WLog_Print(log, g_LogLevel, "}");
}

static void smartcard_trace_locate_cards_return(wLog* log, const LocateCards_Return* ret)
{
	WINPR_ASSERT(ret);

	if (!WLog_IsLevelActive(log, g_LogLevel))
		return;

	WLog_Print(log, g_LogLevel, "LocateCards_Return {");
	WLog_Print(log, g_LogLevel, "  ReturnCode: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(ret->ReturnCode),
	           WINPR_CXX_COMPAT_CAST(UINT32, ret->ReturnCode));

	if (ret->ReturnCode == SCARD_S_SUCCESS)
	{
		WLog_Print(log, g_LogLevel, "  cReaders=%" PRIu32, ret->cReaders);
	}
	WLog_Print(log, g_LogLevel, "}");
}

static void smartcard_trace_get_reader_icon_return(wLog* log, const GetReaderIcon_Return* ret)
{
	WINPR_ASSERT(ret);

	if (!WLog_IsLevelActive(log, g_LogLevel))
		return;

	WLog_Print(log, g_LogLevel, "GetReaderIcon_Return {");
	WLog_Print(log, g_LogLevel, "  ReturnCode: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(ret->ReturnCode),
	           WINPR_CXX_COMPAT_CAST(UINT32, ret->ReturnCode));

	if (ret->ReturnCode == SCARD_S_SUCCESS)
	{
		WLog_Print(log, g_LogLevel, "  cbDataLen=%" PRIu32, ret->cbDataLen);
	}
	WLog_Print(log, g_LogLevel, "}");
}

static void smartcard_trace_get_transmit_count_return(wLog* log, const GetTransmitCount_Return* ret)
{
	WINPR_ASSERT(ret);

	if (!WLog_IsLevelActive(log, g_LogLevel))
		return;

	WLog_Print(log, g_LogLevel, "GetTransmitCount_Return {");
	WLog_Print(log, g_LogLevel, "  ReturnCode: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(ret->ReturnCode),
	           WINPR_CXX_COMPAT_CAST(UINT32, ret->ReturnCode));

	WLog_Print(log, g_LogLevel, "  cTransmitCount=%" PRIu32, ret->cTransmitCount);
	WLog_Print(log, g_LogLevel, "}");
}

static void smartcard_trace_read_cache_return(wLog* log, const ReadCache_Return* ret)
{
	WINPR_ASSERT(ret);

	if (!WLog_IsLevelActive(log, g_LogLevel))
		return;

	WLog_Print(log, g_LogLevel, "ReadCache_Return {");
	WLog_Print(log, g_LogLevel, "  ReturnCode: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(ret->ReturnCode),
	           WINPR_CXX_COMPAT_CAST(UINT32, ret->ReturnCode));

	if (ret->ReturnCode == SCARD_S_SUCCESS)
	{
		char buffer[1024] = { 0 };
		WLog_Print(log, g_LogLevel, " cbDataLen=%" PRIu32, ret->cbDataLen);
		WLog_Print(log, g_LogLevel, "  cbData: %s",
		           smartcard_array_dump(ret->pbData, ret->cbDataLen, buffer, sizeof(buffer)));
	}
	WLog_Print(log, g_LogLevel, "}");
}

static void smartcard_trace_locate_cards_w_call(wLog* log, const LocateCardsW_Call* call)
{
	WINPR_ASSERT(call);
	char buffer[8192] = { 0 };

	if (!WLog_IsLevelActive(log, g_LogLevel))
		return;

	WLog_Print(log, g_LogLevel, "LocateCardsW_Call {");
	smartcard_log_context(log, &call->handles.hContext);
	WLog_Print(log, g_LogLevel, " cBytes=%" PRIu32, call->cBytes);
	WLog_Print(log, g_LogLevel, " sz2=%s",
	           smartcard_msz_dump_w(call->mszCards, call->cBytes, buffer, sizeof(buffer)));
	WLog_Print(log, g_LogLevel, " cReaders=%" PRIu32, call->cReaders);
	dump_reader_states_w(log, call->rgReaderStates, call->cReaders);
	WLog_Print(log, g_LogLevel, "}");
}

static void smartcard_trace_list_readers_return(wLog* log, const ListReaders_Return* ret,
                                                BOOL unicode)
{
	WINPR_ASSERT(ret);

	if (!WLog_IsLevelActive(log, g_LogLevel))
		return;

	WLog_Print(log, g_LogLevel, "ListReaders%s_Return {", unicode ? "W" : "A");
	WLog_Print(log, g_LogLevel, "  ReturnCode: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(ret->ReturnCode),
	           WINPR_CXX_COMPAT_CAST(UINT32, ret->ReturnCode));

	if (ret->ReturnCode != SCARD_S_SUCCESS)
	{
		WLog_Print(log, g_LogLevel, "}");
		return;
	}

	char* mszA = smartcard_convert_string_list(ret->msz, ret->cBytes, unicode);

	WLog_Print(log, g_LogLevel, "  cBytes: %" PRIu32 " msz: %s", ret->cBytes, mszA);
	WLog_Print(log, g_LogLevel, "}");
	free(mszA);
}

static void smartcard_trace_get_status_change_return(wLog* log, const GetStatusChange_Return* ret,
                                                     BOOL unicode)
{
	WINPR_ASSERT(ret);

	if (!WLog_IsLevelActive(log, g_LogLevel))
		return;

	WLog_Print(log, g_LogLevel, "GetStatusChange%s_Return {", unicode ? "W" : "A");
	WLog_Print(log, g_LogLevel, "  ReturnCode: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(ret->ReturnCode),
	           WINPR_CXX_COMPAT_CAST(UINT32, ret->ReturnCode));
	WLog_Print(log, g_LogLevel, "  cReaders: %" PRIu32 "", ret->cReaders);

	dump_reader_states_return(log, ret->rgReaderStates, ret->cReaders);

	if (!ret->rgReaderStates && (ret->cReaders > 0))
	{
		WLog_Print(log, g_LogLevel, "    [INVALID STATE] rgReaderStates=NULL, cReaders=%" PRIu32,
		           ret->cReaders);
	}
	else if (ret->ReturnCode != SCARD_S_SUCCESS)
	{
		WLog_Print(log, g_LogLevel, "    [INVALID RETURN] rgReaderStates, cReaders=%" PRIu32,
		           ret->cReaders);
	}
	else
	{
		for (UINT32 index = 0; index < ret->cReaders; index++)
		{
			char buffer[1024] = { 0 };
			const ReaderState_Return* rgReaderState = &(ret->rgReaderStates[index]);
			char* szCurrentState = SCardGetReaderStateString(rgReaderState->dwCurrentState);
			char* szEventState = SCardGetReaderStateString(rgReaderState->dwEventState);
			WLog_Print(log, g_LogLevel, "    [%" PRIu32 "]: dwCurrentState: %s (0x%08" PRIX32 ")",
			           index, szCurrentState, rgReaderState->dwCurrentState);
			WLog_Print(log, g_LogLevel, "    [%" PRIu32 "]: dwEventState: %s (0x%08" PRIX32 ")",
			           index, szEventState, rgReaderState->dwEventState);
			WLog_Print(log, g_LogLevel, "    [%" PRIu32 "]: cbAtr: %" PRIu32 " rgbAtr: %s", index,
			           rgReaderState->cbAtr,
			           smartcard_array_dump(rgReaderState->rgbAtr, rgReaderState->cbAtr, buffer,
			                                sizeof(buffer)));
			free(szCurrentState);
			free(szEventState);
		}
	}

	WLog_Print(log, g_LogLevel, "}");
}

static void smartcard_trace_context_and_two_strings_a_call(wLog* log,
                                                           const ContextAndTwoStringA_Call* call)
{
	WINPR_ASSERT(call);

	if (!WLog_IsLevelActive(log, g_LogLevel))
		return;

	WLog_Print(log, g_LogLevel, "ContextAndTwoStringW_Call {");
	smartcard_log_context(log, &call->handles.hContext);
	WLog_Print(log, g_LogLevel, " sz1=%s", call->sz1);
	WLog_Print(log, g_LogLevel, " sz2=%s", call->sz2);
	WLog_Print(log, g_LogLevel, "}");
}

static void smartcard_trace_context_and_two_strings_w_call(wLog* log,
                                                           const ContextAndTwoStringW_Call* call)
{
	WINPR_ASSERT(call);
	char sz1[1024] = { 0 };
	char sz2[1024] = { 0 };

	if (!WLog_IsLevelActive(log, g_LogLevel))
		return;
	if (call->sz1)
		(void)ConvertWCharToUtf8(call->sz1, sz1, ARRAYSIZE(sz1));
	if (call->sz2)
		(void)ConvertWCharToUtf8(call->sz2, sz2, ARRAYSIZE(sz2));

	WLog_Print(log, g_LogLevel, "ContextAndTwoStringW_Call {");
	smartcard_log_context(log, &call->handles.hContext);
	WLog_Print(log, g_LogLevel, " sz1=%s", sz1);
	WLog_Print(log, g_LogLevel, " sz2=%s", sz2);
	WLog_Print(log, g_LogLevel, "}");
}

static void smartcard_trace_get_transmit_count_call(wLog* log, const GetTransmitCount_Call* call)
{
	WINPR_ASSERT(call);

	if (!WLog_IsLevelActive(log, g_LogLevel))
		return;

	WLog_Print(log, g_LogLevel, "GetTransmitCount_Call {");
	smartcard_log_context(log, &call->handles.hContext);
	smartcard_log_redir_handle(log, &call->handles.hCard);

	WLog_Print(log, g_LogLevel, "}");
}

static void smartcard_trace_write_cache_a_call(wLog* log, const WriteCacheA_Call* call)
{
	WINPR_ASSERT(call);
	char buffer[1024] = { 0 };

	if (!WLog_IsLevelActive(log, g_LogLevel))
		return;

	WLog_Print(log, g_LogLevel, "WriteCacheA_Call {");

	WLog_Print(log, g_LogLevel, "  szLookupName=%s", call->szLookupName);

	smartcard_log_context(log, &call->Common.handles.hContext);
	WLog_Print(
	    log, g_LogLevel, "..CardIdentifier=%s",
	    smartcard_array_dump(call->Common.CardIdentifier, sizeof(UUID), buffer, sizeof(buffer)));
	WLog_Print(log, g_LogLevel, "  FreshnessCounter=%" PRIu32, call->Common.FreshnessCounter);
	WLog_Print(log, g_LogLevel, "  cbDataLen=%" PRIu32, call->Common.cbDataLen);
	WLog_Print(
	    log, g_LogLevel, "  pbData=%s",
	    smartcard_array_dump(call->Common.pbData, call->Common.cbDataLen, buffer, sizeof(buffer)));
	WLog_Print(log, g_LogLevel, "}");
}

static void smartcard_trace_write_cache_w_call(wLog* log, const WriteCacheW_Call* call)
{
	WINPR_ASSERT(call);
	char tmp[1024] = { 0 };
	char buffer[1024] = { 0 };

	if (!WLog_IsLevelActive(log, g_LogLevel))
		return;

	WLog_Print(log, g_LogLevel, "WriteCacheW_Call {");

	if (call->szLookupName)
		(void)ConvertWCharToUtf8(call->szLookupName, tmp, ARRAYSIZE(tmp));
	WLog_Print(log, g_LogLevel, "  szLookupName=%s", tmp);

	smartcard_log_context(log, &call->Common.handles.hContext);
	WLog_Print(
	    log, g_LogLevel, "..CardIdentifier=%s",
	    smartcard_array_dump(call->Common.CardIdentifier, sizeof(UUID), buffer, sizeof(buffer)));
	WLog_Print(log, g_LogLevel, "  FreshnessCounter=%" PRIu32, call->Common.FreshnessCounter);
	WLog_Print(log, g_LogLevel, "  cbDataLen=%" PRIu32, call->Common.cbDataLen);
	WLog_Print(
	    log, g_LogLevel, "  pbData=%s",
	    smartcard_array_dump(call->Common.pbData, call->Common.cbDataLen, buffer, sizeof(buffer)));
	WLog_Print(log, g_LogLevel, "}");
}

static void smartcard_trace_read_cache_a_call(wLog* log, const ReadCacheA_Call* call)
{
	WINPR_ASSERT(call);
	char buffer[1024] = { 0 };

	if (!WLog_IsLevelActive(log, g_LogLevel))
		return;

	WLog_Print(log, g_LogLevel, "ReadCacheA_Call {");

	WLog_Print(log, g_LogLevel, "  szLookupName=%s", call->szLookupName);
	smartcard_log_context(log, &call->Common.handles.hContext);
	WLog_Print(
	    log, g_LogLevel, "..CardIdentifier=%s",
	    smartcard_array_dump(call->Common.CardIdentifier, sizeof(UUID), buffer, sizeof(buffer)));
	WLog_Print(log, g_LogLevel, "  FreshnessCounter=%" PRIu32, call->Common.FreshnessCounter);
	WLog_Print(log, g_LogLevel, "  fPbDataIsNULL=%" PRId32, call->Common.fPbDataIsNULL);
	WLog_Print(log, g_LogLevel, "  cbDataLen=%" PRIu32, call->Common.cbDataLen);

	WLog_Print(log, g_LogLevel, "}");
}

static void smartcard_trace_read_cache_w_call(wLog* log, const ReadCacheW_Call* call)
{
	WINPR_ASSERT(call);
	char tmp[1024] = { 0 };
	char buffer[1024] = { 0 };

	if (!WLog_IsLevelActive(log, g_LogLevel))
		return;

	WLog_Print(log, g_LogLevel, "ReadCacheW_Call {");
	if (call->szLookupName)
		(void)ConvertWCharToUtf8(call->szLookupName, tmp, ARRAYSIZE(tmp));
	WLog_Print(log, g_LogLevel, "  szLookupName=%s", tmp);

	smartcard_log_context(log, &call->Common.handles.hContext);
	WLog_Print(
	    log, g_LogLevel, "..CardIdentifier=%s",
	    smartcard_array_dump(call->Common.CardIdentifier, sizeof(UUID), buffer, sizeof(buffer)));
	WLog_Print(log, g_LogLevel, "  FreshnessCounter=%" PRIu32, call->Common.FreshnessCounter);
	WLog_Print(log, g_LogLevel, "  fPbDataIsNULL=%" PRId32, call->Common.fPbDataIsNULL);
	WLog_Print(log, g_LogLevel, "  cbDataLen=%" PRIu32, call->Common.cbDataLen);

	WLog_Print(log, g_LogLevel, "}");
}

static void smartcard_trace_transmit_call(wLog* log, const Transmit_Call* call)
{
	WINPR_ASSERT(call);
	UINT32 cbExtraBytes = 0;
	BYTE* pbExtraBytes = NULL;

	if (!WLog_IsLevelActive(log, g_LogLevel))
		return;

	WLog_Print(log, g_LogLevel, "Transmit_Call {");
	smartcard_log_context(log, &call->handles.hContext);
	smartcard_log_redir_handle(log, &call->handles.hCard);

	if (call->pioSendPci)
	{
		cbExtraBytes = (UINT32)(call->pioSendPci->cbPciLength - sizeof(SCARD_IO_REQUEST));
		pbExtraBytes = &((BYTE*)call->pioSendPci)[sizeof(SCARD_IO_REQUEST)];
		WLog_Print(log, g_LogLevel, "pioSendPci: dwProtocol: %" PRIu32 " cbExtraBytes: %" PRIu32 "",
		           call->pioSendPci->dwProtocol, cbExtraBytes);

		if (cbExtraBytes)
		{
			char buffer[1024] = { 0 };
			WLog_Print(log, g_LogLevel, "pbExtraBytes: %s",
			           smartcard_array_dump(pbExtraBytes, cbExtraBytes, buffer, sizeof(buffer)));
		}
	}
	else
	{
		WLog_Print(log, g_LogLevel, "pioSendPci: null");
	}

	WLog_Print(log, g_LogLevel, "cbSendLength: %" PRIu32 "", call->cbSendLength);

	if (call->pbSendBuffer)
	{
		char buffer[1024] = { 0 };
		WLog_Print(
		    log, g_LogLevel, "pbSendBuffer: %s",
		    smartcard_array_dump(call->pbSendBuffer, call->cbSendLength, buffer, sizeof(buffer)));
	}
	else
	{
		WLog_Print(log, g_LogLevel, "pbSendBuffer: null");
	}

	if (call->pioRecvPci)
	{
		cbExtraBytes = (UINT32)(call->pioRecvPci->cbPciLength - sizeof(SCARD_IO_REQUEST));
		pbExtraBytes = &((BYTE*)call->pioRecvPci)[sizeof(SCARD_IO_REQUEST)];
		WLog_Print(log, g_LogLevel, "pioRecvPci: dwProtocol: %" PRIu32 " cbExtraBytes: %" PRIu32 "",
		           call->pioRecvPci->dwProtocol, cbExtraBytes);

		if (cbExtraBytes)
		{
			char buffer[1024] = { 0 };
			WLog_Print(log, g_LogLevel, "pbExtraBytes: %s",
			           smartcard_array_dump(pbExtraBytes, cbExtraBytes, buffer, sizeof(buffer)));
		}
	}
	else
	{
		WLog_Print(log, g_LogLevel, "pioRecvPci: null");
	}

	WLog_Print(log, g_LogLevel, "fpbRecvBufferIsNULL: %" PRId32 " cbRecvLength: %" PRIu32 "",
	           call->fpbRecvBufferIsNULL, call->cbRecvLength);
	WLog_Print(log, g_LogLevel, "}");
}

static void smartcard_trace_locate_cards_by_atr_w_call(wLog* log,
                                                       const LocateCardsByATRW_Call* call)
{
	WINPR_ASSERT(call);

	if (!WLog_IsLevelActive(log, g_LogLevel))
		return;

	WLog_Print(log, g_LogLevel, "LocateCardsByATRW_Call {");
	smartcard_log_context(log, &call->handles.hContext);

	dump_reader_states_w(log, call->rgReaderStates, call->cReaders);

	WLog_Print(log, g_LogLevel, "}");
}

static void smartcard_trace_transmit_return(wLog* log, const Transmit_Return* ret)
{
	WINPR_ASSERT(ret);
	UINT32 cbExtraBytes = 0;
	BYTE* pbExtraBytes = NULL;

	if (!WLog_IsLevelActive(log, g_LogLevel))
		return;

	WLog_Print(log, g_LogLevel, "Transmit_Return {");
	WLog_Print(log, g_LogLevel, "  ReturnCode: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(ret->ReturnCode),
	           WINPR_CXX_COMPAT_CAST(UINT32, ret->ReturnCode));

	if (ret->pioRecvPci)
	{
		cbExtraBytes = (UINT32)(ret->pioRecvPci->cbPciLength - sizeof(SCARD_IO_REQUEST));
		pbExtraBytes = &((BYTE*)ret->pioRecvPci)[sizeof(SCARD_IO_REQUEST)];
		WLog_Print(log, g_LogLevel,
		           "  pioRecvPci: dwProtocol: %" PRIu32 " cbExtraBytes: %" PRIu32 "",
		           ret->pioRecvPci->dwProtocol, cbExtraBytes);

		if (cbExtraBytes)
		{
			char buffer[1024] = { 0 };
			WLog_Print(log, g_LogLevel, "  pbExtraBytes: %s",
			           smartcard_array_dump(pbExtraBytes, cbExtraBytes, buffer, sizeof(buffer)));
		}
	}
	else
	{
		WLog_Print(log, g_LogLevel, "  pioRecvPci: null");
	}

	WLog_Print(log, g_LogLevel, "  cbRecvLength: %" PRIu32 "", ret->cbRecvLength);

	if (ret->pbRecvBuffer)
	{
		char buffer[1024] = { 0 };
		WLog_Print(
		    log, g_LogLevel, "  pbRecvBuffer: %s",
		    smartcard_array_dump(ret->pbRecvBuffer, ret->cbRecvLength, buffer, sizeof(buffer)));
	}
	else
	{
		WLog_Print(log, g_LogLevel, "  pbRecvBuffer: null");
	}

	WLog_Print(log, g_LogLevel, "}");
}

static void smartcard_trace_control_return(wLog* log, const Control_Return* ret)
{
	WINPR_ASSERT(ret);

	if (!WLog_IsLevelActive(log, g_LogLevel))
		return;

	WLog_Print(log, g_LogLevel, "Control_Return {");
	WLog_Print(log, g_LogLevel, "  ReturnCode: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(ret->ReturnCode),
	           WINPR_CXX_COMPAT_CAST(UINT32, ret->ReturnCode));
	WLog_Print(log, g_LogLevel, "  cbOutBufferSize: %" PRIu32 "", ret->cbOutBufferSize);

	if (ret->pvOutBuffer)
	{
		char buffer[1024] = { 0 };
		WLog_Print(
		    log, g_LogLevel, "pvOutBuffer: %s",
		    smartcard_array_dump(ret->pvOutBuffer, ret->cbOutBufferSize, buffer, sizeof(buffer)));
	}
	else
	{
		WLog_Print(log, g_LogLevel, "pvOutBuffer: null");
	}

	WLog_Print(log, g_LogLevel, "}");
}

static void smartcard_trace_control_call(wLog* log, const Control_Call* call)
{
	WINPR_ASSERT(call);

	if (!WLog_IsLevelActive(log, g_LogLevel))
		return;

	WLog_Print(log, g_LogLevel, "Control_Call {");
	smartcard_log_context(log, &call->handles.hContext);
	smartcard_log_redir_handle(log, &call->handles.hCard);

	WLog_Print(log, g_LogLevel,
	           "dwControlCode: 0x%08" PRIX32 " cbInBufferSize: %" PRIu32
	           " fpvOutBufferIsNULL: %" PRId32 " cbOutBufferSize: %" PRIu32 "",
	           call->dwControlCode, call->cbInBufferSize, call->fpvOutBufferIsNULL,
	           call->cbOutBufferSize);

	if (call->pvInBuffer)
	{
		char buffer[1024] = { 0 };
		WLog_Print(
		    log, WLOG_DEBUG, "pbInBuffer: %s",
		    smartcard_array_dump(call->pvInBuffer, call->cbInBufferSize, buffer, sizeof(buffer)));
	}
	else
	{
		WLog_Print(log, g_LogLevel, "pvInBuffer: null");
	}

	WLog_Print(log, g_LogLevel, "}");
}

static void smartcard_trace_set_attrib_call(wLog* log, const SetAttrib_Call* call)
{
	WINPR_ASSERT(call);
	char buffer[8192] = { 0 };

	if (!WLog_IsLevelActive(log, g_LogLevel))
		return;

	WLog_Print(log, g_LogLevel, "GetAttrib_Call {");
	smartcard_log_context(log, &call->handles.hContext);
	smartcard_log_redir_handle(log, &call->handles.hCard);
	WLog_Print(log, g_LogLevel, "dwAttrId: 0x%08" PRIX32, call->dwAttrId);
	WLog_Print(log, g_LogLevel, "cbAttrLen: 0x%08" PRIx32, call->cbAttrLen);
	WLog_Print(log, g_LogLevel, "pbAttr: %s",
	           smartcard_array_dump(call->pbAttr, call->cbAttrLen, buffer, sizeof(buffer)));
	WLog_Print(log, g_LogLevel, "}");
}

static void smartcard_trace_get_attrib_return(wLog* log, const GetAttrib_Return* ret,
                                              DWORD dwAttrId)
{
	WINPR_ASSERT(ret);
	char buffer[1024] = { 0 };

	if (!WLog_IsLevelActive(log, g_LogLevel))
		return;

	WLog_Print(log, g_LogLevel, "GetAttrib_Return {");
	WLog_Print(log, g_LogLevel, "  ReturnCode: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(ret->ReturnCode),
	           WINPR_CXX_COMPAT_CAST(UINT32, ret->ReturnCode));
	WLog_Print(log, g_LogLevel, "  dwAttrId: %s (0x%08" PRIX32 ") cbAttrLen: 0x%08" PRIX32 "",
	           SCardGetAttributeString(dwAttrId), dwAttrId, ret->cbAttrLen);
	WLog_Print(log, g_LogLevel, "  %s",
	           smartcard_array_dump(ret->pbAttr, ret->cbAttrLen, buffer, sizeof(buffer)));

	WLog_Print(log, g_LogLevel, "}");
}

static void smartcard_trace_get_attrib_call(wLog* log, const GetAttrib_Call* call)
{
	WINPR_ASSERT(call);

	if (!WLog_IsLevelActive(log, g_LogLevel))
		return;

	WLog_Print(log, g_LogLevel, "GetAttrib_Call {");
	smartcard_log_context(log, &call->handles.hContext);
	smartcard_log_redir_handle(log, &call->handles.hCard);

	WLog_Print(log, g_LogLevel,
	           "dwAttrId: %s (0x%08" PRIX32 ") fpbAttrIsNULL: %" PRId32 " cbAttrLen: 0x%08" PRIX32
	           "",
	           SCardGetAttributeString(call->dwAttrId), call->dwAttrId, call->fpbAttrIsNULL,
	           call->cbAttrLen);
	WLog_Print(log, g_LogLevel, "}");
}

static void smartcard_trace_status_call(wLog* log, const Status_Call* call, BOOL unicode)
{
	WINPR_ASSERT(call);

	if (!WLog_IsLevelActive(log, g_LogLevel))
		return;

	WLog_Print(log, g_LogLevel, "Status%s_Call {", unicode ? "W" : "A");
	smartcard_log_context(log, &call->handles.hContext);
	smartcard_log_redir_handle(log, &call->handles.hCard);

	WLog_Print(log, g_LogLevel,
	           "fmszReaderNamesIsNULL: %" PRId32 " cchReaderLen: %" PRIu32 " cbAtrLen: %" PRIu32 "",
	           call->fmszReaderNamesIsNULL, call->cchReaderLen, call->cbAtrLen);
	WLog_Print(log, g_LogLevel, "}");
}

static void smartcard_trace_status_return(wLog* log, const Status_Return* ret, BOOL unicode)
{
	WINPR_ASSERT(ret);
	char* mszReaderNamesA = NULL;
	char buffer[1024] = { 0 };
	DWORD cBytes = 0;

	if (!WLog_IsLevelActive(log, g_LogLevel))
		return;
	cBytes = ret->cBytes;
	if (ret->ReturnCode != SCARD_S_SUCCESS)
		cBytes = 0;
	if (cBytes == SCARD_AUTOALLOCATE)
		cBytes = 0;
	mszReaderNamesA = smartcard_convert_string_list(ret->mszReaderNames, cBytes, unicode);

	WLog_Print(log, g_LogLevel, "Status%s_Return {", unicode ? "W" : "A");
	WLog_Print(log, g_LogLevel, "  ReturnCode: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(ret->ReturnCode),
	           WINPR_CXX_COMPAT_CAST(UINT32, ret->ReturnCode));
	WLog_Print(log, g_LogLevel, "  dwState: %s (0x%08" PRIX32 ") dwProtocol: %s (0x%08" PRIX32 ")",
	           SCardGetCardStateString(ret->dwState), ret->dwState,
	           SCardGetProtocolString(ret->dwProtocol), ret->dwProtocol);

	WLog_Print(log, g_LogLevel, "  cBytes: %" PRIu32 " mszReaderNames: %s", ret->cBytes,
	           mszReaderNamesA);

	WLog_Print(log, g_LogLevel, "  cbAtrLen: %" PRIu32 " pbAtr: %s", ret->cbAtrLen,
	           smartcard_array_dump(ret->pbAtr, ret->cbAtrLen, buffer, sizeof(buffer)));
	WLog_Print(log, g_LogLevel, "}");
	free(mszReaderNamesA);
}

static void smartcard_trace_state_return(wLog* log, const State_Return* ret)
{
	WINPR_ASSERT(ret);
	char buffer[1024] = { 0 };
	char* state = NULL;

	if (!WLog_IsLevelActive(log, g_LogLevel))
		return;

	state = SCardGetReaderStateString(ret->dwState);
	WLog_Print(log, g_LogLevel, "Reconnect_Return {");
	WLog_Print(log, g_LogLevel, "  ReturnCode: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(ret->ReturnCode),
	           WINPR_CXX_COMPAT_CAST(UINT32, ret->ReturnCode));
	WLog_Print(log, g_LogLevel, "  dwState:    %s (0x%08" PRIX32 ")", state, ret->dwState);
	WLog_Print(log, g_LogLevel, "  dwProtocol: %s (0x%08" PRIX32 ")",
	           SCardGetProtocolString(ret->dwProtocol), ret->dwProtocol);
	WLog_Print(log, g_LogLevel, "  cbAtrLen:      (0x%08" PRIX32 ")", ret->cbAtrLen);
	WLog_Print(log, g_LogLevel, "  rgAtr:      %s",
	           smartcard_array_dump(ret->rgAtr, sizeof(ret->rgAtr), buffer, sizeof(buffer)));
	WLog_Print(log, g_LogLevel, "}");
	free(state);
}

static void smartcard_trace_reconnect_return(wLog* log, const Reconnect_Return* ret)
{
	WINPR_ASSERT(ret);

	if (!WLog_IsLevelActive(log, g_LogLevel))
		return;

	WLog_Print(log, g_LogLevel, "Reconnect_Return {");
	WLog_Print(log, g_LogLevel, "  ReturnCode: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(ret->ReturnCode),
	           WINPR_CXX_COMPAT_CAST(UINT32, ret->ReturnCode));
	WLog_Print(log, g_LogLevel, "  dwActiveProtocol: %s (0x%08" PRIX32 ")",
	           SCardGetProtocolString(ret->dwActiveProtocol), ret->dwActiveProtocol);
	WLog_Print(log, g_LogLevel, "}");
}

static void smartcard_trace_connect_a_call(wLog* log, const ConnectA_Call* call)
{
	WINPR_ASSERT(call);

	if (!WLog_IsLevelActive(log, g_LogLevel))
		return;

	WLog_Print(log, g_LogLevel, "ConnectA_Call {");
	smartcard_log_context(log, &call->Common.handles.hContext);

	WLog_Print(log, g_LogLevel,
	           "szReader: %s dwShareMode: %s (0x%08" PRIX32
	           ") dwPreferredProtocols: %s (0x%08" PRIX32 ")",
	           call->szReader, SCardGetShareModeString(call->Common.dwShareMode),
	           call->Common.dwShareMode, SCardGetProtocolString(call->Common.dwPreferredProtocols),
	           call->Common.dwPreferredProtocols);
	WLog_Print(log, g_LogLevel, "}");
}

static void smartcard_trace_connect_w_call(wLog* log, const ConnectW_Call* call)
{
	WINPR_ASSERT(call);
	char szReaderA[1024] = { 0 };

	if (!WLog_IsLevelActive(log, g_LogLevel))
		return;

	if (call->szReader)
		(void)ConvertWCharToUtf8(call->szReader, szReaderA, ARRAYSIZE(szReaderA));
	WLog_Print(log, g_LogLevel, "ConnectW_Call {");
	smartcard_log_context(log, &call->Common.handles.hContext);

	WLog_Print(log, g_LogLevel,
	           "szReader: %s dwShareMode: %s (0x%08" PRIX32
	           ") dwPreferredProtocols: %s (0x%08" PRIX32 ")",
	           szReaderA, SCardGetShareModeString(call->Common.dwShareMode),
	           call->Common.dwShareMode, SCardGetProtocolString(call->Common.dwPreferredProtocols),
	           call->Common.dwPreferredProtocols);
	WLog_Print(log, g_LogLevel, "}");
}

static void smartcard_trace_hcard_and_disposition_call(wLog* log,
                                                       const HCardAndDisposition_Call* call,
                                                       const char* name)
{
	WINPR_ASSERT(call);

	if (!WLog_IsLevelActive(log, g_LogLevel))
		return;

	WLog_Print(log, g_LogLevel, "%s_Call {", name);
	smartcard_log_context(log, &call->handles.hContext);
	smartcard_log_redir_handle(log, &call->handles.hCard);

	WLog_Print(log, g_LogLevel, "dwDisposition: %s (0x%08" PRIX32 ")",
	           SCardGetDispositionString(call->dwDisposition), call->dwDisposition);
	WLog_Print(log, g_LogLevel, "}");
}

static void smartcard_trace_establish_context_call(wLog* log, const EstablishContext_Call* call)
{
	WINPR_ASSERT(call);

	if (!WLog_IsLevelActive(log, g_LogLevel))
		return;

	WLog_Print(log, g_LogLevel, "EstablishContext_Call {");
	WLog_Print(log, g_LogLevel, "dwScope: %s (0x%08" PRIX32 ")", SCardGetScopeString(call->dwScope),
	           call->dwScope);
	WLog_Print(log, g_LogLevel, "}");
}

static void smartcard_trace_establish_context_return(wLog* log, const EstablishContext_Return* ret)
{
	WINPR_ASSERT(ret);

	if (!WLog_IsLevelActive(log, g_LogLevel))
		return;

	WLog_Print(log, g_LogLevel, "EstablishContext_Return {");
	WLog_Print(log, g_LogLevel, "  ReturnCode: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(ret->ReturnCode),
	           WINPR_CXX_COMPAT_CAST(UINT32, ret->ReturnCode));
	smartcard_log_context(log, &ret->hContext);

	WLog_Print(log, g_LogLevel, "}");
}

void smartcard_trace_long_return(const Long_Return* ret, const char* name)
{
	wLog* log = scard_log();
	smartcard_trace_long_return_int(log, ret, name);
}

void smartcard_trace_long_return_int(wLog* log, const Long_Return* ret, const char* name)
{
	WINPR_ASSERT(ret);

	if (!WLog_IsLevelActive(log, g_LogLevel))
		return;

	WLog_Print(log, g_LogLevel, "%s_Return {", name);
	WLog_Print(log, g_LogLevel, "  ReturnCode: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(ret->ReturnCode),
	           WINPR_CXX_COMPAT_CAST(UINT32, ret->ReturnCode));
	WLog_Print(log, g_LogLevel, "}");
}

static void smartcard_trace_connect_return(wLog* log, const Connect_Return* ret)
{
	WINPR_ASSERT(ret);

	if (!WLog_IsLevelActive(log, g_LogLevel))
		return;

	WLog_Print(log, g_LogLevel, "Connect_Return {");
	WLog_Print(log, g_LogLevel, "  ReturnCode: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(ret->ReturnCode),
	           WINPR_CXX_COMPAT_CAST(UINT32, ret->ReturnCode));
	smartcard_log_context(log, &ret->hContext);
	smartcard_log_redir_handle(log, &ret->hCard);

	WLog_Print(log, g_LogLevel, "  dwActiveProtocol: %s (0x%08" PRIX32 ")",
	           SCardGetProtocolString(ret->dwActiveProtocol), ret->dwActiveProtocol);
	WLog_Print(log, g_LogLevel, "}");
}

static void smartcard_trace_reconnect_call(wLog* log, const Reconnect_Call* call)
{
	WINPR_ASSERT(call);

	if (!WLog_IsLevelActive(log, g_LogLevel))
		return;

	WLog_Print(log, g_LogLevel, "Reconnect_Call {");
	smartcard_log_context(log, &call->handles.hContext);
	smartcard_log_redir_handle(log, &call->handles.hCard);

	WLog_Print(log, g_LogLevel,
	           "dwShareMode: %s (0x%08" PRIX32 ") dwPreferredProtocols: %s (0x%08" PRIX32
	           ") dwInitialization: %s (0x%08" PRIX32 ")",
	           SCardGetShareModeString(call->dwShareMode), call->dwShareMode,
	           SCardGetProtocolString(call->dwPreferredProtocols), call->dwPreferredProtocols,
	           SCardGetDispositionString(call->dwInitialization), call->dwInitialization);
	WLog_Print(log, g_LogLevel, "}");
}

static void smartcard_trace_device_type_id_return(wLog* log, const GetDeviceTypeId_Return* ret)
{
	WINPR_ASSERT(ret);

	if (!WLog_IsLevelActive(log, g_LogLevel))
		return;

	WLog_Print(log, g_LogLevel, "GetDeviceTypeId_Return {");
	WLog_Print(log, g_LogLevel, "  ReturnCode: %s (0x%08" PRIX32 ")",
	           SCardGetErrorString(ret->ReturnCode),
	           WINPR_CXX_COMPAT_CAST(UINT32, ret->ReturnCode));
	WLog_Print(log, g_LogLevel, "  dwDeviceId=%08" PRIx32, ret->dwDeviceId);

	WLog_Print(log, g_LogLevel, "}");
}

static LONG smartcard_unpack_common_context_and_string_a(wLog* log, wStream* s,
                                                         REDIR_SCARDCONTEXT* phContext,
                                                         CHAR** pszReaderName)
{
	UINT32 index = 0;
	UINT32 pbContextNdrPtr = 0;
	LONG status = smartcard_unpack_redir_scard_context(log, s, phContext, &index, &pbContextNdrPtr);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!smartcard_ndr_pointer_read(log, s, &index, NULL))
		return ERROR_INVALID_DATA;

	status = smartcard_unpack_redir_scard_context_ref(log, s, pbContextNdrPtr, phContext);
	if (status != SCARD_S_SUCCESS)
		return status;

	status = smartcard_ndr_read_a(log, s, pszReaderName, NDR_PTR_FULL);
	if (status != SCARD_S_SUCCESS)
		return status;

	smartcard_trace_context_and_string_call_a(log, __func__, phContext, *pszReaderName);
	return SCARD_S_SUCCESS;
}

static LONG smartcard_unpack_common_context_and_string_w(wLog* log, wStream* s,
                                                         REDIR_SCARDCONTEXT* phContext,
                                                         WCHAR** pszReaderName)
{
	UINT32 index = 0;
	UINT32 pbContextNdrPtr = 0;

	LONG status = smartcard_unpack_redir_scard_context(log, s, phContext, &index, &pbContextNdrPtr);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!smartcard_ndr_pointer_read(log, s, &index, NULL))
		return ERROR_INVALID_DATA;

	status = smartcard_unpack_redir_scard_context_ref(log, s, pbContextNdrPtr, phContext);
	if (status != SCARD_S_SUCCESS)
		return status;

	status = smartcard_ndr_read_w(log, s, pszReaderName, NDR_PTR_FULL);
	if (status != SCARD_S_SUCCESS)
		return status;

	smartcard_trace_context_and_string_call_w(log, __func__, phContext, *pszReaderName);
	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_common_type_header(wStream* s)
{
	wLog* log = scard_log();
	UINT8 version = 0;
	UINT32 filler = 0;
	UINT8 endianness = 0;
	UINT16 commonHeaderLength = 0;

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 8))
		return STATUS_BUFFER_TOO_SMALL;

	/* Process CommonTypeHeader */
	Stream_Read_UINT8(s, version);             /* Version (1 byte) */
	Stream_Read_UINT8(s, endianness);          /* Endianness (1 byte) */
	Stream_Read_UINT16(s, commonHeaderLength); /* CommonHeaderLength (2 bytes) */
	Stream_Read_UINT32(s, filler);             /* Filler (4 bytes), should be 0xCCCCCCCC */

	if (version != 1)
	{
		WLog_Print(log, WLOG_WARN, "Unsupported CommonTypeHeader Version %" PRIu8 "", version);
		return STATUS_INVALID_PARAMETER;
	}

	if (endianness != 0x10)
	{
		WLog_Print(log, WLOG_WARN, "Unsupported CommonTypeHeader Endianness %" PRIu8 "",
		           endianness);
		return STATUS_INVALID_PARAMETER;
	}

	if (commonHeaderLength != 8)
	{
		WLog_Print(log, WLOG_WARN, "Unsupported CommonTypeHeader CommonHeaderLength %" PRIu16 "",
		           commonHeaderLength);
		return STATUS_INVALID_PARAMETER;
	}

	if (filler != 0xCCCCCCCC)
	{
		WLog_Print(log, WLOG_WARN, "Unexpected CommonTypeHeader Filler 0x%08" PRIX32 "", filler);
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
	wLog* log = scard_log();
	UINT32 filler = 0;
	UINT32 objectBufferLength = 0;

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 8))
		return STATUS_BUFFER_TOO_SMALL;

	Stream_Read_UINT32(s, objectBufferLength); /* ObjectBufferLength (4 bytes) */
	Stream_Read_UINT32(s, filler);             /* Filler (4 bytes), should be 0x00000000 */

	if (filler != 0x00000000)
	{
		WLog_Print(log, WLOG_WARN, "Unexpected PrivateTypeHeader Filler 0x%08" PRIX32 "", filler);
		return STATUS_INVALID_PARAMETER;
	}

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, objectBufferLength))
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
	size_t pad = 0;

	pad = size;
	size = (size + alignment - 1) & ~(alignment - 1);
	pad = size - pad;

	if (pad)
		Stream_Seek(s, pad);

	return (LONG)pad;
}

LONG smartcard_pack_write_size_align(wStream* s, size_t size, UINT32 alignment)
{
	size_t pad = 0;

	pad = size;
	size = (size + alignment - 1) & ~(alignment - 1);
	pad = size - pad;

	if (pad)
	{
		if (!Stream_EnsureRemainingCapacity(s, pad))
		{
			wLog* log = scard_log();
			WLog_Print(log, WLOG_ERROR, "Stream_EnsureRemainingCapacity failed!");
			return SCARD_F_INTERNAL_ERROR;
		}

		Stream_Zero(s, pad);
	}

	return SCARD_S_SUCCESS;
}

SCARDCONTEXT smartcard_scard_context_native_from_redir(REDIR_SCARDCONTEXT* context)
{
	SCARDCONTEXT hContext = { 0 };

	WINPR_ASSERT(context);
	if ((context->cbContext != sizeof(ULONG_PTR)) && (context->cbContext != 0))
	{
		wLog* log = scard_log();
		WLog_Print(log, WLOG_WARN,
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
	WINPR_ASSERT(context);
	ZeroMemory(context, sizeof(REDIR_SCARDCONTEXT));
	context->cbContext = sizeof(ULONG_PTR);
	CopyMemory(&(context->pbContext), &hContext, context->cbContext);
}

SCARDHANDLE smartcard_scard_handle_native_from_redir(REDIR_SCARDHANDLE* handle)
{
	SCARDHANDLE hCard = 0;

	WINPR_ASSERT(handle);
	if (handle->cbHandle == 0)
		return hCard;

	if (handle->cbHandle != sizeof(ULONG_PTR))
	{
		wLog* log = scard_log();
		WLog_Print(log, WLOG_WARN,
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
	WINPR_ASSERT(handle);
	ZeroMemory(handle, sizeof(REDIR_SCARDHANDLE));
	handle->cbHandle = sizeof(ULONG_PTR);
	CopyMemory(&(handle->pbHandle), &hCard, handle->cbHandle);
}

#define smartcard_context_supported(log, size) \
	smartcard_context_supported_((log), (size), __FILE__, __func__, __LINE__)
static LONG smartcard_context_supported_(wLog* log, uint32_t size, const char* file,
                                         const char* fkt, size_t line)
{
	switch (size)
	{
		case 0:
		case 4:
		case 8:
			return SCARD_S_SUCCESS;
		default:
		{
			const uint32_t level = WLOG_WARN;
			if (WLog_IsLevelActive(log, level))
			{
				WLog_PrintTextMessage(log, level, line, file, fkt,
				                      "REDIR_SCARDCONTEXT length is not 0, 4 or 8: %" PRIu32 "",
				                      size);
			}
			return STATUS_INVALID_PARAMETER;
		}
	}
}

LONG smartcard_unpack_redir_scard_context_(wLog* log, wStream* s, REDIR_SCARDCONTEXT* context,
                                           UINT32* index, UINT32* ppbContextNdrPtr,
                                           const char* file, const char* function, size_t line)
{
	UINT32 pbContextNdrPtr = 0;

	WINPR_UNUSED(file);
	WINPR_ASSERT(context);

	ZeroMemory(context, sizeof(REDIR_SCARDCONTEXT));

	if (!Stream_CheckAndLogRequiredLengthWLogEx(log, WLOG_WARN, s, 4, 1, "%s(%s:%" PRIuz ")", file,
	                                            function, line))
		return STATUS_BUFFER_TOO_SMALL;

	const LONG status = smartcard_context_supported_(log, context->cbContext, file, function, line);
	if (status != SCARD_S_SUCCESS)
		return status;

	Stream_Read_UINT32(s, context->cbContext); /* cbContext (4 bytes) */

	if (!smartcard_ndr_pointer_read_(log, s, index, &pbContextNdrPtr, file, function, line))
		return ERROR_INVALID_DATA;

	if (((context->cbContext == 0) && pbContextNdrPtr) ||
	    ((context->cbContext != 0) && !pbContextNdrPtr))
	{
		WLog_Print(log, WLOG_WARN,
		           "REDIR_SCARDCONTEXT cbContext (%" PRIu32 ") pbContextNdrPtr (%" PRIu32
		           ") inconsistency",
		           context->cbContext, pbContextNdrPtr);
		return STATUS_INVALID_PARAMETER;
	}

	if (!Stream_CheckAndLogRequiredLengthWLogEx(log, WLOG_WARN, s, context->cbContext, 1,
	                                            "%s(%s:%" PRIuz ")", file, function, line))
		return STATUS_INVALID_PARAMETER;

	*ppbContextNdrPtr = pbContextNdrPtr;
	return SCARD_S_SUCCESS;
}

LONG smartcard_pack_redir_scard_context(WINPR_ATTR_UNUSED wLog* log, wStream* s,
                                        const REDIR_SCARDCONTEXT* context, DWORD* index)
{
	const UINT32 pbContextNdrPtr = 0x00020000 + *index * 4;

	WINPR_ASSERT(context);
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

LONG smartcard_unpack_redir_scard_context_ref(wLog* log, wStream* s,
                                              WINPR_ATTR_UNUSED UINT32 pbContextNdrPtr,
                                              REDIR_SCARDCONTEXT* context)
{
	UINT32 length = 0;

	WINPR_ASSERT(context);
	if (context->cbContext == 0)
		return SCARD_S_SUCCESS;

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 4))
		return STATUS_BUFFER_TOO_SMALL;

	Stream_Read_UINT32(s, length); /* Length (4 bytes) */

	if (length != context->cbContext)
	{
		WLog_Print(log, WLOG_WARN,
		           "REDIR_SCARDCONTEXT length (%" PRIu32 ") cbContext (%" PRIu32 ") mismatch",
		           length, context->cbContext);
		return STATUS_INVALID_PARAMETER;
	}

	const LONG status = smartcard_context_supported(log, context->cbContext);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, context->cbContext))
		return STATUS_BUFFER_TOO_SMALL;

	if (context->cbContext)
		Stream_Read(s, &(context->pbContext), context->cbContext);
	else
		ZeroMemory(&(context->pbContext), sizeof(context->pbContext));

	return SCARD_S_SUCCESS;
}

LONG smartcard_pack_redir_scard_context_ref(WINPR_ATTR_UNUSED wLog* log, wStream* s,
                                            const REDIR_SCARDCONTEXT* context)
{
	WINPR_ASSERT(context);
	Stream_Write_UINT32(s, context->cbContext); /* Length (4 bytes) */

	if (context->cbContext)
	{
		Stream_Write(s, &(context->pbContext), context->cbContext);
	}

	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_redir_scard_handle_(wLog* log, wStream* s, REDIR_SCARDHANDLE* handle,
                                          UINT32* index, const char* file, const char* function,
                                          size_t line)
{
	WINPR_ASSERT(handle);
	ZeroMemory(handle, sizeof(REDIR_SCARDHANDLE));

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 4))
		return STATUS_BUFFER_TOO_SMALL;

	Stream_Read_UINT32(s, handle->cbHandle); /* Length (4 bytes) */

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, handle->cbHandle))
		return STATUS_BUFFER_TOO_SMALL;

	if (!smartcard_ndr_pointer_read_(log, s, index, NULL, file, function, line))
		return ERROR_INVALID_DATA;

	return SCARD_S_SUCCESS;
}

LONG smartcard_pack_redir_scard_handle(WINPR_ATTR_UNUSED wLog* log, wStream* s,
                                       const REDIR_SCARDHANDLE* handle, DWORD* index)
{
	const UINT32 pbContextNdrPtr = 0x00020000 + *index * 4;

	WINPR_ASSERT(handle);
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

LONG smartcard_unpack_redir_scard_handle_ref(wLog* log, wStream* s, REDIR_SCARDHANDLE* handle)
{
	UINT32 length = 0;

	WINPR_ASSERT(handle);
	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 4))
		return STATUS_BUFFER_TOO_SMALL;

	Stream_Read_UINT32(s, length); /* Length (4 bytes) */

	if (length != handle->cbHandle)
	{
		WLog_Print(log, WLOG_WARN,
		           "REDIR_SCARDHANDLE length (%" PRIu32 ") cbHandle (%" PRIu32 ") mismatch", length,
		           handle->cbHandle);
		return STATUS_INVALID_PARAMETER;
	}

	if ((handle->cbHandle != 4) && (handle->cbHandle != 8))
	{
		WLog_Print(log, WLOG_WARN, "REDIR_SCARDHANDLE length is not 4 or 8: %" PRIu32 "",
		           handle->cbHandle);
		return STATUS_INVALID_PARAMETER;
	}

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, handle->cbHandle))
		return STATUS_BUFFER_TOO_SMALL;

	if (handle->cbHandle)
		Stream_Read(s, &(handle->pbHandle), handle->cbHandle);

	return SCARD_S_SUCCESS;
}

LONG smartcard_pack_redir_scard_handle_ref(WINPR_ATTR_UNUSED wLog* log, wStream* s,
                                           const REDIR_SCARDHANDLE* handle)
{
	WINPR_ASSERT(handle);
	Stream_Write_UINT32(s, handle->cbHandle); /* Length (4 bytes) */

	if (handle->cbHandle)
		Stream_Write(s, &(handle->pbHandle), handle->cbHandle);

	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_establish_context_call(wStream* s, EstablishContext_Call* call)
{
	WINPR_ASSERT(call);
	wLog* log = scard_log();

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 4))
		return STATUS_BUFFER_TOO_SMALL;

	Stream_Read_UINT32(s, call->dwScope); /* dwScope (4 bytes) */
	smartcard_trace_establish_context_call(log, call);
	return SCARD_S_SUCCESS;
}

LONG smartcard_pack_establish_context_return(wStream* s, const EstablishContext_Return* ret)
{
	WINPR_ASSERT(ret);
	wLog* log = scard_log();
	LONG status = 0;
	DWORD index = 0;

	smartcard_trace_establish_context_return(log, ret);
	if (ret->ReturnCode != SCARD_S_SUCCESS)
		return ret->ReturnCode;

	status = smartcard_pack_redir_scard_context(log, s, &(ret->hContext), &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	return smartcard_pack_redir_scard_context_ref(log, s, &(ret->hContext));
}

LONG smartcard_unpack_context_call(wStream* s, Context_Call* call, const char* name)
{
	UINT32 index = 0;
	UINT32 pbContextNdrPtr = 0;
	wLog* log = scard_log();

	WINPR_ASSERT(call);
	LONG status = smartcard_unpack_redir_scard_context(log, s, &(call->handles.hContext), &index,
	                                                   &pbContextNdrPtr);
	if (status != SCARD_S_SUCCESS)
		return status;

	status = smartcard_unpack_redir_scard_context_ref(log, s, pbContextNdrPtr,
	                                                  &(call->handles.hContext));
	if (status != SCARD_S_SUCCESS)
		WLog_Print(log, WLOG_ERROR,
		           "smartcard_unpack_redir_scard_context_ref failed with error %" PRId32 "",
		           status);

	smartcard_trace_context_call(log, call, name);
	return status;
}

LONG smartcard_unpack_list_reader_groups_call(wStream* s, ListReaderGroups_Call* call, BOOL unicode)
{
	UINT32 index = 0;
	UINT32 pbContextNdrPtr = 0;
	wLog* log = scard_log();

	WINPR_ASSERT(call);
	LONG status = smartcard_unpack_redir_scard_context(log, s, &(call->handles.hContext), &index,
	                                                   &pbContextNdrPtr);

	if (status != SCARD_S_SUCCESS)
		return status;

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 8))
		return STATUS_BUFFER_TOO_SMALL;

	Stream_Read_INT32(s, call->fmszGroupsIsNULL); /* fmszGroupsIsNULL (4 bytes) */
	Stream_Read_UINT32(s, call->cchGroups);       /* cchGroups (4 bytes) */
	status = smartcard_unpack_redir_scard_context_ref(log, s, pbContextNdrPtr,
	                                                  &(call->handles.hContext));

	if (status != SCARD_S_SUCCESS)
		return status;

	smartcard_trace_list_reader_groups_call(log, call, unicode);
	return SCARD_S_SUCCESS;
}

LONG smartcard_pack_list_reader_groups_return(wStream* s, const ListReaderGroups_Return* ret,
                                              BOOL unicode)
{
	WINPR_ASSERT(ret);
	wLog* log = scard_log();
	LONG status = 0;
	DWORD cBytes = ret->cBytes;
	UINT32 index = 0;

	smartcard_trace_list_reader_groups_return(log, ret, unicode);
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
	UINT32 index = 0;
	UINT32 mszGroupsNdrPtr = 0;
	UINT32 pbContextNdrPtr = 0;
	wLog* log = scard_log();

	WINPR_ASSERT(call);
	call->mszGroups = NULL;

	LONG status = smartcard_unpack_redir_scard_context(log, s, &(call->handles.hContext), &index,
	                                                   &pbContextNdrPtr);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 4))
		return STATUS_BUFFER_TOO_SMALL;

	Stream_Read_UINT32(s, call->cBytes); /* cBytes (4 bytes) */
	if (!smartcard_ndr_pointer_read(log, s, &index, &mszGroupsNdrPtr))
		return ERROR_INVALID_DATA;

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 8))
		return STATUS_BUFFER_TOO_SMALL;
	Stream_Read_INT32(s, call->fmszReadersIsNULL); /* fmszReadersIsNULL (4 bytes) */
	Stream_Read_UINT32(s, call->cchReaders);       /* cchReaders (4 bytes) */

	status = smartcard_unpack_redir_scard_context_ref(log, s, pbContextNdrPtr,
	                                                  &(call->handles.hContext));
	if (status != SCARD_S_SUCCESS)
		return status;

	if (mszGroupsNdrPtr)
	{
		status = smartcard_ndr_read(log, s, &call->mszGroups, call->cBytes, 1, NDR_PTR_SIMPLE);
		if (status != SCARD_S_SUCCESS)
			return status;
	}

	smartcard_trace_list_readers_call(log, call, unicode);
	return SCARD_S_SUCCESS;
}

LONG smartcard_pack_list_readers_return(wStream* s, const ListReaders_Return* ret, BOOL unicode)
{
	WINPR_ASSERT(ret);
	wLog* log = scard_log();
	LONG status = 0;
	UINT32 index = 0;
	UINT32 size = ret->cBytes;

	smartcard_trace_list_readers_return(log, ret, unicode);
	if (ret->ReturnCode != SCARD_S_SUCCESS)
		size = 0;

	if (!Stream_EnsureRemainingCapacity(s, 4))
	{
		WLog_Print(log, WLOG_ERROR, "Stream_EnsureRemainingCapacity failed!");
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

static LONG smartcard_unpack_connect_common(wLog* log, wStream* s, Connect_Common_Call* common,
                                            UINT32* index, UINT32* ppbContextNdrPtr)
{
	WINPR_ASSERT(common);
	LONG status = smartcard_unpack_redir_scard_context(log, s, &(common->handles.hContext), index,
	                                                   ppbContextNdrPtr);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 8))
		return STATUS_BUFFER_TOO_SMALL;

	Stream_Read_UINT32(s, common->dwShareMode);          /* dwShareMode (4 bytes) */
	Stream_Read_UINT32(s, common->dwPreferredProtocols); /* dwPreferredProtocols (4 bytes) */
	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_connect_a_call(wStream* s, ConnectA_Call* call)
{
	LONG status = 0;
	UINT32 index = 0;
	UINT32 pbContextNdrPtr = 0;

	WINPR_ASSERT(call);
	wLog* log = scard_log();

	call->szReader = NULL;

	if (!smartcard_ndr_pointer_read(log, s, &index, NULL))
		return ERROR_INVALID_DATA;

	status = smartcard_unpack_connect_common(log, s, &(call->Common), &index, &pbContextNdrPtr);
	if (status != SCARD_S_SUCCESS)
	{
		WLog_Print(log, WLOG_ERROR, "smartcard_unpack_connect_common failed with error %" PRId32 "",
		           status);
		return status;
	}

	status = smartcard_ndr_read_a(log, s, &call->szReader, NDR_PTR_FULL);
	if (status != SCARD_S_SUCCESS)
		return status;

	status = smartcard_unpack_redir_scard_context_ref(log, s, pbContextNdrPtr,
	                                                  &(call->Common.handles.hContext));
	if (status != SCARD_S_SUCCESS)
		WLog_Print(log, WLOG_ERROR,
		           "smartcard_unpack_redir_scard_context_ref failed with error %" PRId32 "",
		           status);

	smartcard_trace_connect_a_call(log, call);
	return status;
}

LONG smartcard_unpack_connect_w_call(wStream* s, ConnectW_Call* call)
{
	LONG status = 0;
	UINT32 index = 0;
	UINT32 pbContextNdrPtr = 0;

	WINPR_ASSERT(call);
	wLog* log = scard_log();
	call->szReader = NULL;

	if (!smartcard_ndr_pointer_read(log, s, &index, NULL))
		return ERROR_INVALID_DATA;

	status = smartcard_unpack_connect_common(log, s, &(call->Common), &index, &pbContextNdrPtr);
	if (status != SCARD_S_SUCCESS)
	{
		WLog_Print(log, WLOG_ERROR, "smartcard_unpack_connect_common failed with error %" PRId32 "",
		           status);
		return status;
	}

	status = smartcard_ndr_read_w(log, s, &call->szReader, NDR_PTR_FULL);
	if (status != SCARD_S_SUCCESS)
		return status;

	status = smartcard_unpack_redir_scard_context_ref(log, s, pbContextNdrPtr,
	                                                  &(call->Common.handles.hContext));
	if (status != SCARD_S_SUCCESS)
		WLog_Print(log, WLOG_ERROR,
		           "smartcard_unpack_redir_scard_context_ref failed with error %" PRId32 "",
		           status);

	smartcard_trace_connect_w_call(log, call);
	return status;
}

LONG smartcard_pack_connect_return(wStream* s, const Connect_Return* ret)
{
	LONG status = 0;
	DWORD index = 0;

	WINPR_ASSERT(ret);
	wLog* log = scard_log();
	smartcard_trace_connect_return(log, ret);

	status = smartcard_pack_redir_scard_context(log, s, &ret->hContext, &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	status = smartcard_pack_redir_scard_handle(log, s, &ret->hCard, &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!Stream_EnsureRemainingCapacity(s, 4))
		return SCARD_E_NO_MEMORY;

	Stream_Write_UINT32(s, ret->dwActiveProtocol); /* dwActiveProtocol (4 bytes) */
	status = smartcard_pack_redir_scard_context_ref(log, s, &ret->hContext);
	if (status != SCARD_S_SUCCESS)
		return status;
	return smartcard_pack_redir_scard_handle_ref(log, s, &(ret->hCard));
}

LONG smartcard_unpack_reconnect_call(wStream* s, Reconnect_Call* call)
{
	UINT32 index = 0;
	UINT32 pbContextNdrPtr = 0;

	WINPR_ASSERT(call);
	wLog* log = scard_log();
	LONG status = smartcard_unpack_redir_scard_context(log, s, &(call->handles.hContext), &index,
	                                                   &pbContextNdrPtr);
	if (status != SCARD_S_SUCCESS)
		return status;

	status = smartcard_unpack_redir_scard_handle(log, s, &(call->handles.hCard), &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 12))
		return STATUS_BUFFER_TOO_SMALL;

	Stream_Read_UINT32(s, call->dwShareMode);          /* dwShareMode (4 bytes) */
	Stream_Read_UINT32(s, call->dwPreferredProtocols); /* dwPreferredProtocols (4 bytes) */
	Stream_Read_UINT32(s, call->dwInitialization);     /* dwInitialization (4 bytes) */

	status = smartcard_unpack_redir_scard_context_ref(log, s, pbContextNdrPtr,
	                                                  &(call->handles.hContext));
	if (status != SCARD_S_SUCCESS)
	{
		WLog_Print(log, WLOG_ERROR,
		           "smartcard_unpack_redir_scard_context_ref failed with error %" PRId32 "",
		           status);
		return status;
	}

	status = smartcard_unpack_redir_scard_handle_ref(log, s, &(call->handles.hCard));
	if (status != SCARD_S_SUCCESS)
		WLog_Print(log, WLOG_ERROR,
		           "smartcard_unpack_redir_scard_handle_ref failed with error %" PRId32 "", status);

	smartcard_trace_reconnect_call(log, call);
	return status;
}

LONG smartcard_pack_reconnect_return(wStream* s, const Reconnect_Return* ret)
{
	WINPR_ASSERT(ret);
	wLog* log = scard_log();
	smartcard_trace_reconnect_return(log, ret);

	if (!Stream_EnsureRemainingCapacity(s, 4))
		return SCARD_E_NO_MEMORY;
	Stream_Write_UINT32(s, ret->dwActiveProtocol); /* dwActiveProtocol (4 bytes) */
	return ret->ReturnCode;
}

LONG smartcard_unpack_hcard_and_disposition_call(wStream* s, HCardAndDisposition_Call* call,
                                                 const char* name)
{
	UINT32 index = 0;
	UINT32 pbContextNdrPtr = 0;

	WINPR_ASSERT(call);
	wLog* log = scard_log();

	LONG status = smartcard_unpack_redir_scard_context(log, s, &(call->handles.hContext), &index,
	                                                   &pbContextNdrPtr);
	if (status != SCARD_S_SUCCESS)
		return status;

	status = smartcard_unpack_redir_scard_handle(log, s, &(call->handles.hCard), &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 4))
		return STATUS_BUFFER_TOO_SMALL;

	Stream_Read_UINT32(s, call->dwDisposition); /* dwDisposition (4 bytes) */

	status = smartcard_unpack_redir_scard_context_ref(log, s, pbContextNdrPtr,
	                                                  &(call->handles.hContext));
	if (status != SCARD_S_SUCCESS)
		return status;

	status = smartcard_unpack_redir_scard_handle_ref(log, s, &(call->handles.hCard));
	if (status != SCARD_S_SUCCESS)
		return status;

	smartcard_trace_hcard_and_disposition_call(log, call, name);
	return status;
}

static void smartcard_trace_get_status_change_a_call(wLog* log, const GetStatusChangeA_Call* call)
{
	WINPR_ASSERT(call);

	if (!WLog_IsLevelActive(log, g_LogLevel))
		return;

	WLog_Print(log, g_LogLevel, "GetStatusChangeA_Call {");
	smartcard_log_context(log, &call->handles.hContext);

	WLog_Print(log, g_LogLevel, "dwTimeOut: 0x%08" PRIX32 " cReaders: %" PRIu32 "", call->dwTimeOut,
	           call->cReaders);

	dump_reader_states_a(log, call->rgReaderStates, call->cReaders);

	WLog_Print(log, g_LogLevel, "}");
}

static LONG smartcard_unpack_reader_state_a(wLog* log, wStream* s, LPSCARD_READERSTATEA* ppcReaders,
                                            UINT32 cReaders, UINT32* ptrIndex)
{
	LONG status = SCARD_E_NO_MEMORY;

	WINPR_ASSERT(ppcReaders || (cReaders == 0));
	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 4))
		return status;

	const UINT32 len = Stream_Get_UINT32(s);
	if (len != cReaders)
	{
		WLog_Print(log, WLOG_ERROR, "Count mismatch when reading LPSCARD_READERSTATEA");
		return status;
	}

	LPSCARD_READERSTATEA rgReaderStates =
	    (LPSCARD_READERSTATEA)calloc(cReaders, sizeof(SCARD_READERSTATEA));
	BOOL* states = calloc(cReaders, sizeof(BOOL));
	if (!rgReaderStates || !states)
		goto fail;
	status = ERROR_INVALID_DATA;

	for (UINT32 index = 0; index < cReaders; index++)
	{
		UINT32 ptr = UINT32_MAX;
		LPSCARD_READERSTATEA readerState = &rgReaderStates[index];

		if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 52))
			goto fail;

		if (!smartcard_ndr_pointer_read(log, s, ptrIndex, &ptr))
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

	for (UINT32 index = 0; index < cReaders; index++)
	{
		LPSCARD_READERSTATEA readerState = &rgReaderStates[index];

		/* Ignore empty strings */
		if (!states[index])
			continue;
		status = smartcard_ndr_read_a(log, s, &readerState->szReader, NDR_PTR_FULL);
		if (status != SCARD_S_SUCCESS)
			goto fail;
	}

	*ppcReaders = rgReaderStates;
	free(states);
	return SCARD_S_SUCCESS;
fail:
	if (rgReaderStates)
	{
		for (UINT32 index = 0; index < cReaders; index++)
		{
			LPSCARD_READERSTATEA readerState = &rgReaderStates[index];
			free(readerState->szReader);
		}
	}
	free(rgReaderStates);
	free(states);
	return status;
}

static LONG smartcard_unpack_reader_state_w(wLog* log, wStream* s, LPSCARD_READERSTATEW* ppcReaders,
                                            UINT32 cReaders, UINT32* ptrIndex)
{
	LONG status = SCARD_E_NO_MEMORY;

	WINPR_ASSERT(ppcReaders || (cReaders == 0));
	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 4))
		return status;

	const UINT32 len = Stream_Get_UINT32(s);
	if (len != cReaders)
	{
		WLog_Print(log, WLOG_ERROR, "Count mismatch when reading LPSCARD_READERSTATEW");
		return status;
	}

	LPSCARD_READERSTATEW rgReaderStates =
	    (LPSCARD_READERSTATEW)calloc(cReaders, sizeof(SCARD_READERSTATEW));
	BOOL* states = calloc(cReaders, sizeof(BOOL));

	if (!rgReaderStates || !states)
		goto fail;

	status = ERROR_INVALID_DATA;
	for (UINT32 index = 0; index < cReaders; index++)
	{
		UINT32 ptr = UINT32_MAX;
		LPSCARD_READERSTATEW readerState = &rgReaderStates[index];

		if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 52))
			goto fail;

		if (!smartcard_ndr_pointer_read(log, s, ptrIndex, &ptr))
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

	for (UINT32 index = 0; index < cReaders; index++)
	{
		LPSCARD_READERSTATEW readerState = &rgReaderStates[index];

		/* Skip NULL pointers */
		if (!states[index])
			continue;

		status = smartcard_ndr_read_w(log, s, &readerState->szReader, NDR_PTR_FULL);
		if (status != SCARD_S_SUCCESS)
			goto fail;
	}

	*ppcReaders = rgReaderStates;
	free(states);
	return SCARD_S_SUCCESS;
fail:
	if (rgReaderStates)
	{
		for (UINT32 index = 0; index < cReaders; index++)
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
	UINT32 ndrPtr = 0;
	UINT32 index = 0;
	UINT32 pbContextNdrPtr = 0;

	WINPR_ASSERT(call);
	wLog* log = scard_log();

	call->rgReaderStates = NULL;

	LONG status = smartcard_unpack_redir_scard_context(log, s, &(call->handles.hContext), &index,
	                                                   &pbContextNdrPtr);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 8))
		return STATUS_BUFFER_TOO_SMALL;

	Stream_Read_UINT32(s, call->dwTimeOut); /* dwTimeOut (4 bytes) */
	Stream_Read_UINT32(s, call->cReaders);  /* cReaders (4 bytes) */
	if (!smartcard_ndr_pointer_read(log, s, &index, &ndrPtr))
		return ERROR_INVALID_DATA;

	status = smartcard_unpack_redir_scard_context_ref(log, s, pbContextNdrPtr,
	                                                  &(call->handles.hContext));
	if (status != SCARD_S_SUCCESS)
		return status;

	if (ndrPtr)
	{
		status =
		    smartcard_unpack_reader_state_a(log, s, &call->rgReaderStates, call->cReaders, &index);
		if (status != SCARD_S_SUCCESS)
			return status;
	}
	else
	{
		WLog_Print(log, WLOG_WARN, "ndrPtr=0x%08" PRIx32 ", can not read rgReaderStates", ndrPtr);
		return SCARD_E_UNEXPECTED;
	}

	smartcard_trace_get_status_change_a_call(log, call);
	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_get_status_change_w_call(wStream* s, GetStatusChangeW_Call* call)
{
	UINT32 ndrPtr = 0;
	UINT32 index = 0;
	UINT32 pbContextNdrPtr = 0;

	WINPR_ASSERT(call);
	wLog* log = scard_log();
	call->rgReaderStates = NULL;

	LONG status = smartcard_unpack_redir_scard_context(log, s, &(call->handles.hContext), &index,
	                                                   &pbContextNdrPtr);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 8))
		return STATUS_BUFFER_TOO_SMALL;

	Stream_Read_UINT32(s, call->dwTimeOut); /* dwTimeOut (4 bytes) */
	Stream_Read_UINT32(s, call->cReaders);  /* cReaders (4 bytes) */
	if (!smartcard_ndr_pointer_read(log, s, &index, &ndrPtr))
		return ERROR_INVALID_DATA;

	status = smartcard_unpack_redir_scard_context_ref(log, s, pbContextNdrPtr,
	                                                  &(call->handles.hContext));
	if (status != SCARD_S_SUCCESS)
		return status;

	if (ndrPtr)
	{
		status =
		    smartcard_unpack_reader_state_w(log, s, &call->rgReaderStates, call->cReaders, &index);
		if (status != SCARD_S_SUCCESS)
			return status;
	}
	else
	{
		WLog_Print(log, WLOG_WARN, "ndrPtr=0x%08" PRIx32 ", can not read rgReaderStates", ndrPtr);
		return SCARD_E_UNEXPECTED;
	}

	smartcard_trace_get_status_change_w_call(log, call);
	return SCARD_S_SUCCESS;
}

LONG smartcard_pack_get_status_change_return(wStream* s, const GetStatusChange_Return* ret,
                                             BOOL unicode)
{
	WINPR_ASSERT(ret);
	wLog* log = scard_log();

	LONG status = 0;
	DWORD cReaders = ret->cReaders;
	UINT32 index = 0;

	smartcard_trace_get_status_change_return(log, ret, unicode);
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
	UINT32 index = 0;
	UINT32 pbContextNdrPtr = 0;

	wLog* log = scard_log();

	WINPR_ASSERT(call);
	LONG status = smartcard_unpack_redir_scard_context(log, s, &(call->handles.hContext), &index,
	                                                   &pbContextNdrPtr);
	if (status != SCARD_S_SUCCESS)
		return status;

	status = smartcard_unpack_redir_scard_handle(log, s, &(call->handles.hCard), &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 8))
		return STATUS_BUFFER_TOO_SMALL;

	Stream_Read_INT32(s, call->fpbAtrIsNULL); /* fpbAtrIsNULL (4 bytes) */
	Stream_Read_UINT32(s, call->cbAtrLen);    /* cbAtrLen (4 bytes) */

	status = smartcard_unpack_redir_scard_context_ref(log, s, pbContextNdrPtr,
	                                                  &(call->handles.hContext));
	if (status != SCARD_S_SUCCESS)
		return status;

	status = smartcard_unpack_redir_scard_handle_ref(log, s, &(call->handles.hCard));
	if (status != SCARD_S_SUCCESS)
		return status;

	return status;
}

LONG smartcard_pack_state_return(wStream* s, const State_Return* ret)
{
	WINPR_ASSERT(ret);
	wLog* log = scard_log();
	LONG status = 0;
	DWORD cbAtrLen = ret->cbAtrLen;
	UINT32 index = 0;

	smartcard_trace_state_return(log, ret);
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
	UINT32 index = 0;
	UINT32 pbContextNdrPtr = 0;

	WINPR_ASSERT(call);
	wLog* log = scard_log();

	LONG status = smartcard_unpack_redir_scard_context(log, s, &(call->handles.hContext), &index,
	                                                   &pbContextNdrPtr);
	if (status != SCARD_S_SUCCESS)
		return status;

	status = smartcard_unpack_redir_scard_handle(log, s, &(call->handles.hCard), &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 12))
		return STATUS_BUFFER_TOO_SMALL;

	Stream_Read_INT32(s, call->fmszReaderNamesIsNULL); /* fmszReaderNamesIsNULL (4 bytes) */
	Stream_Read_UINT32(s, call->cchReaderLen);         /* cchReaderLen (4 bytes) */
	Stream_Read_UINT32(s, call->cbAtrLen);             /* cbAtrLen (4 bytes) */

	status = smartcard_unpack_redir_scard_context_ref(log, s, pbContextNdrPtr,
	                                                  &(call->handles.hContext));
	if (status != SCARD_S_SUCCESS)
		return status;

	status = smartcard_unpack_redir_scard_handle_ref(log, s, &(call->handles.hCard));
	if (status != SCARD_S_SUCCESS)
		return status;

	smartcard_trace_status_call(log, call, unicode);
	return status;
}

LONG smartcard_pack_status_return(wStream* s, const Status_Return* ret, BOOL unicode)
{
	WINPR_ASSERT(ret);
	wLog* log = scard_log();

	LONG status = 0;
	UINT32 index = 0;
	DWORD cBytes = ret->cBytes;

	smartcard_trace_status_return(log, ret, unicode);
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
	WINPR_ASSERT(call);
	wLog* log = scard_log();
	UINT32 index = 0;
	UINT32 pbContextNdrPtr = 0;

	LONG status = smartcard_unpack_redir_scard_context(log, s, &(call->handles.hContext), &index,
	                                                   &pbContextNdrPtr);
	if (status != SCARD_S_SUCCESS)
		return status;

	status = smartcard_unpack_redir_scard_handle(log, s, &(call->handles.hCard), &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 12))
		return STATUS_BUFFER_TOO_SMALL;

	Stream_Read_UINT32(s, call->dwAttrId);     /* dwAttrId (4 bytes) */
	Stream_Read_INT32(s, call->fpbAttrIsNULL); /* fpbAttrIsNULL (4 bytes) */
	Stream_Read_UINT32(s, call->cbAttrLen);    /* cbAttrLen (4 bytes) */

	status = smartcard_unpack_redir_scard_context_ref(log, s, pbContextNdrPtr,
	                                                  &(call->handles.hContext));
	if (status != SCARD_S_SUCCESS)
		return status;

	status = smartcard_unpack_redir_scard_handle_ref(log, s, &(call->handles.hCard));
	if (status != SCARD_S_SUCCESS)
		return status;

	smartcard_trace_get_attrib_call(log, call);
	return status;
}

LONG smartcard_pack_get_attrib_return(wStream* s, const GetAttrib_Return* ret, DWORD dwAttrId,
                                      DWORD cbAttrCallLen)
{
	WINPR_ASSERT(ret);
	wLog* log = scard_log();
	LONG status = 0;
	DWORD cbAttrLen = 0;
	UINT32 index = 0;
	smartcard_trace_get_attrib_return(log, ret, dwAttrId);

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
	WINPR_ASSERT(call);
	wLog* log = scard_log();

	UINT32 index = 0;
	UINT32 pvInBufferNdrPtr = 0;
	UINT32 pbContextNdrPtr = 0;

	call->pvInBuffer = NULL;

	LONG status = smartcard_unpack_redir_scard_context(log, s, &(call->handles.hContext), &index,
	                                                   &pbContextNdrPtr);
	if (status != SCARD_S_SUCCESS)
		return status;

	status = smartcard_unpack_redir_scard_handle(log, s, &(call->handles.hCard), &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 20))
		return STATUS_BUFFER_TOO_SMALL;

	Stream_Read_UINT32(s, call->dwControlCode);                    /* dwControlCode (4 bytes) */
	Stream_Read_UINT32(s, call->cbInBufferSize);                   /* cbInBufferSize (4 bytes) */
	if (!smartcard_ndr_pointer_read(log, s, &index,
	                                &pvInBufferNdrPtr)) /* pvInBufferNdrPtr (4 bytes) */
		return ERROR_INVALID_DATA;
	Stream_Read_INT32(s, call->fpvOutBufferIsNULL); /* fpvOutBufferIsNULL (4 bytes) */
	Stream_Read_UINT32(s, call->cbOutBufferSize);   /* cbOutBufferSize (4 bytes) */

	status = smartcard_unpack_redir_scard_context_ref(log, s, pbContextNdrPtr,
	                                                  &(call->handles.hContext));
	if (status != SCARD_S_SUCCESS)
		return status;

	status = smartcard_unpack_redir_scard_handle_ref(log, s, &(call->handles.hCard));
	if (status != SCARD_S_SUCCESS)
		return status;

	if (pvInBufferNdrPtr)
	{
		status =
		    smartcard_ndr_read(log, s, &call->pvInBuffer, call->cbInBufferSize, 1, NDR_PTR_SIMPLE);
		if (status != SCARD_S_SUCCESS)
			return status;
	}

	smartcard_trace_control_call(log, call);
	return SCARD_S_SUCCESS;
}

LONG smartcard_pack_control_return(wStream* s, const Control_Return* ret)
{
	WINPR_ASSERT(ret);
	wLog* log = scard_log();

	LONG status = 0;
	DWORD cbDataLen = ret->cbOutBufferSize;
	UINT32 index = 0;

	smartcard_trace_control_return(log, ret);
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
	UINT32 length = 0;
	BYTE* pbExtraBytes = NULL;
	UINT32 pbExtraBytesNdrPtr = 0;
	UINT32 pbSendBufferNdrPtr = 0;
	UINT32 pioRecvPciNdrPtr = 0;
	SCardIO_Request ioSendPci;
	SCardIO_Request ioRecvPci;
	UINT32 index = 0;
	UINT32 pbContextNdrPtr = 0;

	WINPR_ASSERT(call);
	wLog* log = scard_log();

	call->pioSendPci = NULL;
	call->pioRecvPci = NULL;
	call->pbSendBuffer = NULL;

	LONG status = smartcard_unpack_redir_scard_context(log, s, &(call->handles.hContext), &index,
	                                                   &pbContextNdrPtr);
	if (status != SCARD_S_SUCCESS)
		return status;

	status = smartcard_unpack_redir_scard_handle(log, s, &(call->handles.hCard), &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 32))
		return STATUS_BUFFER_TOO_SMALL;

	Stream_Read_UINT32(s, ioSendPci.dwProtocol);   /* dwProtocol (4 bytes) */
	Stream_Read_UINT32(s, ioSendPci.cbExtraBytes); /* cbExtraBytes (4 bytes) */
	if (!smartcard_ndr_pointer_read(log, s, &index,
	                                &pbExtraBytesNdrPtr)) /* pbExtraBytesNdrPtr (4 bytes) */
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, call->cbSendLength); /* cbSendLength (4 bytes) */
	if (!smartcard_ndr_pointer_read(log, s, &index,
	                                &pbSendBufferNdrPtr)) /* pbSendBufferNdrPtr (4 bytes) */
		return ERROR_INVALID_DATA;

	if (!smartcard_ndr_pointer_read(log, s, &index,
	                                &pioRecvPciNdrPtr)) /* pioRecvPciNdrPtr (4 bytes) */
		return ERROR_INVALID_DATA;

	Stream_Read_INT32(s, call->fpbRecvBufferIsNULL); /* fpbRecvBufferIsNULL (4 bytes) */
	Stream_Read_UINT32(s, call->cbRecvLength);       /* cbRecvLength (4 bytes) */

	if (ioSendPci.cbExtraBytes > 1024)
	{
		WLog_Print(log, WLOG_WARN,
		           "Transmit_Call ioSendPci.cbExtraBytes is out of bounds: %" PRIu32 " (max: 1024)",
		           ioSendPci.cbExtraBytes);
		return STATUS_INVALID_PARAMETER;
	}

	if (call->cbSendLength > 66560)
	{
		WLog_Print(log, WLOG_WARN,
		           "Transmit_Call cbSendLength is out of bounds: %" PRIu32 " (max: 66560)",
		           ioSendPci.cbExtraBytes);
		return STATUS_INVALID_PARAMETER;
	}

	status = smartcard_unpack_redir_scard_context_ref(log, s, pbContextNdrPtr,
	                                                  &(call->handles.hContext));
	if (status != SCARD_S_SUCCESS)
		return status;

	status = smartcard_unpack_redir_scard_handle_ref(log, s, &(call->handles.hCard));
	if (status != SCARD_S_SUCCESS)
		return status;

	if (ioSendPci.cbExtraBytes && !pbExtraBytesNdrPtr)
	{
		WLog_Print(
		    log, WLOG_WARN,
		    "Transmit_Call ioSendPci.cbExtraBytes is non-zero but pbExtraBytesNdrPtr is null");
		return STATUS_INVALID_PARAMETER;
	}

	if (pbExtraBytesNdrPtr)
	{
		// TODO: Use unified pointer reading
		if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 4))
			return STATUS_BUFFER_TOO_SMALL;

		Stream_Read_UINT32(s, length); /* Length (4 bytes) */

		if (!Stream_CheckAndLogRequiredLengthWLog(log, s, ioSendPci.cbExtraBytes))
			return STATUS_BUFFER_TOO_SMALL;

		ioSendPci.pbExtraBytes = Stream_Pointer(s);
		call->pioSendPci =
		    (LPSCARD_IO_REQUEST)malloc(sizeof(SCARD_IO_REQUEST) + ioSendPci.cbExtraBytes);

		if (!call->pioSendPci)
		{
			WLog_Print(log, WLOG_WARN, "Transmit_Call out of memory error (pioSendPci)");
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
			WLog_Print(log, WLOG_WARN, "Transmit_Call out of memory error (pioSendPci)");
			return STATUS_NO_MEMORY;
		}

		call->pioSendPci->dwProtocol = ioSendPci.dwProtocol;
		call->pioSendPci->cbPciLength = sizeof(SCARD_IO_REQUEST);
	}

	if (pbSendBufferNdrPtr)
	{
		status =
		    smartcard_ndr_read(log, s, &call->pbSendBuffer, call->cbSendLength, 1, NDR_PTR_SIMPLE);
		if (status != SCARD_S_SUCCESS)
			return status;
	}

	if (pioRecvPciNdrPtr)
	{
		if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 12))
			return STATUS_BUFFER_TOO_SMALL;

		Stream_Read_UINT32(s, ioRecvPci.dwProtocol);   /* dwProtocol (4 bytes) */
		Stream_Read_UINT32(s, ioRecvPci.cbExtraBytes); /* cbExtraBytes (4 bytes) */
		if (!smartcard_ndr_pointer_read(log, s, &index,
		                                &pbExtraBytesNdrPtr)) /* pbExtraBytesNdrPtr (4 bytes) */
			return ERROR_INVALID_DATA;

		if (ioRecvPci.cbExtraBytes && !pbExtraBytesNdrPtr)
		{
			WLog_Print(
			    log, WLOG_WARN,
			    "Transmit_Call ioRecvPci.cbExtraBytes is non-zero but pbExtraBytesNdrPtr is null");
			return STATUS_INVALID_PARAMETER;
		}

		if (pbExtraBytesNdrPtr)
		{
			// TODO: Unify ndr pointer reading
			if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 4))
				return STATUS_BUFFER_TOO_SMALL;

			Stream_Read_UINT32(s, length); /* Length (4 bytes) */

			if (ioRecvPci.cbExtraBytes > 1024)
			{
				WLog_Print(log, WLOG_WARN,
				           "Transmit_Call ioRecvPci.cbExtraBytes is out of bounds: %" PRIu32
				           " (max: 1024)",
				           ioRecvPci.cbExtraBytes);
				return STATUS_INVALID_PARAMETER;
			}

			if (length != ioRecvPci.cbExtraBytes)
			{
				WLog_Print(log, WLOG_WARN,
				           "Transmit_Call unexpected length: Actual: %" PRIu32
				           ", Expected: %" PRIu32 " (ioRecvPci.cbExtraBytes)",
				           length, ioRecvPci.cbExtraBytes);
				return STATUS_INVALID_PARAMETER;
			}

			if (!Stream_CheckAndLogRequiredLengthWLog(log, s, ioRecvPci.cbExtraBytes))
				return STATUS_BUFFER_TOO_SMALL;

			ioRecvPci.pbExtraBytes = Stream_Pointer(s);
			call->pioRecvPci =
			    (LPSCARD_IO_REQUEST)malloc(sizeof(SCARD_IO_REQUEST) + ioRecvPci.cbExtraBytes);

			if (!call->pioRecvPci)
			{
				WLog_Print(log, WLOG_WARN, "Transmit_Call out of memory error (pioRecvPci)");
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
				WLog_Print(log, WLOG_WARN, "Transmit_Call out of memory error (pioRecvPci)");
				return STATUS_NO_MEMORY;
			}

			call->pioRecvPci->dwProtocol = ioRecvPci.dwProtocol;
			call->pioRecvPci->cbPciLength = sizeof(SCARD_IO_REQUEST);
		}
	}

	smartcard_trace_transmit_call(log, call);
	return SCARD_S_SUCCESS;
}

LONG smartcard_pack_transmit_return(wStream* s, const Transmit_Return* ret)
{
	WINPR_ASSERT(ret);
	wLog* log = scard_log();

	LONG status = 0;
	UINT32 index = 0;
	LONG error = 0;
	UINT32 cbRecvLength = ret->cbRecvLength;
	UINT32 cbRecvPci = ret->pioRecvPci ? ret->pioRecvPci->cbPciLength : 0;

	smartcard_trace_transmit_return(log, ret);

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
			WLog_Print(log, WLOG_ERROR, "Stream_EnsureRemainingCapacity failed!");
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
	UINT32 rgReaderStatesNdrPtr = 0;
	UINT32 rgAtrMasksNdrPtr = 0;
	UINT32 index = 0;
	UINT32 pbContextNdrPtr = 0;

	WINPR_ASSERT(call);
	wLog* log = scard_log();

	call->rgReaderStates = NULL;

	LONG status = smartcard_unpack_redir_scard_context(log, s, &(call->handles.hContext), &index,
	                                                   &pbContextNdrPtr);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 16))
		return STATUS_BUFFER_TOO_SMALL;

	Stream_Read_UINT32(s, call->cAtrs);
	if (!smartcard_ndr_pointer_read(log, s, &index, &rgAtrMasksNdrPtr))
		return ERROR_INVALID_DATA;
	Stream_Read_UINT32(s, call->cReaders); /* cReaders (4 bytes) */
	if (!smartcard_ndr_pointer_read(log, s, &index, &rgReaderStatesNdrPtr))
		return ERROR_INVALID_DATA;

	status = smartcard_unpack_redir_scard_context_ref(log, s, pbContextNdrPtr,
	                                                  &(call->handles.hContext));
	if (status != SCARD_S_SUCCESS)
		return status;

	if ((rgAtrMasksNdrPtr && !call->cAtrs) || (!rgAtrMasksNdrPtr && call->cAtrs))
	{
		WLog_Print(log, WLOG_WARN,
		           "LocateCardsByATRA_Call rgAtrMasksNdrPtr (0x%08" PRIX32
		           ") and cAtrs (0x%08" PRIX32 ") inconsistency",
		           rgAtrMasksNdrPtr, call->cAtrs);
		return STATUS_INVALID_PARAMETER;
	}

	if (rgAtrMasksNdrPtr)
	{
		status = smartcard_ndr_read_atrmask(log, s, &call->rgAtrMasks, call->cAtrs, NDR_PTR_SIMPLE);
		if (status != SCARD_S_SUCCESS)
			return status;
	}

	if (rgReaderStatesNdrPtr)
	{
		status =
		    smartcard_unpack_reader_state_a(log, s, &call->rgReaderStates, call->cReaders, &index);
		if (status != SCARD_S_SUCCESS)
			return status;
	}

	smartcard_trace_locate_cards_by_atr_a_call(log, call);
	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_context_and_two_strings_a_call(wStream* s, ContextAndTwoStringA_Call* call)
{
	UINT32 sz1NdrPtr = 0;
	UINT32 sz2NdrPtr = 0;
	UINT32 index = 0;
	UINT32 pbContextNdrPtr = 0;

	WINPR_ASSERT(call);
	wLog* log = scard_log();

	LONG status = smartcard_unpack_redir_scard_context(log, s, &(call->handles.hContext), &index,
	                                                   &pbContextNdrPtr);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!smartcard_ndr_pointer_read(log, s, &index, &sz1NdrPtr))
		return ERROR_INVALID_DATA;
	if (!smartcard_ndr_pointer_read(log, s, &index, &sz2NdrPtr))
		return ERROR_INVALID_DATA;

	status =
	    smartcard_unpack_redir_scard_context_ref(log, s, pbContextNdrPtr, &call->handles.hContext);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (sz1NdrPtr)
	{
		status = smartcard_ndr_read_a(log, s, &call->sz1, NDR_PTR_FULL);
		if (status != SCARD_S_SUCCESS)
			return status;
	}
	if (sz2NdrPtr)
	{
		status = smartcard_ndr_read_a(log, s, &call->sz2, NDR_PTR_FULL);
		if (status != SCARD_S_SUCCESS)
			return status;
	}
	smartcard_trace_context_and_two_strings_a_call(log, call);
	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_context_and_two_strings_w_call(wStream* s, ContextAndTwoStringW_Call* call)
{
	UINT32 sz1NdrPtr = 0;
	UINT32 sz2NdrPtr = 0;
	UINT32 index = 0;
	UINT32 pbContextNdrPtr = 0;

	WINPR_ASSERT(call);
	wLog* log = scard_log();

	LONG status = smartcard_unpack_redir_scard_context(log, s, &(call->handles.hContext), &index,
	                                                   &pbContextNdrPtr);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!smartcard_ndr_pointer_read(log, s, &index, &sz1NdrPtr))
		return ERROR_INVALID_DATA;
	if (!smartcard_ndr_pointer_read(log, s, &index, &sz2NdrPtr))
		return ERROR_INVALID_DATA;

	status =
	    smartcard_unpack_redir_scard_context_ref(log, s, pbContextNdrPtr, &call->handles.hContext);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (sz1NdrPtr)
	{
		status = smartcard_ndr_read_w(log, s, &call->sz1, NDR_PTR_FULL);
		if (status != SCARD_S_SUCCESS)
			return status;
	}
	if (sz2NdrPtr)
	{
		status = smartcard_ndr_read_w(log, s, &call->sz2, NDR_PTR_FULL);
		if (status != SCARD_S_SUCCESS)
			return status;
	}
	smartcard_trace_context_and_two_strings_w_call(log, call);
	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_locate_cards_a_call(wStream* s, LocateCardsA_Call* call)
{
	UINT32 sz1NdrPtr = 0;
	UINT32 sz2NdrPtr = 0;
	UINT32 index = 0;
	UINT32 pbContextNdrPtr = 0;

	WINPR_ASSERT(call);
	wLog* log = scard_log();

	LONG status = smartcard_unpack_redir_scard_context(log, s, &(call->handles.hContext), &index,
	                                                   &pbContextNdrPtr);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 16))
		return STATUS_BUFFER_TOO_SMALL;

	Stream_Read_UINT32(s, call->cBytes);
	if (!smartcard_ndr_pointer_read(log, s, &index, &sz1NdrPtr))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, call->cReaders);
	if (!smartcard_ndr_pointer_read(log, s, &index, &sz2NdrPtr))
		return ERROR_INVALID_DATA;

	status = smartcard_unpack_redir_scard_context_ref(log, s, pbContextNdrPtr,
	                                                  &(call->handles.hContext));
	if (status != SCARD_S_SUCCESS)
		return status;

	if (sz1NdrPtr)
	{
		status = smartcard_ndr_read_fixed_string_a(log, s, &call->mszCards, call->cBytes,
		                                           NDR_PTR_SIMPLE);
		if (status != SCARD_S_SUCCESS)
			return status;
	}
	if (sz2NdrPtr)
	{
		status =
		    smartcard_unpack_reader_state_a(log, s, &call->rgReaderStates, call->cReaders, &index);
		if (status != SCARD_S_SUCCESS)
			return status;
	}
	smartcard_trace_locate_cards_a_call(log, call);
	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_locate_cards_w_call(wStream* s, LocateCardsW_Call* call)
{
	UINT32 sz1NdrPtr = 0;
	UINT32 sz2NdrPtr = 0;
	UINT32 index = 0;
	UINT32 pbContextNdrPtr = 0;

	WINPR_ASSERT(call);
	wLog* log = scard_log();

	LONG status = smartcard_unpack_redir_scard_context(log, s, &(call->handles.hContext), &index,
	                                                   &pbContextNdrPtr);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 16))
		return STATUS_BUFFER_TOO_SMALL;

	Stream_Read_UINT32(s, call->cBytes);
	if (!smartcard_ndr_pointer_read(log, s, &index, &sz1NdrPtr))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, call->cReaders);
	if (!smartcard_ndr_pointer_read(log, s, &index, &sz2NdrPtr))
		return ERROR_INVALID_DATA;

	status = smartcard_unpack_redir_scard_context_ref(log, s, pbContextNdrPtr,
	                                                  &(call->handles.hContext));
	if (status != SCARD_S_SUCCESS)
		return status;

	if (sz1NdrPtr)
	{
		status = smartcard_ndr_read_fixed_string_w(log, s, &call->mszCards, call->cBytes,
		                                           NDR_PTR_SIMPLE);
		if (status != SCARD_S_SUCCESS)
			return status;
	}
	if (sz2NdrPtr)
	{
		status =
		    smartcard_unpack_reader_state_w(log, s, &call->rgReaderStates, call->cReaders, &index);
		if (status != SCARD_S_SUCCESS)
			return status;
	}
	smartcard_trace_locate_cards_w_call(log, call);
	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_set_attrib_call(wStream* s, SetAttrib_Call* call)
{
	UINT32 index = 0;
	UINT32 ndrPtr = 0;
	UINT32 pbContextNdrPtr = 0;

	WINPR_ASSERT(call);
	wLog* log = scard_log();

	LONG status = smartcard_unpack_redir_scard_context(log, s, &(call->handles.hContext), &index,
	                                                   &pbContextNdrPtr);
	if (status != SCARD_S_SUCCESS)
		return status;
	status = smartcard_unpack_redir_scard_handle(log, s, &(call->handles.hCard), &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 12))
		return STATUS_BUFFER_TOO_SMALL;
	Stream_Read_UINT32(s, call->dwAttrId);
	Stream_Read_UINT32(s, call->cbAttrLen);

	if (!smartcard_ndr_pointer_read(log, s, &index, &ndrPtr))
		return ERROR_INVALID_DATA;

	status = smartcard_unpack_redir_scard_context_ref(log, s, pbContextNdrPtr,
	                                                  &(call->handles.hContext));
	if (status != SCARD_S_SUCCESS)
		return status;

	status = smartcard_unpack_redir_scard_handle_ref(log, s, &(call->handles.hCard));
	if (status != SCARD_S_SUCCESS)
		return status;

	if (ndrPtr)
	{
		size_t len = 0;
		status = smartcard_ndr_read_ex(log, s, &call->pbAttr, 0, 1, NDR_PTR_SIMPLE, &len);
		if (status != SCARD_S_SUCCESS)
			return status;
		if (call->cbAttrLen > len)
			call->cbAttrLen = WINPR_ASSERTING_INT_CAST(DWORD, len);
	}
	else
		call->cbAttrLen = 0;
	smartcard_trace_set_attrib_call(log, call);
	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_locate_cards_by_atr_w_call(wStream* s, LocateCardsByATRW_Call* call)
{
	UINT32 rgReaderStatesNdrPtr = 0;
	UINT32 rgAtrMasksNdrPtr = 0;
	UINT32 index = 0;
	UINT32 pbContextNdrPtr = 0;

	WINPR_ASSERT(call);
	wLog* log = scard_log();

	call->rgReaderStates = NULL;

	LONG status = smartcard_unpack_redir_scard_context(log, s, &(call->handles.hContext), &index,
	                                                   &pbContextNdrPtr);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 16))
		return STATUS_BUFFER_TOO_SMALL;

	Stream_Read_UINT32(s, call->cAtrs);
	if (!smartcard_ndr_pointer_read(log, s, &index, &rgAtrMasksNdrPtr))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, call->cReaders); /* cReaders (4 bytes) */
	if (!smartcard_ndr_pointer_read(log, s, &index, &rgReaderStatesNdrPtr))
		return ERROR_INVALID_DATA;

	status = smartcard_unpack_redir_scard_context_ref(log, s, pbContextNdrPtr,
	                                                  &(call->handles.hContext));
	if (status != SCARD_S_SUCCESS)
		return status;

	if ((rgAtrMasksNdrPtr && !call->cAtrs) || (!rgAtrMasksNdrPtr && call->cAtrs))
	{
		WLog_Print(log, WLOG_WARN,
		           "LocateCardsByATRW_Call rgAtrMasksNdrPtr (0x%08" PRIX32
		           ") and cAtrs (0x%08" PRIX32 ") inconsistency",
		           rgAtrMasksNdrPtr, call->cAtrs);
		return STATUS_INVALID_PARAMETER;
	}

	if (rgAtrMasksNdrPtr)
	{
		status = smartcard_ndr_read_atrmask(log, s, &call->rgAtrMasks, call->cAtrs, NDR_PTR_SIMPLE);
		if (status != SCARD_S_SUCCESS)
			return status;
	}

	if (rgReaderStatesNdrPtr)
	{
		status =
		    smartcard_unpack_reader_state_w(log, s, &call->rgReaderStates, call->cReaders, &index);
		if (status != SCARD_S_SUCCESS)
			return status;
	}

	smartcard_trace_locate_cards_by_atr_w_call(log, call);
	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_read_cache_a_call(wStream* s, ReadCacheA_Call* call)
{
	UINT32 mszNdrPtr = 0;
	UINT32 contextNdrPtr = 0;
	UINT32 index = 0;
	UINT32 pbContextNdrPtr = 0;

	WINPR_ASSERT(call);
	wLog* log = scard_log();

	if (!smartcard_ndr_pointer_read(log, s, &index, &mszNdrPtr))
		return ERROR_INVALID_DATA;

	LONG status = smartcard_unpack_redir_scard_context(log, s, &(call->Common.handles.hContext),
	                                                   &index, &pbContextNdrPtr);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!smartcard_ndr_pointer_read(log, s, &index, &contextNdrPtr))
		return ERROR_INVALID_DATA;

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 12))
		return STATUS_BUFFER_TOO_SMALL;
	Stream_Read_UINT32(s, call->Common.FreshnessCounter);
	Stream_Read_INT32(s, call->Common.fPbDataIsNULL);
	Stream_Read_UINT32(s, call->Common.cbDataLen);

	call->szLookupName = NULL;
	if (mszNdrPtr)
	{
		status = smartcard_ndr_read_a(log, s, &call->szLookupName, NDR_PTR_FULL);
		if (status != SCARD_S_SUCCESS)
			return status;
	}

	status = smartcard_unpack_redir_scard_context_ref(log, s, pbContextNdrPtr,
	                                                  &call->Common.handles.hContext);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (contextNdrPtr)
	{
		status = smartcard_ndr_read_u(log, s, &call->Common.CardIdentifier);
		if (status != SCARD_S_SUCCESS)
			return status;
	}
	smartcard_trace_read_cache_a_call(log, call);
	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_read_cache_w_call(wStream* s, ReadCacheW_Call* call)
{
	UINT32 mszNdrPtr = 0;
	UINT32 contextNdrPtr = 0;
	UINT32 index = 0;
	UINT32 pbContextNdrPtr = 0;

	WINPR_ASSERT(call);
	wLog* log = scard_log();

	if (!smartcard_ndr_pointer_read(log, s, &index, &mszNdrPtr))
		return ERROR_INVALID_DATA;

	LONG status = smartcard_unpack_redir_scard_context(log, s, &(call->Common.handles.hContext),
	                                                   &index, &pbContextNdrPtr);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!smartcard_ndr_pointer_read(log, s, &index, &contextNdrPtr))
		return ERROR_INVALID_DATA;

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 12))
		return STATUS_BUFFER_TOO_SMALL;
	Stream_Read_UINT32(s, call->Common.FreshnessCounter);
	Stream_Read_INT32(s, call->Common.fPbDataIsNULL);
	Stream_Read_UINT32(s, call->Common.cbDataLen);

	call->szLookupName = NULL;
	if (mszNdrPtr)
	{
		status = smartcard_ndr_read_w(log, s, &call->szLookupName, NDR_PTR_FULL);
		if (status != SCARD_S_SUCCESS)
			return status;
	}

	status = smartcard_unpack_redir_scard_context_ref(log, s, pbContextNdrPtr,
	                                                  &call->Common.handles.hContext);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (contextNdrPtr)
	{
		status = smartcard_ndr_read_u(log, s, &call->Common.CardIdentifier);
		if (status != SCARD_S_SUCCESS)
			return status;
	}
	smartcard_trace_read_cache_w_call(log, call);
	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_write_cache_a_call(wStream* s, WriteCacheA_Call* call)
{
	UINT32 mszNdrPtr = 0;
	UINT32 contextNdrPtr = 0;
	UINT32 pbDataNdrPtr = 0;
	UINT32 index = 0;
	UINT32 pbContextNdrPtr = 0;

	WINPR_ASSERT(call);
	wLog* log = scard_log();

	if (!smartcard_ndr_pointer_read(log, s, &index, &mszNdrPtr))
		return ERROR_INVALID_DATA;

	LONG status = smartcard_unpack_redir_scard_context(log, s, &(call->Common.handles.hContext),
	                                                   &index, &pbContextNdrPtr);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!smartcard_ndr_pointer_read(log, s, &index, &contextNdrPtr))
		return ERROR_INVALID_DATA;

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 8))
		return STATUS_BUFFER_TOO_SMALL;

	Stream_Read_UINT32(s, call->Common.FreshnessCounter);
	Stream_Read_UINT32(s, call->Common.cbDataLen);

	if (!smartcard_ndr_pointer_read(log, s, &index, &pbDataNdrPtr))
		return ERROR_INVALID_DATA;

	call->szLookupName = NULL;
	if (mszNdrPtr)
	{
		status = smartcard_ndr_read_a(log, s, &call->szLookupName, NDR_PTR_FULL);
		if (status != SCARD_S_SUCCESS)
			return status;
	}

	status = smartcard_unpack_redir_scard_context_ref(log, s, pbContextNdrPtr,
	                                                  &call->Common.handles.hContext);
	if (status != SCARD_S_SUCCESS)
		return status;

	call->Common.CardIdentifier = NULL;
	if (contextNdrPtr)
	{
		status = smartcard_ndr_read_u(log, s, &call->Common.CardIdentifier);
		if (status != SCARD_S_SUCCESS)
			return status;
	}

	call->Common.pbData = NULL;
	if (pbDataNdrPtr)
	{
		status = smartcard_ndr_read(log, s, &call->Common.pbData, call->Common.cbDataLen, 1,
		                            NDR_PTR_SIMPLE);
		if (status != SCARD_S_SUCCESS)
			return status;
	}
	smartcard_trace_write_cache_a_call(log, call);
	return SCARD_S_SUCCESS;
}

LONG smartcard_unpack_write_cache_w_call(wStream* s, WriteCacheW_Call* call)
{
	UINT32 mszNdrPtr = 0;
	UINT32 contextNdrPtr = 0;
	UINT32 pbDataNdrPtr = 0;
	UINT32 index = 0;
	UINT32 pbContextNdrPtr = 0;

	WINPR_ASSERT(call);
	wLog* log = scard_log();

	if (!smartcard_ndr_pointer_read(log, s, &index, &mszNdrPtr))
		return ERROR_INVALID_DATA;

	LONG status = smartcard_unpack_redir_scard_context(log, s, &(call->Common.handles.hContext),
	                                                   &index, &pbContextNdrPtr);
	if (status != SCARD_S_SUCCESS)
		return status;

	if (!smartcard_ndr_pointer_read(log, s, &index, &contextNdrPtr))
		return ERROR_INVALID_DATA;

	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 8))
		return STATUS_BUFFER_TOO_SMALL;
	Stream_Read_UINT32(s, call->Common.FreshnessCounter);
	Stream_Read_UINT32(s, call->Common.cbDataLen);

	if (!smartcard_ndr_pointer_read(log, s, &index, &pbDataNdrPtr))
		return ERROR_INVALID_DATA;

	call->szLookupName = NULL;
	if (mszNdrPtr)
	{
		status = smartcard_ndr_read_w(log, s, &call->szLookupName, NDR_PTR_FULL);
		if (status != SCARD_S_SUCCESS)
			return status;
	}

	status = smartcard_unpack_redir_scard_context_ref(log, s, pbContextNdrPtr,
	                                                  &call->Common.handles.hContext);
	if (status != SCARD_S_SUCCESS)
		return status;

	call->Common.CardIdentifier = NULL;
	if (contextNdrPtr)
	{
		status = smartcard_ndr_read_u(log, s, &call->Common.CardIdentifier);
		if (status != SCARD_S_SUCCESS)
			return status;
	}

	call->Common.pbData = NULL;
	if (pbDataNdrPtr)
	{
		status = smartcard_ndr_read(log, s, &call->Common.pbData, call->Common.cbDataLen, 1,
		                            NDR_PTR_SIMPLE);
		if (status != SCARD_S_SUCCESS)
			return status;
	}
	smartcard_trace_write_cache_w_call(log, call);
	return status;
}

LONG smartcard_unpack_get_transmit_count_call(wStream* s, GetTransmitCount_Call* call)
{
	WINPR_ASSERT(call);
	wLog* log = scard_log();

	UINT32 index = 0;
	UINT32 pbContextNdrPtr = 0;

	LONG status = smartcard_unpack_redir_scard_context(log, s, &(call->handles.hContext), &index,
	                                                   &pbContextNdrPtr);
	if (status != SCARD_S_SUCCESS)
		return status;

	status = smartcard_unpack_redir_scard_handle(log, s, &(call->handles.hCard), &index);
	if (status != SCARD_S_SUCCESS)
		return status;

	status = smartcard_unpack_redir_scard_context_ref(log, s, pbContextNdrPtr,
	                                                  &(call->handles.hContext));
	if (status != SCARD_S_SUCCESS)
	{
		WLog_Print(log, WLOG_ERROR,
		           "smartcard_unpack_redir_scard_context_ref failed with error %" PRId32 "",
		           status);
		return status;
	}

	status = smartcard_unpack_redir_scard_handle_ref(log, s, &(call->handles.hCard));
	if (status != SCARD_S_SUCCESS)
		WLog_Print(log, WLOG_ERROR,
		           "smartcard_unpack_redir_scard_handle_ref failed with error %" PRId32 "", status);

	smartcard_trace_get_transmit_count_call(log, call);
	return status;
}

LONG smartcard_unpack_get_reader_icon_call(wStream* s, GetReaderIcon_Call* call)
{
	WINPR_ASSERT(call);
	wLog* log = scard_log();
	return smartcard_unpack_common_context_and_string_w(log, s, &call->handles.hContext,
	                                                    &call->szReaderName);
}

LONG smartcard_unpack_context_and_string_a_call(wStream* s, ContextAndStringA_Call* call)
{
	WINPR_ASSERT(call);
	wLog* log = scard_log();
	return smartcard_unpack_common_context_and_string_a(log, s, &call->handles.hContext, &call->sz);
}

LONG smartcard_unpack_context_and_string_w_call(wStream* s, ContextAndStringW_Call* call)
{
	WINPR_ASSERT(call);
	wLog* log = scard_log();
	return smartcard_unpack_common_context_and_string_w(log, s, &call->handles.hContext, &call->sz);
}

LONG smartcard_unpack_get_device_type_id_call(wStream* s, GetDeviceTypeId_Call* call)
{
	WINPR_ASSERT(call);
	wLog* log = scard_log();
	return smartcard_unpack_common_context_and_string_w(log, s, &call->handles.hContext,
	                                                    &call->szReaderName);
}

LONG smartcard_pack_device_type_id_return(wStream* s, const GetDeviceTypeId_Return* ret)
{
	WINPR_ASSERT(ret);
	wLog* log = scard_log();
	smartcard_trace_device_type_id_return(log, ret);

	if (!Stream_EnsureRemainingCapacity(s, 4))
	{
		WLog_Print(log, WLOG_ERROR, "Stream_EnsureRemainingCapacity failed!");
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Write_UINT32(s, ret->dwDeviceId); /* cBytes (4 bytes) */

	return ret->ReturnCode;
}

LONG smartcard_pack_locate_cards_return(wStream* s, const LocateCards_Return* ret)
{
	WINPR_ASSERT(ret);
	wLog* log = scard_log();

	LONG status = 0;
	DWORD cbDataLen = ret->cReaders;
	UINT32 index = 0;

	smartcard_trace_locate_cards_return(log, ret);
	if (ret->ReturnCode != SCARD_S_SUCCESS)
		cbDataLen = 0;
	if (cbDataLen == SCARD_AUTOALLOCATE)
		cbDataLen = 0;

	if (!Stream_EnsureRemainingCapacity(s, 4))
	{
		WLog_Print(log, WLOG_ERROR, "Stream_EnsureRemainingCapacity failed!");
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
	WINPR_ASSERT(ret);
	wLog* log = scard_log();

	LONG status = 0;
	UINT32 index = 0;
	DWORD cbDataLen = ret->cbDataLen;
	smartcard_trace_get_reader_icon_return(log, ret);
	if (ret->ReturnCode != SCARD_S_SUCCESS)
		cbDataLen = 0;
	if (cbDataLen == SCARD_AUTOALLOCATE)
		cbDataLen = 0;

	if (!Stream_EnsureRemainingCapacity(s, 4))
	{
		WLog_Print(log, WLOG_ERROR, "Stream_EnsureRemainingCapacity failed!");
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
	WINPR_ASSERT(ret);
	wLog* log = scard_log();

	smartcard_trace_get_transmit_count_return(log, ret);

	if (!Stream_EnsureRemainingCapacity(s, 4))
	{
		WLog_Print(log, WLOG_ERROR, "Stream_EnsureRemainingCapacity failed!");
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Write_UINT32(s, ret->cTransmitCount); /* cBytes (4 cbDataLen) */

	return ret->ReturnCode;
}

LONG smartcard_pack_read_cache_return(wStream* s, const ReadCache_Return* ret)
{
	WINPR_ASSERT(ret);
	wLog* log = scard_log();

	LONG status = 0;
	UINT32 index = 0;
	DWORD cbDataLen = ret->cbDataLen;
	smartcard_trace_read_cache_return(log, ret);
	if (ret->ReturnCode != SCARD_S_SUCCESS)
		cbDataLen = 0;

	if (cbDataLen == SCARD_AUTOALLOCATE)
		cbDataLen = 0;

	if (!Stream_EnsureRemainingCapacity(s, 4))
	{
		WLog_Print(log, WLOG_ERROR, "Stream_EnsureRemainingCapacity failed!");
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
