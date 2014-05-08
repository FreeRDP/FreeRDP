/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Smart Card Structure Packing
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

#include <winpr/crt.h>
#include <winpr/print.h>

#include "smartcard_pack.h"

UINT32 smartcard_unpack_common_type_header(SMARTCARD_DEVICE* smartcard, wStream* s)
{
	UINT8 version;
	UINT32 filler;
	UINT8 endianness;
	UINT16 commonHeaderLength;

	if (Stream_GetRemainingLength(s) < 8)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "CommonTypeHeader is too short: %d",
				(int) Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	/* Process CommonTypeHeader */

	Stream_Read_UINT8(s, version); /* Version (1 byte) */
	Stream_Read_UINT8(s, endianness); /* Endianness (1 byte) */
	Stream_Read_UINT16(s, commonHeaderLength); /* CommonHeaderLength (2 bytes) */
	Stream_Read_UINT32(s, filler); /* Filler (4 bytes), should be 0xCCCCCCCC */

	if (version != 1)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "Unsupported CommonTypeHeader Version %d", version);
		return STATUS_INVALID_PARAMETER;
	}

	if (endianness != 0x10)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "Unsupported CommonTypeHeader Endianness %d", endianness);
		return STATUS_INVALID_PARAMETER;
	}

	if (commonHeaderLength != 8)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "Unsupported CommonTypeHeader CommonHeaderLength %d", commonHeaderLength);
		return STATUS_INVALID_PARAMETER;
	}

	if (filler != 0xCCCCCCCC)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "Unexpected CommonTypeHeader Filler 0x%08X", filler);
		return STATUS_INVALID_PARAMETER;
	}

	return 0;
}

UINT32 smartcard_pack_common_type_header(SMARTCARD_DEVICE* smartcard, wStream* s)
{
	Stream_Write_UINT8(s, 1); /* Version (1 byte) */
	Stream_Write_UINT8(s, 0x10); /* Endianness (1 byte) */
	Stream_Write_UINT16(s, 8); /* CommonHeaderLength (2 bytes) */
	Stream_Write_UINT32(s, 0xCCCCCCCC); /* Filler (4 bytes), should be 0xCCCCCCCC */

	return 0;
}

UINT32 smartcard_unpack_private_type_header(SMARTCARD_DEVICE* smartcard, wStream* s)
{
	UINT32 filler;
	UINT32 objectBufferLength;

	if (Stream_GetRemainingLength(s) < 8)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "PrivateTypeHeader is too short: %d",
				(int) Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Read_UINT32(s, objectBufferLength); /* ObjectBufferLength (4 bytes) */
	Stream_Read_UINT32(s, filler); /* Filler (4 bytes), should be 0x00000000 */

	if (filler != 0x00000000)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "Unexpected PrivateTypeHeader Filler 0x%08X", filler);
		return STATUS_INVALID_PARAMETER;
	}

	if (objectBufferLength != Stream_GetRemainingLength(s))
	{
		WLog_Print(smartcard->log, WLOG_WARN, "PrivateTypeHeader ObjectBufferLength mismatch: Actual: %d, Expected: %d",
				(int) objectBufferLength, Stream_GetRemainingLength(s));
		return STATUS_INVALID_PARAMETER;
	}

	return 0;
}

UINT32 smartcard_pack_private_type_header(SMARTCARD_DEVICE* smartcard, wStream* s, UINT32 objectBufferLength)
{
	Stream_Write_UINT32(s, objectBufferLength); /* ObjectBufferLength (4 bytes) */
	Stream_Write_UINT32(s, 0x00000000); /* Filler (4 bytes), should be 0x00000000 */

	return 0;
}

UINT32 smartcard_unpack_read_size_align(SMARTCARD_DEVICE* smartcard, wStream* s, UINT32 size, UINT32 alignment)
{
	UINT32 pad;

	pad = size;
	size = (size + alignment - 1) & ~(alignment - 1);
	pad = size - pad;

	if (pad)
		Stream_Seek(s, pad);

	return pad;
}

UINT32 smartcard_pack_write_size_align(SMARTCARD_DEVICE* smartcard, wStream* s, UINT32 size, UINT32 alignment)
{
	UINT32 pad;

	pad = size;
	size = (size + alignment - 1) & ~(alignment - 1);
	pad = size - pad;

	if (pad)
		Stream_Zero(s, pad);

	return pad;
}

SCARDCONTEXT smartcard_scard_context_native_from_redir(SMARTCARD_DEVICE* smartcard, REDIR_SCARDCONTEXT* context)
{
	SCARDCONTEXT hContext = 0;

	if ((context->cbContext != sizeof(ULONG_PTR)) && (context->cbContext != 0))
	{
		WLog_Print(smartcard->log, WLOG_WARN,
			"REDIR_SCARDCONTEXT does not match native size: Actual: %d, Expected: %d",
			context->cbContext, sizeof(ULONG_PTR));
		return 0;
	}

	if (context->cbContext)
		CopyMemory(&hContext, &(context->pbContext), context->cbContext);
	else
		ZeroMemory(&hContext, sizeof(ULONG_PTR));

	return hContext;
}

void smartcard_scard_context_native_to_redir(SMARTCARD_DEVICE* smartcard, REDIR_SCARDCONTEXT* context, SCARDCONTEXT hContext)
{
	ZeroMemory(context, sizeof(REDIR_SCARDCONTEXT));
	context->cbContext = sizeof(ULONG_PTR);
	CopyMemory(&(context->pbContext), &hContext, context->cbContext);
}

SCARDHANDLE smartcard_scard_handle_native_from_redir(SMARTCARD_DEVICE* smartcard, REDIR_SCARDHANDLE* handle)
{
	SCARDHANDLE hCard = 0;

	if (handle->cbHandle != sizeof(ULONG_PTR))
	{
		WLog_Print(smartcard->log, WLOG_WARN,
			"REDIR_SCARDHANDLE does not match native size: Actual: %d, Expected: %d",
			handle->cbHandle, sizeof(ULONG_PTR));
		return 0;
	}

	if (handle->cbHandle)
		CopyMemory(&hCard, &(handle->pbHandle), handle->cbHandle);

	return hCard;
}

void smartcard_scard_handle_native_to_redir(SMARTCARD_DEVICE* smartcard, REDIR_SCARDHANDLE* handle, SCARDHANDLE hCard)
{
	ZeroMemory(handle, sizeof(REDIR_SCARDHANDLE));
	handle->cbHandle = sizeof(ULONG_PTR);
	CopyMemory(&(handle->pbHandle), &hCard, handle->cbHandle);
}

UINT32 smartcard_unpack_redir_scard_context(SMARTCARD_DEVICE* smartcard, wStream* s, REDIR_SCARDCONTEXT* context)
{
	UINT32 pbContextNdrPtr;

	ZeroMemory(context, sizeof(REDIR_SCARDCONTEXT));

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "REDIR_SCARDCONTEXT is too short: %d",
				(int) Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Read_UINT32(s, context->cbContext); /* cbContext (4 bytes) */

	if (Stream_GetRemainingLength(s) < context->cbContext)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "REDIR_SCARDCONTEXT is too short: Actual: %d, Expected: %d",
				(int) Stream_GetRemainingLength(s), context->cbContext);
		return STATUS_BUFFER_TOO_SMALL;
	}

	if ((context->cbContext != 0) && (context->cbContext != 4) && (context->cbContext != 8))
	{
		WLog_Print(smartcard->log, WLOG_WARN, "REDIR_SCARDCONTEXT length is not 0, 4 or 8: %d\n", context->cbContext);
		return STATUS_INVALID_PARAMETER;
	}

	Stream_Read_UINT32(s, pbContextNdrPtr); /* pbContextNdrPtr (4 bytes) */

	if (((context->cbContext == 0) && pbContextNdrPtr) || ((context->cbContext != 0) && !pbContextNdrPtr))
	{
		WLog_Print(smartcard->log, WLOG_WARN, "REDIR_SCARDCONTEXT cbContext (%d) pbContextNdrPtr (%d) inconsistency",
				(int) context->cbContext, (int) pbContextNdrPtr);
		return STATUS_INVALID_PARAMETER;
	}

	if (context->cbContext > Stream_GetRemainingLength(s))
	{
		WLog_Print(smartcard->log, WLOG_WARN, "REDIR_SCARDCONTEXT is too long: Actual: %d, Expected: %d",
				(int) Stream_GetRemainingLength(s), context->cbContext);
		return STATUS_INVALID_PARAMETER;
	}

	return SCARD_S_SUCCESS;
}

UINT32 smartcard_pack_redir_scard_context(SMARTCARD_DEVICE* smartcard, wStream* s, REDIR_SCARDCONTEXT* context)
{
	UINT32 pbContextNdrPtr;

	pbContextNdrPtr = (context->cbContext) ? 0x00020001 : 0;

	Stream_Write_UINT32(s, context->cbContext); /* cbContext (4 bytes) */
	Stream_Write_UINT32(s, pbContextNdrPtr); /* pbContextNdrPtr (4 bytes) */

	return SCARD_S_SUCCESS;
}

UINT32 smartcard_unpack_redir_scard_context_ref(SMARTCARD_DEVICE* smartcard, wStream* s, REDIR_SCARDCONTEXT* context)
{
	UINT32 length;

	if (context->cbContext == 0)
		return SCARD_S_SUCCESS;

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "REDIR_SCARDCONTEXT is too short: Actual: %d, Expected: %d\n",
				(int) Stream_GetRemainingLength(s), 4);
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Read_UINT32(s, length); /* Length (4 bytes) */

	if (length != context->cbContext)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "REDIR_SCARDCONTEXT length (%d) cbContext (%d) mismatch\n",
			length, context->cbContext);
		return STATUS_INVALID_PARAMETER;
	}

	if ((context->cbContext != 0) && (context->cbContext != 4) && (context->cbContext != 8))
	{
		WLog_Print(smartcard->log, WLOG_WARN, "REDIR_SCARDCONTEXT length is not 4 or 8: %d\n", context->cbContext);
		return STATUS_INVALID_PARAMETER;
	}

	if (Stream_GetRemainingLength(s) < context->cbContext)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "REDIR_SCARDCONTEXT is too short: Actual: %d, Expected: %d\n",
				(int) Stream_GetRemainingLength(s), context->cbContext);
		return STATUS_BUFFER_TOO_SMALL;
	}

	if (context->cbContext)
		Stream_Read(s, &(context->pbContext), context->cbContext);
	else
		ZeroMemory(&(context->pbContext), sizeof(context->pbContext));

	return SCARD_S_SUCCESS;
}

UINT32 smartcard_pack_redir_scard_context_ref(SMARTCARD_DEVICE* smartcard, wStream* s, REDIR_SCARDCONTEXT* context)
{
	Stream_Write_UINT32(s, context->cbContext); /* Length (4 bytes) */

	if (context->cbContext)
	{
		Stream_Write(s, &(context->pbContext), context->cbContext);
	}

	return SCARD_S_SUCCESS;
}

