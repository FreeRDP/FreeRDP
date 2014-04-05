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
		return SCARD_F_INTERNAL_ERROR;
	}

	/* Process CommonTypeHeader */

	Stream_Read_UINT8(s, version); /* Version (1 byte) */
	Stream_Read_UINT8(s, endianness); /* Endianness (1 byte) */
	Stream_Read_UINT16(s, commonHeaderLength); /* CommonHeaderLength (2 bytes) */
	Stream_Read_UINT32(s, filler); /* Filler (4 bytes), should be 0xCCCCCCCC */

	if (version != 1)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "Unsupported CommonTypeHeader Version %d", version);
		return SCARD_F_INTERNAL_ERROR;
	}

	if (endianness != 0x10)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "Unsupported CommonTypeHeader Endianness %d", endianness);
		return SCARD_F_INTERNAL_ERROR;
	}

	if (commonHeaderLength != 8)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "Unsupported CommonTypeHeader CommonHeaderLength %d", commonHeaderLength);
		return SCARD_F_INTERNAL_ERROR;
	}

	if (filler != 0xCCCCCCCC)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "Unexpected CommonTypeHeader Filler 0x%08X", filler);
		return SCARD_F_INTERNAL_ERROR;
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
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Read_UINT32(s, objectBufferLength); /* ObjectBufferLength (4 bytes) */
	Stream_Read_UINT32(s, filler); /* Filler (4 bytes), should be 0x00000000 */

	if (filler != 0x00000000)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "Unexpected PrivateTypeHeader Filler 0x%08X", filler);
		return SCARD_F_INTERNAL_ERROR;
	}

	if (objectBufferLength != Stream_GetRemainingLength(s))
	{
		WLog_Print(smartcard->log, WLOG_WARN, "PrivateTypeHeader ObjectBufferLength mismatch: Actual: %d, Expected: %d",
				(int) objectBufferLength, Stream_GetRemainingLength(s));
		return SCARD_F_INTERNAL_ERROR;
	}

	return 0;
}

UINT32 smartcard_pack_private_type_header(SMARTCARD_DEVICE* smartcard, wStream* s, UINT32 objectBufferLength)
{
	Stream_Write_UINT32(s, objectBufferLength); /* ObjectBufferLength (4 bytes) */
	Stream_Write_UINT32(s, 0x00000000); /* Filler (4 bytes), should be 0x00000000 */

	return 0;
}

UINT32 smartcard_unpack_establish_context_call(SMARTCARD_DEVICE* smartcard, wStream* s, EstablishContext_Call* call)
{
	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "EstablishContext_Call is too short: Actual: %d, Expected: %d\n",
				(int) Stream_GetRemainingLength(s), 4);
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Read_UINT32(s, call->dwScope); /* dwScope (4 bytes) */

	return SCARD_S_SUCCESS;
}

UINT32 smartcard_unpack_list_readers_call(SMARTCARD_DEVICE* smartcard, wStream* s, ListReaders_Call* call)
{
	if (Stream_GetRemainingLength(s) < 16)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "ListReaders_Call is too short: %d",
				(int) Stream_GetRemainingLength(s));
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Read_UINT32(s, call->cBytes); /* cBytes (4 bytes) */

	if (Stream_GetRemainingLength(s) < call->cBytes)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "ListReaders_Call is too short: Actual: %d, Expected: %d",
				(int) Stream_GetRemainingLength(s), call->cBytes);
		return SCARD_F_INTERNAL_ERROR;
	}

	call->mszGroups = NULL;
	Stream_Seek(s, 4); /* mszGroupsPtr (4 bytes) */

	Stream_Read_UINT32(s, call->fmszReadersIsNULL); /* fmszReadersIsNULL (4 bytes) */
	Stream_Read_UINT32(s, call->cchReaders); /* cchReaders (4 bytes) */

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "ListReaders_Call is too short: %d",
				(int) Stream_GetRemainingLength(s));
		return SCARD_F_INTERNAL_ERROR;
	}

	/* FIXME: complete parsing */

	return SCARD_S_SUCCESS;
}

