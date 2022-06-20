/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Smartcard Device Service Virtual Channel
 *
 * Copyright (C) Alexi Volkov <alexi@myrealbox.com> 2006
 * Copyright 2011 O.S. Systems Software Ltda.
 * Copyright 2011 Anthony Tong <atong@trustedcs.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
 * Copyright 2017 Armin Novak <armin.novak@thincast.com>
 * Copyright 2017 Thincast Technologies GmbH
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

#include <winpr/assert.h>

#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/stream.h>
#include <winpr/smartcard.h>

#include <freerdp/freerdp.h>
#include <freerdp/channels/rdpdr.h>
#include <freerdp/channels/scard.h>

#include <freerdp/utils/rdpdr_utils.h>

#include <freerdp/utils/smartcard_operations.h>
#include <freerdp/utils/smartcard_pack.h>

#include <freerdp/log.h>
#define TAG FREERDP_TAG("utils.smartcard.ops")

static LONG smartcard_call_to_operation_handle(SMARTCARD_OPERATION* operation)
{
	WINPR_ASSERT(operation);

	operation->hContext =
	    smartcard_scard_context_native_from_redir(&(operation->call.handles.hContext));
	operation->hCard = smartcard_scard_handle_native_from_redir(&(operation->call.handles.hCard));

	return SCARD_S_SUCCESS;
}

static LONG smartcard_EstablishContext_Decode(wStream* s, SMARTCARD_OPERATION* operation)
{
	LONG status;

	WINPR_ASSERT(s);
	WINPR_ASSERT(operation);

	status = smartcard_unpack_establish_context_call(s, &operation->call.establishContext);
	if (status != SCARD_S_SUCCESS)
	{
		return scard_log_status_error(TAG, "smartcard_unpack_establish_context_call", status);
	}

	return SCARD_S_SUCCESS;
}

static LONG smartcard_ReleaseContext_Decode(wStream* s, SMARTCARD_OPERATION* operation)
{
	LONG status;

	WINPR_ASSERT(s);
	WINPR_ASSERT(operation);

	status = smartcard_unpack_context_call(s, &operation->call.context, "ReleaseContext");
	if (status != SCARD_S_SUCCESS)
		scard_log_status_error(TAG, "smartcard_unpack_context_call", status);

	return status;
}

static LONG smartcard_IsValidContext_Decode(wStream* s, SMARTCARD_OPERATION* operation)
{
	LONG status;

	WINPR_ASSERT(s);
	WINPR_ASSERT(operation);

	status = smartcard_unpack_context_call(s, &operation->call.context, "IsValidContext");

	return status;
}

static LONG smartcard_ListReaderGroupsA_Decode(wStream* s, SMARTCARD_OPERATION* operation)
{
	LONG status;

	WINPR_ASSERT(s);
	WINPR_ASSERT(operation);

	status = smartcard_unpack_list_reader_groups_call(s, &operation->call.listReaderGroups, FALSE);

	return status;
}

static LONG smartcard_ListReaderGroupsW_Decode(wStream* s, SMARTCARD_OPERATION* operation)
{
	LONG status;

	WINPR_ASSERT(s);
	WINPR_ASSERT(operation);

	status = smartcard_unpack_list_reader_groups_call(s, &operation->call.listReaderGroups, TRUE);

	return status;
}

static LONG smartcard_ListReadersA_Decode(wStream* s, SMARTCARD_OPERATION* operation)
{
	LONG status;

	WINPR_ASSERT(s);
	WINPR_ASSERT(operation);

	status = smartcard_unpack_list_readers_call(s, &operation->call.listReaders, FALSE);

	return status;
}

static LONG smartcard_ListReadersW_Decode(wStream* s, SMARTCARD_OPERATION* operation)
{
	LONG status;

	WINPR_ASSERT(s);
	WINPR_ASSERT(operation);

	status = smartcard_unpack_list_readers_call(s, &operation->call.listReaders, TRUE);

	return status;
}

