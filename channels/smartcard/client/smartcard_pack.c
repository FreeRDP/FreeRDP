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

static char SMARTCARD_PNP_NOTIFICATION_A[] = "\\\\?PnP?\\Notification";

static WCHAR SMARTCARD_PNP_NOTIFICATION_W[] = { '\\','\\','?','P','n','P','?',
		'\\','N','o','t','i','f','i','c','a','t','i','o','n','\0' };

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

UINT32 smartcard_unpack_read_offset_align(SMARTCARD_DEVICE* smartcard, wStream* s, UINT32 alignment)
{
	UINT32 pad;
	UINT32 offset;

	offset = Stream_GetPosition(s);

	pad = offset;
	offset = (offset + alignment - 1) & ~(alignment - 1);
	pad = offset - pad;

	if (pad)
		Stream_Seek(s, pad);

	return pad;
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

UINT32 smartcard_pack_write_offset_align(SMARTCARD_DEVICE* smartcard, wStream* s, UINT32 alignment)
{
	UINT32 pad;
	UINT32 offset;

	offset = Stream_GetPosition(s);

	pad = offset;
	offset = (offset + alignment - 1) & ~(alignment - 1);
	pad = offset - pad;

	if (pad)
		Stream_Zero(s, pad);

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

UINT32 smartcard_unpack_redir_scard_context(SMARTCARD_DEVICE* smartcard, wStream* s, REDIR_SCARDCONTEXT* context)
{
	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "REDIR_SCARDCONTEXT is too short: %d",
				(int) Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Read_UINT32(s, context->cbContext); /* cbContext (4 bytes) */

	if ((Stream_GetRemainingLength(s) < context->cbContext) || (!context->cbContext))
	{
		WLog_Print(smartcard->log, WLOG_WARN, "REDIR_SCARDCONTEXT is too short: Actual: %d, Expected: %d",
				(int) Stream_GetRemainingLength(s), context->cbContext);
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Seek_UINT32(s); /* pbContextNdrPtr (4 bytes) */

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

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "REDIR_SCARDCONTEXT is too short: Actual: %d, Expected: %d\n",
				(int) Stream_GetRemainingLength(s), 4);
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Read_UINT32(s, length); /* Length (4 bytes) */

	if ((length != 4) && (length != 8))
	{
		WLog_Print(smartcard->log, WLOG_WARN, "REDIR_SCARDCONTEXT length is not 4 or 8: %d\n", length);
		return STATUS_INVALID_PARAMETER;
	}

	if ((Stream_GetRemainingLength(s) < length) || (!length))
	{
		WLog_Print(smartcard->log, WLOG_WARN, "REDIR_SCARDCONTEXT is too short: Actual: %d, Expected: %d\n",
				(int) Stream_GetRemainingLength(s), length);
		return STATUS_BUFFER_TOO_SMALL;
	}

	if (length > 4)
		Stream_Read_UINT64(s, context->pbContext);
	else
		Stream_Read_UINT32(s, context->pbContext);

	return SCARD_S_SUCCESS;
}

UINT32 smartcard_pack_redir_scard_context_ref(SMARTCARD_DEVICE* smartcard, wStream* s, REDIR_SCARDCONTEXT* context)
{
	if (context->cbContext)
	{
		Stream_Write_UINT32(s, context->cbContext); /* Length (4 bytes) */

		if (context->cbContext > 4)
			Stream_Write_UINT64(s, context->pbContext);
		else if (context->cbContext > 0)
			Stream_Write_UINT32(s, context->pbContext);
	}

	return SCARD_S_SUCCESS;
}

UINT32 smartcard_unpack_redir_scard_handle(SMARTCARD_DEVICE* smartcard, wStream* s, REDIR_SCARDHANDLE* handle)
{
	UINT32 status;
	UINT32 length;

	status = smartcard_unpack_redir_scard_context(smartcard, s, &(handle->Context));

	if (status)
		return status;

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "SCARDHANDLE is too short: %d",
				(int) Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Read_UINT32(s, length); /* Length (4 bytes) */

	if ((Stream_GetRemainingLength(s) < length) || (!length))
	{
		WLog_Print(smartcard->log, WLOG_WARN, "SCARDHANDLE is too short: Actual: %d, Expected: %d",
				(int) Stream_GetRemainingLength(s), length);
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Seek_UINT32(s); /* NdrPtr (4 bytes) */

	return 0;
}

UINT32 smartcard_pack_redir_scard_handle(SMARTCARD_DEVICE* smartcard, wStream* s, REDIR_SCARDHANDLE* handle)
{
	UINT32 status;
	UINT32 pbHandleNdrPtr;

	status = smartcard_pack_redir_scard_context(smartcard, s, &(handle->Context));

	if (status)
		return status;

	pbHandleNdrPtr = (handle->cbHandle) ? 0x00020002 : 0;

	Stream_Write_UINT32(s, handle->cbHandle); /* cbHandle (4 bytes) */
	Stream_Write_UINT32(s, pbHandleNdrPtr); /* pbHandleNdrPtr (4 bytes) */

	return SCARD_S_SUCCESS;
}

UINT32 smartcard_unpack_redir_scard_handle_ref(SMARTCARD_DEVICE* smartcard, wStream* s, REDIR_SCARDHANDLE* handle)
{
	UINT32 length;
	UINT32 status;

	status = smartcard_unpack_redir_scard_context_ref(smartcard, s, &(handle->Context));

	if (status)
		return status;

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "REDIR_SCARDHANDLE is too short: Actual: %d, Expected: %d\n",
				(int) Stream_GetRemainingLength(s), 4);
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Read_UINT32(s, length); /* Length (4 bytes) */

	if ((length != 4) && (length != 8))
	{
		WLog_Print(smartcard->log, WLOG_WARN, "REDIR_SCARDHANDLE length is not 4 or 8: %d\n", length);
		return STATUS_INVALID_PARAMETER;
	}

	if ((Stream_GetRemainingLength(s) < length) || (!length))
	{
		WLog_Print(smartcard->log, WLOG_WARN, "REDIR_SCARDHANDLE is too short: Actual: %d, Expected: %d\n",
				(int) Stream_GetRemainingLength(s), length);
		return STATUS_BUFFER_TOO_SMALL;
	}

	if (length > 4)
		Stream_Read_UINT64(s, handle->pbHandle);
	else
		Stream_Read_UINT32(s, handle->pbHandle);

	return 0;
}

UINT32 smartcard_pack_redir_scard_handle_ref(SMARTCARD_DEVICE* smartcard, wStream* s, REDIR_SCARDHANDLE* handle)
{
	UINT32 status;

	status = smartcard_pack_redir_scard_context_ref(smartcard, s, &(handle->Context));

	if (status)
		return status;

	if (handle->cbHandle)
	{
		Stream_Write_UINT32(s, handle->cbHandle); /* Length (4 bytes) */

		if (handle->cbHandle > 4)
			Stream_Write_UINT64(s, handle->pbHandle);
		else
			Stream_Write_UINT32(s, handle->pbHandle);
	}

	return 0;
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

UINT32 smartcard_pack_establish_context_return(SMARTCARD_DEVICE* smartcard, wStream* s, EstablishContext_Return* ret)
{
	UINT32 status;

	status = smartcard_pack_redir_scard_context(smartcard, s, &(ret->Context));

	if (status)
		return status;

	status = smartcard_pack_redir_scard_context_ref(smartcard, s, &(ret->Context));

	if (status)
		return status;

	return SCARD_S_SUCCESS;
}

UINT32 smartcard_unpack_context_call(SMARTCARD_DEVICE* smartcard, wStream* s, Context_Call* call)
{
	UINT32 status;

	status = smartcard_unpack_redir_scard_context(smartcard, s, &(call->Context));

	if (status)
		return status;

	status = smartcard_unpack_redir_scard_context_ref(smartcard, s, &(call->Context));

	if (status)
		return status;

	return SCARD_S_SUCCESS;
}

UINT32 smartcard_unpack_list_readers_call(SMARTCARD_DEVICE* smartcard, wStream* s, ListReaders_Call* call)
{
	UINT32 status;
	UINT32 mszGroupsNdrPtr;

	call->mszGroups = NULL;

	status = smartcard_unpack_redir_scard_context(smartcard, s, &(call->Context));

	if (status)
		return status;

	if (Stream_GetRemainingLength(s) < 16)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "ListReaders_Call is too short: %d",
				(int) Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Read_UINT32(s, call->cBytes); /* cBytes (4 bytes) */

	if (Stream_GetRemainingLength(s) < call->cBytes)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "ListReaders_Call is too short: Actual: %d, Expected: %d",
				(int) Stream_GetRemainingLength(s), call->cBytes);
		return STATUS_BUFFER_TOO_SMALL;
	}

	Stream_Read_UINT32(s, mszGroupsNdrPtr); /* mszGroupsNdrPtr (4 bytes) */

	Stream_Read_UINT32(s, call->fmszReadersIsNULL); /* fmszReadersIsNULL (4 bytes) */
	Stream_Read_UINT32(s, call->cchReaders); /* cchReaders (4 bytes) */

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "ListReaders_Call is too short: %d",
				(int) Stream_GetRemainingLength(s));
		return STATUS_BUFFER_TOO_SMALL;
	}

	if (mszGroupsNdrPtr)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "ListReaders_Call unimplemented mszGroups parsing");
	}

	status = smartcard_unpack_redir_scard_context_ref(smartcard, s, &(call->Context));

	if (status)
		return status;

	return SCARD_S_SUCCESS;
}

