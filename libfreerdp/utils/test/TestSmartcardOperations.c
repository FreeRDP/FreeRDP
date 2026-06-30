/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2026 Joan Torres Lopez <joantolo@redhat.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>

#include <winpr/crt.h>
#include <winpr/stream.h>
#include <winpr/smartcard.h>

#include <freerdp/channels/rdpdr.h>
#include <freerdp/channels/scard.h>
#include <freerdp/utils/rdpdr_utils.h>
#include <freerdp/utils/smartcard_operations.h>
#include <freerdp/utils/smartcard_pack.h>

static inline void fill_redir_context(REDIR_SCARDCONTEXT* ctx, SCARDCONTEXT val)
{
	smartcard_scard_context_native_to_redir(ctx, val);
}

static inline void fill_redir_handle(REDIR_SCARDHANDLE* handle, SCARDHANDLE val)
{
	smartcard_scard_handle_native_to_redir(handle, val);
}

static wStream* build_device_control_request(UINT32 ioControlCode, const void* inputBuffer,
                                             size_t inputBufferLength)
{
	const char* name = scard_get_ioctl_string(ioControlCode, TRUE);
	const size_t totalLength = RDPDR_DEVICE_IO_CONTROL_REQ_HDR_LENGTH + inputBufferLength;

	wStream* s = Stream_New(nullptr, totalLength);
	if (!s)
	{
		(void)fprintf(stderr, "build_device_control_request(%s): Stream_New(%" PRIuz ") failed\n",
		              name, totalLength);
		return nullptr;
	}

	UINT32 inputLength = WINPR_ASSERTING_INT_CAST(UINT32, inputBufferLength);
	Stream_Write_UINT32(s, 2048);          /* OutputBufferLength */
	Stream_Write_UINT32(s, inputLength);   /* InputBufferLength */
	Stream_Write_UINT32(s, ioControlCode); /* IoControlCode */
	Stream_Zero(s, 20);                    /* Padding */

	if (inputBufferLength > 0)
		Stream_Write(s, inputBuffer, inputBufferLength);

	Stream_SealLength(s);
	if (!Stream_SetPosition(s, 0))
	{
		(void)fprintf(stderr, "build_device_control_request(%s): Stream_SetPosition failed\n",
		              name);
		Stream_Release(s);
		return nullptr;
	}

	return s;
}

static BOOL test_request_roundtrip(const SMARTCARD_OPERATION* opIn, SMARTCARD_OPERATION* opOut)
{
	const UINT32 ioControlCode = opIn->ioControlCode;
	const char* name = scard_get_ioctl_string(ioControlCode, TRUE);

	wStream* sEnc = Stream_New(nullptr, 4096);
	if (!sEnc)
	{
		(void)fprintf(stderr, "%s: Stream_New for encode failed\n", name);
		return FALSE;
	}

	LONG status = smartcard_irp_device_control_encode_request(sEnc, opIn);
	if (status != SCARD_S_SUCCESS)
	{
		(void)fprintf(stderr, "%s: encode_request failed with 0x%08" PRIX32 "\n", name,
		              (UINT32)status);
		Stream_Release(sEnc);
		return FALSE;
	}

	wStream* sDec =
	    build_device_control_request(ioControlCode, Stream_Buffer(sEnc), Stream_Length(sEnc));
	Stream_Release(sEnc);
	if (!sDec)
		return FALSE;

	*opOut = (SMARTCARD_OPERATION)WINPR_C_ARRAY_INIT;
	status = smartcard_irp_device_control_decode_request(sDec, 1, 0, opOut);
	Stream_Release(sDec);
	if (status != SCARD_S_SUCCESS)
	{
		(void)fprintf(stderr, "%s: decode_request failed with 0x%08" PRIX32 "\n", name,
		              (UINT32)status);
		return FALSE;
	}

	if (opOut->ioControlCode != ioControlCode)
	{
		(void)fprintf(stderr, "%s: ioControlCode mismatch: 0x%08" PRIX32 " != 0x%08" PRIX32 "\n",
		              name, opOut->ioControlCode, ioControlCode);
		smartcard_operation_free(opOut, FALSE);
		return FALSE;
	}

	return TRUE;
}

static inline BOOL check_field(const char* name, const char* field, UINT32 a, UINT32 b)
{
	if (a != b)
	{
		(void)fprintf(stderr, "%s: %s mismatch: 0x%" PRIX32 " != 0x%" PRIX32 "\n", name, field, a,
		              b);
		return FALSE;
	}
	return TRUE;
}

static inline BOOL check_bytes(const char* name, const char* field, const void* a, const void* b,
                               size_t len)
{
	if (!a || !b || memcmp(a, b, len) != 0)
	{
		(void)fprintf(stderr, "%s: %s mismatch (%" PRIuz " bytes)\n", name, field, len);
		return FALSE;
	}
	return TRUE;
}

static inline BOOL check_redir_context(const char* name, const REDIR_SCARDCONTEXT* a,
                                       const REDIR_SCARDCONTEXT* b)
{
	return check_field(name, "hContext.cbContext", a->cbContext, b->cbContext) &&
	       check_bytes(name, "hContext.pbContext", a->pbContext, b->pbContext, a->cbContext);
}

static inline BOOL check_redir_handle(const char* name, const REDIR_SCARDHANDLE* a,
                                      const REDIR_SCARDHANDLE* b)
{
	return check_field(name, "hCard.cbHandle", a->cbHandle, b->cbHandle) &&
	       check_bytes(name, "hCard.pbHandle", a->pbHandle, b->pbHandle, a->cbHandle);
}

static BOOL test_establish_context_encode_decode_request(void)
{
	BOOL success = FALSE;
	SMARTCARD_OPERATION opOut = WINPR_C_ARRAY_INIT;
	SMARTCARD_OPERATION opIn = WINPR_C_ARRAY_INIT;
	opIn.ioControlCode = SCARD_IOCTL_ESTABLISHCONTEXT;
	opIn.call.establishContext.dwScope = SCARD_SCOPE_SYSTEM;

	if (!test_request_roundtrip(&opIn, &opOut))
		goto out;
	if (!check_field("EstablishContext", "dwScope", opIn.call.establishContext.dwScope,
	                 opOut.call.establishContext.dwScope))
		goto out;

	success = TRUE;
out:
	smartcard_operation_free(&opIn, FALSE);
	smartcard_operation_free(&opOut, FALSE);
	return success;
}

static BOOL test_release_context_encode_decode_request(void)
{
	BOOL success = FALSE;
	SMARTCARD_OPERATION opOut = WINPR_C_ARRAY_INIT;
	SMARTCARD_OPERATION opIn = WINPR_C_ARRAY_INIT;
	opIn.ioControlCode = SCARD_IOCTL_RELEASECONTEXT;
	fill_redir_context(&opIn.call.context.handles.hContext, 0x1234);

	if (!test_request_roundtrip(&opIn, &opOut))
		goto out;
	if (!check_redir_context("ReleaseContext", &opIn.call.context.handles.hContext,
	                         &opOut.call.context.handles.hContext))
		goto out;

	success = TRUE;
out:
	smartcard_operation_free(&opIn, FALSE);
	smartcard_operation_free(&opOut, FALSE);
	return success;
}

static BOOL test_is_valid_context_encode_decode_request(void)
{
	BOOL success = FALSE;
	SMARTCARD_OPERATION opOut = WINPR_C_ARRAY_INIT;
	SMARTCARD_OPERATION opIn = WINPR_C_ARRAY_INIT;
	opIn.ioControlCode = SCARD_IOCTL_ISVALIDCONTEXT;
	fill_redir_context(&opIn.call.context.handles.hContext, 0x5678);

	if (!test_request_roundtrip(&opIn, &opOut))
		goto out;
	if (!check_redir_context("IsValidContext", &opIn.call.context.handles.hContext,
	                         &opOut.call.context.handles.hContext))
		goto out;

	success = TRUE;
out:
	smartcard_operation_free(&opIn, FALSE);
	smartcard_operation_free(&opOut, FALSE);
	return success;
}