UINT32 smartcard_unpack_redir_scard_handle(SMARTCARD_DEVICE* smartcard, wStream* s, REDIR_SCARDHANDLE* handle)
{
	UINT32 pbHandleNdrPtr;

	ZeroMemory(handle, sizeof(REDIR_SCARDHANDLE));

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "SCARDHANDLE is too short: %d",
				(int) Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Read_UINT32(s, handle->cbHandle); /* Length (4 bytes) */

	if ((Stream_GetRemainingLength(s) < handle->cbHandle) || (!handle->cbHandle))
	{
		WLog_Print(smartcard->log, WLOG_WARN, "SCARDHANDLE is too short: Actual: %d, Expected: %d",
				(int) Stream_GetRemainingLength(s), handle->cbHandle);
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Read_UINT32(s, pbHandleNdrPtr); /* NdrPtr (4 bytes) */

	return SCARD_S_SUCCESS;
}

UINT32 smartcard_pack_redir_scard_handle(SMARTCARD_DEVICE* smartcard, wStream* s, REDIR_SCARDHANDLE* handle)
{
	UINT32 pbHandleNdrPtr;

	pbHandleNdrPtr = (handle->cbHandle) ? 0x00020002 : 0;

	Stream_Write_UINT32(s, handle->cbHandle); /* cbHandle (4 bytes) */
	Stream_Write_UINT32(s, pbHandleNdrPtr); /* pbHandleNdrPtr (4 bytes) */

	return SCARD_S_SUCCESS;
}

UINT32 smartcard_unpack_redir_scard_handle_ref(SMARTCARD_DEVICE* smartcard, wStream* s, REDIR_SCARDHANDLE* handle)
{
	UINT32 length;

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "REDIR_SCARDHANDLE is too short: Actual: %d, Expected: %d\n",
				(int) Stream_GetRemainingLength(s), 4);
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Read_UINT32(s, length); /* Length (4 bytes) */

	if (length != handle->cbHandle)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "REDIR_SCARDHANDLE length (%d) cbHandle (%d) mismatch\n",
			length, handle->cbHandle);
		return STATUS_INVALID_PARAMETER;
	}

	if ((handle->cbHandle != 4) && (handle->cbHandle != 8))
	{
		WLog_Print(smartcard->log, WLOG_WARN, "REDIR_SCARDHANDLE length is not 4 or 8: %d\n", handle->cbHandle);
		return STATUS_INVALID_PARAMETER;
	}

	if ((Stream_GetRemainingLength(s) < handle->cbHandle) || (!handle->cbHandle))
	{
		WLog_Print(smartcard->log, WLOG_WARN, "REDIR_SCARDHANDLE is too short: Actual: %d, Expected: %d\n",
				(int) Stream_GetRemainingLength(s), handle->cbHandle);
		return STATUS_BUFFER_TOO_SMALL;
	}

	if (handle->cbHandle)
		Stream_Read(s, &(handle->pbHandle), handle->cbHandle);

	return SCARD_S_SUCCESS;
}

UINT32 smartcard_pack_redir_scard_handle_ref(SMARTCARD_DEVICE* smartcard, wStream* s, REDIR_SCARDHANDLE* handle)
{
	Stream_Write_UINT32(s, handle->cbHandle); /* Length (4 bytes) */

	if (handle->cbHandle)
		Stream_Write(s, &(handle->pbHandle), handle->cbHandle);

	return SCARD_S_SUCCESS;
}

UINT32 smartcard_unpack_establish_context_call(SMARTCARD_DEVICE* smartcard, wStream* s, EstablishContext_Call* call)
{
	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "EstablishContext_Call is too short: Actual: %d, Expected: %d\n",
				(int) Stream_GetRemainingLength(s), 4);
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Read_UINT32(s, call->dwScope); /* dwScope (4 bytes) */

	return SCARD_S_SUCCESS;
}

void smartcard_trace_establish_context_call(SMARTCARD_DEVICE* smartcard, EstablishContext_Call* call)
{
	if (!WLog_IsLevelActive(smartcard->log, WLOG_DEBUG))
		return;

	WLog_Print(smartcard->log, WLOG_DEBUG, "EstablishContext_Call {");

	WLog_Print(smartcard->log, WLOG_DEBUG, "dwScope: %s (0x%08X)",
			SCardGetScopeString(call->dwScope), call->dwScope);

	WLog_Print(smartcard->log, WLOG_DEBUG, "}");
}

UINT32 smartcard_pack_establish_context_return(SMARTCARD_DEVICE* smartcard, wStream* s, EstablishContext_Return* ret)
{
	UINT32 status;

	status = smartcard_pack_redir_scard_context(smartcard, s, &(ret->hContext));

	if (status)
		return status;

	status = smartcard_pack_redir_scard_context_ref(smartcard, s, &(ret->hContext));

	if (status)
		return status;

	return SCARD_S_SUCCESS;
}

void smartcard_trace_establish_context_return(SMARTCARD_DEVICE* smartcard, EstablishContext_Return* ret)
{
	BYTE* pb;

	if (!WLog_IsLevelActive(smartcard->log, WLOG_DEBUG))
		return;

	WLog_Print(smartcard->log, WLOG_DEBUG, "EstablishContext_Return {");

	WLog_Print(smartcard->log, WLOG_DEBUG, "ReturnCode: %s (0x%08X)",
		SCardGetErrorString(ret->ReturnCode), ret->ReturnCode);

	pb = (BYTE*) &(ret->hContext.pbContext);

	if (ret->hContext.cbContext > 4)
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "hContext: 0x%02X%02X%02X%02X%02X%02X%02X%02X (%d)",
			pb[0], pb[1], pb[2], pb[3], pb[4], pb[5], pb[6], pb[7], ret->hContext.cbContext);
	}
	else
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "hContext: 0x%02X%02X%02X%02X (%d)",
			pb[0], pb[1], pb[2], pb[3], ret->hContext.cbContext);
	}

	WLog_Print(smartcard->log, WLOG_DEBUG, "}");
}

UINT32 smartcard_unpack_context_call(SMARTCARD_DEVICE* smartcard, wStream* s, Context_Call* call)
{
	UINT32 status;

	status = smartcard_unpack_redir_scard_context(smartcard, s, &(call->hContext));

	if (status)
		return status;

	status = smartcard_unpack_redir_scard_context_ref(smartcard, s, &(call->hContext));

	if (status)
		return status;

	return SCARD_S_SUCCESS;
}

void smartcard_trace_context_call(SMARTCARD_DEVICE* smartcard, Context_Call* call, const char* name)
{
	BYTE* pb;

	if (!WLog_IsLevelActive(smartcard->log, WLOG_DEBUG))
		return;

	WLog_Print(smartcard->log, WLOG_DEBUG, "%s_Call {", name);

	pb = (BYTE*) &(call->hContext.pbContext);

	if (call->hContext.cbContext > 4)
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "hContext: 0x%02X%02X%02X%02X%02X%02X%02X%02X (%d)",
			pb[0], pb[1], pb[2], pb[3], pb[4], pb[5], pb[6], pb[7], call->hContext.cbContext);
	}
	else
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "hContext: 0x%02X%02X%02X%02X (%d)",
			pb[0], pb[1], pb[2], pb[3], call->hContext.cbContext);
	}

	WLog_Print(smartcard->log, WLOG_DEBUG, "}");
}

void smartcard_trace_long_return(SMARTCARD_DEVICE* smartcard, Long_Return* ret, const char* name)
{
	if (!WLog_IsLevelActive(smartcard->log, WLOG_DEBUG))
		return;

	WLog_Print(smartcard->log, WLOG_DEBUG, "%s_Return {", name);

	WLog_Print(smartcard->log, WLOG_DEBUG, "ReturnCode: %s (0x%08X)",
		SCardGetErrorString(ret->ReturnCode), ret->ReturnCode);

	WLog_Print(smartcard->log, WLOG_DEBUG, "}");
}

UINT32 smartcard_unpack_list_readers_call(SMARTCARD_DEVICE* smartcard, wStream* s, ListReaders_Call* call)
{
	UINT32 status;
	UINT32 count;
	UINT32 mszGroupsNdrPtr;

	call->mszGroups = NULL;

	status = smartcard_unpack_redir_scard_context(smartcard, s, &(call->hContext));

	if (status)
		return status;

	if (Stream_GetRemainingLength(s) < 16)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "ListReaders_Call is too short: %d",
				(int) Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Read_UINT32(s, call->cBytes); /* cBytes (4 bytes) */
	Stream_Read_UINT32(s, mszGroupsNdrPtr); /* mszGroupsNdrPtr (4 bytes) */
	Stream_Read_UINT32(s, call->fmszReadersIsNULL); /* fmszReadersIsNULL (4 bytes) */
	Stream_Read_UINT32(s, call->cchReaders); /* cchReaders (4 bytes) */

	status = smartcard_unpack_redir_scard_context_ref(smartcard, s, &(call->hContext));

	if (status)
		return status;
	
	if ((mszGroupsNdrPtr && !call->cBytes) || (!mszGroupsNdrPtr && call->cBytes))
	{
		WLog_Print(smartcard->log, WLOG_WARN,
			   "ListReaders_Call mszGroupsNdrPtr (0x%08X) and cBytes (0x%08X) inconsistency",
			   (int) mszGroupsNdrPtr, (int) call->cBytes);
		return STATUS_INVALID_PARAMETER;
	}
	
	if (mszGroupsNdrPtr)
	{
		Stream_Read_UINT32(s, count); /* NdrCount (4 bytes) */
		
		if (count != call->cBytes)
		{
			WLog_Print(smartcard->log, WLOG_WARN,
				   "ListReaders_Call NdrCount (0x%08X) and cBytes (0x%08X) inconsistency",
				   (int) count, (int) call->cBytes);
			return STATUS_INVALID_PARAMETER;
		}
		
		if (Stream_GetRemainingLength(s) < call->cBytes)
		{
			WLog_Print(smartcard->log, WLOG_WARN, "ListReaders_Call is too short: Actual: %d, Expected: %d",
				   (int) Stream_GetRemainingLength(s), call->cBytes);
			return STATUS_BUFFER_TOO_SMALL;
		}
		
		call->mszGroups = (BYTE*) calloc(1, call->cBytes + 4);
		
		if (!call->mszGroups)
		{
			WLog_Print(smartcard->log, WLOG_WARN, "ListReaders_Call out of memory error (mszGroups)");
			return STATUS_NO_MEMORY;
		}
		
		Stream_Read(s, call->mszGroups, call->cBytes);
		
		smartcard_unpack_read_size_align(smartcard, s, call->cBytes, 4);
	}

	return SCARD_S_SUCCESS;
}

void smartcard_trace_list_readers_call(SMARTCARD_DEVICE* smartcard, ListReaders_Call* call, BOOL unicode)
{
	BYTE* pb;
	char* mszGroupsA = NULL;

	if (!WLog_IsLevelActive(smartcard->log, WLOG_DEBUG))
		return;

	if (unicode)
		ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) call->mszGroups, call->cBytes / 2, &mszGroupsA, 0, NULL, NULL);

	WLog_Print(smartcard->log, WLOG_DEBUG, "ListReaders%S_Call {", unicode ? "W" : "A");

	pb = (BYTE*) &(call->hContext.pbContext);

	if (call->hContext.cbContext > 4)
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "hContext: 0x%02X%02X%02X%02X%02X%02X%02X%02X (%d)",
			pb[0], pb[1], pb[2], pb[3], pb[4], pb[5], pb[6], pb[7], call->hContext.cbContext);
	}
	else
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "hContext: 0x%02X%02X%02X%02X (%d)",
			pb[0], pb[1], pb[2], pb[3], call->hContext.cbContext);
	}

	WLog_Print(smartcard->log, WLOG_DEBUG,
		"cBytes: %d mszGroups: %s fmszReadersIsNULL: %d cchReaders: 0x%08X",
		call->cBytes, mszGroupsA, call->fmszReadersIsNULL, call->cchReaders);

	WLog_Print(smartcard->log, WLOG_DEBUG, "}");

	if (unicode)
		free(mszGroupsA);
}