static LONG smartcard_context_and_two_strings_a_Decode(wStream* s, SMARTCARD_OPERATION* operation)
{
	LONG status;

	WINPR_ASSERT(s);
	WINPR_ASSERT(operation);

	status =
	    smartcard_unpack_context_and_two_strings_a_call(s, &operation->call.contextAndTwoStringA);

	return status;
}

static LONG smartcard_context_and_two_strings_w_Decode(wStream* s, SMARTCARD_OPERATION* operation)
{
	LONG status;

	WINPR_ASSERT(s);
	WINPR_ASSERT(operation);

	status =
	    smartcard_unpack_context_and_two_strings_w_call(s, &operation->call.contextAndTwoStringW);

	return status;
}

static LONG smartcard_context_and_string_a_Decode(wStream* s, SMARTCARD_OPERATION* operation)
{
	LONG status;

	WINPR_ASSERT(s);
	WINPR_ASSERT(operation);

	status = smartcard_unpack_context_and_string_a_call(s, &operation->call.contextAndStringA);

	return status;
}

static LONG smartcard_context_and_string_w_Decode(wStream* s, SMARTCARD_OPERATION* operation)
{
	LONG status;

	WINPR_ASSERT(s);
	WINPR_ASSERT(operation);

	status = smartcard_unpack_context_and_string_w_call(s, &operation->call.contextAndStringW);

	return status;
}

static LONG smartcard_LocateCardsA_Decode(wStream* s, SMARTCARD_OPERATION* operation)
{
	LONG status;

	WINPR_ASSERT(s);
	WINPR_ASSERT(operation);

	status = smartcard_unpack_locate_cards_a_call(s, &operation->call.locateCardsA);

	return status;
}

static LONG smartcard_LocateCardsW_Decode(wStream* s, SMARTCARD_OPERATION* operation)
{
	LONG status;

	WINPR_ASSERT(s);
	WINPR_ASSERT(operation);

	status = smartcard_unpack_locate_cards_w_call(s, &operation->call.locateCardsW);

	return status;
}

static LONG smartcard_GetStatusChangeA_Decode(wStream* s, SMARTCARD_OPERATION* operation)
{
	LONG status;

	WINPR_ASSERT(s);
	WINPR_ASSERT(operation);

	status = smartcard_unpack_get_status_change_a_call(s, &operation->call.getStatusChangeA);

	return status;
}

static LONG smartcard_GetStatusChangeW_Decode(wStream* s, SMARTCARD_OPERATION* operation)
{
	LONG status;

	WINPR_ASSERT(s);
	WINPR_ASSERT(operation);

	status = smartcard_unpack_get_status_change_w_call(s, &operation->call.getStatusChangeW);

	return status;
}

static LONG smartcard_Cancel_Decode(wStream* s, SMARTCARD_OPERATION* operation)
{
	LONG status;

	WINPR_ASSERT(s);
	WINPR_ASSERT(operation);

	status = smartcard_unpack_context_call(s, &operation->call.context, "Cancel");

	return status;
}

static LONG smartcard_ConnectA_Decode(wStream* s, SMARTCARD_OPERATION* operation)
{
	LONG status;

	WINPR_ASSERT(s);
	WINPR_ASSERT(operation);

	status = smartcard_unpack_connect_a_call(s, &operation->call.connectA);

	return status;
}

static LONG smartcard_ConnectW_Decode(wStream* s, SMARTCARD_OPERATION* operation)
{
	LONG status;

	WINPR_ASSERT(s);
	WINPR_ASSERT(operation);

	status = smartcard_unpack_connect_w_call(s, &operation->call.connectW);

	return status;
}

static LONG smartcard_Reconnect_Decode(wStream* s, SMARTCARD_OPERATION* operation)
{
	LONG status;

	WINPR_ASSERT(s);
	WINPR_ASSERT(operation);

	status = smartcard_unpack_reconnect_call(s, &operation->call.reconnect);

	return status;
}

static LONG smartcard_Disconnect_Decode(wStream* s, SMARTCARD_OPERATION* operation)
{
	LONG status;

	WINPR_ASSERT(s);
	WINPR_ASSERT(operation);

	status = smartcard_unpack_hcard_and_disposition_call(s, &operation->call.hCardAndDisposition,
	                                                     "Disconnect");

	return status;
}