UINT32 smartcard_pack_list_readers_return(SMARTCARD_DEVICE* smartcard, wStream* s, ListReaders_Return* ret)
{
	Stream_Write_UINT32(s, ret->cBytes); /* cBytes (4 bytes) */
	Stream_Write_UINT32(s, 0x00020008); /* mszNdrPtr (4 bytes) */
	Stream_Write_UINT32(s, ret->cBytes); /* mszNdrLen (4 bytes) */
	
	if (ret->msz)
		Stream_Write(s, ret->msz, ret->cBytes);
	else
		Stream_Zero(s, ret->cBytes);
	
	smartcard_pack_write_size_align(smartcard, s, ret->cBytes, 4);

	return SCARD_S_SUCCESS;
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

	status = smartcard_unpack_redir_scard_context(smartcard, s, &(common->Context));

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

	call->szReader = malloc(count + 1);
	Stream_Read(s, call->szReader, count);
	smartcard_unpack_read_size_align(smartcard, s, count, 4);
	call->szReader[count] = '\0';

	smartcard_unpack_redir_scard_context_ref(smartcard, s, &(call->Common.Context));

	return SCARD_S_SUCCESS;
}

UINT32 smartcard_unpack_connect_w_call(SMARTCARD_DEVICE* smartcard, wStream* s, ConnectW_Call* call)
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

	call->szReader = malloc((count + 1) * 2);
	Stream_Read(s, call->szReader, (count * 2));
	smartcard_unpack_read_size_align(smartcard, s, (count * 2), 4);
	call->szReader[count] = '\0';

	smartcard_unpack_redir_scard_context_ref(smartcard, s, &(call->Common.Context));

	return SCARD_S_SUCCESS;
}