UINT32 smartcard_pack_list_readers_return(SMARTCARD_DEVICE* smartcard, wStream* s, ListReaders_Return* ret)
{
	UINT32 mszNdrPtr;

	mszNdrPtr = (ret->cBytes) ? 0x00020008 : 0;

	Stream_EnsureRemainingCapacity(s, ret->cBytes + 32);

	Stream_Write_UINT32(s, ret->cBytes); /* cBytes (4 bytes) */
	Stream_Write_UINT32(s, mszNdrPtr); /* mszNdrPtr (4 bytes) */
	
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

void smartcard_trace_list_readers_return(SMARTCARD_DEVICE* smartcard, ListReaders_Return* ret, BOOL unicode)
{
	int index;
	int length;
	char* mszA = NULL;

	if (!WLog_IsLevelActive(smartcard->log, WLOG_DEBUG))
		return;

	if (unicode)
	{
		length = ret->cBytes / 2;
		ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) ret->msz, length, &mszA, 0, NULL, NULL);
	}
	else
	{
		length = ret->cBytes;
		mszA = (char*) malloc(length);
		CopyMemory(mszA, ret->msz, ret->cBytes);
	}

	for (index = 0; index < length - 2; index++)
	{
		if (mszA[index] == '\0')
			mszA[index] = ',';
	}

	WLog_Print(smartcard->log, WLOG_DEBUG, "ListReaders%s_Return {", unicode ? "W" : "A");

	WLog_Print(smartcard->log, WLOG_DEBUG, "ReturnCode: %s (0x%08X)",
		SCardGetErrorString(ret->ReturnCode), ret->ReturnCode);

	WLog_Print(smartcard->log, WLOG_DEBUG, "cBytes: %d msz: %s", ret->cBytes, mszA);

	WLog_Print(smartcard->log, WLOG_DEBUG, "}");

	free(mszA);
}

UINT32 smartcard_unpack_connect_common(SMARTCARD_DEVICE* smartcard, wStream* s, Connect_Common* common)
{
	UINT32 status;

	if (Stream_GetRemainingLength(s) < 8)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "Connect_Common is too short: %d",
				(int) Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	status = smartcard_unpack_redir_scard_context(smartcard, s, &(common->hContext));

	if (status)
		return status;

	Stream_Read_UINT32(s, common->dwShareMode); /* dwShareMode (4 bytes) */
	Stream_Read_UINT32(s, common->dwPreferredProtocols); /* dwPreferredProtocols (4 bytes) */

	return SCARD_S_SUCCESS;
}

UINT32 smartcard_unpack_connect_a_call(SMARTCARD_DEVICE* smartcard, wStream* s, ConnectA_Call* call)
{
	UINT32 status;
	UINT32 count;

	call->szReader = NULL;

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "ConnectA_Call is too short: %d",
				(int) Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Seek_UINT32(s); /* szReaderNdrPtr (4 bytes) */

	status = smartcard_unpack_connect_common(smartcard, s, &(call->Common));

	if (status)
		return status;

	/* szReader */

	Stream_Seek_UINT32(s); /* NdrMaxCount (4 bytes) */
	Stream_Seek_UINT32(s); /* NdrOffset (4 bytes) */
	Stream_Read_UINT32(s, count); /* NdrActualCount (4 bytes) */

	call->szReader = (unsigned char*) malloc(count + 1);

	if (!call->szReader)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "ConnectA_Call out of memory error (call->szReader)");
		return STATUS_NO_MEMORY;
	}

	Stream_Read(s, call->szReader, count);
	smartcard_unpack_read_size_align(smartcard, s, count, 4);
	call->szReader[count] = '\0';

	smartcard_unpack_redir_scard_context_ref(smartcard, s, &(call->Common.hContext));

	return SCARD_S_SUCCESS;
}

void smartcard_trace_connect_a_call(SMARTCARD_DEVICE* smartcard, ConnectA_Call* call)
{
	BYTE* pb;

	if (!WLog_IsLevelActive(smartcard->log, WLOG_DEBUG))
		return;

	WLog_Print(smartcard->log, WLOG_DEBUG, "ConnectA_Call {");

	pb = (BYTE*) &(call->Common.hContext.pbContext);

	if (call->Common.hContext.cbContext > 4)
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "hContext: 0x%02X%02X%02X%02X%02X%02X%02X%02X (%d)",
			pb[0], pb[1], pb[2], pb[3], pb[4], pb[5], pb[6], pb[7], call->Common.hContext.cbContext);
	}
	else
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "hContext: 0x%02X%02X%02X%02X (%d)",
			pb[0], pb[1], pb[2], pb[3], call->Common.hContext.cbContext);
	}

	WLog_Print(smartcard->log, WLOG_DEBUG, "szReader: %s dwShareMode: %s (0x%08X) dwPreferredProtocols: %s (0x%08X)",
		call->szReader, SCardGetShareModeString(call->Common.dwShareMode), call->Common.dwShareMode,
		SCardGetProtocolString(call->Common.dwPreferredProtocols), call->Common.dwPreferredProtocols);

	WLog_Print(smartcard->log, WLOG_DEBUG, "}");
}

UINT32 smartcard_unpack_connect_w_call(SMARTCARD_DEVICE* smartcard, wStream* s, ConnectW_Call* call)
{
	UINT32 status;
	UINT32 count;

	call->szReader = NULL;

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "ConnectW_Call is too short: %d",
				(int) Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Seek_UINT32(s); /* szReaderNdrPtr (4 bytes) */

	status = smartcard_unpack_connect_common(smartcard, s, &(call->Common));

	if (status)
		return status;

	/* szReader */

	Stream_Seek_UINT32(s); /* NdrMaxCount (4 bytes) */
	Stream_Seek_UINT32(s); /* NdrOffset (4 bytes) */
	Stream_Read_UINT32(s, count); /* NdrActualCount (4 bytes) */

	call->szReader = (WCHAR*) malloc((count + 1) * 2);

	if (!call->szReader)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "ConnectW_Call out of memory error (call->szReader)");
		return STATUS_NO_MEMORY;
	}

	Stream_Read(s, call->szReader, (count * 2));
	smartcard_unpack_read_size_align(smartcard, s, (count * 2), 4);
	call->szReader[count] = '\0';

	smartcard_unpack_redir_scard_context_ref(smartcard, s, &(call->Common.hContext));

	return SCARD_S_SUCCESS;
}

void smartcard_trace_connect_w_call(SMARTCARD_DEVICE* smartcard, ConnectW_Call* call)
{
	BYTE* pb;
	char* szReaderA = NULL;

	if (!WLog_IsLevelActive(smartcard->log, WLOG_DEBUG))
		return;

	ConvertFromUnicode(CP_UTF8, 0, call->szReader, -1, &szReaderA, 0, NULL, NULL);

	WLog_Print(smartcard->log, WLOG_DEBUG, "ConnectW_Call {");

	pb = (BYTE*) &(call->Common.hContext.pbContext);

	if (call->Common.hContext.cbContext > 4)
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "hContext: 0x%02X%02X%02X%02X%02X%02X%02X%02X (%d)",
			pb[0], pb[1], pb[2], pb[3], pb[4], pb[5], pb[6], pb[7], call->Common.hContext.cbContext);
	}
	else
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "hContext: 0x%02X%02X%02X%02X (%d)",
			pb[0], pb[1], pb[2], pb[3], call->Common.hContext.cbContext);
	}

	WLog_Print(smartcard->log, WLOG_DEBUG, "szReader: %s dwShareMode: %s (0x%08X) dwPreferredProtocols: %s (0x%08X)",
		szReaderA, SCardGetShareModeString(call->Common.dwShareMode), call->Common.dwShareMode,
		SCardGetProtocolString(call->Common.dwPreferredProtocols), call->Common.dwPreferredProtocols);

	WLog_Print(smartcard->log, WLOG_DEBUG, "}");

	free(szReaderA);
}

UINT32 smartcard_pack_connect_return(SMARTCARD_DEVICE* smartcard, wStream* s, Connect_Return* ret)
{
	UINT32 status;

	status = smartcard_pack_redir_scard_context(smartcard, s, &(ret->hContext));

	if (status)
		return status;

	status = smartcard_pack_redir_scard_handle(smartcard, s, &(ret->hCard));

	if (status)
		return status;

	Stream_Write_UINT32(s, ret->dwActiveProtocol); /* dwActiveProtocol (4 bytes) */

	status = smartcard_pack_redir_scard_context_ref(smartcard, s, &(ret->hContext));

	if (status)
		return status;

	status = smartcard_pack_redir_scard_handle_ref(smartcard, s, &(ret->hCard));

	if (status)
		return status;

	return SCARD_S_SUCCESS;
}

void smartcard_trace_connect_return(SMARTCARD_DEVICE* smartcard, Connect_Return* ret)
{
	BYTE* pb;

	if (!WLog_IsLevelActive(smartcard->log, WLOG_DEBUG))
		return;

	WLog_Print(smartcard->log, WLOG_DEBUG, "Connect_Return {");

	WLog_Print(smartcard->log, WLOG_DEBUG, "ReturnCode: %s (0x%08X)",
		SCardGetErrorString(ret->ReturnCode), ret->ReturnCode);

	pb = (BYTE*) &(ret->hContext.pbContext);

	if (ret->hContext.cbContext > 4)
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "hContext: 0x%02X%02X%02X%02X%02X%02X%02X%02X (%d)",
			pb[0], pb[1], pb[2], pb[3], pb[4], pb[5], pb[6], pb[7], ret->hContext.cbContext);
	}
	else
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "hContext: 0x%02X%02X%02X%02X (%d)",
			pb[0], pb[1], pb[2], pb[3], ret->hContext.cbContext);
	}

	pb = (BYTE*) &(ret->hCard.pbHandle);

	if (ret->hCard.cbHandle > 4)
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "hCard: 0x%02X%02X%02X%02X%02X%02X%02X%02X (%d)",
			pb[0], pb[1], pb[2], pb[3], pb[4], pb[5], pb[6], pb[7], ret->hCard.cbHandle);
	}
	else
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "hCard: 0x%02X%02X%02X%02X (%d)",
			pb[0], pb[1], pb[2], pb[3], ret->hCard.cbHandle);
	}

	WLog_Print(smartcard->log, WLOG_DEBUG, "dwActiveProtocol: %s (0x%08X)",
		SCardGetProtocolString(ret->dwActiveProtocol), ret->dwActiveProtocol);

	WLog_Print(smartcard->log, WLOG_DEBUG, "}");
}

UINT32 smartcard_unpack_reconnect_call(SMARTCARD_DEVICE* smartcard, wStream* s, Reconnect_Call* call)
{
	UINT32 status;

	status = smartcard_unpack_redir_scard_context(smartcard, s, &(call->hContext));

	if (status)
		return status;

	status = smartcard_unpack_redir_scard_handle(smartcard, s, &(call->hCard));

	if (status)
		return status;

	if (Stream_GetRemainingLength(s) < 12)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "Reconnect_Call is too short: %d",
				(int) Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Read_UINT32(s, call->dwShareMode); /* dwShareMode (4 bytes) */
	Stream_Read_UINT32(s, call->dwPreferredProtocols); /* dwPreferredProtocols (4 bytes) */
	Stream_Read_UINT32(s, call->dwInitialization); /* dwInitialization (4 bytes) */

	status = smartcard_unpack_redir_scard_context_ref(smartcard, s, &(call->hContext));

	if (status)
		return status;

	status = smartcard_unpack_redir_scard_handle_ref(smartcard, s, &(call->hCard));

	if (status)
		return status;

	return SCARD_S_SUCCESS;
}