static BOOL test_list_reader_groups_encode_decode_request_impl(UINT32 ioControlCode)
{
	BOOL success = FALSE;
	const char* n = scard_get_ioctl_string(ioControlCode, TRUE);
	SMARTCARD_OPERATION opOut = WINPR_C_ARRAY_INIT;
	SMARTCARD_OPERATION opIn = WINPR_C_ARRAY_INIT;
	opIn.ioControlCode = ioControlCode;
	fill_redir_context(&opIn.call.listReaderGroups.handles.hContext, 0x1234);
	opIn.call.listReaderGroups.fmszGroupsIsNULL = 0;
	opIn.call.listReaderGroups.cchGroups = SCARD_AUTOALLOCATE;

	if (!test_request_roundtrip(&opIn, &opOut))
		goto out;
	if (!check_redir_context(n, &opIn.call.listReaderGroups.handles.hContext,
	                         &opOut.call.listReaderGroups.handles.hContext))
		goto out;
	if (!check_field(n, "fmszGroupsIsNULL", opIn.call.listReaderGroups.fmszGroupsIsNULL,
	                 opOut.call.listReaderGroups.fmszGroupsIsNULL))
		goto out;
	if (!check_field(n, "cchGroups", opIn.call.listReaderGroups.cchGroups,
	                 opOut.call.listReaderGroups.cchGroups))
		goto out;

	success = TRUE;
out:
	smartcard_operation_free(&opIn, FALSE);
	smartcard_operation_free(&opOut, FALSE);
	return success;
}

static BOOL test_list_reader_groups_a_encode_decode_request(void)
{
	return test_list_reader_groups_encode_decode_request_impl(SCARD_IOCTL_LISTREADERGROUPSA);
}

static BOOL test_list_reader_groups_w_encode_decode_request(void)
{
	return test_list_reader_groups_encode_decode_request_impl(SCARD_IOCTL_LISTREADERGROUPSW);
}

static BOOL test_list_readers_encode_decode_request_impl(UINT32 ioControlCode)
{
	BOOL success = FALSE;
	const char* n = scard_get_ioctl_string(ioControlCode, TRUE);
	SMARTCARD_OPERATION opOut = WINPR_C_ARRAY_INIT;
	SMARTCARD_OPERATION opIn = WINPR_C_ARRAY_INIT;
	opIn.ioControlCode = ioControlCode;
	fill_redir_context(&opIn.call.listReaders.handles.hContext, 0x1234);
	opIn.call.listReaders.fmszReadersIsNULL = 0;
	opIn.call.listReaders.cchReaders = SCARD_AUTOALLOCATE;

	if (!test_request_roundtrip(&opIn, &opOut))
		goto out;
	if (!check_redir_context(n, &opIn.call.listReaders.handles.hContext,
	                         &opOut.call.listReaders.handles.hContext))
		goto out;
	if (!check_field(n, "cchReaders", opIn.call.listReaders.cchReaders,
	                 opOut.call.listReaders.cchReaders))
		goto out;

	success = TRUE;
out:
	smartcard_operation_free(&opIn, FALSE);
	smartcard_operation_free(&opOut, FALSE);
	return success;
}

static BOOL test_list_readers_a_encode_decode_request(void)
{
	return test_list_readers_encode_decode_request_impl(SCARD_IOCTL_LISTREADERSA);
}

static BOOL test_list_readers_w_encode_decode_request(void)
{
	return test_list_readers_encode_decode_request_impl(SCARD_IOCTL_LISTREADERSW);
}

static BOOL test_cancel_encode_decode_request(void)
{
	BOOL success = FALSE;
	SMARTCARD_OPERATION opOut = WINPR_C_ARRAY_INIT;
	SMARTCARD_OPERATION opIn = WINPR_C_ARRAY_INIT;
	opIn.ioControlCode = SCARD_IOCTL_CANCEL;
	fill_redir_context(&opIn.call.context.handles.hContext, 0xABCD);

	if (!test_request_roundtrip(&opIn, &opOut))
		goto out;
	if (!check_redir_context("Cancel", &opIn.call.context.handles.hContext,
	                         &opOut.call.context.handles.hContext))
		goto out;

	success = TRUE;
out:
	smartcard_operation_free(&opIn, FALSE);
	smartcard_operation_free(&opOut, FALSE);
	return success;
}

static BOOL test_connect_a_encode_decode_request(void)
{
	BOOL success = FALSE;
	SMARTCARD_OPERATION opOut = WINPR_C_ARRAY_INIT;
	SMARTCARD_OPERATION opIn = WINPR_C_ARRAY_INIT;
	opIn.ioControlCode = SCARD_IOCTL_CONNECTA;
	fill_redir_context(&opIn.call.connectA.Common.handles.hContext, 0x1234);
	opIn.call.connectA.Common.dwShareMode = SCARD_SHARE_SHARED;
	opIn.call.connectA.Common.dwPreferredProtocols = SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1;
	opIn.call.connectA.szReader = _strdup("TestReader");

	if (!test_request_roundtrip(&opIn, &opOut))
		goto out;
	if (!check_redir_context("ConnectA", &opIn.call.connectA.Common.handles.hContext,
	                         &opOut.call.connectA.Common.handles.hContext))
		goto out;
	if (!check_field("ConnectA", "dwShareMode", opIn.call.connectA.Common.dwShareMode,
	                 opOut.call.connectA.Common.dwShareMode))
		goto out;
	if (!check_field("ConnectA", "dwPreferredProtocols",
	                 opIn.call.connectA.Common.dwPreferredProtocols,
	                 opOut.call.connectA.Common.dwPreferredProtocols))
		goto out;
	if (!check_bytes("ConnectA", "szReader", opIn.call.connectA.szReader,
	                 opOut.call.connectA.szReader, strlen(opIn.call.connectA.szReader) + 1))
		goto out;

	success = TRUE;
out:
	smartcard_operation_free(&opIn, FALSE);
	smartcard_operation_free(&opOut, FALSE);
	return success;
}

static BOOL test_connect_w_encode_decode_request(void)
{
	BOOL success = FALSE;
	SMARTCARD_OPERATION opOut = WINPR_C_ARRAY_INIT;
	SMARTCARD_OPERATION opIn = WINPR_C_ARRAY_INIT;
	opIn.ioControlCode = SCARD_IOCTL_CONNECTW;
	fill_redir_context(&opIn.call.connectW.Common.handles.hContext, 0x1234);
	opIn.call.connectW.Common.dwShareMode = SCARD_SHARE_SHARED;
	opIn.call.connectW.Common.dwPreferredProtocols = SCARD_PROTOCOL_T0;
	opIn.call.connectW.szReader = ConvertUtf8ToWCharAlloc("TestReader", nullptr);

	if (!test_request_roundtrip(&opIn, &opOut))
		goto out;
	if (!check_redir_context("ConnectW", &opIn.call.connectW.Common.handles.hContext,
	                         &opOut.call.connectW.Common.handles.hContext))
		goto out;
	if (!check_field("ConnectW", "dwShareMode", opIn.call.connectW.Common.dwShareMode,
	                 opOut.call.connectW.Common.dwShareMode))
		goto out;
	if (!check_bytes("ConnectW", "szReader", opIn.call.connectW.szReader,
	                 opOut.call.connectW.szReader,
	                 (_wcslen(opIn.call.connectW.szReader) + 1) * sizeof(WCHAR)))
		goto out;

	success = TRUE;
out:
	smartcard_operation_free(&opIn, FALSE);
	smartcard_operation_free(&opOut, FALSE);
	return success;
}