UINT32 smartcard_pack_connect_return(SMARTCARD_DEVICE* smartcard, wStream* s, Connect_Return* ret)
{
	UINT32 status;

	status = smartcard_pack_redir_scard_handle(smartcard, s, &(ret->hCard));

	if (status)
		return status;

	Stream_Write_UINT32(s, ret->dwActiveProtocol); /* dwActiveProtocol (4 bytes) */

	status = smartcard_pack_redir_scard_handle_ref(smartcard, s, &(ret->hCard));

	if (status)
		return status;

	return SCARD_S_SUCCESS;
}

UINT32 smartcard_unpack_reconnect_call(SMARTCARD_DEVICE* smartcard, wStream* s, Reconnect_Call* call)
{
	UINT32 status;

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

	status = smartcard_unpack_redir_scard_handle_ref(smartcard, s, &(call->hCard));

	if (status)
		return status;

	return SCARD_S_SUCCESS;
}

UINT32 smartcard_pack_reconnect_return(SMARTCARD_DEVICE* smartcard, wStream* s, Reconnect_Return* ret)
{
	Stream_Write_UINT32(s, ret->dwActiveProtocol); /* dwActiveProtocol (4 bytes) */

	return SCARD_S_SUCCESS;
}

UINT32 smartcard_unpack_hcard_and_disposition_call(SMARTCARD_DEVICE* smartcard, wStream* s, HCardAndDisposition_Call* call)
{
	UINT32 status;

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

	status = smartcard_unpack_redir_scard_handle_ref(smartcard, s, &(call->hCard));

	if (status)
		return status;

	return SCARD_S_SUCCESS;
}