void smartcard_trace_reconnect_call(SMARTCARD_DEVICE* smartcard, Reconnect_Call* call)
{
	BYTE* pb;

	if (!WLog_IsLevelActive(smartcard->log, WLOG_DEBUG))
		return;

	WLog_Print(smartcard->log, WLOG_DEBUG, "Reconnect_Call {");

	pb = (BYTE*) &(call->hContext.pbContext);

	if (call->hContext.cbContext > 4)
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "hContext: 0x%02X%02X%02X%02X%02X%02X%02X%02X (%d)",
			pb[0], pb[1], pb[2], pb[3], pb[4], pb[5], pb[6], pb[7], call->hContext.cbContext);
	}
	else
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "hContext: 0x%02X%02X%02X%02X (%d)",
			pb[0], pb[1], pb[2], pb[3], call->hContext.cbContext);
	}

	pb = (BYTE*) &(call->hCard.pbHandle);

	if (call->hCard.cbHandle > 4)
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "hCard: 0x%02X%02X%02X%02X%02X%02X%02X%02X (%d)",
			pb[0], pb[1], pb[2], pb[3], pb[4], pb[5], pb[6], pb[7], call->hCard.cbHandle);
	}
	else
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "hCard: 0x%02X%02X%02X%02X (%d)",
			pb[0], pb[1], pb[2], pb[3], call->hCard.cbHandle);
	}

	WLog_Print(smartcard->log, WLOG_DEBUG,
		"dwShareMode: %s (0x%08X) dwPreferredProtocols: %s (0x%08X) dwInitialization: %s (0x%08X)",
		SCardGetShareModeString(call->dwShareMode), call->dwShareMode,
		SCardGetProtocolString(call->dwPreferredProtocols), call->dwPreferredProtocols,
		SCardGetDispositionString(call->dwInitialization), call->dwInitialization);

	WLog_Print(smartcard->log, WLOG_DEBUG, "}");
}

UINT32 smartcard_pack_reconnect_return(SMARTCARD_DEVICE* smartcard, wStream* s, Reconnect_Return* ret)
{
	Stream_Write_UINT32(s, ret->dwActiveProtocol); /* dwActiveProtocol (4 bytes) */

	return SCARD_S_SUCCESS;
}

void smartcard_trace_reconnect_return(SMARTCARD_DEVICE* smartcard, Reconnect_Return* ret)
{
	if (!WLog_IsLevelActive(smartcard->log, WLOG_DEBUG))
		return;

	WLog_Print(smartcard->log, WLOG_DEBUG, "Reconnect_Return {");

	WLog_Print(smartcard->log, WLOG_DEBUG, "ReturnCode: %s (0x%08X)",
		SCardGetErrorString(ret->ReturnCode), ret->ReturnCode);

	WLog_Print(smartcard->log, WLOG_DEBUG, "dwActiveProtocol: %s (0x%08X)",
		SCardGetProtocolString(ret->dwActiveProtocol), ret->dwActiveProtocol);

	WLog_Print(smartcard->log, WLOG_DEBUG, "}");
}

UINT32 smartcard_unpack_hcard_and_disposition_call(SMARTCARD_DEVICE* smartcard, wStream* s, HCardAndDisposition_Call* call)
{
	UINT32 status;

	status = smartcard_unpack_redir_scard_context(smartcard, s, &(call->hContext));

	if (status)
		return status;

	status = smartcard_unpack_redir_scard_handle(smartcard, s, &(call->hCard));

	if (status)
		return status;

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "HCardAndDisposition_Call is too short: %d",
				(int) Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Read_UINT32(s, call->dwDisposition); /* dwDisposition (4 bytes) */

	status = smartcard_unpack_redir_scard_context_ref(smartcard, s, &(call->hContext));

	if (status)
		return status;

	status = smartcard_unpack_redir_scard_handle_ref(smartcard, s, &(call->hCard));

	if (status)
		return status;

	return SCARD_S_SUCCESS;
}

void smartcard_trace_hcard_and_disposition_call(SMARTCARD_DEVICE* smartcard, HCardAndDisposition_Call* call, const char* name)
{
	BYTE* pb;

	if (!WLog_IsLevelActive(smartcard->log, WLOG_DEBUG))
		return;

	WLog_Print(smartcard->log, WLOG_DEBUG, "%s_Call {", name);

	pb = (BYTE*) &(call->hContext.pbContext);

	if (call->hContext.cbContext > 4)
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "hContext: 0x%02X%02X%02X%02X%02X%02X%02X%02X (%d)",
			pb[0], pb[1], pb[2], pb[3], pb[4], pb[5], pb[6], pb[7], call->hContext.cbContext);
	}
	else
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "hContext: 0x%02X%02X%02X%02X (%d)",
			pb[0], pb[1], pb[2], pb[3], call->hContext.cbContext);
	}

	pb = (BYTE*) &(call->hCard.pbHandle);

	if (call->hCard.cbHandle > 4)
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "hCard: 0x%02X%02X%02X%02X%02X%02X%02X%02X (%d)",
			pb[0], pb[1], pb[2], pb[3], pb[4], pb[5], pb[6], pb[7], call->hCard.cbHandle);
	}
	else
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "hCard: 0x%02X%02X%02X%02X (%d)",
			pb[0], pb[1], pb[2], pb[3], call->hCard.cbHandle);
	}

	WLog_Print(smartcard->log, WLOG_DEBUG, "dwDisposition: %s (0x%08X)",
		SCardGetDispositionString(call->dwDisposition), call->dwDisposition);

	WLog_Print(smartcard->log, WLOG_DEBUG, "}");
}

UINT32 smartcard_unpack_get_status_change_a_call(SMARTCARD_DEVICE* smartcard, wStream* s, GetStatusChangeA_Call* call)
{
	UINT32 index;
	UINT32 count;
	UINT32 status;
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
		WLog_Print(smartcard->log, WLOG_WARN, "GetStatusChangeA_Call is too short: %d",
				(int) Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Read_UINT32(s, call->dwTimeOut); /* dwTimeOut (4 bytes) */
	Stream_Read_UINT32(s, call->cReaders); /* cReaders (4 bytes) */
	Stream_Read_UINT32(s, rgReaderStatesNdrPtr); /* rgReaderStatesNdrPtr (4 bytes) */

	status = smartcard_unpack_redir_scard_context_ref(smartcard, s, &(call->hContext));

	if (status)
		return status;

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "GetStatusChangeA_Call is too short: %d",
				(int) Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Read_UINT32(s, count); /* NdrCount (4 bytes) */

	if (count != call->cReaders)
	{
		WLog_Print(smartcard->log, WLOG_WARN,
				"GetStatusChangeA_Call unexpected reader count: Actual: %d, Expected: %d",
				(int) count, call->cReaders);
		return STATUS_INVALID_PARAMETER;
	}

	if (call->cReaders > 0)
	{
		call->rgReaderStates = (LPSCARD_READERSTATEA) calloc(call->cReaders, sizeof(SCARD_READERSTATEA));

		if (!call->rgReaderStates)
		{
			WLog_Print(smartcard->log, WLOG_WARN, "GetStatusChangeA_Call out of memory error (call->rgReaderStates)");
			return STATUS_NO_MEMORY;
		}

		for (index = 0; index < call->cReaders; index++)
		{
			readerState = &call->rgReaderStates[index];

			if (Stream_GetRemainingLength(s) < 52)
			{
				WLog_Print(smartcard->log, WLOG_WARN, "GetStatusChangeA_Call is too short: %d",
						(int) Stream_GetRemainingLength(s));
				return STATUS_BUFFER_TOO_SMALL;
			}

			Stream_Read_UINT32(s, szReaderNdrPtr); /* szReaderNdrPtr (4 bytes) */
			Stream_Read_UINT32(s, readerState->dwCurrentState); /* dwCurrentState (4 bytes) */
			Stream_Read_UINT32(s, readerState->dwEventState); /* dwEventState (4 bytes) */
			Stream_Read_UINT32(s, readerState->cbAtr); /* cbAtr (4 bytes) */
			Stream_Read(s, readerState->rgbAtr, 32); /* rgbAtr [0..32] (32 bytes) */
			Stream_Seek(s, 4); /* rgbAtr [32..36] (4 bytes) */
		}

		for (index = 0; index < call->cReaders; index++)
		{
			readerState = &call->rgReaderStates[index];

			if (Stream_GetRemainingLength(s) < 12)
			{
				WLog_Print(smartcard->log, WLOG_WARN, "GetStatusChangeA_Call is too short: %d",
						(int) Stream_GetRemainingLength(s));
				return STATUS_BUFFER_TOO_SMALL;
			}

			Stream_Read_UINT32(s, maxCount); /* NdrMaxCount (4 bytes) */
			Stream_Read_UINT32(s, offset); /* NdrOffset (4 bytes) */
			Stream_Read_UINT32(s, count); /* NdrActualCount (4 bytes) */

			if (Stream_GetRemainingLength(s) < count)
			{
				WLog_Print(smartcard->log, WLOG_WARN, "GetStatusChangeA_Call is too short: %d",
						(int) Stream_GetRemainingLength(s));
				return STATUS_BUFFER_TOO_SMALL;
			}

			readerState->szReader = (LPCSTR) malloc(count + 1);

			if (!readerState->szReader)
			{
				WLog_Print(smartcard->log, WLOG_WARN,
					"GetStatusChangeA_Call out of memory error (readerState->szReader)");
				return STATUS_NO_MEMORY;
			}

			Stream_Read(s, (void*) readerState->szReader, count);
			smartcard_unpack_read_size_align(smartcard, s, count, 4);
			((char*) readerState->szReader)[count] = '\0';

			if (!readerState->szReader)
			{
				WLog_Print(smartcard->log, WLOG_WARN, "GetStatusChangeA_Call null reader name");
				return STATUS_INVALID_PARAMETER;
			}
		}
	}

	return SCARD_S_SUCCESS;
}

void smartcard_trace_get_status_change_a_call(SMARTCARD_DEVICE* smartcard, GetStatusChangeA_Call* call)
{
	BYTE* pb;
	UINT32 index;
	char* szEventState;
	char* szCurrentState;
	LPSCARD_READERSTATEA readerState;

	if (!WLog_IsLevelActive(smartcard->log, WLOG_DEBUG))
		return;

	WLog_Print(smartcard->log, WLOG_DEBUG, "GetStatusChangeA_Call {");

	pb = (BYTE*) &(call->hContext.pbContext);

	if (call->hContext.cbContext > 4)
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "hContext: 0x%02X%02X%02X%02X%02X%02X%02X%02X (%d)",
			pb[0], pb[1], pb[2], pb[3], pb[4], pb[5], pb[6], pb[7], call->hContext.cbContext);
	}
	else
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "hContext: 0x%02X%02X%02X%02X (%d)",
			pb[0], pb[1], pb[2], pb[3], call->hContext.cbContext);
	}

	WLog_Print(smartcard->log, WLOG_DEBUG, "dwTimeOut: 0x%08X cReaders: %d",
		call->dwTimeOut, call->cReaders);

	for (index = 0; index < call->cReaders; index++)
	{
		readerState = &call->rgReaderStates[index];

		WLog_Print(smartcard->log, WLOG_DEBUG,
			"\t[%d]: szReader: %s cbAtr: %d",
			index, readerState->szReader, readerState->cbAtr);

		szCurrentState = SCardGetReaderStateString(readerState->dwCurrentState);
		szEventState = SCardGetReaderStateString(readerState->dwEventState);

		WLog_Print(smartcard->log, WLOG_DEBUG,
			"\t[%d]: dwCurrentState: %s (0x%08X)",
			index, szCurrentState, readerState->dwCurrentState);

		WLog_Print(smartcard->log, WLOG_DEBUG,
			"\t[%d]: dwEventState: %s (0x%08X)",
			index, szEventState, readerState->dwEventState);

		free(szCurrentState);
		free(szEventState);
	}

	WLog_Print(smartcard->log, WLOG_DEBUG, "}");
}