UINT32 smartcard_unpack_redir_scard_context(SMARTCARD_DEVICE* smartcard, wStream* s, REDIR_SCARDCONTEXT* context)
{
	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "REDIR_SCARDCONTEXT is too short: %d",
				(int) Stream_GetRemainingLength(s));
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Read_UINT32(s, context->cbContext); /* cbContext (4 bytes) */

	if ((Stream_GetRemainingLength(s) < context->cbContext) || (!context->cbContext))
	{
		WLog_Print(smartcard->log, WLOG_WARN, "REDIR_SCARDCONTEXT is too short: Actual: %d, Expected: %d",
				(int) Stream_GetRemainingLength(s), context->cbContext);
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Seek_UINT32(s); /* pbContextNdrPtr (4 bytes) */

	if (context->cbContext > Stream_GetRemainingLength(s))
	{
		WLog_Print(smartcard->log, WLOG_WARN, "REDIR_SCARDCONTEXT is too long: Actual: %d, Expected: %d",
				(int) Stream_GetRemainingLength(s), context->cbContext);
		return SCARD_F_INTERNAL_ERROR;
	}

	return SCARD_S_SUCCESS;
}

UINT32 smartcard_unpack_redir_scard_context_ref(SMARTCARD_DEVICE* smartcard, wStream* s, REDIR_SCARDCONTEXT* context)
{
	UINT32 length;
	ULONG_PTR contextVal;

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "REDIR_SCARDCONTEXT is too short: Actual: %d, Expected: %d\n",
				(int) Stream_GetRemainingLength(s), 4);
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Read_UINT32(s, length); /* Length (4 bytes) */

	if ((length != 4) && (length != 8))
	{
		WLog_Print(smartcard->log, WLOG_WARN, "REDIR_SCARDCONTEXT length is not 4 or 8: %d\n", length);
		return SCARD_F_INTERNAL_ERROR;
	}

	if ((Stream_GetRemainingLength(s) < length) || (!length))
	{
		WLog_Print(smartcard->log, WLOG_WARN, "REDIR_SCARDCONTEXT is too short: Actual: %d, Expected: %d\n",
				(int) Stream_GetRemainingLength(s), length);
		return SCARD_F_INTERNAL_ERROR;
	}

	if (length > 4)
		Stream_Read_UINT64(s, contextVal);
	else
		Stream_Read_UINT32(s, contextVal);

	*((ULONG_PTR*) context->pbContext) = contextVal;

	return SCARD_S_SUCCESS;
}

UINT32 smartcard_unpack_connect_common(SMARTCARD_DEVICE* smartcard, wStream* s, Connect_Common* common)
{
	UINT32 status;

	if (Stream_GetRemainingLength(s) < 8)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "Connect_Common is too short: %d",
				(int) Stream_GetRemainingLength(s));
		return SCARD_F_INTERNAL_ERROR;
	}

	status = smartcard_unpack_redir_scard_context(smartcard, s, &(common->Context));

	if (status)
		return status;

	Stream_Read_UINT32(s, common->dwShareMode); /* dwShareMode (4 bytes) */
	Stream_Read_UINT32(s, common->dwPreferredProtocols); /* dwPreferredProtocols (4 bytes) */

	return SCARD_S_SUCCESS;
}

UINT32 smartcard_unpack_read_offset_align(SMARTCARD_DEVICE* smartcard, wStream* s, UINT32 alignment)
{
	UINT32 pad;
	UINT32 offset;

	offset = Stream_GetPosition(s);

	pad = offset;
	offset = (offset + alignment - 1) & ~(alignment - 1);
	pad = offset - pad;

	Stream_Seek(s, pad);

	return pad;
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
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Seek_UINT32(s); /* szReaderPointer (4 bytes) */

	status = smartcard_unpack_connect_common(smartcard, s, &(call->Common));

	if (status)
		return status;

	/* szReader */

	Stream_Seek_UINT32(s); /* NdrMaxCount (4 bytes) */
	Stream_Seek_UINT32(s); /* NdrOffset (4 bytes) */
	Stream_Read_UINT32(s, count); /* NdrActualCount (4 bytes) */

	call->szReader = malloc(count + 1);
	Stream_Read(s, call->szReader, count);
	smartcard_unpack_read_offset_align(smartcard, s, 4);
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
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Seek_UINT32(s); /* szReaderPointer (4 bytes) */

	status = smartcard_unpack_connect_common(smartcard, s, &(call->Common));

	if (status)
		return status;

	/* szReader */

	Stream_Seek_UINT32(s); /* NdrMaxCount (4 bytes) */
	Stream_Seek_UINT32(s); /* NdrOffset (4 bytes) */
	Stream_Read_UINT32(s, count); /* NdrActualCount (4 bytes) */

	call->szReader = malloc((count + 1) * 2);
	Stream_Read(s, call->szReader, (count * 2));
	smartcard_unpack_read_offset_align(smartcard, s, 4);
	call->szReader[count] = '\0';

	smartcard_unpack_redir_scard_context_ref(smartcard, s, &(call->Common.Context));

	return SCARD_S_SUCCESS;
}