UINT32 smartcard_unpack_get_status_change_a_call(SMARTCARD_DEVICE* smartcard, wStream* s, GetStatusChangeA_Call* call)
{
	int index;
	UINT32 count;
	UINT32 status;
	UINT32 szReaderNdrPtr;
	ReaderStateA* readerState;
	UINT32 rgReaderStatesNdrPtr;

	call->rgReaderStates = NULL;

	status = smartcard_unpack_redir_scard_context(smartcard, s, &(call->Context));

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

	status = smartcard_unpack_redir_scard_context_ref(smartcard, s, &(call->Context));

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
		call->rgReaderStates = (ReaderStateA*) calloc(call->cReaders, sizeof(ReaderStateA));

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
			Stream_Read_UINT32(s, readerState->Common.dwCurrentState); /* dwCurrentState (4 bytes) */
			Stream_Read_UINT32(s, readerState->Common.dwEventState); /* dwEventState (4 bytes) */
			Stream_Read_UINT32(s, readerState->Common.cbAtr); /* cbAtr (4 bytes) */
			Stream_Read(s, readerState->Common.rgbAtr, 32); /* rgbAtr [0..32] (32 bytes) */
			Stream_Seek_UINT32(s); /* rgbAtr [32..36] (4 bytes) */

			/* what is this used for? */
			readerState->Common.dwCurrentState &= 0x0000FFFF;
			readerState->Common.dwEventState = 0;
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

			Stream_Seek_UINT32(s); /* NdrMaxCount (4 bytes) */
			Stream_Seek_UINT32(s); /* NdrOffset (4 bytes) */
			Stream_Read_UINT32(s, count); /* NdrActualCount (4 bytes) */

			if (Stream_GetRemainingLength(s) < count)
			{
				WLog_Print(smartcard->log, WLOG_WARN, "GetStatusChangeA_Call is too short: %d",
						(int) Stream_GetRemainingLength(s));
				return STATUS_BUFFER_TOO_SMALL;
			}

			readerState->szReader = malloc(count + 1);
			Stream_Read(s, readerState->szReader, count);
			smartcard_unpack_read_size_align(smartcard, s, count, 4);
			readerState->szReader[count] = '\0';

			if (!readerState->szReader)
			{
				WLog_Print(smartcard->log, WLOG_WARN, "GetStatusChangeA_Call null reader name");
				return STATUS_INVALID_PARAMETER;
			}

			if (strcmp((char*) readerState->szReader, SMARTCARD_PNP_NOTIFICATION_A) == 0)
			{
				readerState->Common.dwCurrentState |= SCARD_STATE_IGNORE;
			}
		}
	}

	return SCARD_S_SUCCESS;
}

UINT32 smartcard_unpack_get_status_change_w_call(SMARTCARD_DEVICE* smartcard, wStream* s, GetStatusChangeW_Call* call)
{
	int index;
	UINT32 count;
	UINT32 status;
	ReaderStateW* readerState;
	UINT32 rgReaderStatesNdrPtr;

	call->rgReaderStates = NULL;

	status = smartcard_unpack_redir_scard_context(smartcard, s, &(call->Context));

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

	status = smartcard_unpack_redir_scard_context_ref(smartcard, s, &(call->Context));

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
		call->rgReaderStates = (ReaderStateW*) calloc(call->cReaders, sizeof(ReaderStateW));

		for (index = 0; index < call->cReaders; index++)
		{
			readerState = &call->rgReaderStates[index];

			if (Stream_GetRemainingLength(s) < 52)
			{
				WLog_Print(smartcard->log, WLOG_WARN, "GetStatusChangeW_Call is too short: %d",
						(int) Stream_GetRemainingLength(s));
				return STATUS_BUFFER_TOO_SMALL;
			}

			Stream_Seek_UINT32(s); /* (4 bytes) */
			Stream_Read_UINT32(s, readerState->Common.dwCurrentState); /* dwCurrentState (4 bytes) */
			Stream_Read_UINT32(s, readerState->Common.dwEventState); /* dwEventState (4 bytes) */
			Stream_Read_UINT32(s, readerState->Common.cbAtr); /* cbAtr (4 bytes) */
			Stream_Read(s, readerState->Common.rgbAtr, 32); /* rgbAtr [0..32] (32 bytes) */
			Stream_Seek_UINT32(s); /* rgbAtr [32..36] (4 bytes) */

			/* what is this used for? */
			readerState->Common.dwCurrentState &= 0x0000FFFF;
			readerState->Common.dwEventState = 0;
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

			Stream_Seek_UINT32(s); /* NdrMaxCount (4 bytes) */
			Stream_Seek_UINT32(s); /* NdrOffset (4 bytes) */
			Stream_Read_UINT32(s, count); /* NdrActualCount (4 bytes) */

			if (Stream_GetRemainingLength(s) < (count * 2))
			{
				WLog_Print(smartcard->log, WLOG_WARN, "GetStatusChangeW_Call is too short: %d",
						(int) Stream_GetRemainingLength(s));
				return STATUS_BUFFER_TOO_SMALL;
			}

			readerState->szReader = malloc((count + 1) * 2);
			Stream_Read(s, readerState->szReader, (count * 2));
			smartcard_unpack_read_size_align(smartcard, s, (count * 2), 4);
			readerState->szReader[count] = '\0';

			if (!readerState->szReader)
			{
				WLog_Print(smartcard->log, WLOG_WARN, "GetStatusChangeW_Call null reader name");
				return STATUS_INVALID_PARAMETER;
			}

			if (_wcscmp((WCHAR*) readerState->szReader, SMARTCARD_PNP_NOTIFICATION_W) == 0)
			{
				readerState->Common.dwCurrentState |= SCARD_STATE_IGNORE;
			}
		}
	}

	return SCARD_S_SUCCESS;
}