static LONG smartcard_BeginTransaction_Decode(wStream* s, SMARTCARD_OPERATION* operation)
{
	LONG status;

	WINPR_ASSERT(s);
	WINPR_ASSERT(operation);

	status = smartcard_unpack_hcard_and_disposition_call(s, &operation->call.hCardAndDisposition,
	                                                     "BeginTransaction");

	return status;
}

static LONG smartcard_EndTransaction_Decode(wStream* s, SMARTCARD_OPERATION* operation)
{
	LONG status;

	WINPR_ASSERT(s);
	WINPR_ASSERT(operation);

	status = smartcard_unpack_hcard_and_disposition_call(s, &operation->call.hCardAndDisposition,
	                                                     "EndTransaction");

	return status;
}

static LONG smartcard_State_Decode(wStream* s, SMARTCARD_OPERATION* operation)
{
	LONG status;

	WINPR_ASSERT(s);
	WINPR_ASSERT(operation);

	status = smartcard_unpack_state_call(s, &operation->call.state);

	return status;
}

static LONG smartcard_StatusA_Decode(wStream* s, SMARTCARD_OPERATION* operation)
{
	LONG status;

	WINPR_ASSERT(s);
	WINPR_ASSERT(operation);

	status = smartcard_unpack_status_call(s, &operation->call.status, FALSE);

	return status;
}

static LONG smartcard_StatusW_Decode(wStream* s, SMARTCARD_OPERATION* operation)
{
	LONG status;

	WINPR_ASSERT(s);
	WINPR_ASSERT(operation);

	status = smartcard_unpack_status_call(s, &operation->call.status, TRUE);

	return status;
}

static LONG smartcard_Transmit_Decode(wStream* s, SMARTCARD_OPERATION* operation)
{
	LONG status;

	WINPR_ASSERT(s);
	WINPR_ASSERT(operation);

	status = smartcard_unpack_transmit_call(s, &operation->call.transmit);

	return status;
}

static LONG smartcard_Control_Decode(wStream* s, SMARTCARD_OPERATION* operation)
{
	LONG status;

	WINPR_ASSERT(s);
	WINPR_ASSERT(operation);

	status = smartcard_unpack_control_call(s, &operation->call.control);

	return status;
}

static LONG smartcard_GetAttrib_Decode(wStream* s, SMARTCARD_OPERATION* operation)
{
	LONG status;

	WINPR_ASSERT(s);
	WINPR_ASSERT(operation);

	status = smartcard_unpack_get_attrib_call(s, &operation->call.getAttrib);

	return status;
}

static LONG smartcard_SetAttrib_Decode(wStream* s, SMARTCARD_OPERATION* operation)
{
	LONG status;

	WINPR_ASSERT(s);
	WINPR_ASSERT(operation);

	status = smartcard_unpack_set_attrib_call(s, &operation->call.setAttrib);

	return status;
}

static LONG smartcard_AccessStartedEvent_Decode(wStream* s, SMARTCARD_OPERATION* operation)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(operation);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return SCARD_F_INTERNAL_ERROR;

	Stream_Read_INT32(s, operation->call.lng.LongValue); /* Unused (4 bytes) */

	return SCARD_S_SUCCESS;
}

static LONG smartcard_LocateCardsByATRA_Decode(wStream* s, SMARTCARD_OPERATION* operation)
{
	LONG status;

	WINPR_ASSERT(s);
	WINPR_ASSERT(operation);

	status = smartcard_unpack_locate_cards_by_atr_a_call(s, &operation->call.locateCardsByATRA);

	return status;
}

static LONG smartcard_LocateCardsByATRW_Decode(wStream* s, SMARTCARD_OPERATION* operation)
{
	LONG status;

	WINPR_ASSERT(s);
	WINPR_ASSERT(operation);

	status = smartcard_unpack_locate_cards_by_atr_w_call(s, &operation->call.locateCardsByATRW);

	return status;
}

static LONG smartcard_ReadCacheA_Decode(wStream* s, SMARTCARD_OPERATION* operation)
{
	LONG status;

	WINPR_ASSERT(s);
	WINPR_ASSERT(operation);

	status = smartcard_unpack_read_cache_a_call(s, &operation->call.readCacheA);

	return status;
}

