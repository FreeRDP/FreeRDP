/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Smartcard Device Service Virtual Channel
 *
 * Copyright 2011 O.S. Systems Software Ltda.
 * Copyright 2011 Eduardo Fiss Beloni <beloni@ossystems.com.br>
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

#ifndef FREERDP_CHANNEL_SMARTCARD_OPERATIONS_MAIN_H
#define FREERDP_CHANNEL_SMARTCARD_OPERATIONS_MAIN_H

#include <winpr/crt.h>
#include <winpr/stream.h>
#include <winpr/smartcard.h>

#include <freerdp/api.h>
#include <freerdp/channels/scard.h>

#ifdef __cplusplus
extern "C"
{
#endif
	typedef struct
	{
		union
		{
			Handles_Call handles;
			Long_Call lng;
			Context_Call context;
			ContextAndStringA_Call contextAndStringA;
			ContextAndStringW_Call contextAndStringW;
			ContextAndTwoStringA_Call contextAndTwoStringA;
			ContextAndTwoStringW_Call contextAndTwoStringW;
			EstablishContext_Call establishContext;
			ListReaderGroups_Call listReaderGroups;
			ListReaders_Call listReaders;
			GetStatusChangeA_Call getStatusChangeA;
			LocateCardsA_Call locateCardsA;
			LocateCardsW_Call locateCardsW;
			LocateCards_ATRMask locateCardsATRMask;
			LocateCardsByATRA_Call locateCardsByATRA;
			LocateCardsByATRW_Call locateCardsByATRW;
			GetStatusChangeW_Call getStatusChangeW;
			GetReaderIcon_Call getReaderIcon;
			GetDeviceTypeId_Call getDeviceTypeId;
			Connect_Common_Call connect;
			ConnectA_Call connectA;
			ConnectW_Call connectW;
			Reconnect_Call reconnect;
			HCardAndDisposition_Call hCardAndDisposition;
			State_Call state;
			Status_Call status;
			SCardIO_Request scardIO;
			Transmit_Call transmit;
			GetTransmitCount_Call getTransmitCount;
			Control_Call control;
			GetAttrib_Call getAttrib;
			SetAttrib_Call setAttrib;
			ReadCache_Common readCache;
			ReadCacheA_Call readCacheA;
			ReadCacheW_Call readCacheW;
			WriteCache_Common writeCache;
			WriteCacheA_Call writeCacheA;
			WriteCacheW_Call writeCacheW;
		} call;
		UINT32 ioControlCode;
		UINT32 completionID;
		UINT32 deviceID;
		SCARDCONTEXT hContext;
		SCARDHANDLE hCard;
		const char* ioControlCodeName;
		UINT32 outputBufferLength; /** @since version 3.13.0 */
		union
		{
			EstablishContext_Return establishContext;
			ListReaders_Return listReaders;
			GetStatusChange_Return getStatusChange;
			Connect_Return connect;
			Control_Return control;
			Transmit_Return transmit;
			GetAttrib_Return getAttrib;
			Reconnect_Return reconnect;
			Status_Return status;
		} ret;           /** @since version 3.28.0 */
		LONG returnCode; /** @since version 3.28.0 */
	} SMARTCARD_OPERATION;

	/** @brief Alias for \ref smartcard_irp_device_control_decode_request.
	 *
	 *  Kept for backward compatibility; new code should use the _request variant.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_irp_device_control_decode(wStream* s, UINT32 CompletionId,
	                                                     UINT32 FileId,
	                                                     SMARTCARD_OPERATION* operation);

	/** @brief Decode a smartcard IOCTL request received from the client.
	 *
	 *  Parses the Device Control Request fields (OutputBufferLength, InputBufferLength,
	 *  IoControlCode, Padding), then unpacks the Common Type Header, Private Type Header,
	 *  and the IOCTL-specific call payload into \p operation->call.
	 *
	 *  @param s        Stream positioned at OutputBufferLength, after the DeviceIoRequest header.
	 *  @param CompletionId  The CompletionId from the DeviceIoRequest header.
	 *  @param FileId   The FileId from the DeviceIoRequest header.
	 *  @param operation [out] The decoded ioControlCode, call union, and context handles.
	 *  @return \b SCARD_S_SUCCESS on success, a smartcard error code on failure.
	 *
	 *  @since version 3.28.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_irp_device_control_decode_request(wStream* s, UINT32 CompletionId,
	                                                             UINT32 FileId,
	                                                             SMARTCARD_OPERATION* operation);

	/** @brief Decode a smartcard IOCTL response received from the client.
	 *
	 *  Parses the OutputBufferLength, Common Type Header, Private Type Header, and the
	 *  ReturnCode, then unpacks the IOCTL-specific return payload into \p operation->ret.
	 *
	 *  @param s              Stream positioned at OutputBufferLength, after the DeviceIoReply
	 * header.
	 *  @param ioControlCode  The SCARD_IOCTL_* code identifying the response type.
	 *  @param operation [out] The decoded returnCode and ret union.
	 *  @return \b SCARD_S_SUCCESS on success, a smartcard error code on failure.
	 *
	 *  @since version 3.28.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_irp_device_control_decode_response(wStream* s, UINT32 ioControlCode,
	                                                              SMARTCARD_OPERATION* operation);

	/** @brief Encode a smartcard IOCTL request to send to the client.
	 *
	 *  Writes the Common Type Header, Private Type Header, and the IOCTL-specific
	 *  call payload from \p operation->call into the stream. The ioControlCode field
	 *  in \p operation selects which pack_call function is dispatched.
	 *
	 *  @param s         Stream to write into, positioned where the type headers should start.
	 *  @param operation with the populated call union and ioControlCode to encode.
	 *  @return \b SCARD_S_SUCCESS on success, a smartcard error code on failure.
	 *
	 *  @since version 3.28.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG
	smartcard_irp_device_control_encode_request(wStream* s, const SMARTCARD_OPERATION* operation);

	/** @brief Free resources held by a SMARTCARD_OPERATION.
	 *
	 *  Releases any memory allocated inside the call and ret unions (e.g., reader name
	 *  strings, reader state arrays, receive buffers).
	 *
	 *  @param op        The operation to clean up. May be NULL (no-op).
	 *  @param allocated If TRUE, also frees the SMARTCARD_OPERATION struct itself.
	 */
	FREERDP_API void smartcard_operation_free(SMARTCARD_OPERATION* op, BOOL allocated);

#ifdef __cplusplus
}
#endif
#endif /* FREERDP_CHANNEL_SMARTCARD_CLIENT_OPERATIONS_H */