UINT32 smartcard_pack_get_status_change_return(SMARTCARD_DEVICE* smartcard, wStream* s, GetStatusChange_Return* ret)
{
	int index;
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
		Stream_Zero(s, 4); /* rgbAtr [32..36] (4 bytes) */
	}

	return SCARD_S_SUCCESS;
}

UINT32 smartcard_unpack_state_call(SMARTCARD_DEVICE* smartcard, wStream* s, State_Call* call)
{
	UINT32 status;

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

	status = smartcard_unpack_redir_scard_handle_ref(smartcard, s, &(call->hCard));

	if (status)
		return status;

	return SCARD_S_SUCCESS;
}

UINT32 smartcard_pack_status_return(SMARTCARD_DEVICE* smartcard, wStream* s, Status_Return* ret)
{
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

UINT32 smartcard_unpack_get_attrib_call(SMARTCARD_DEVICE* smartcard, wStream* s, GetAttrib_Call* call)
{
	UINT32 status;

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

	status = smartcard_unpack_redir_scard_handle_ref(smartcard, s, &(call->hCard));

	if (status)
		return status;

	return SCARD_S_SUCCESS;
}

UINT32 smartcard_pack_get_attrib_return(SMARTCARD_DEVICE* smartcard, wStream* s, GetAttrib_Return* ret)
{
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

UINT32 smartcard_unpack_control_call(SMARTCARD_DEVICE* smartcard, wStream* s, Control_Call* call)
{
	UINT32 status;
	UINT32 length;

	call->pvInBuffer = NULL;

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
		call->cbInBufferSize = length;

		Stream_Read(s, call->pvInBuffer, length);
	}

	return SCARD_S_SUCCESS;
}

UINT32 smartcard_pack_control_return(SMARTCARD_DEVICE* smartcard, wStream* s, Control_Return* ret)
{
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
		CopyMemory(pbExtraBytes, ioSendPci.pbExtraBytes, ioSendPci.cbExtraBytes);
		Stream_Seek(s, ioSendPci.cbExtraBytes);
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

		if (length < call->cbSendLength)
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

		winpr_HexDump(Stream_Pointer(s), Stream_GetRemainingLength(s));

		Stream_Read_UINT32(s, length); /* Length (4 bytes) */

		Stream_Read_UINT16(s, ioRecvPci.dwProtocol); /* dwProtocol (2 bytes) */
		Stream_Read_UINT16(s, ioRecvPci.cbExtraBytes); /* cbExtraBytes (2 bytes) */

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
		CopyMemory(pbExtraBytes, ioRecvPci.pbExtraBytes, ioRecvPci.cbExtraBytes);
		Stream_Seek(s, ioRecvPci.cbExtraBytes);
	}

	return SCARD_S_SUCCESS;
}

UINT32 smartcard_pack_transmit_return(SMARTCARD_DEVICE* smartcard, wStream* s, Transmit_Return* ret)
{
	Stream_EnsureRemainingCapacity(s, 32);

	if (ret->pioRecvPci)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "Transmit_Return unimplemented pioRecvPci encoding");
	}

	Stream_Write_UINT32(s, 0); /* pioRecvPciNdrPtr (4 bytes) */

	if (ret->pbRecvBuffer)
	{
		Stream_Write_UINT32(s, ret->cbRecvLength); /* cbRecvLength (4 bytes) */
		Stream_Write_UINT32(s, 0x00020004); /* pbRecvBufferNdrPtr (4 bytes) */
		Stream_Write_UINT32(s, ret->cbRecvLength); /* pbRecvBufferNdrLen (4 bytes) */

		Stream_EnsureRemainingCapacity(s, ret->cbRecvLength);
		Stream_Write(s, ret->pbRecvBuffer, ret->cbRecvLength);
		smartcard_pack_write_size_align(smartcard, s, ret->cbRecvLength, 4);
	}

	return SCARD_S_SUCCESS;
}