UINT32 smartcard_unpack_get_status_change_w_call(SMARTCARD_DEVICE* smartcard, wStream* s, GetStatusChangeW_Call* call)
{
	UINT32 index;
	UINT32 count;
	UINT32 status;
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
		WLog_Print(smartcard->log, WLOG_WARN, "GetStatusChangeW_Call is too short: %d",
				(int) Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Read_UINT32(s, call->dwTimeOut); /* dwTimeOut (4 bytes) */
	Stream_Read_UINT32(s, call->cReaders); /* cReaders (4 bytes) */
	Stream_Read_UINT32(s, rgReaderStatesNdrPtr); /* rgReaderStatesNdrPtr (4 bytes) */

	status = smartcard_unpack_redir_scard_context_ref(smartcard, s, &(call->hContext));

	if (status)
		return status;

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "GetStatusChangeW_Call is too short: %d",
				(int) Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Seek_UINT32(s); /* NdrConformant (4 bytes) */

	if (call->cReaders > 0)
	{
		call->rgReaderStates = (LPSCARD_READERSTATEW) calloc(call->cReaders, sizeof(SCARD_READERSTATEW));

		if (!call->rgReaderStates)
		{
			WLog_Print(smartcard->log, WLOG_WARN, "GetStatusChangeW_Call out of memory error (call->rgReaderStates)");
			return STATUS_NO_MEMORY;
		}

		for (index = 0; index < call->cReaders; index++)
		{
			readerState = &call->rgReaderStates[index];

			if (Stream_GetRemainingLength(s) < 52)
			{
				WLog_Print(smartcard->log, WLOG_WARN, "GetStatusChangeW_Call is too short: %d",
						(int) Stream_GetRemainingLength(s));
				return STATUS_BUFFER_TOO_SMALL;
			}

			Stream_Read_UINT32(s, szReaderNdrPtr); /* (4 bytes) */
			Stream_Read_UINT32(s, readerState->dwCurrentState); /* dwCurrentState (4 bytes) */
			Stream_Read_UINT32(s, readerState->dwEventState); /* dwEventState (4 bytes) */
			Stream_Read_UINT32(s, readerState->cbAtr); /* cbAtr (4 bytes) */
			Stream_Read(s, readerState->rgbAtr, 32); /* rgbAtr [0..32] (32 bytes) */
			Stream_Seek(s, 4); /* rgbAtr [32..36] (4 bytes) */
		}

		for (index = 0; index < call->cReaders; index++)
		{
			readerState = &call->rgReaderStates[index];

			if (Stream_GetRemainingLength(s) < 12)
			{
				WLog_Print(smartcard->log, WLOG_WARN, "GetStatusChangeW_Call is too short: %d",
						(int) Stream_GetRemainingLength(s));
				return STATUS_BUFFER_TOO_SMALL;
			}

			Stream_Read_UINT32(s, maxCount); /* NdrMaxCount (4 bytes) */
			Stream_Read_UINT32(s, offset); /* NdrOffset (4 bytes) */
			Stream_Read_UINT32(s, count); /* NdrActualCount (4 bytes) */

			if (Stream_GetRemainingLength(s) < (count * 2))
			{
				WLog_Print(smartcard->log, WLOG_WARN, "GetStatusChangeW_Call is too short: %d",
						(int) Stream_GetRemainingLength(s));
				return STATUS_BUFFER_TOO_SMALL;
			}

			readerState->szReader = (WCHAR*) malloc((count + 1) * 2);

			if (!readerState->szReader)
			{
				WLog_Print(smartcard->log, WLOG_WARN,
					"GetStatusChangeW_Call out of memory error (readerState->szReader)");
				return STATUS_NO_MEMORY;
			}

			Stream_Read(s, (void*) readerState->szReader, (count * 2));
			smartcard_unpack_read_size_align(smartcard, s, (count * 2), 4);
			((WCHAR*) readerState->szReader)[count] = '\0';

			if (!readerState->szReader)
			{
				WLog_Print(smartcard->log, WLOG_WARN, "GetStatusChangeW_Call null reader name");
				return STATUS_INVALID_PARAMETER;
			}
		}
	}

	return SCARD_S_SUCCESS;
}

void smartcard_trace_get_status_change_w_call(SMARTCARD_DEVICE* smartcard, GetStatusChangeW_Call* call)
{
	BYTE* pb;
	UINT32 index;
	char* szEventState;
	char* szCurrentState;
	LPSCARD_READERSTATEW readerState;

	if (!WLog_IsLevelActive(smartcard->log, WLOG_DEBUG))
		return;

	WLog_Print(smartcard->log, WLOG_DEBUG, "GetStatusChangeW_Call {");

	pb = (BYTE*) &(call->hContext.pbContext);

	if (call->hContext.cbContext > 4)
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "hContext: 0x%02X%02X%02X%02X%02X%02X%02X%02X (%d)",
			pb[0], pb[1], pb[2], pb[3], pb[4], pb[5], pb[6], pb[7], call->hContext.cbContext);
	}
	else
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "hContext: 0x%02X%02X%02X%02X (%d)",
			pb[0], pb[1], pb[2], pb[3], call->hContext.cbContext);
	}

	WLog_Print(smartcard->log, WLOG_DEBUG, "dwTimeOut: 0x%08X cReaders: %d",
		call->dwTimeOut, call->cReaders);

	for (index = 0; index < call->cReaders; index++)
	{
		char* szReaderA = NULL;

		readerState = &call->rgReaderStates[index];

		ConvertFromUnicode(CP_UTF8, 0, readerState->szReader, -1, &szReaderA, 0, NULL, NULL);

		WLog_Print(smartcard->log, WLOG_DEBUG,
			"\t[%d]: szReader: %s cbAtr: %d",
			index, szReaderA, readerState->cbAtr);

		szCurrentState = SCardGetReaderStateString(readerState->dwCurrentState);
		szEventState = SCardGetReaderStateString(readerState->dwEventState);

		WLog_Print(smartcard->log, WLOG_DEBUG,
			"\t[%d]: dwCurrentState: %s (0x%08X)",
			index, szCurrentState, readerState->dwCurrentState);

		WLog_Print(smartcard->log, WLOG_DEBUG,
			"\t[%d]: dwEventState: %s (0x%08X)",
			index, szEventState, readerState->dwEventState);

		free(szCurrentState);
		free(szEventState);

		free(szReaderA);
	}

	WLog_Print(smartcard->log, WLOG_DEBUG, "}");
}

UINT32 smartcard_pack_get_status_change_return(SMARTCARD_DEVICE* smartcard, wStream* s, GetStatusChange_Return* ret)
{
	UINT32 index;
	ReaderState_Return* rgReaderState;

	Stream_Write_UINT32(s, ret->cReaders); /* cReaders (4 bytes) */
	Stream_Write_UINT32(s, 0x00020100); /* rgReaderStatesNdrPtr (4 bytes) */
	Stream_Write_UINT32(s, ret->cReaders); /* rgReaderStatesNdrCount (4 bytes) */

	for (index = 0; index < ret->cReaders; index++)
	{
		rgReaderState = &(ret->rgReaderStates[index]);
		Stream_Write_UINT32(s, rgReaderState->dwCurrentState); /* dwCurrentState (4 bytes) */
		Stream_Write_UINT32(s, rgReaderState->dwEventState); /* dwEventState (4 bytes) */
		Stream_Write_UINT32(s, rgReaderState->cbAtr); /* cbAtr (4 bytes) */
		Stream_Write(s, rgReaderState->rgbAtr, 32); /* rgbAtr [0..32] (32 bytes) */
		Stream_Zero(s, 4); /* rgbAtr [32..36] (32 bytes) */
	}

	return SCARD_S_SUCCESS;
}

void smartcard_trace_get_status_change_return(SMARTCARD_DEVICE* smartcard, GetStatusChange_Return* ret, BOOL unicode)
{
	UINT32 index;
	char* rgbAtr;
	char* szEventState;
	char* szCurrentState;
	ReaderState_Return* rgReaderState;

	if (!WLog_IsLevelActive(smartcard->log, WLOG_DEBUG))
		return;

	WLog_Print(smartcard->log, WLOG_DEBUG, "GetStatusChange%s_Return {", unicode ? "W" : "A");

	WLog_Print(smartcard->log, WLOG_DEBUG, "ReturnCode: %s (0x%08X)",
		SCardGetErrorString(ret->ReturnCode), ret->ReturnCode);

	WLog_Print(smartcard->log, WLOG_DEBUG, "cReaders: %d", ret->cReaders);

	for (index = 0; index < ret->cReaders; index++)
	{
		rgReaderState = &(ret->rgReaderStates[index]);

		szCurrentState = SCardGetReaderStateString(rgReaderState->dwCurrentState);
		szEventState = SCardGetReaderStateString(rgReaderState->dwEventState);
		rgbAtr = winpr_BinToHexString((BYTE*) &(rgReaderState->rgbAtr), rgReaderState->cbAtr, FALSE);

		WLog_Print(smartcard->log, WLOG_DEBUG,
			"\t[%d]: dwCurrentState: %s (0x%08X)",
			index, szCurrentState, rgReaderState->dwCurrentState);

		WLog_Print(smartcard->log, WLOG_DEBUG,
			"\t[%d]: dwEventState: %s (0x%08X)",
			index, szEventState, rgReaderState->dwEventState);

		WLog_Print(smartcard->log, WLOG_DEBUG,
			"\t[%d]: cbAtr: %d rgbAtr: %s",
			index, rgReaderState->cbAtr, rgbAtr);

		free(szCurrentState);
		free(szEventState);
		free(rgbAtr);
	}

	WLog_Print(smartcard->log, WLOG_DEBUG, "}");
}

UINT32 smartcard_unpack_state_call(SMARTCARD_DEVICE* smartcard, wStream* s, State_Call* call)
{
	UINT32 status;

	status = smartcard_unpack_redir_scard_context(smartcard, s, &(call->hContext));

	if (status)
		return status;

	status = smartcard_unpack_redir_scard_handle(smartcard, s, &(call->hCard));

	if (status)
		return status;

	if (Stream_GetRemainingLength(s) < 8)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "State_Call is too short: %d",
				(int) Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Read_UINT32(s, call->fpbAtrIsNULL); /* fpbAtrIsNULL (4 bytes) */
	Stream_Read_UINT32(s, call->cbAtrLen); /* cbAtrLen (4 bytes) */

	status = smartcard_unpack_redir_scard_context_ref(smartcard, s, &(call->hContext));

	if (status)
		return status;

	status = smartcard_unpack_redir_scard_handle_ref(smartcard, s, &(call->hCard));

	if (status)
		return status;

	return SCARD_S_SUCCESS;
}

UINT32 smartcard_pack_state_return(SMARTCARD_DEVICE* smartcard, wStream* s, State_Return* ret)
{
	Stream_Write_UINT32(s, ret->dwState); /* dwState (4 bytes) */
	Stream_Write_UINT32(s, ret->dwProtocol); /* dwProtocol (4 bytes) */
	Stream_Write_UINT32(s, ret->cbAtrLen); /* cbAtrLen (4 bytes) */
	Stream_Write_UINT32(s, 0x00020020); /* rgAtrNdrPtr (4 bytes) */
	Stream_Write_UINT32(s, ret->cbAtrLen); /* rgAtrLength (4 bytes) */
	Stream_Write(s, ret->rgAtr, ret->cbAtrLen); /* rgAtr */

	smartcard_pack_write_size_align(smartcard, s, ret->cbAtrLen, 4);

	return SCARD_S_SUCCESS;
}