static LONG smartcard_ReadCacheW_Decode(wStream* s, SMARTCARD_OPERATION* operation)
{
	LONG status;

	WINPR_ASSERT(s);
	WINPR_ASSERT(operation);

	status = smartcard_unpack_read_cache_w_call(s, &operation->call.readCacheW);

	return status;
}

static LONG smartcard_WriteCacheA_Decode(wStream* s, SMARTCARD_OPERATION* operation)
{
	LONG status;

	WINPR_ASSERT(s);
	WINPR_ASSERT(operation);

	status = smartcard_unpack_write_cache_a_call(s, &operation->call.writeCacheA);

	return status;
}

static LONG smartcard_WriteCacheW_Decode(wStream* s, SMARTCARD_OPERATION* operation)
{
	LONG status;

	WINPR_ASSERT(s);
	WINPR_ASSERT(operation);

	status = smartcard_unpack_write_cache_w_call(s, &operation->call.writeCacheW);

	return status;
}

static LONG smartcard_GetTransmitCount_Decode(wStream* s, SMARTCARD_OPERATION* operation)
{
	LONG status;

	WINPR_ASSERT(s);
	WINPR_ASSERT(operation);

	status = smartcard_unpack_get_transmit_count_call(s, &operation->call.getTransmitCount);

	return status;
}

static LONG smartcard_ReleaseStartedEvent_Decode(wStream* s, SMARTCARD_OPERATION* operation)
{
	WINPR_UNUSED(s);
	WINPR_UNUSED(operation);
	WLog_WARN(TAG, "According to [MS-RDPESC] 3.1.4 Message Processing Events and Sequencing Rules "
	               "SCARD_IOCTL_RELEASETARTEDEVENT is not supported");
	return SCARD_E_UNSUPPORTED_FEATURE;
}

static LONG smartcard_GetReaderIcon_Decode(wStream* s, SMARTCARD_OPERATION* operation)
{
	LONG status;

	WINPR_ASSERT(s);
	WINPR_ASSERT(operation);

	status = smartcard_unpack_get_reader_icon_call(s, &operation->call.getReaderIcon);

	return status;
}

static LONG smartcard_GetDeviceTypeId_Decode(wStream* s, SMARTCARD_OPERATION* operation)
{
	LONG status;

	WINPR_ASSERT(s);
	WINPR_ASSERT(operation);

	status = smartcard_unpack_get_device_type_id_call(s, &operation->call.getDeviceTypeId);

	return status;
}