static BOOL test_reconnect_encode_decode_request(void)
{
	BOOL success = FALSE;
	const char* n = "Reconnect";
	SMARTCARD_OPERATION opOut = WINPR_C_ARRAY_INIT;
	SMARTCARD_OPERATION opIn = WINPR_C_ARRAY_INIT;
	opIn.ioControlCode = SCARD_IOCTL_RECONNECT;
	fill_redir_context(&opIn.call.reconnect.handles.hContext, 0x1234);
	fill_redir_handle(&opIn.call.reconnect.handles.hCard, 0x5678);
	opIn.call.reconnect.dwShareMode = SCARD_SHARE_EXCLUSIVE;
	opIn.call.reconnect.dwPreferredProtocols = SCARD_PROTOCOL_T1;
	opIn.call.reconnect.dwInitialization = SCARD_LEAVE_CARD;

	if (!test_request_roundtrip(&opIn, &opOut))
		goto out;
	if (!check_redir_context(n, &opIn.call.reconnect.handles.hContext,
	                         &opOut.call.reconnect.handles.hContext))
		goto out;
	if (!check_redir_handle(n, &opIn.call.reconnect.handles.hCard,
	                        &opOut.call.reconnect.handles.hCard))
		goto out;
	if (!check_field(n, "dwShareMode", opIn.call.reconnect.dwShareMode,
	                 opOut.call.reconnect.dwShareMode))
		goto out;
	if (!check_field(n, "dwPreferredProtocols", opIn.call.reconnect.dwPreferredProtocols,
	                 opOut.call.reconnect.dwPreferredProtocols))
		goto out;
	if (!check_field(n, "dwInitialization", opIn.call.reconnect.dwInitialization,
	                 opOut.call.reconnect.dwInitialization))
		goto out;

	success = TRUE;
out:
	smartcard_operation_free(&opIn, FALSE);
	smartcard_operation_free(&opOut, FALSE);
	return success;
}

static BOOL test_hcard_and_disposition_encode_decode_request_impl(UINT32 ioControlCode,
                                                                  DWORD dwDisposition)
{
	BOOL success = FALSE;
	const char* n = scard_get_ioctl_string(ioControlCode, TRUE);
	SMARTCARD_OPERATION opOut = WINPR_C_ARRAY_INIT;
	SMARTCARD_OPERATION opIn = WINPR_C_ARRAY_INIT;
	opIn.ioControlCode = ioControlCode;
	fill_redir_context(&opIn.call.hCardAndDisposition.handles.hContext, 0x1234);
	fill_redir_handle(&opIn.call.hCardAndDisposition.handles.hCard, 0x5678);
	opIn.call.hCardAndDisposition.dwDisposition = dwDisposition;

	if (!test_request_roundtrip(&opIn, &opOut))
		goto out;
	if (!check_redir_context(n, &opIn.call.hCardAndDisposition.handles.hContext,
	                         &opOut.call.hCardAndDisposition.handles.hContext))
		goto out;
	if (!check_redir_handle(n, &opIn.call.hCardAndDisposition.handles.hCard,
	                        &opOut.call.hCardAndDisposition.handles.hCard))
		goto out;
	if (!check_field(n, "dwDisposition", opIn.call.hCardAndDisposition.dwDisposition,
	                 opOut.call.hCardAndDisposition.dwDisposition))
		goto out;

	success = TRUE;
out:
	smartcard_operation_free(&opIn, FALSE);
	smartcard_operation_free(&opOut, FALSE);
	return success;
}

static BOOL test_disconnect_encode_decode_request(void)
{
	return test_hcard_and_disposition_encode_decode_request_impl(SCARD_IOCTL_DISCONNECT,
	                                                             SCARD_LEAVE_CARD);
}

static BOOL test_begin_transaction_encode_decode_request(void)
{
	return test_hcard_and_disposition_encode_decode_request_impl(SCARD_IOCTL_BEGINTRANSACTION,
	                                                             SCARD_LEAVE_CARD);
}

static BOOL test_end_transaction_encode_decode_request(void)
{
	return test_hcard_and_disposition_encode_decode_request_impl(SCARD_IOCTL_ENDTRANSACTION,
	                                                             SCARD_RESET_CARD);
}

static BOOL test_status_encode_decode_request_impl(UINT32 ioControlCode)
{
	BOOL success = FALSE;
	const char* n = scard_get_ioctl_string(ioControlCode, TRUE);
	SMARTCARD_OPERATION opOut = WINPR_C_ARRAY_INIT;
	SMARTCARD_OPERATION opIn = WINPR_C_ARRAY_INIT;
	opIn.ioControlCode = ioControlCode;
	fill_redir_context(&opIn.call.status.handles.hContext, 0x1234);
	fill_redir_handle(&opIn.call.status.handles.hCard, 0x5678);
	opIn.call.status.fmszReaderNamesIsNULL = 0;
	opIn.call.status.cchReaderLen = SCARD_AUTOALLOCATE;
	opIn.call.status.cbAtrLen = 36;

	if (!test_request_roundtrip(&opIn, &opOut))
		goto out;
	if (!check_redir_context(n, &opIn.call.status.handles.hContext,
	                         &opOut.call.status.handles.hContext))
		goto out;
	if (!check_redir_handle(n, &opIn.call.status.handles.hCard, &opOut.call.status.handles.hCard))
		goto out;
	if (!check_field(n, "fmszReaderNamesIsNULL", opIn.call.status.fmszReaderNamesIsNULL,
	                 opOut.call.status.fmszReaderNamesIsNULL))
		goto out;
	if (!check_field(n, "cchReaderLen", opIn.call.status.cchReaderLen,
	                 opOut.call.status.cchReaderLen))
		goto out;
	if (!check_field(n, "cbAtrLen", opIn.call.status.cbAtrLen, opOut.call.status.cbAtrLen))
		goto out;

	success = TRUE;
out:
	smartcard_operation_free(&opIn, FALSE);
	smartcard_operation_free(&opOut, FALSE);
	return success;
}

static BOOL test_status_a_encode_decode_request(void)
{
	return test_status_encode_decode_request_impl(SCARD_IOCTL_STATUSA);
}

static BOOL test_status_w_encode_decode_request(void)
{
	return test_status_encode_decode_request_impl(SCARD_IOCTL_STATUSW);
}

static BOOL test_transmit_encode_decode_request(void)
{
	BOOL success = FALSE;
	BYTE sendBuf[] = { 0x00, 0xA4, 0x04, 0x00, 0x07 };
	SCARD_IO_REQUEST pioSendPci = { .dwProtocol = SCARD_PROTOCOL_T0, .cbPciLength = 8 };
	SMARTCARD_OPERATION opIn = WINPR_C_ARRAY_INIT;
	opIn.ioControlCode = SCARD_IOCTL_TRANSMIT;
	fill_redir_context(&opIn.call.transmit.handles.hContext, 0x1234);
	fill_redir_handle(&opIn.call.transmit.handles.hCard, 0x5678);
	opIn.call.transmit.pioSendPci = &pioSendPci;
	opIn.call.transmit.cbSendLength = sizeof(sendBuf);
	opIn.call.transmit.pbSendBuffer = sendBuf;
	opIn.call.transmit.fpbRecvBufferIsNULL = 0;
	opIn.call.transmit.cbRecvLength = 256;

	SMARTCARD_OPERATION opOut = WINPR_C_ARRAY_INIT;
	if (!test_request_roundtrip(&opIn, &opOut))
		goto out;
	if (!check_redir_context("Transmit", &opIn.call.transmit.handles.hContext,
	                         &opOut.call.transmit.handles.hContext))
		goto out;
	if (!check_redir_handle("Transmit", &opIn.call.transmit.handles.hCard,
	                        &opOut.call.transmit.handles.hCard))
		goto out;
	if (!check_field("Transmit", "cbSendLength", opIn.call.transmit.cbSendLength,
	                 opOut.call.transmit.cbSendLength))
		goto out;
	if (!check_bytes("Transmit", "pbSendBuffer", opIn.call.transmit.pbSendBuffer,
	                 opOut.call.transmit.pbSendBuffer, opIn.call.transmit.cbSendLength))
		goto out;
	if (!check_field("Transmit", "fpbRecvBufferIsNULL",
	                 (UINT32)opIn.call.transmit.fpbRecvBufferIsNULL,
	                 (UINT32)opOut.call.transmit.fpbRecvBufferIsNULL))
		goto out;
	if (!check_field("Transmit", "cbRecvLength", opIn.call.transmit.cbRecvLength,
	                 opOut.call.transmit.cbRecvLength))
		goto out;
	if (!check_field("Transmit", "pioSendPci.dwProtocol", opIn.call.transmit.pioSendPci->dwProtocol,
	                 opOut.call.transmit.pioSendPci->dwProtocol))
		goto out;

	success = TRUE;
out:
	opIn.call.transmit.pioSendPci = nullptr;
	opIn.call.transmit.pbSendBuffer = nullptr;
	smartcard_operation_free(&opIn, FALSE);
	smartcard_operation_free(&opOut, FALSE);
	return success;
}