UINT32 smartcard_unpack_status_call(SMARTCARD_DEVICE* smartcard, wStream* s, Status_Call* call)
{
	UINT32 status;

	status = smartcard_unpack_redir_scard_context(smartcard, s, &(call->hContext));

	if (status)
		return status;

	status = smartcard_unpack_redir_scard_handle(smartcard, s, &(call->hCard));

	if (status)
		return status;

	if (Stream_GetRemainingLength(s) < 12)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "Status_Call is too short: %d",
				(int) Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Read_UINT32(s, call->fmszReaderNamesIsNULL); /* fmszReaderNamesIsNULL (4 bytes) */
	Stream_Read_UINT32(s, call->cchReaderLen); /* cchReaderLen (4 bytes) */
	Stream_Read_UINT32(s, call->cbAtrLen); /* cbAtrLen (4 bytes) */

	status = smartcard_unpack_redir_scard_context_ref(smartcard, s, &(call->hContext));

	if (status)
		return status;

	status = smartcard_unpack_redir_scard_handle_ref(smartcard, s, &(call->hCard));

	if (status)
		return status;

	return SCARD_S_SUCCESS;
}

void smartcard_trace_status_call(SMARTCARD_DEVICE* smartcard, Status_Call* call, BOOL unicode)
{
	BYTE* pb;

	if (!WLog_IsLevelActive(smartcard->log, WLOG_DEBUG))
		return;

	WLog_Print(smartcard->log, WLOG_DEBUG, "Status%s_Call {", unicode ? "W" : "A");

	pb = (BYTE*) &(call->hContext.pbContext);

	if (call->hContext.cbContext > 4)
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "hContext: 0x%02X%02X%02X%02X%02X%02X%02X%02X (%d)",
			pb[0], pb[1], pb[2], pb[3], pb[4], pb[5], pb[6], pb[7], call->hContext.cbContext);
	}
	else
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "hContext: 0x%02X%02X%02X%02X (%d)",
			pb[0], pb[1], pb[2], pb[3], call->hContext.cbContext);
	}

	pb = (BYTE*) &(call->hCard.pbHandle);

	if (call->hCard.cbHandle > 4)
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "hCard: 0x%02X%02X%02X%02X%02X%02X%02X%02X (%d)",
			pb[0], pb[1], pb[2], pb[3], pb[4], pb[5], pb[6], pb[7], call->hCard.cbHandle);
	}
	else
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "hCard: 0x%02X%02X%02X%02X (%d)",
			pb[0], pb[1], pb[2], pb[3], call->hCard.cbHandle);
	}

	WLog_Print(smartcard->log, WLOG_DEBUG, "fmszReaderNamesIsNULL: %d cchReaderLen: %d cbAtrLen: %d",
		call->fmszReaderNamesIsNULL, call->cchReaderLen, call->cbAtrLen);

	WLog_Print(smartcard->log, WLOG_DEBUG, "}");
}

UINT32 smartcard_pack_status_return(SMARTCARD_DEVICE* smartcard, wStream* s, Status_Return* ret)
{
	Stream_EnsureRemainingCapacity(s, ret->cBytes + 64);

	Stream_Write_UINT32(s, ret->cBytes); /* cBytes (4 bytes) */
	Stream_Write_UINT32(s, 0x00020010); /* mszReaderNamesNdrPtr (4 bytes) */
	Stream_Write_UINT32(s, ret->dwState); /* dwState (4 bytes) */
	Stream_Write_UINT32(s, ret->dwProtocol); /* dwProtocol (4 bytes) */
	Stream_Write(s, ret->pbAtr, 32); /* pbAtr (32 bytes) */
	Stream_Write_UINT32(s, ret->cbAtrLen); /* cbAtrLen (4 bytes) */

	Stream_Write_UINT32(s, ret->cBytes); /* mszReaderNamesNdrLen (4 bytes) */
	
	if (ret->mszReaderNames)
		Stream_Write(s, ret->mszReaderNames, ret->cBytes);
	else
		Stream_Zero(s, ret->cBytes);
	
	smartcard_pack_write_size_align(smartcard, s, ret->cBytes, 4);

	return SCARD_S_SUCCESS;
}

void smartcard_trace_status_return(SMARTCARD_DEVICE* smartcard, Status_Return* ret, BOOL unicode)
{
	int index;
	int length;
	char* pbAtr = NULL;
	char* mszReaderNamesA = NULL;

	if (!WLog_IsLevelActive(smartcard->log, WLOG_DEBUG))
		return;

	if (unicode)
	{
		length = (int) ret->cBytes / 2;
		ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) ret->mszReaderNames, length, &mszReaderNamesA, 0, NULL, NULL);
	}
	else
	{
		length = (int) ret->cBytes;
		mszReaderNamesA = (char*) malloc(length);
		CopyMemory(mszReaderNamesA, ret->mszReaderNames, ret->cBytes);
	}

	if (!mszReaderNamesA)
		length = 0;

	if (length > 2)
	{
		for (index = 0; index < length - 2; index++)
		{
			if (mszReaderNamesA[index] == '\0')
				mszReaderNamesA[index] = ',';
		}
	}

	pbAtr = winpr_BinToHexString(ret->pbAtr, ret->cbAtrLen, FALSE);

	WLog_Print(smartcard->log, WLOG_DEBUG, "Status%s_Return {", unicode ? "W" : "A");

	WLog_Print(smartcard->log, WLOG_DEBUG, "ReturnCode: %s (0x%08X)",
		SCardGetErrorString(ret->ReturnCode), ret->ReturnCode);

	WLog_Print(smartcard->log, WLOG_DEBUG, "dwState: %s (0x%08X) dwProtocol: %s (0x%08X)",
		SCardGetCardStateString(ret->dwState), ret->dwState,
		SCardGetProtocolString(ret->dwProtocol), ret->dwProtocol);

	if (mszReaderNamesA)
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "cBytes: %d mszReaderNames: %s",
			ret->cBytes, mszReaderNamesA);
	}

	WLog_Print(smartcard->log, WLOG_DEBUG,
		"cbAtrLen: %d pbAtr: %s", ret->cbAtrLen, pbAtr);

	WLog_Print(smartcard->log, WLOG_DEBUG, "}");

	free(mszReaderNamesA);
	free(pbAtr);
}

UINT32 smartcard_unpack_get_attrib_call(SMARTCARD_DEVICE* smartcard, wStream* s, GetAttrib_Call* call)
{
	UINT32 status;

	status = smartcard_unpack_redir_scard_context(smartcard, s, &(call->hContext));

	if (status)
		return status;

	status = smartcard_unpack_redir_scard_handle(smartcard, s, &(call->hCard));

	if (status)
		return status;

	if (Stream_GetRemainingLength(s) < 12)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "GetAttrib_Call is too short: %d",
				(int) Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Read_UINT32(s, call->dwAttrId); /* dwAttrId (4 bytes) */
	Stream_Read_UINT32(s, call->fpbAttrIsNULL); /* fpbAttrIsNULL (4 bytes) */
	Stream_Read_UINT32(s, call->cbAttrLen); /* cbAttrLen (4 bytes) */

	status = smartcard_unpack_redir_scard_context_ref(smartcard, s, &(call->hContext));

	if (status)
		return status;

	status = smartcard_unpack_redir_scard_handle_ref(smartcard, s, &(call->hCard));

	if (status)
		return status;

	return SCARD_S_SUCCESS;
}

void smartcard_trace_get_attrib_call(SMARTCARD_DEVICE* smartcard, GetAttrib_Call* call)
{
	BYTE* pb;

	if (!WLog_IsLevelActive(smartcard->log, WLOG_DEBUG))
		return;

	WLog_Print(smartcard->log, WLOG_DEBUG, "GetAttrib_Call {");

	pb = (BYTE*) &(call->hContext.pbContext);

	if (call->hContext.cbContext > 4)
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "hContext: 0x%02X%02X%02X%02X%02X%02X%02X%02X (%d)",
			pb[0], pb[1], pb[2], pb[3], pb[4], pb[5], pb[6], pb[7], call->hContext.cbContext);
	}
	else
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "hContext: 0x%02X%02X%02X%02X (%d)",
			pb[0], pb[1], pb[2], pb[3], call->hContext.cbContext);
	}

	pb = (BYTE*) &(call->hCard.pbHandle);

	if (call->hCard.cbHandle > 4)
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "hCard: 0x%02X%02X%02X%02X%02X%02X%02X%02X (%d)",
			pb[0], pb[1], pb[2], pb[3], pb[4], pb[5], pb[6], pb[7], call->hCard.cbHandle);
	}
	else
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "hCard: 0x%02X%02X%02X%02X (%d)",
			pb[0], pb[1], pb[2], pb[3], call->hCard.cbHandle);
	}

	WLog_Print(smartcard->log, WLOG_DEBUG, "dwAttrId: %s (0x%08X) fpbAttrIsNULL: %d cbAttrLen: 0x%08X",
		SCardGetAttributeString(call->dwAttrId), call->dwAttrId, call->fpbAttrIsNULL, call->cbAttrLen);

	WLog_Print(smartcard->log, WLOG_DEBUG, "}");
}

UINT32 smartcard_pack_get_attrib_return(SMARTCARD_DEVICE* smartcard, wStream* s, GetAttrib_Return* ret)
{
	Stream_EnsureRemainingCapacity(s, ret->cbAttrLen + 32);

	Stream_Write_UINT32(s, ret->cbAttrLen); /* cbAttrLen (4 bytes) */
	Stream_Write_UINT32(s, 0x00020080); /* pbAttrNdrPtr (4 bytes) */
	Stream_Write_UINT32(s, ret->cbAttrLen); /* pbAttrNdrCount (4 bytes) */

	if (!ret->pbAttr)
		Stream_Zero(s, ret->cbAttrLen); /* pbAttr */
	else
		Stream_Write(s, ret->pbAttr, ret->cbAttrLen); /* pbAttr */

	smartcard_pack_write_size_align(smartcard, s, ret->cbAttrLen, 4);

	return SCARD_S_SUCCESS;
}

void smartcard_trace_get_attrib_return(SMARTCARD_DEVICE* smartcard, GetAttrib_Return* ret, DWORD dwAttrId)
{
	if (!WLog_IsLevelActive(smartcard->log, WLOG_DEBUG))
		return;

	WLog_Print(smartcard->log, WLOG_DEBUG, "GetAttrib_Return {");

	WLog_Print(smartcard->log, WLOG_DEBUG, "ReturnCode: %s (0x%08X)",
		SCardGetErrorString(ret->ReturnCode), ret->ReturnCode);

	WLog_Print(smartcard->log, WLOG_DEBUG, "dwAttrId: %s (0x%08X) cbAttrLen: 0x%08X",
		SCardGetAttributeString(dwAttrId), dwAttrId, ret->cbAttrLen);

	if (dwAttrId == SCARD_ATTR_VENDOR_NAME)
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "pbAttr: %.*s", ret->cbAttrLen, (char*) ret->pbAttr);
	}

	WLog_Print(smartcard->log, WLOG_DEBUG, "}");
}

