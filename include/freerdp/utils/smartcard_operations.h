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
} SMARTCARD_OPERATION;

FREERDP_API LONG smartcard_irp_device_control_decode(wStream* s, UINT32 CompletionId, UINT32 FileId,
                                                     SMARTCARD_OPERATION* operation);
FREERDP_API void smartcard_operation_free(SMARTCARD_OPERATION* op, BOOL allocated);

#endif /* FREERDP_CHANNEL_SMARTCARD_CLIENT_OPERATIONS_H */