static BOOL test_control_encode_decode_request(void)
{
	BOOL success = FALSE;
	BYTE inBuf[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };
	SMARTCARD_OPERATION opIn = WINPR_C_ARRAY_INIT;
	opIn.ioControlCode = SCARD_IOCTL_CONTROL;
	fill_redir_context(&opIn.call.control.handles.hContext, 0x1234);
	fill_redir_handle(&opIn.call.control.handles.hCard, 0x5678);
	opIn.call.control.dwControlCode = 0x42000001;
	opIn.call.control.cbInBufferSize = sizeof(inBuf);
	opIn.call.control.pvInBuffer = inBuf;
	opIn.call.control.fpvOutBufferIsNULL = 0;
	opIn.call.control.cbOutBufferSize = 256;

	SMARTCARD_OPERATION opOut = WINPR_C_ARRAY_INIT;
	if (!test_request_roundtrip(&opIn, &opOut))
		goto out;
	if (!check_redir_context("Control", &opIn.call.control.handles.hContext,
	                         &opOut.call.control.handles.hContext))
		goto out;
	if (!check_redir_handle("Control", &opIn.call.control.handles.hCard,
	                        &opOut.call.control.handles.hCard))
		goto out;
	if (!check_field("Control", "dwControlCode", opIn.call.control.dwControlCode,
	                 opOut.call.control.dwControlCode))
		goto out;
	if (!check_field("Control", "cbInBufferSize", opIn.call.control.cbInBufferSize,
	                 opOut.call.control.cbInBufferSize))
		goto out;
	if (!check_bytes("Control", "pvInBuffer", opIn.call.control.pvInBuffer,
	                 opOut.call.control.pvInBuffer, opIn.call.control.cbInBufferSize))
		goto out;
	if (!check_field("Control", "cbOutBufferSize", opIn.call.control.cbOutBufferSize,
	                 opOut.call.control.cbOutBufferSize))
		goto out;

	success = TRUE;
out:
	opIn.call.control.pvInBuffer = nullptr;
	smartcard_operation_free(&opIn, FALSE);
	smartcard_operation_free(&opOut, FALSE);
	return success;
}

static BOOL test_get_attrib_encode_decode_request(void)
{
	BOOL success = FALSE;
	SMARTCARD_OPERATION opOut = WINPR_C_ARRAY_INIT;
	SMARTCARD_OPERATION opIn = WINPR_C_ARRAY_INIT;
	opIn.ioControlCode = SCARD_IOCTL_GETATTRIB;
	fill_redir_context(&opIn.call.getAttrib.handles.hContext, 0x1234);
	fill_redir_handle(&opIn.call.getAttrib.handles.hCard, 0x5678);
	opIn.call.getAttrib.dwAttrId = SCARD_ATTR_ATR_STRING;
	opIn.call.getAttrib.fpbAttrIsNULL = 0;
	opIn.call.getAttrib.cbAttrLen = 36;

	if (!test_request_roundtrip(&opIn, &opOut))
		goto out;
	if (!check_redir_context("GetAttrib", &opIn.call.getAttrib.handles.hContext,
	                         &opOut.call.getAttrib.handles.hContext))
		goto out;
	if (!check_redir_handle("GetAttrib", &opIn.call.getAttrib.handles.hCard,
	                        &opOut.call.getAttrib.handles.hCard))
		goto out;
	if (!check_field("GetAttrib", "dwAttrId", opIn.call.getAttrib.dwAttrId,
	                 opOut.call.getAttrib.dwAttrId))
		goto out;
	if (!check_field("GetAttrib", "cbAttrLen", opIn.call.getAttrib.cbAttrLen,
	                 opOut.call.getAttrib.cbAttrLen))
		goto out;

	success = TRUE;
out:
	smartcard_operation_free(&opIn, FALSE);
	smartcard_operation_free(&opOut, FALSE);
	return success;
}

static BOOL test_set_attrib_encode_decode_request(void)
{
	BOOL success = FALSE;
	BYTE attrBuf[] = { 0x3B, 0x00 };
	SMARTCARD_OPERATION opOut = WINPR_C_ARRAY_INIT;
	SMARTCARD_OPERATION opIn = WINPR_C_ARRAY_INIT;
	opIn.ioControlCode = SCARD_IOCTL_SETATTRIB;
	fill_redir_context(&opIn.call.setAttrib.handles.hContext, 0x1234);
	fill_redir_handle(&opIn.call.setAttrib.handles.hCard, 0x5678);
	opIn.call.setAttrib.dwAttrId = SCARD_ATTR_ATR_STRING;
	opIn.call.setAttrib.cbAttrLen = sizeof(attrBuf);
	opIn.call.setAttrib.pbAttr = attrBuf;

	if (!test_request_roundtrip(&opIn, &opOut))
		goto out;
	if (!check_redir_context("SetAttrib", &opIn.call.setAttrib.handles.hContext,
	                         &opOut.call.setAttrib.handles.hContext))
		goto out;
	if (!check_redir_handle("SetAttrib", &opIn.call.setAttrib.handles.hCard,
	                        &opOut.call.setAttrib.handles.hCard))
		goto out;
	if (!check_field("SetAttrib", "dwAttrId", opIn.call.setAttrib.dwAttrId,
	                 opOut.call.setAttrib.dwAttrId))
		goto out;
	if (!check_field("SetAttrib", "cbAttrLen", opIn.call.setAttrib.cbAttrLen,
	                 opOut.call.setAttrib.cbAttrLen))
		goto out;
	if (!check_bytes("SetAttrib", "pbAttr", opIn.call.setAttrib.pbAttr, opOut.call.setAttrib.pbAttr,
	                 opIn.call.setAttrib.cbAttrLen))
		goto out;

	success = TRUE;
out:
	opIn.call.setAttrib.pbAttr = nullptr;
	smartcard_operation_free(&opIn, FALSE);
	smartcard_operation_free(&opOut, FALSE);
	return success;
}

static BOOL test_encode_decode_requests(void)
{
	BOOL success = TRUE;

	if (!test_establish_context_encode_decode_request())
		success = FALSE;
	if (!test_release_context_encode_decode_request())
		success = FALSE;
	if (!test_is_valid_context_encode_decode_request())
		success = FALSE;
	if (!test_list_reader_groups_a_encode_decode_request())
		success = FALSE;
	if (!test_list_reader_groups_w_encode_decode_request())
		success = FALSE;
	if (!test_list_readers_a_encode_decode_request())
		success = FALSE;
	if (!test_list_readers_w_encode_decode_request())
		success = FALSE;
	if (!test_cancel_encode_decode_request())
		success = FALSE;
	if (!test_connect_a_encode_decode_request())
		success = FALSE;
	if (!test_connect_w_encode_decode_request())
		success = FALSE;
	if (!test_reconnect_encode_decode_request())
		success = FALSE;
	if (!test_disconnect_encode_decode_request())
		success = FALSE;
	if (!test_begin_transaction_encode_decode_request())
		success = FALSE;
	if (!test_end_transaction_encode_decode_request())
		success = FALSE;
	if (!test_status_a_encode_decode_request())
		success = FALSE;
	if (!test_status_w_encode_decode_request())
		success = FALSE;
	if (!test_transmit_encode_decode_request())
		success = FALSE;
	if (!test_control_encode_decode_request())
		success = FALSE;
	if (!test_get_attrib_encode_decode_request())
		success = FALSE;
	if (!test_set_attrib_encode_decode_request())
		success = FALSE;

	return success;
}