UINT32 smartcard_unpack_control_call(SMARTCARD_DEVICE* smartcard, wStream* s, Control_Call* call)
{
	UINT32 status;
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
		WLog_Print(smartcard->log, WLOG_WARN, "Control_Call is too short: %d",
				(int) Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Read_UINT32(s, call->dwControlCode); /* dwControlCode (4 bytes) */
	Stream_Read_UINT32(s, call->cbInBufferSize); /* cbInBufferSize (4 bytes) */
	Stream_Seek_UINT32(s); /* pvInBufferNdrPtr (4 bytes) */
	Stream_Read_UINT32(s, call->fpvOutBufferIsNULL); /* fpvOutBufferIsNULL (4 bytes) */
	Stream_Read_UINT32(s, call->cbOutBufferSize); /* cbOutBufferSize (4 bytes) */

	status = smartcard_unpack_redir_scard_context_ref(smartcard, s, &(call->hContext));

	if (status)
		return status;

	status = smartcard_unpack_redir_scard_handle_ref(smartcard, s, &(call->hCard));

	if (status)
		return status;

	if (call->cbInBufferSize)
	{
		if (Stream_GetRemainingLength(s) < 4)
		{
			WLog_Print(smartcard->log, WLOG_WARN, "Control_Call is too short: %d",
					(int) Stream_GetRemainingLength(s));
			return STATUS_BUFFER_TOO_SMALL;
		}

		Stream_Read_UINT32(s, length); /* Length (4 bytes) */

		if (Stream_GetRemainingLength(s) < length)
		{
			WLog_Print(smartcard->log, WLOG_WARN, "Control_Call is too short: %d",
					(int) Stream_GetRemainingLength(s));
			return STATUS_BUFFER_TOO_SMALL;
		}

		call->pvInBuffer = (BYTE*) malloc(length);

		if (!call->pvInBuffer)
		{
			WLog_Print(smartcard->log, WLOG_WARN, "Control_Call out of memory error (call->pvInBuffer)");
			return STATUS_NO_MEMORY;
		}

		call->cbInBufferSize = length;

		Stream_Read(s, call->pvInBuffer, length);
	}

	return SCARD_S_SUCCESS;
}

void smartcard_trace_control_call(SMARTCARD_DEVICE* smartcard, Control_Call* call)
{
	BYTE* pb;

	if (!WLog_IsLevelActive(smartcard->log, WLOG_DEBUG))
		return;

	WLog_Print(smartcard->log, WLOG_DEBUG, "Control_Call {");

	pb = (BYTE*) &(call->hContext.pbContext);

	if (call->hContext.cbContext > 4)
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "hContext: 0x%02X%02X%02X%02X%02X%02X%02X%02X (%d)",
			pb[0], pb[1], pb[2], pb[3], pb[4], pb[5], pb[6], pb[7], call->hContext.cbContext);
	}
	else
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "hContext: 0x%02X%02X%02X%02X (%d)",
			pb[0], pb[1], pb[2], pb[3], call->hContext.cbContext);
	}

	pb = (BYTE*) &(call->hCard.pbHandle);

	if (call->hCard.cbHandle > 4)
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "hCard: 0x%02X%02X%02X%02X%02X%02X%02X%02X (%d)",
			pb[0], pb[1], pb[2], pb[3], pb[4], pb[5], pb[6], pb[7], call->hCard.cbHandle);
	}
	else
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "hCard: 0x%02X%02X%02X%02X (%d)",
			pb[0], pb[1], pb[2], pb[3], call->hCard.cbHandle);
	}

	WLog_Print(smartcard->log, WLOG_DEBUG,
			"dwControlCode: 0x%08X cbInBufferSize: %d fpvOutBufferIsNULL: %d cbOutBufferSize: %d",
			call->dwControlCode, call->cbInBufferSize, call->fpvOutBufferIsNULL, call->cbOutBufferSize);

	if (call->pvInBuffer)
	{
		char* szInBuffer = winpr_BinToHexString(call->pvInBuffer, call->cbInBufferSize, TRUE);
		WLog_Print(smartcard->log, WLOG_DEBUG, "pbInBuffer: %s", szInBuffer);
		free(szInBuffer);
	}
	else
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "pvInBuffer: null");
	}

	WLog_Print(smartcard->log, WLOG_DEBUG, "}");
}

UINT32 smartcard_pack_control_return(SMARTCARD_DEVICE* smartcard, wStream* s, Control_Return* ret)
{
	Stream_EnsureRemainingCapacity(s, ret->cbOutBufferSize + 32);

	Stream_Write_UINT32(s, ret->cbOutBufferSize); /* cbOutBufferSize (4 bytes) */
	Stream_Write_UINT32(s, 0x00020040); /* pvOutBufferPointer (4 bytes) */
	Stream_Write_UINT32(s, ret->cbOutBufferSize); /* pvOutBufferLength (4 bytes) */

	if (ret->cbOutBufferSize > 0)
	{
		Stream_Write(s, ret->pvOutBuffer, ret->cbOutBufferSize); /* pvOutBuffer */
		smartcard_pack_write_size_align(smartcard, s, ret->cbOutBufferSize, 4);
	}

	return SCARD_S_SUCCESS;
}

void smartcard_trace_control_return(SMARTCARD_DEVICE* smartcard, Control_Return* ret)
{
	if (!WLog_IsLevelActive(smartcard->log, WLOG_DEBUG))
		return;

	WLog_Print(smartcard->log, WLOG_DEBUG, "Control_Return {");

	WLog_Print(smartcard->log, WLOG_DEBUG, "ReturnCode: %s (0x%08X)",
		SCardGetErrorString(ret->ReturnCode), ret->ReturnCode);

	WLog_Print(smartcard->log, WLOG_DEBUG, "cbOutBufferSize: %d", ret->cbOutBufferSize);

	if (ret->pvOutBuffer)
	{
		char* szOutBuffer = winpr_BinToHexString(ret->pvOutBuffer, ret->cbOutBufferSize, TRUE);
		WLog_Print(smartcard->log, WLOG_DEBUG, "pvOutBuffer: %s", szOutBuffer);
		free(szOutBuffer);
	}
	else
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "pvOutBuffer: null");
	}

	WLog_Print(smartcard->log, WLOG_DEBUG, "}");
}

UINT32 smartcard_unpack_transmit_call(SMARTCARD_DEVICE* smartcard, wStream* s, Transmit_Call* call)
{
	UINT32 status;
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
		WLog_Print(smartcard->log, WLOG_WARN, "Transmit_Call is too short: Actual: %d, Expected: %d",
				(int) Stream_GetRemainingLength(s), 32);
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Read_UINT32(s, ioSendPci.dwProtocol); /* dwProtocol (4 bytes) */
	Stream_Read_UINT32(s, ioSendPci.cbExtraBytes); /* cbExtraBytes (4 bytes) */
	Stream_Read_UINT32(s, pbExtraBytesNdrPtr); /* pbExtraBytesNdrPtr (4 bytes) */
	Stream_Read_UINT32(s, call->cbSendLength); /* cbSendLength (4 bytes) */
	Stream_Read_UINT32(s, pbSendBufferNdrPtr); /* pbSendBufferNdrPtr (4 bytes) */
	Stream_Read_UINT32(s, pioRecvPciNdrPtr); /* pioRecvPciNdrPtr (4 bytes) */
	Stream_Read_UINT32(s, call->fpbRecvBufferIsNULL); /* fpbRecvBufferIsNULL (4 bytes) */
	Stream_Read_UINT32(s, call->cbRecvLength); /* cbRecvLength (4 bytes) */

	if (ioSendPci.cbExtraBytes > 1024)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "Transmit_Call ioSendPci.cbExtraBytes is out of bounds: %d (max: %d)",
				(int) ioSendPci.cbExtraBytes, 1024);
		return STATUS_INVALID_PARAMETER;
	}

	if (call->cbSendLength > 66560)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "Transmit_Call cbSendLength is out of bounds: %d (max: %d)",
				(int) ioSendPci.cbExtraBytes, 66560);
		return STATUS_INVALID_PARAMETER;
	}

	status = smartcard_unpack_redir_scard_context_ref(smartcard, s, &(call->hContext));

	if (status)
		return status;

	status = smartcard_unpack_redir_scard_handle_ref(smartcard, s, &(call->hCard));

	if (status)
		return status;

	if (ioSendPci.cbExtraBytes && !pbExtraBytesNdrPtr)
	{
		WLog_Print(smartcard->log, WLOG_WARN,
				"Transmit_Call cbExtraBytes is non-zero but pbExtraBytesNdrPtr is null");
		return STATUS_INVALID_PARAMETER;
	}

	if (pbExtraBytesNdrPtr)
	{
		if (Stream_GetRemainingLength(s) < 4)
		{
			WLog_Print(smartcard->log, WLOG_WARN, "Transmit_Call is too short: %d",
					(int) Stream_GetRemainingLength(s));
			return STATUS_BUFFER_TOO_SMALL;
		}

		Stream_Read_UINT32(s, length); /* Length (4 bytes) */

		if (Stream_GetRemainingLength(s) < ioSendPci.cbExtraBytes)
		{
			WLog_Print(smartcard->log, WLOG_WARN,
					"Transmit_Call is too short: Actual: %d, Expected: %d (ioSendPci.cbExtraBytes)",
					(int) Stream_GetRemainingLength(s), (int) ioSendPci.cbExtraBytes);
			return STATUS_BUFFER_TOO_SMALL;
		}

		ioSendPci.pbExtraBytes = (BYTE*) Stream_Pointer(s);

		call->pioSendPci = (LPSCARD_IO_REQUEST) malloc(sizeof(SCARD_IO_REQUEST) + ioSendPci.cbExtraBytes);

		if (!call->pioSendPci)
		{
			WLog_Print(smartcard->log, WLOG_WARN, "Transmit_Call out of memory error (pioSendPci)");
			return STATUS_NO_MEMORY;
		}

		call->pioSendPci->dwProtocol = ioSendPci.dwProtocol;
		call->pioSendPci->cbPciLength = ioSendPci.cbExtraBytes + sizeof(SCARD_IO_REQUEST);

		pbExtraBytes = &((BYTE*) call->pioSendPci)[sizeof(SCARD_IO_REQUEST)];
		Stream_Read(s, pbExtraBytes, ioSendPci.cbExtraBytes);
	}
	else
	{
		call->pioSendPci = (LPSCARD_IO_REQUEST) calloc(1, sizeof(SCARD_IO_REQUEST));

		if (!call->pioSendPci)
		{
			WLog_Print(smartcard->log, WLOG_WARN, "Transmit_Call out of memory error (pioSendPci)");
			return STATUS_NO_MEMORY;
		}

		call->pioSendPci->dwProtocol = ioSendPci.dwProtocol;
		call->pioSendPci->cbPciLength = sizeof(SCARD_IO_REQUEST);
	}

	if (pbSendBufferNdrPtr)
	{
		if (Stream_GetRemainingLength(s) < 4)
		{
			WLog_Print(smartcard->log, WLOG_WARN, "Transmit_Call is too short: %d",
					(int) Stream_GetRemainingLength(s));
			return STATUS_BUFFER_TOO_SMALL;
		}

		Stream_Read_UINT32(s, length); /* Length (4 bytes) */

		if (length != call->cbSendLength)
		{
			WLog_Print(smartcard->log, WLOG_WARN,
					"Transmit_Call unexpected length: Actual: %d, Expected: %d (cbSendLength)",
					(int) length, (int) call->cbSendLength);
			return STATUS_INVALID_PARAMETER;
		}

		if (Stream_GetRemainingLength(s) < call->cbSendLength)
		{
			WLog_Print(smartcard->log, WLOG_WARN,
					"Transmit_Call is too short: Actual: %d, Expected: %d (cbSendLength)",
					(int) Stream_GetRemainingLength(s), (int) call->cbSendLength);
			return STATUS_BUFFER_TOO_SMALL;
		}

		call->pbSendBuffer = (BYTE*) malloc(call->cbSendLength);

		if (!call->pbSendBuffer)
		{
			WLog_Print(smartcard->log, WLOG_WARN, "Transmit_Call out of memory error (pbSendBuffer)");
			return STATUS_NO_MEMORY;
		}

		Stream_Read(s, call->pbSendBuffer, call->cbSendLength);
	}

	if (pioRecvPciNdrPtr)
	{
		if (Stream_GetRemainingLength(s) < 8)
		{
			WLog_Print(smartcard->log, WLOG_WARN, "Transmit_Call is too short: Actual: %d, Expected: %d",
					(int) Stream_GetRemainingLength(s), 16);
			return STATUS_BUFFER_TOO_SMALL;
		}

		Stream_Read_UINT32(s, length); /* Length (4 bytes) */

		Stream_Read_UINT32(s, ioRecvPci.dwProtocol); /* dwProtocol (4 bytes) */
		Stream_Read_UINT32(s, ioRecvPci.cbExtraBytes); /* cbExtraBytes (4 bytes) */

		if (ioRecvPci.cbExtraBytes > 1024)
		{
			WLog_Print(smartcard->log, WLOG_WARN, "Transmit_Call ioRecvPci.cbExtraBytes is out of bounds: %d (max: %d)",
					(int) ioSendPci.cbExtraBytes, 1024);
			return STATUS_INVALID_PARAMETER;
		}

		if (length < ioRecvPci.cbExtraBytes)
		{
			WLog_Print(smartcard->log, WLOG_WARN,
					"Transmit_Call unexpected length: Actual: %d, Expected: %d (ioRecvPci.cbExtraBytes)",
					(int) length, (int) ioRecvPci.cbExtraBytes);
			return STATUS_INVALID_PARAMETER;
		}

		if (Stream_GetRemainingLength(s) < ioRecvPci.cbExtraBytes)
		{
			WLog_Print(smartcard->log, WLOG_WARN,
					"Transmit_Call is too short: Actual: %d, Expected: %d (ioRecvPci.cbExtraBytes)",
					(int) Stream_GetRemainingLength(s), (int) ioRecvPci.cbExtraBytes);
			return STATUS_BUFFER_TOO_SMALL;
		}

		ioRecvPci.pbExtraBytes = (BYTE*) Stream_Pointer(s);

		call->pioRecvPci = (LPSCARD_IO_REQUEST) malloc(sizeof(SCARD_IO_REQUEST) + ioRecvPci.cbExtraBytes);

		if (!call->pioRecvPci)
		{
			WLog_Print(smartcard->log, WLOG_WARN, "Transmit_Call out of memory error (pioRecvPci)");
			return STATUS_NO_MEMORY;
		}

		call->pioRecvPci->dwProtocol = ioRecvPci.dwProtocol;
		call->pioRecvPci->cbPciLength = ioRecvPci.cbExtraBytes + sizeof(SCARD_IO_REQUEST);

		pbExtraBytes = &((BYTE*) call->pioRecvPci)[sizeof(SCARD_IO_REQUEST)];
		Stream_Read(s, pbExtraBytes, ioRecvPci.cbExtraBytes);
	}

	return SCARD_S_SUCCESS;
}