LONG smartcard_irp_device_control_decode(wStream* s, UINT32 CompletionId, UINT32 FileId,
                                         SMARTCARD_OPERATION* operation)
{
	LONG status;
	UINT32 offset;
	UINT32 ioControlCode;
	UINT32 outputBufferLength;
	UINT32 inputBufferLength;

	WINPR_ASSERT(s);
	WINPR_ASSERT(operation);

	/* Device Control Request */

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 32))
		return SCARD_F_INTERNAL_ERROR;

	Stream_Read_UINT32(s, outputBufferLength); /* OutputBufferLength (4 bytes) */
	Stream_Read_UINT32(s, inputBufferLength);  /* InputBufferLength (4 bytes) */
	Stream_Read_UINT32(s, ioControlCode);      /* IoControlCode (4 bytes) */
	Stream_Seek(s, 20);                        /* Padding (20 bytes) */
	operation->ioControlCode = ioControlCode;
	operation->ioControlCodeName = scard_get_ioctl_string(ioControlCode, FALSE);

	if (Stream_Length(s) != (Stream_GetPosition(s) + inputBufferLength))
	{
		WLog_WARN(TAG, "InputBufferLength mismatch: Actual: %" PRIuz " Expected: %" PRIuz "",
		          Stream_Length(s), Stream_GetPosition(s) + inputBufferLength);
		return SCARD_F_INTERNAL_ERROR;
	}

	WLog_DBG(TAG, "%s (0x%08" PRIX32 ") FileId: %" PRIu32 " CompletionId: %" PRIu32 "",
	         scard_get_ioctl_string(ioControlCode, TRUE), ioControlCode, FileId, CompletionId);

	if ((ioControlCode != SCARD_IOCTL_ACCESSSTARTEDEVENT) &&
	    (ioControlCode != SCARD_IOCTL_RELEASETARTEDEVENT))
	{
		status = smartcard_unpack_common_type_header(s);
		if (status != SCARD_S_SUCCESS)
			return status;

		status = smartcard_unpack_private_type_header(s);
		if (status != SCARD_S_SUCCESS)
			return status;
	}

	/* Decode */
	switch (ioControlCode)
	{
		case SCARD_IOCTL_ESTABLISHCONTEXT:
			status = smartcard_EstablishContext_Decode(s, operation);
			break;

		case SCARD_IOCTL_RELEASECONTEXT:
			status = smartcard_ReleaseContext_Decode(s, operation);
			break;

		case SCARD_IOCTL_ISVALIDCONTEXT:
			status = smartcard_IsValidContext_Decode(s, operation);
			break;

		case SCARD_IOCTL_LISTREADERGROUPSA:
			status = smartcard_ListReaderGroupsA_Decode(s, operation);
			break;

		case SCARD_IOCTL_LISTREADERGROUPSW:
			status = smartcard_ListReaderGroupsW_Decode(s, operation);
			break;

		case SCARD_IOCTL_LISTREADERSA:
			status = smartcard_ListReadersA_Decode(s, operation);
			break;

		case SCARD_IOCTL_LISTREADERSW:
			status = smartcard_ListReadersW_Decode(s, operation);
			break;

		case SCARD_IOCTL_INTRODUCEREADERGROUPA:
			status = smartcard_context_and_string_a_Decode(s, operation);
			break;

		case SCARD_IOCTL_INTRODUCEREADERGROUPW:
			status = smartcard_context_and_string_w_Decode(s, operation);
			break;

		case SCARD_IOCTL_FORGETREADERGROUPA:
			status = smartcard_context_and_string_a_Decode(s, operation);
			break;

		case SCARD_IOCTL_FORGETREADERGROUPW:
			status = smartcard_context_and_string_w_Decode(s, operation);
			break;

		case SCARD_IOCTL_INTRODUCEREADERA:
			status = smartcard_context_and_two_strings_a_Decode(s, operation);
			break;

		case SCARD_IOCTL_INTRODUCEREADERW:
			status = smartcard_context_and_two_strings_w_Decode(s, operation);
			break;

		case SCARD_IOCTL_FORGETREADERA:
			status = smartcard_context_and_string_a_Decode(s, operation);
			break;

		case SCARD_IOCTL_FORGETREADERW:
			status = smartcard_context_and_string_w_Decode(s, operation);
			break;

		case SCARD_IOCTL_ADDREADERTOGROUPA:
			status = smartcard_context_and_two_strings_a_Decode(s, operation);
			break;

		case SCARD_IOCTL_ADDREADERTOGROUPW:
			status = smartcard_context_and_two_strings_w_Decode(s, operation);
			break;

		case SCARD_IOCTL_REMOVEREADERFROMGROUPA:
			status = smartcard_context_and_two_strings_a_Decode(s, operation);
			break;

		case SCARD_IOCTL_REMOVEREADERFROMGROUPW:
			status = smartcard_context_and_two_strings_w_Decode(s, operation);
			break;

		case SCARD_IOCTL_LOCATECARDSA:
			status = smartcard_LocateCardsA_Decode(s, operation);
			break;

		case SCARD_IOCTL_LOCATECARDSW:
			status = smartcard_LocateCardsW_Decode(s, operation);
			break;

		case SCARD_IOCTL_GETSTATUSCHANGEA:
			status = smartcard_GetStatusChangeA_Decode(s, operation);
			break;

		case SCARD_IOCTL_GETSTATUSCHANGEW:
			status = smartcard_GetStatusChangeW_Decode(s, operation);
			break;

		case SCARD_IOCTL_CANCEL:
			status = smartcard_Cancel_Decode(s, operation);
			break;

		case SCARD_IOCTL_CONNECTA:
			status = smartcard_ConnectA_Decode(s, operation);
			break;

		case SCARD_IOCTL_CONNECTW:
			status = smartcard_ConnectW_Decode(s, operation);
			break;

		case SCARD_IOCTL_RECONNECT:
			status = smartcard_Reconnect_Decode(s, operation);
			break;

		case SCARD_IOCTL_DISCONNECT:
			status = smartcard_Disconnect_Decode(s, operation);
			break;

		case SCARD_IOCTL_BEGINTRANSACTION:
			status = smartcard_BeginTransaction_Decode(s, operation);
			break;

		case SCARD_IOCTL_ENDTRANSACTION:
			status = smartcard_EndTransaction_Decode(s, operation);
			break;

		case SCARD_IOCTL_STATE:
			status = smartcard_State_Decode(s, operation);
			break;

		case SCARD_IOCTL_STATUSA:
			status = smartcard_StatusA_Decode(s, operation);
			break;

		case SCARD_IOCTL_STATUSW:
			status = smartcard_StatusW_Decode(s, operation);
			break;

		case SCARD_IOCTL_TRANSMIT:
			status = smartcard_Transmit_Decode(s, operation);
			break;

		case SCARD_IOCTL_CONTROL:
			status = smartcard_Control_Decode(s, operation);
			break;

		case SCARD_IOCTL_GETATTRIB:
			status = smartcard_GetAttrib_Decode(s, operation);
			break;

		case SCARD_IOCTL_SETATTRIB:
			status = smartcard_SetAttrib_Decode(s, operation);
			break;

		case SCARD_IOCTL_ACCESSSTARTEDEVENT:
			status = smartcard_AccessStartedEvent_Decode(s, operation);
			break;

		case SCARD_IOCTL_LOCATECARDSBYATRA:
			status = smartcard_LocateCardsByATRA_Decode(s, operation);
			break;

		case SCARD_IOCTL_LOCATECARDSBYATRW:
			status = smartcard_LocateCardsByATRW_Decode(s, operation);
			break;

		case SCARD_IOCTL_READCACHEA:
			status = smartcard_ReadCacheA_Decode(s, operation);
			break;

		case SCARD_IOCTL_READCACHEW:
			status = smartcard_ReadCacheW_Decode(s, operation);
			break;

		case SCARD_IOCTL_WRITECACHEA:
			status = smartcard_WriteCacheA_Decode(s, operation);
			break;

		case SCARD_IOCTL_WRITECACHEW:
			status = smartcard_WriteCacheW_Decode(s, operation);
			break;

		case SCARD_IOCTL_GETTRANSMITCOUNT:
			status = smartcard_GetTransmitCount_Decode(s, operation);
			break;

		case SCARD_IOCTL_RELEASETARTEDEVENT:
			status = smartcard_ReleaseStartedEvent_Decode(s, operation);
			break;

		case SCARD_IOCTL_GETREADERICON:
			status = smartcard_GetReaderIcon_Decode(s, operation);
			break;

		case SCARD_IOCTL_GETDEVICETYPEID:
			status = smartcard_GetDeviceTypeId_Decode(s, operation);
			break;

		default:
			status = SCARD_F_INTERNAL_ERROR;
			break;
	}

	smartcard_call_to_operation_handle(operation);

	if ((ioControlCode != SCARD_IOCTL_ACCESSSTARTEDEVENT) &&
	    (ioControlCode != SCARD_IOCTL_RELEASETARTEDEVENT))
	{
		offset = (RDPDR_DEVICE_IO_REQUEST_LENGTH + RDPDR_DEVICE_IO_CONTROL_REQ_HDR_LENGTH);
		smartcard_unpack_read_size_align(s, Stream_GetPosition(s) - offset, 8);
	}

	if (Stream_GetPosition(s) < Stream_Length(s))
	{
		SIZE_T difference;
		difference = Stream_Length(s) - Stream_GetPosition(s);
		WLog_WARN(TAG,
		          "IRP was not fully parsed %s (%s [0x%08" PRIX32 "]): Actual: %" PRIuz
		          ", Expected: %" PRIuz ", Difference: %" PRIuz "",
		          scard_get_ioctl_string(ioControlCode, TRUE),
		          scard_get_ioctl_string(ioControlCode, FALSE), ioControlCode,
		          Stream_GetPosition(s), Stream_Length(s), difference);
		winpr_HexDump(TAG, WLOG_WARN, Stream_Pointer(s), difference);
	}

	if (Stream_GetPosition(s) > Stream_Length(s))
	{
		SIZE_T difference;
		difference = Stream_GetPosition(s) - Stream_Length(s);
		WLog_WARN(TAG,
		          "IRP was parsed beyond its end %s (0x%08" PRIX32 "): Actual: %" PRIuz
		          ", Expected: %" PRIuz ", Difference: %" PRIuz "",
		          scard_get_ioctl_string(ioControlCode, TRUE), ioControlCode, Stream_GetPosition(s),
		          Stream_Length(s), difference);
	}

	return status;
}