static LONG encode_response_payload(wStream* s, UINT32 ioControlCode,
                                    const SMARTCARD_OPERATION* opIn)
{
	switch (ioControlCode)
	{
		case SCARD_IOCTL_ESTABLISHCONTEXT:
			return smartcard_pack_establish_context_return(s, &opIn->ret.establishContext);
		case SCARD_IOCTL_RECONNECT:
			return smartcard_pack_reconnect_return(s, &opIn->ret.reconnect);
		case SCARD_IOCTL_CONNECTA:
		case SCARD_IOCTL_CONNECTW:
			return smartcard_pack_connect_return(s, &opIn->ret.connect);
		case SCARD_IOCTL_CONTROL:
			return smartcard_pack_control_return(s, &opIn->ret.control);
		case SCARD_IOCTL_TRANSMIT:
			return smartcard_pack_transmit_return(s, &opIn->ret.transmit);
		case SCARD_IOCTL_GETATTRIB:
			return smartcard_pack_get_attrib_return(s, &opIn->ret.getAttrib, 0,
			                                        opIn->ret.getAttrib.cbAttrLen);
		case SCARD_IOCTL_GETSTATUSCHANGEA:
			return smartcard_pack_get_status_change_return(s, &opIn->ret.getStatusChange, FALSE);
		case SCARD_IOCTL_GETSTATUSCHANGEW:
			return smartcard_pack_get_status_change_return(s, &opIn->ret.getStatusChange, TRUE);
		case SCARD_IOCTL_LISTREADERGROUPSA:
			return smartcard_pack_list_reader_groups_return(s, &opIn->ret.listReaders, FALSE);
		case SCARD_IOCTL_LISTREADERGROUPSW:
			return smartcard_pack_list_reader_groups_return(s, &opIn->ret.listReaders, TRUE);
		case SCARD_IOCTL_LISTREADERSA:
			return smartcard_pack_list_readers_return(s, &opIn->ret.listReaders, FALSE);
		case SCARD_IOCTL_LISTREADERSW:
			return smartcard_pack_list_readers_return(s, &opIn->ret.listReaders, TRUE);
		case SCARD_IOCTL_STATUSA:
			return smartcard_pack_status_return(s, &opIn->ret.status, FALSE);
		case SCARD_IOCTL_STATUSW:
			return smartcard_pack_status_return(s, &opIn->ret.status, TRUE);
		case SCARD_IOCTL_RELEASECONTEXT:
		case SCARD_IOCTL_ISVALIDCONTEXT:
		case SCARD_IOCTL_CANCEL:
		case SCARD_IOCTL_DISCONNECT:
		case SCARD_IOCTL_BEGINTRANSACTION:
		case SCARD_IOCTL_ENDTRANSACTION:
		case SCARD_IOCTL_SETATTRIB:
			return SCARD_S_SUCCESS;
		default:
			return SCARD_F_INTERNAL_ERROR;
	}
}

static wStream* build_device_control_response(UINT32 ioControlCode, const SMARTCARD_OPERATION* opIn)
{
	const char* name = scard_get_ioctl_string(ioControlCode, TRUE);

	wStream* payload = Stream_New(NULL, 4096);
	if (!payload)
	{
		(void)fprintf(stderr, "%s: Stream_New for payload failed\n", name);
		return nullptr;
	}

	LONG rc = encode_response_payload(payload, ioControlCode, opIn);
	if (rc != SCARD_S_SUCCESS)
	{
		(void)fprintf(stderr, "%s: encode_response_payload failed with 0x%08" PRIX32 "\n", name,
		              (UINT32)rc);
		Stream_Release(payload);
		return nullptr;
	}
	Stream_SealLength(payload);

	wStream* s = Stream_New(NULL, 256 + Stream_Length(payload));
	if (!s)
	{
		(void)fprintf(stderr, "%s: Stream_New for response failed\n", name);
		Stream_Release(payload);
		return nullptr;
	}

	Stream_Seek(s, 4); /* Placeholder for outputBufferLength */
	const size_t headerPos = Stream_GetPosition(s);
	Stream_Seek(s, SMARTCARD_COMMON_TYPE_HEADER_LENGTH +
	                   SMARTCARD_PRIVATE_TYPE_HEADER_LENGTH); /* Placeholder for headers */
	Stream_Write_INT32(s, opIn->returnCode);
	Stream_Write(s, Stream_Buffer(payload), Stream_Length(payload));
	Stream_Release(payload);

	const size_t payloadSize = Stream_GetPosition(s) - headerPos;
	const size_t objectBufferLength =
	    payloadSize - SMARTCARD_COMMON_TYPE_HEADER_LENGTH - SMARTCARD_PRIVATE_TYPE_HEADER_LENGTH;

	WINPR_UNUSED(smartcard_pack_write_size_align(s, payloadSize, 8));

	const size_t endPos = Stream_GetPosition(s);
	const size_t outputBufferLength = endPos - headerPos;

	Stream_SetPosition(s, 0);
	UINT32 outputLength = WINPR_ASSERTING_INT_CAST(UINT32, outputBufferLength);
	UINT32 objectLength = WINPR_ASSERTING_INT_CAST(UINT32, objectBufferLength);
	Stream_Write_UINT32(s, outputLength);
	smartcard_pack_common_type_header(s);
	smartcard_pack_private_type_header(s, objectLength);

	Stream_SetPosition(s, endPos);
	Stream_SealLength(s);
	if (!Stream_SetPosition(s, 0))
	{
		(void)fprintf(stderr, "%s: Stream_SetPosition failed\n", name);
		Stream_Release(s);
		return nullptr;
	}
	return s;
}

static BOOL test_response_roundtrip(const SMARTCARD_OPERATION* opIn, SMARTCARD_OPERATION* opOut)
{
	const UINT32 ioControlCode = opIn->ioControlCode;
	const char* name = scard_get_ioctl_string(ioControlCode, TRUE);

	wStream* s = build_device_control_response(ioControlCode, opIn);
	if (!s)
		return FALSE;

	*opOut = (SMARTCARD_OPERATION)WINPR_C_ARRAY_INIT;
	LONG status = smartcard_irp_device_control_decode_response(s, ioControlCode, opOut);
	Stream_Release(s);

	if (status != SCARD_S_SUCCESS)
	{
		(void)fprintf(stderr, "%s: decode_response failed with 0x%08" PRIX32 "\n", name,
		              (UINT32)status);
		return FALSE;
	}

	return TRUE;
}

static BOOL test_establish_context_decode_response(void)
{
	BOOL success = FALSE;
	SMARTCARD_OPERATION opOut = WINPR_C_ARRAY_INIT;
	SMARTCARD_OPERATION opIn = WINPR_C_ARRAY_INIT;
	opIn.ioControlCode = SCARD_IOCTL_ESTABLISHCONTEXT;
	fill_redir_context(&opIn.ret.establishContext.hContext, 0xABCD);
	opIn.returnCode = SCARD_S_SUCCESS;

	if (!test_response_roundtrip(&opIn, &opOut))
		goto out;
	if (!check_redir_context("EstablishContext", &opIn.ret.establishContext.hContext,
	                         &opOut.ret.establishContext.hContext))
		goto out;
	if (!check_field("EstablishContext", "returnCode", (UINT32)opIn.returnCode,
	                 (UINT32)opOut.returnCode))
		goto out;

	success = TRUE;
out:
	smartcard_operation_free(&opIn, FALSE);
	smartcard_operation_free(&opOut, FALSE);
	return success;
}