void smartcard_trace_transmit_call(SMARTCARD_DEVICE* smartcard, Transmit_Call* call)
{
	BYTE* pb;
	UINT32 cbExtraBytes;
	BYTE* pbExtraBytes;

	if (!WLog_IsLevelActive(smartcard->log, WLOG_DEBUG))
		return;

	WLog_Print(smartcard->log, WLOG_DEBUG, "Transmit_Call {");

	pb = (BYTE*) &(call->hContext.pbContext);

	if (call->hContext.cbContext > 4)
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "hContext: 0x%02X%02X%02X%02X%02X%02X%02X%02X (%d)",
			pb[0], pb[1], pb[2], pb[3], pb[4], pb[5], pb[6], pb[7], call->hContext.cbContext);
	}
	else
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "hContext: 0x%02X%02X%02X%02X (%d)",
			pb[0], pb[1], pb[2], pb[3], call->hContext.cbContext);
	}

	pb = (BYTE*) &(call->hCard.pbHandle);

	if (call->hCard.cbHandle > 4)
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "hCard: 0x%02X%02X%02X%02X%02X%02X%02X%02X (%d)",
			pb[0], pb[1], pb[2], pb[3], pb[4], pb[5], pb[6], pb[7], call->hCard.cbHandle);
	}
	else
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "hCard: 0x%02X%02X%02X%02X (%d)",
			pb[0], pb[1], pb[2], pb[3], call->hCard.cbHandle);
	}

	if (call->pioSendPci)
	{
		cbExtraBytes = call->pioSendPci->cbPciLength - sizeof(SCARD_IO_REQUEST);
		pbExtraBytes = &((BYTE*) call->pioSendPci)[sizeof(SCARD_IO_REQUEST)];

		WLog_Print(smartcard->log, WLOG_DEBUG, "pioSendPci: dwProtocol: %d cbExtraBytes: %d",
				call->pioSendPci->dwProtocol, cbExtraBytes);

		if (cbExtraBytes)
		{
			char* szExtraBytes = winpr_BinToHexString(pbExtraBytes, cbExtraBytes, TRUE);
			WLog_Print(smartcard->log, WLOG_DEBUG, "pbExtraBytes: %s", szExtraBytes);
			free(szExtraBytes);
		}
	}
	else
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "pioSendPci: null");
	}

	WLog_Print(smartcard->log, WLOG_DEBUG, "cbSendLength: %d", call->cbSendLength);

	if (call->pbSendBuffer)
	{
		char* szSendBuffer = winpr_BinToHexString(call->pbSendBuffer, call->cbSendLength, TRUE);
		WLog_Print(smartcard->log, WLOG_DEBUG, "pbSendBuffer: %s", szSendBuffer);
		free(szSendBuffer);
	}
	else
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "pbSendBuffer: null");
	}

	if (call->pioRecvPci)
	{
		cbExtraBytes = call->pioRecvPci->cbPciLength - sizeof(SCARD_IO_REQUEST);
		pbExtraBytes = &((BYTE*) call->pioRecvPci)[sizeof(SCARD_IO_REQUEST)];

		WLog_Print(smartcard->log, WLOG_DEBUG, "pioRecvPci: dwProtocol: %d cbExtraBytes: %d",
				call->pioRecvPci->dwProtocol, cbExtraBytes);

		if (cbExtraBytes)
		{
			char* szExtraBytes = winpr_BinToHexString(pbExtraBytes, cbExtraBytes, TRUE);
			WLog_Print(smartcard->log, WLOG_DEBUG, "pbExtraBytes: %s", szExtraBytes);
			free(szExtraBytes);
		}
	}
	else
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "pioRecvPci: null");
	}

	WLog_Print(smartcard->log, WLOG_DEBUG, "fpbRecvBufferIsNULL: %d cbRecvLength: 0x%08X",
			call->fpbRecvBufferIsNULL, call->cbRecvLength);

	WLog_Print(smartcard->log, WLOG_DEBUG, "}");
}

UINT32 smartcard_pack_transmit_return(SMARTCARD_DEVICE* smartcard, wStream* s, Transmit_Return* ret)
{
	UINT32 cbExtraBytes;
	BYTE* pbExtraBytes;
	UINT32 pioRecvPciNdrPtr;
	UINT32 pbRecvBufferNdrPtr;

	if (!ret->pbRecvBuffer)
		ret->cbRecvLength = 0;

	pioRecvPciNdrPtr = (ret->pioRecvPci) ? 0x00020200 : 0;
	pbRecvBufferNdrPtr = (ret->pbRecvBuffer) ? 0x00020400 : 0;

	Stream_Write_UINT32(s, pioRecvPciNdrPtr); /* pioRecvPciNdrPtr (4 bytes) */
	Stream_Write_UINT32(s, ret->cbRecvLength); /* cbRecvLength (4 bytes) */
	Stream_Write_UINT32(s, pbRecvBufferNdrPtr); /* pbRecvBufferNdrPtr (4 bytes) */

	if (pioRecvPciNdrPtr)
	{
		cbExtraBytes = ret->pioRecvPci->cbPciLength - sizeof(SCARD_IO_REQUEST);
		pbExtraBytes = &((BYTE*) ret->pioRecvPci)[sizeof(SCARD_IO_REQUEST)];

		Stream_EnsureRemainingCapacity(s, cbExtraBytes + 16);
		Stream_Write_UINT32(s, cbExtraBytes); /* Length (4 bytes) */
		Stream_Write_UINT32(s, ret->pioRecvPci->dwProtocol); /* dwProtocol (4 bytes) */
		Stream_Write_UINT32(s, cbExtraBytes); /* cbExtraBytes (4 bytes) */
		Stream_Write(s, pbExtraBytes, cbExtraBytes);
		smartcard_pack_write_size_align(smartcard, s, cbExtraBytes, 4);
	}

	if (pbRecvBufferNdrPtr)
	{
		Stream_EnsureRemainingCapacity(s, ret->cbRecvLength + 16);
		Stream_Write_UINT32(s, ret->cbRecvLength); /* pbRecvBufferNdrLen (4 bytes) */
		Stream_Write(s, ret->pbRecvBuffer, ret->cbRecvLength);
		smartcard_pack_write_size_align(smartcard, s, ret->cbRecvLength, 4);
	}

	return SCARD_S_SUCCESS;
}

void smartcard_trace_transmit_return(SMARTCARD_DEVICE* smartcard, Transmit_Return* ret)
{
	UINT32 cbExtraBytes;
	BYTE* pbExtraBytes;

	if (!WLog_IsLevelActive(smartcard->log, WLOG_DEBUG))
		return;

	WLog_Print(smartcard->log, WLOG_DEBUG, "Transmit_Return {");

	WLog_Print(smartcard->log, WLOG_DEBUG, "ReturnCode: %s (0x%08X)",
		SCardGetErrorString(ret->ReturnCode), ret->ReturnCode);

	if (ret->pioRecvPci)
	{
		cbExtraBytes = ret->pioRecvPci->cbPciLength - sizeof(SCARD_IO_REQUEST);
		pbExtraBytes = &((BYTE*) ret->pioRecvPci)[sizeof(SCARD_IO_REQUEST)];

		WLog_Print(smartcard->log, WLOG_DEBUG, "pioRecvPci: dwProtocol: %d cbExtraBytes: %d",
				ret->pioRecvPci->dwProtocol, cbExtraBytes);

		if (cbExtraBytes)
		{
			char* szExtraBytes = winpr_BinToHexString(pbExtraBytes, cbExtraBytes, TRUE);
			WLog_Print(smartcard->log, WLOG_DEBUG, "pbExtraBytes: %s", szExtraBytes);
			free(szExtraBytes);
		}
	}
	else
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "pioRecvPci: null");
	}

	WLog_Print(smartcard->log, WLOG_DEBUG, "cbRecvLength: %d", ret->cbRecvLength);

	if (ret->pbRecvBuffer)
	{
		char* szRecvBuffer = winpr_BinToHexString(ret->pbRecvBuffer, ret->cbRecvLength, TRUE);
		WLog_Print(smartcard->log, WLOG_DEBUG, "pbRecvBuffer: %s", szRecvBuffer);
		free(szRecvBuffer);
	}
	else
	{
		WLog_Print(smartcard->log, WLOG_DEBUG, "pbRecvBuffer: null");
	}

	WLog_Print(smartcard->log, WLOG_DEBUG, "}");
}