static void free_reader_states_a(LPSCARD_READERSTATEA rgReaderStates, UINT32 cReaders)
{
	UINT32 x;
	for (x = 0; x < cReaders; x++)
	{
		SCARD_READERSTATEA* state = &rgReaderStates[x];
		free(state->szReader);
	}

	free(rgReaderStates);
}

static void free_reader_states_w(LPSCARD_READERSTATEW rgReaderStates, UINT32 cReaders)
{
	UINT32 x;
	for (x = 0; x < cReaders; x++)
	{
		SCARD_READERSTATEW* state = &rgReaderStates[x];
		free(state->szReader);
	}

	free(rgReaderStates);
}

void smartcard_operation_free(SMARTCARD_OPERATION* op, BOOL allocated)
{
	if (!op)
		return;
	switch (op->ioControlCode)
	{
		case SCARD_IOCTL_CANCEL:
		case SCARD_IOCTL_ACCESSSTARTEDEVENT:
		case SCARD_IOCTL_RELEASETARTEDEVENT:
		case SCARD_IOCTL_LISTREADERGROUPSA:
		case SCARD_IOCTL_LISTREADERGROUPSW:
		case SCARD_IOCTL_RECONNECT:
		case SCARD_IOCTL_DISCONNECT:
		case SCARD_IOCTL_BEGINTRANSACTION:
		case SCARD_IOCTL_ENDTRANSACTION:
		case SCARD_IOCTL_STATE:
		case SCARD_IOCTL_STATUSA:
		case SCARD_IOCTL_STATUSW:
		case SCARD_IOCTL_ESTABLISHCONTEXT:
		case SCARD_IOCTL_RELEASECONTEXT:
		case SCARD_IOCTL_ISVALIDCONTEXT:
		case SCARD_IOCTL_GETATTRIB:
		case SCARD_IOCTL_GETTRANSMITCOUNT:
			break;

		case SCARD_IOCTL_LOCATECARDSA:
		{
			LocateCardsA_Call* call = &op->call.locateCardsA;
			free(call->mszCards);

			free_reader_states_a(call->rgReaderStates, call->cReaders);
		}
		break;
		case SCARD_IOCTL_LOCATECARDSW:
		{
			LocateCardsW_Call* call = &op->call.locateCardsW;
			free(call->mszCards);

			free_reader_states_w(call->rgReaderStates, call->cReaders);
		}
		break;

		case SCARD_IOCTL_LOCATECARDSBYATRA:
		{
			LocateCardsByATRA_Call* call = &op->call.locateCardsByATRA;

			free_reader_states_a(call->rgReaderStates, call->cReaders);
		}
		break;
		case SCARD_IOCTL_LOCATECARDSBYATRW:
		{
			LocateCardsByATRW_Call* call = &op->call.locateCardsByATRW;
			free_reader_states_w(call->rgReaderStates, call->cReaders);
		}
		break;
		case SCARD_IOCTL_FORGETREADERA:
		case SCARD_IOCTL_INTRODUCEREADERGROUPA:
		case SCARD_IOCTL_FORGETREADERGROUPA:
		{
			ContextAndStringA_Call* call = &op->call.contextAndStringA;
			free(call->sz);
		}
		break;

		case SCARD_IOCTL_FORGETREADERW:
		case SCARD_IOCTL_INTRODUCEREADERGROUPW:
		case SCARD_IOCTL_FORGETREADERGROUPW:
		{
			ContextAndStringW_Call* call = &op->call.contextAndStringW;
			free(call->sz);
		}
		break;

		case SCARD_IOCTL_INTRODUCEREADERA:
		case SCARD_IOCTL_REMOVEREADERFROMGROUPA:
		case SCARD_IOCTL_ADDREADERTOGROUPA:

		{
			ContextAndTwoStringA_Call* call = &op->call.contextAndTwoStringA;
			free(call->sz1);
			free(call->sz2);
		}
		break;

		case SCARD_IOCTL_INTRODUCEREADERW:
		case SCARD_IOCTL_REMOVEREADERFROMGROUPW:
		case SCARD_IOCTL_ADDREADERTOGROUPW:

		{
			ContextAndTwoStringW_Call* call = &op->call.contextAndTwoStringW;
			free(call->sz1);
			free(call->sz2);
		}
		break;

		case SCARD_IOCTL_LISTREADERSA:
		case SCARD_IOCTL_LISTREADERSW:
		{
			ListReaders_Call* call = &op->call.listReaders;
			free(call->mszGroups);
		}
		break;
		case SCARD_IOCTL_GETSTATUSCHANGEA:
		{
			GetStatusChangeA_Call* call = &op->call.getStatusChangeA;
			free_reader_states_a(call->rgReaderStates, call->cReaders);
		}
		break;

		case SCARD_IOCTL_GETSTATUSCHANGEW:
		{
			GetStatusChangeW_Call* call = &op->call.getStatusChangeW;
			free_reader_states_w(call->rgReaderStates, call->cReaders);
		}
		break;
		case SCARD_IOCTL_GETREADERICON:
		{
			GetReaderIcon_Call* call = &op->call.getReaderIcon;
			free(call->szReaderName);
		}
		break;
		case SCARD_IOCTL_GETDEVICETYPEID:
		{
			GetDeviceTypeId_Call* call = &op->call.getDeviceTypeId;
			free(call->szReaderName);
		}
		break;
		case SCARD_IOCTL_CONNECTA:
		{
			ConnectA_Call* call = &op->call.connectA;
			free(call->szReader);
		}
		break;
		case SCARD_IOCTL_CONNECTW:
		{
			ConnectW_Call* call = &op->call.connectW;
			free(call->szReader);
		}
		break;
		case SCARD_IOCTL_SETATTRIB:
			free(op->call.setAttrib.pbAttr);
			break;
		case SCARD_IOCTL_TRANSMIT:
		{
			Transmit_Call* call = &op->call.transmit;
			free(call->pbSendBuffer);
			free(call->pioSendPci);
			free(call->pioRecvPci);
		}
		break;
		case SCARD_IOCTL_CONTROL:
		{
			Control_Call* call = &op->call.control;
			free(call->pvInBuffer);
		}
		break;
		case SCARD_IOCTL_READCACHEA:
		{
			ReadCacheA_Call* call = &op->call.readCacheA;
			free(call->szLookupName);
			free(call->Common.CardIdentifier);
		}
		break;
		case SCARD_IOCTL_READCACHEW:
		{
			ReadCacheW_Call* call = &op->call.readCacheW;
			free(call->szLookupName);
			free(call->Common.CardIdentifier);
		}
		break;
		case SCARD_IOCTL_WRITECACHEA:
		{
			WriteCacheA_Call* call = &op->call.writeCacheA;
			free(call->szLookupName);
			free(call->Common.CardIdentifier);
			free(call->Common.pbData);
		}
		break;
		case SCARD_IOCTL_WRITECACHEW:
		{
			WriteCacheW_Call* call = &op->call.writeCacheW;
			free(call->szLookupName);
			free(call->Common.CardIdentifier);
			free(call->Common.pbData);
		}
		break;
		default:
			break;
	}

	{
		SMARTCARD_OPERATION empty = { 0 };
		*op = empty;
	}

	if (allocated)
		free(op);
}