static BOOL test_release_context_decode_response(void)
{
	BOOL success = FALSE;
	SMARTCARD_OPERATION opOut = WINPR_C_ARRAY_INIT;
	SMARTCARD_OPERATION opIn = WINPR_C_ARRAY_INIT;
	opIn.ioControlCode = SCARD_IOCTL_RELEASECONTEXT;
	opIn.returnCode = SCARD_S_SUCCESS;

	if (!test_response_roundtrip(&opIn, &opOut))
		goto out;
	if (!check_field("ReleaseContext", "returnCode", (UINT32)opIn.returnCode,
	                 (UINT32)opOut.returnCode))
		goto out;

	success = TRUE;
out:
	smartcard_operation_free(&opIn, FALSE);
	smartcard_operation_free(&opOut, FALSE);
	return success;
}

static BOOL test_is_valid_context_decode_response(void)
{
	BOOL success = FALSE;
	SMARTCARD_OPERATION opOut = WINPR_C_ARRAY_INIT;
	SMARTCARD_OPERATION opIn = WINPR_C_ARRAY_INIT;
	opIn.ioControlCode = SCARD_IOCTL_ISVALIDCONTEXT;
	opIn.returnCode = SCARD_S_SUCCESS;

	if (!test_response_roundtrip(&opIn, &opOut))
		goto out;
	if (!check_field("IsValidContext", "returnCode", (UINT32)opIn.returnCode,
	                 (UINT32)opOut.returnCode))
		goto out;

	success = TRUE;
out:
	smartcard_operation_free(&opIn, FALSE);
	smartcard_operation_free(&opOut, FALSE);
	return success;
}

static BOOL test_list_reader_groups_decode_response_impl(UINT32 ioControlCode)
{
	BOOL success = FALSE;
	const char* n = scard_get_ioctl_string(ioControlCode, TRUE);
	SMARTCARD_OPERATION opOut = WINPR_C_ARRAY_INIT;
	SMARTCARD_OPERATION opIn = WINPR_C_ARRAY_INIT;
	opIn.ioControlCode = ioControlCode;
	opIn.returnCode = SCARD_S_SUCCESS;
	BYTE groups[] = { 'G', 'r', 'p', '\0', '\0' };
	opIn.ret.listReaders.cBytes = sizeof(groups);
	opIn.ret.listReaders.msz = groups;

	if (!test_response_roundtrip(&opIn, &opOut))
		goto out;
	if (!check_field(n, "returnCode", (UINT32)opIn.returnCode, (UINT32)opOut.returnCode))
		goto out;
	if (!check_field(n, "cBytes", opIn.ret.listReaders.cBytes, opOut.ret.listReaders.cBytes))
		goto out;
	if (!check_bytes(n, "msz", opIn.ret.listReaders.msz, opOut.ret.listReaders.msz,
	                 opIn.ret.listReaders.cBytes))
		goto out;

	success = TRUE;
out:
	opIn.ret.listReaders.msz = nullptr;
	smartcard_operation_free(&opIn, FALSE);
	smartcard_operation_free(&opOut, FALSE);
	return success;
}

static BOOL test_list_reader_groups_a_decode_response(void)
{
	return test_list_reader_groups_decode_response_impl(SCARD_IOCTL_LISTREADERGROUPSA);
}

static BOOL test_list_reader_groups_w_decode_response(void)
{
	return test_list_reader_groups_decode_response_impl(SCARD_IOCTL_LISTREADERGROUPSW);
}

static BOOL test_list_readers_decode_response_impl(UINT32 ioControlCode)
{
	BOOL success = FALSE;
	const char* n = scard_get_ioctl_string(ioControlCode, TRUE);
	SMARTCARD_OPERATION opOut = WINPR_C_ARRAY_INIT;
	SMARTCARD_OPERATION opIn = WINPR_C_ARRAY_INIT;
	opIn.ioControlCode = ioControlCode;
	opIn.returnCode = SCARD_S_SUCCESS;
	BYTE readers[] = { 'R', 'd', 'r', '\0', '\0' };
	opIn.ret.listReaders.cBytes = sizeof(readers);
	opIn.ret.listReaders.msz = readers;

	if (!test_response_roundtrip(&opIn, &opOut))
		goto out;
	if (!check_field(n, "returnCode", (UINT32)opIn.returnCode, (UINT32)opOut.returnCode))
		goto out;
	if (!check_field(n, "cBytes", opIn.ret.listReaders.cBytes, opOut.ret.listReaders.cBytes))
		goto out;
	if (!check_bytes(n, "msz", opIn.ret.listReaders.msz, opOut.ret.listReaders.msz,
	                 opIn.ret.listReaders.cBytes))
		goto out;

	success = TRUE;
out:
	opIn.ret.listReaders.msz = nullptr;
	smartcard_operation_free(&opIn, FALSE);
	smartcard_operation_free(&opOut, FALSE);
	return success;
}

static BOOL test_list_readers_a_decode_response(void)
{
	return test_list_readers_decode_response_impl(SCARD_IOCTL_LISTREADERSA);
}

static BOOL test_list_readers_w_decode_response(void)
{
	return test_list_readers_decode_response_impl(SCARD_IOCTL_LISTREADERSW);
}

static BOOL test_get_status_change_decode_response_impl(UINT32 ioControlCode)
{
	BOOL success = FALSE;
	const char* n = scard_get_ioctl_string(ioControlCode, TRUE);
	SMARTCARD_OPERATION opOut = WINPR_C_ARRAY_INIT;
	SMARTCARD_OPERATION opIn = WINPR_C_ARRAY_INIT;
	opIn.ioControlCode = ioControlCode;
	opIn.returnCode = SCARD_S_SUCCESS;
	ReaderState_Return state = WINPR_C_ARRAY_INIT;
	state.dwCurrentState = SCARD_STATE_PRESENT;
	state.dwEventState = SCARD_STATE_CHANGED;
	state.cbAtr = 2;
	state.rgbAtr[0] = 0x3B;
	state.rgbAtr[1] = 0x00;
	opIn.ret.getStatusChange.cReaders = 1;
	opIn.ret.getStatusChange.rgReaderStates = &state;

	if (!test_response_roundtrip(&opIn, &opOut))
		goto out;
	if (!check_field(n, "returnCode", (UINT32)opIn.returnCode, (UINT32)opOut.returnCode))
		goto out;
	if (!check_field(n, "cReaders", opIn.ret.getStatusChange.cReaders,
	                 opOut.ret.getStatusChange.cReaders))
		goto out;
	if (!check_field(n, "dwCurrentState", state.dwCurrentState,
	                 opOut.ret.getStatusChange.rgReaderStates[0].dwCurrentState))
		goto out;
	if (!check_field(n, "dwEventState", state.dwEventState,
	                 opOut.ret.getStatusChange.rgReaderStates[0].dwEventState))
		goto out;
	if (!check_field(n, "cbAtr", state.cbAtr, opOut.ret.getStatusChange.rgReaderStates[0].cbAtr))
		goto out;
	if (!check_bytes(n, "rgbAtr", state.rgbAtr, opOut.ret.getStatusChange.rgReaderStates[0].rgbAtr,
	                 sizeof(state.rgbAtr)))
		goto out;

	success = TRUE;
out:
	opIn.ret.getStatusChange.rgReaderStates = nullptr;
	smartcard_operation_free(&opIn, FALSE);
	smartcard_operation_free(&opOut, FALSE);
	return success;
}

static BOOL test_get_status_change_a_decode_response(void)
{
	return test_get_status_change_decode_response_impl(SCARD_IOCTL_GETSTATUSCHANGEA);
}

static BOOL test_get_status_change_w_decode_response(void)
{
	return test_get_status_change_decode_response_impl(SCARD_IOCTL_GETSTATUSCHANGEW);
}

static BOOL test_cancel_decode_response(void)
{
	BOOL success = FALSE;
	SMARTCARD_OPERATION opOut = WINPR_C_ARRAY_INIT;
	SMARTCARD_OPERATION opIn = WINPR_C_ARRAY_INIT;
	opIn.ioControlCode = SCARD_IOCTL_CANCEL;
	opIn.returnCode = SCARD_S_SUCCESS;

	if (!test_response_roundtrip(&opIn, &opOut))
		goto out;
	if (!check_field("Cancel", "returnCode", (UINT32)opIn.returnCode, (UINT32)opOut.returnCode))
		goto out;

	success = TRUE;
out:
	smartcard_operation_free(&opIn, FALSE);
	smartcard_operation_free(&opOut, FALSE);
	return success;
}

static BOOL test_connect_decode_response_impl(UINT32 ioControlCode)
{
	BOOL success = FALSE;
	const char* n = scard_get_ioctl_string(ioControlCode, TRUE);
	SMARTCARD_OPERATION opOut = WINPR_C_ARRAY_INIT;
	SMARTCARD_OPERATION opIn = WINPR_C_ARRAY_INIT;
	opIn.ioControlCode = ioControlCode;
	opIn.returnCode = SCARD_S_SUCCESS;
	fill_redir_context(&opIn.ret.connect.hContext, 0x1234);
	fill_redir_handle(&opIn.ret.connect.hCard, 0x5678);
	opIn.ret.connect.dwActiveProtocol = SCARD_PROTOCOL_T0;

	if (!test_response_roundtrip(&opIn, &opOut))
		goto out;
	if (!check_field(n, "returnCode", (UINT32)opIn.returnCode, (UINT32)opOut.returnCode))
		goto out;
	if (!check_redir_context(n, &opIn.ret.connect.hContext, &opOut.ret.connect.hContext))
		goto out;
	if (!check_redir_handle(n, &opIn.ret.connect.hCard, &opOut.ret.connect.hCard))
		goto out;
	if (!check_field(n, "dwActiveProtocol", opIn.ret.connect.dwActiveProtocol,
	                 opOut.ret.connect.dwActiveProtocol))
		goto out;

	success = TRUE;
out:
	smartcard_operation_free(&opIn, FALSE);
	smartcard_operation_free(&opOut, FALSE);
	return success;
}

static BOOL test_connect_a_decode_response(void)
{
	return test_connect_decode_response_impl(SCARD_IOCTL_CONNECTA);
}

static BOOL test_connect_w_decode_response(void)
{
	return test_connect_decode_response_impl(SCARD_IOCTL_CONNECTW);
}

static BOOL test_reconnect_decode_response(void)
{
	BOOL success = FALSE;
	SMARTCARD_OPERATION opOut = WINPR_C_ARRAY_INIT;
	SMARTCARD_OPERATION opIn = WINPR_C_ARRAY_INIT;
	opIn.ioControlCode = SCARD_IOCTL_RECONNECT;
	opIn.returnCode = SCARD_S_SUCCESS;
	opIn.ret.reconnect.dwActiveProtocol = SCARD_PROTOCOL_T1;

	if (!test_response_roundtrip(&opIn, &opOut))
		goto out;
	if (!check_field("Reconnect", "returnCode", (UINT32)opIn.returnCode, (UINT32)opOut.returnCode))
		goto out;
	if (!check_field("Reconnect", "dwActiveProtocol", opIn.ret.reconnect.dwActiveProtocol,
	                 opOut.ret.reconnect.dwActiveProtocol))
		goto out;

	success = TRUE;
out:
	smartcard_operation_free(&opIn, FALSE);
	smartcard_operation_free(&opOut, FALSE);
	return success;
}

static BOOL test_disconnect_decode_response(void)
{
	BOOL success = FALSE;
	SMARTCARD_OPERATION opOut = WINPR_C_ARRAY_INIT;
	SMARTCARD_OPERATION opIn = WINPR_C_ARRAY_INIT;
	opIn.ioControlCode = SCARD_IOCTL_DISCONNECT;
	opIn.returnCode = SCARD_S_SUCCESS;

	if (!test_response_roundtrip(&opIn, &opOut))
		goto out;
	if (!check_field("Disconnect", "returnCode", (UINT32)opIn.returnCode, (UINT32)opOut.returnCode))
		goto out;

	success = TRUE;
out:
	smartcard_operation_free(&opIn, FALSE);
	smartcard_operation_free(&opOut, FALSE);
	return success;
}

static BOOL test_begin_transaction_decode_response(void)
{
	BOOL success = FALSE;
	SMARTCARD_OPERATION opOut = WINPR_C_ARRAY_INIT;
	SMARTCARD_OPERATION opIn = WINPR_C_ARRAY_INIT;
	opIn.ioControlCode = SCARD_IOCTL_BEGINTRANSACTION;
	opIn.returnCode = SCARD_S_SUCCESS;

	if (!test_response_roundtrip(&opIn, &opOut))
		goto out;
	if (!check_field("BeginTransaction", "returnCode", (UINT32)opIn.returnCode,
	                 (UINT32)opOut.returnCode))
		goto out;

	success = TRUE;
out:
	smartcard_operation_free(&opIn, FALSE);
	smartcard_operation_free(&opOut, FALSE);
	return success;
}

static BOOL test_end_transaction_decode_response(void)
{
	BOOL success = FALSE;
	SMARTCARD_OPERATION opOut = WINPR_C_ARRAY_INIT;
	SMARTCARD_OPERATION opIn = WINPR_C_ARRAY_INIT;
	opIn.ioControlCode = SCARD_IOCTL_ENDTRANSACTION;
	opIn.returnCode = SCARD_S_SUCCESS;

	if (!test_response_roundtrip(&opIn, &opOut))
		goto out;
	if (!check_field("EndTransaction", "returnCode", (UINT32)opIn.returnCode,
	                 (UINT32)opOut.returnCode))
		goto out;

	success = TRUE;
out:
	smartcard_operation_free(&opIn, FALSE);
	smartcard_operation_free(&opOut, FALSE);
	return success;
}

static BOOL test_status_decode_response_impl(UINT32 ioControlCode)
{
	BOOL success = FALSE;
	const char* n = scard_get_ioctl_string(ioControlCode, TRUE);
	SMARTCARD_OPERATION opOut = WINPR_C_ARRAY_INIT;
	SMARTCARD_OPERATION opIn = WINPR_C_ARRAY_INIT;
	opIn.ioControlCode = ioControlCode;
	opIn.returnCode = SCARD_S_SUCCESS;
	BYTE readerName[] = { 'R', 'd', 'r', '\0' };
	opIn.ret.status.cBytes = sizeof(readerName);
	opIn.ret.status.mszReaderNames = readerName;
	opIn.ret.status.dwState = SCARD_SPECIFIC;
	opIn.ret.status.dwProtocol = SCARD_PROTOCOL_T1;
	memset(opIn.ret.status.pbAtr, 0x3B, sizeof(opIn.ret.status.pbAtr));
	opIn.ret.status.cbAtrLen = 2;

	if (!test_response_roundtrip(&opIn, &opOut))
		goto out;
	if (!check_field(n, "returnCode", (UINT32)opIn.returnCode, (UINT32)opOut.returnCode))
		goto out;
	if (!check_field(n, "dwState", opIn.ret.status.dwState, opOut.ret.status.dwState))
		goto out;
	if (!check_field(n, "dwProtocol", opIn.ret.status.dwProtocol, opOut.ret.status.dwProtocol))
		goto out;
	if (!check_field(n, "cbAtrLen", opIn.ret.status.cbAtrLen, opOut.ret.status.cbAtrLen))
		goto out;
	if (!check_bytes(n, "pbAtr", opIn.ret.status.pbAtr, opOut.ret.status.pbAtr,
	                 sizeof(opIn.ret.status.pbAtr)))
		goto out;
	if (!check_field(n, "cBytes", opIn.ret.status.cBytes, opOut.ret.status.cBytes))
		goto out;
	if (!check_bytes(n, "mszReaderNames", opIn.ret.status.mszReaderNames,
	                 opOut.ret.status.mszReaderNames, opIn.ret.status.cBytes))
		goto out;

	success = TRUE;
out:
	opIn.ret.status.mszReaderNames = nullptr;
	smartcard_operation_free(&opIn, FALSE);
	smartcard_operation_free(&opOut, FALSE);
	return success;
}

static BOOL test_status_a_decode_response(void)
{
	return test_status_decode_response_impl(SCARD_IOCTL_STATUSA);
}

static BOOL test_status_w_decode_response(void)
{
	return test_status_decode_response_impl(SCARD_IOCTL_STATUSW);
}

static BOOL test_transmit_decode_response(void)
{
	BOOL success = FALSE;
	SMARTCARD_OPERATION opOut = WINPR_C_ARRAY_INIT;
	SMARTCARD_OPERATION opIn = WINPR_C_ARRAY_INIT;
	opIn.ioControlCode = SCARD_IOCTL_TRANSMIT;
	opIn.returnCode = SCARD_S_SUCCESS;
	BYTE recvBuf[] = { 0x90, 0x00 };
	opIn.ret.transmit.cbRecvLength = sizeof(recvBuf);
	opIn.ret.transmit.pbRecvBuffer = recvBuf;
	opIn.ret.transmit.pioRecvPci = nullptr;

	if (!test_response_roundtrip(&opIn, &opOut))
		goto out;
	if (!check_field("Transmit", "returnCode", (UINT32)opIn.returnCode, (UINT32)opOut.returnCode))
		goto out;
	if (!check_field("Transmit", "cbRecvLength", opIn.ret.transmit.cbRecvLength,
	                 opOut.ret.transmit.cbRecvLength))
		goto out;
	if (!check_bytes("Transmit", "pbRecvBuffer", opIn.ret.transmit.pbRecvBuffer,
	                 opOut.ret.transmit.pbRecvBuffer, opIn.ret.transmit.cbRecvLength))
		goto out;

	success = TRUE;
out:
	opIn.ret.transmit.pbRecvBuffer = nullptr;
	smartcard_operation_free(&opIn, FALSE);
	smartcard_operation_free(&opOut, FALSE);
	return success;
}

static BOOL test_control_decode_response(void)
{
	BOOL success = FALSE;
	SMARTCARD_OPERATION opOut = WINPR_C_ARRAY_INIT;
	SMARTCARD_OPERATION opIn = WINPR_C_ARRAY_INIT;
	opIn.ioControlCode = SCARD_IOCTL_CONTROL;
	opIn.returnCode = SCARD_S_SUCCESS;
	BYTE outBuf[] = { 0xAA, 0xBB, 0xCC };
	opIn.ret.control.cbOutBufferSize = sizeof(outBuf);
	opIn.ret.control.pvOutBuffer = outBuf;

	if (!test_response_roundtrip(&opIn, &opOut))
		goto out;
	if (!check_field("Control", "returnCode", (UINT32)opIn.returnCode, (UINT32)opOut.returnCode))
		goto out;
	if (!check_field("Control", "cbOutBufferSize", opIn.ret.control.cbOutBufferSize,
	                 opOut.ret.control.cbOutBufferSize))
		goto out;
	if (!check_bytes("Control", "pvOutBuffer", opIn.ret.control.pvOutBuffer,
	                 opOut.ret.control.pvOutBuffer, opIn.ret.control.cbOutBufferSize))
		goto out;

	success = TRUE;
out:
	opIn.ret.control.pvOutBuffer = nullptr;
	smartcard_operation_free(&opIn, FALSE);
	smartcard_operation_free(&opOut, FALSE);
	return success;
}

static BOOL test_get_attrib_decode_response(void)
{
	BOOL success = FALSE;
	SMARTCARD_OPERATION opOut = WINPR_C_ARRAY_INIT;
	SMARTCARD_OPERATION opIn = WINPR_C_ARRAY_INIT;
	opIn.ioControlCode = SCARD_IOCTL_GETATTRIB;
	opIn.returnCode = SCARD_S_SUCCESS;
	BYTE attr[] = { 0x3B, 0x90, 0x00 };
	opIn.ret.getAttrib.cbAttrLen = sizeof(attr);
	opIn.ret.getAttrib.pbAttr = attr;

	if (!test_response_roundtrip(&opIn, &opOut))
		goto out;
	if (!check_field("GetAttrib", "returnCode", (UINT32)opIn.returnCode, (UINT32)opOut.returnCode))
		goto out;
	if (!check_field("GetAttrib", "cbAttrLen", opIn.ret.getAttrib.cbAttrLen,
	                 opOut.ret.getAttrib.cbAttrLen))
		goto out;
	if (!check_bytes("GetAttrib", "pbAttr", opIn.ret.getAttrib.pbAttr, opOut.ret.getAttrib.pbAttr,
	                 opIn.ret.getAttrib.cbAttrLen))
		goto out;

	success = TRUE;
out:
	opIn.ret.getAttrib.pbAttr = nullptr;
	smartcard_operation_free(&opIn, FALSE);
	smartcard_operation_free(&opOut, FALSE);
	return success;
}

static BOOL test_set_attrib_decode_response(void)
{
	BOOL success = FALSE;
	SMARTCARD_OPERATION opOut = WINPR_C_ARRAY_INIT;
	SMARTCARD_OPERATION opIn = WINPR_C_ARRAY_INIT;
	opIn.ioControlCode = SCARD_IOCTL_SETATTRIB;
	opIn.returnCode = SCARD_S_SUCCESS;

	if (!test_response_roundtrip(&opIn, &opOut))
		goto out;
	if (!check_field("SetAttrib", "returnCode", (UINT32)opIn.returnCode, (UINT32)opOut.returnCode))
		goto out;

	success = TRUE;
out:
	smartcard_operation_free(&opIn, FALSE);
	smartcard_operation_free(&opOut, FALSE);
	return success;
}

static BOOL test_decode_responses(void)
{
	BOOL success = TRUE;

	if (!test_establish_context_decode_response())
		success = FALSE;
	if (!test_release_context_decode_response())
		success = FALSE;
	if (!test_is_valid_context_decode_response())
		success = FALSE;
	if (!test_list_reader_groups_a_decode_response())
		success = FALSE;
	if (!test_list_reader_groups_w_decode_response())
		success = FALSE;
	if (!test_list_readers_a_decode_response())
		success = FALSE;
	if (!test_list_readers_w_decode_response())
		success = FALSE;
	if (!test_get_status_change_a_decode_response())
		success = FALSE;
	if (!test_get_status_change_w_decode_response())
		success = FALSE;
	if (!test_cancel_decode_response())
		success = FALSE;
	if (!test_connect_a_decode_response())
		success = FALSE;
	if (!test_connect_w_decode_response())
		success = FALSE;
	if (!test_reconnect_decode_response())
		success = FALSE;
	if (!test_disconnect_decode_response())
		success = FALSE;
	if (!test_begin_transaction_decode_response())
		success = FALSE;
	if (!test_end_transaction_decode_response())
		success = FALSE;
	if (!test_status_a_decode_response())
		success = FALSE;
	if (!test_status_w_decode_response())
		success = FALSE;
	if (!test_transmit_decode_response())
		success = FALSE;
	if (!test_control_decode_response())
		success = FALSE;
	if (!test_get_attrib_decode_response())
		success = FALSE;
	if (!test_set_attrib_decode_response())
		success = FALSE;

	return success;
}

int TestSmartcardOperations(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	if (!test_encode_decode_requests())
		return -1;

	if (!test_decode_responses())
		return -1;

	return 0;
}
