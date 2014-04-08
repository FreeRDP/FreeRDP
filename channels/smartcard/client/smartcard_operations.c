/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Smartcard Device Service Virtual Channel
 *
 * Copyright (C) Alexi Volkov <alexi@myrealbox.com> 2006
 * Copyright 2011 O.S. Systems Software Ltda.
 * Copyright 2011 Anthony Tong <atong@trustedcs.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/stream.h>
#include <winpr/smartcard.h>

#include <freerdp/freerdp.h>
#include <freerdp/channels/rdpdr.h>

#include "smartcard_main.h"

const char* smartcard_get_ioctl_string(UINT32 ioControlCode, BOOL funcName)
{
	switch (ioControlCode)
	{
		case SCARD_IOCTL_ESTABLISHCONTEXT:
			return funcName ? "SCardEstablishContext" : "SCARD_IOCTL_ESTABLISHCONTEXT";
		case SCARD_IOCTL_RELEASECONTEXT:
			return funcName ? "SCardReleaseContext" : "SCARD_IOCTL_RELEASECONTEXT";
		case SCARD_IOCTL_ISVALIDCONTEXT:
			return funcName ? "SCardIsValidContext" : "SCARD_IOCTL_ISVALIDCONTEXT";
		case SCARD_IOCTL_LISTREADERGROUPSA:
			return funcName ? "SCardListReaderGroupsA" : "SCARD_IOCTL_LISTREADERGROUPSA";
		case SCARD_IOCTL_LISTREADERGROUPSW:
			return funcName ? "SCardListReaderGroupsW" : "SCARD_IOCTL_LISTREADERGROUPSW";
		case SCARD_IOCTL_LISTREADERSA:
			return funcName ? "SCardListReadersA" : "SCARD_IOCTL_LISTREADERSA";
		case SCARD_IOCTL_LISTREADERSW:
			return funcName ? "SCardListReadersW" : "SCARD_IOCTL_LISTREADERSW";
		case SCARD_IOCTL_INTRODUCEREADERGROUPA:
			return funcName ? "SCardIntroduceReaderGroupA" : "SCARD_IOCTL_INTRODUCEREADERGROUPA";
		case SCARD_IOCTL_INTRODUCEREADERGROUPW:
			return funcName ? "SCardIntroduceReaderGroupW" : "SCARD_IOCTL_INTRODUCEREADERGROUPW";
		case SCARD_IOCTL_FORGETREADERGROUPA:
			return funcName ? "SCardForgetReaderGroupA" : "SCARD_IOCTL_FORGETREADERGROUPA";
		case SCARD_IOCTL_FORGETREADERGROUPW:
			return funcName ? "SCardForgetReaderGroupW" : "SCARD_IOCTL_FORGETREADERGROUPW";
		case SCARD_IOCTL_INTRODUCEREADERA:
			return funcName ? "SCardIntroduceReaderA" : "SCARD_IOCTL_INTRODUCEREADERA";
		case SCARD_IOCTL_INTRODUCEREADERW:
			return funcName ? "SCardIntroduceReaderW" : "SCARD_IOCTL_INTRODUCEREADERW";
		case SCARD_IOCTL_FORGETREADERA:
			return funcName ? "SCardForgetReaderA" : "SCARD_IOCTL_FORGETREADERA";
		case SCARD_IOCTL_FORGETREADERW:
			return funcName ? "SCardForgetReaderW" : "SCARD_IOCTL_FORGETREADERW";
		case SCARD_IOCTL_ADDREADERTOGROUPA:
			return funcName ? "SCardAddReaderToGroupA" : "SCARD_IOCTL_ADDREADERTOGROUPA";
		case SCARD_IOCTL_ADDREADERTOGROUPW:
			return funcName ? "SCardAddReaderToGroupW" : "SCARD_IOCTL_ADDREADERTOGROUPW";
		case SCARD_IOCTL_REMOVEREADERFROMGROUPA:
			return funcName ? "SCardRemoveReaderFromGroupA" : "SCARD_IOCTL_REMOVEREADERFROMGROUPA";
		case SCARD_IOCTL_REMOVEREADERFROMGROUPW:
			return funcName ? "SCardRemoveReaderFromGroupW" : "SCARD_IOCTL_REMOVEREADERFROMGROUPW";
		case SCARD_IOCTL_LOCATECARDSA:
			return funcName ? "SCardLocateCardsA" : "SCARD_IOCTL_LOCATECARDSA";
		case SCARD_IOCTL_LOCATECARDSW:
			return funcName ? "SCardLocateCardsW" : "SCARD_IOCTL_LOCATECARDSW";
		case SCARD_IOCTL_GETSTATUSCHANGEA:
			return funcName ? "SCardGetStatusChangeA" : "SCARD_IOCTL_GETSTATUSCHANGEA";
		case SCARD_IOCTL_GETSTATUSCHANGEW:
			return funcName ? "SCardGetStatusChangeW" : "SCARD_IOCTL_GETSTATUSCHANGEW";
		case SCARD_IOCTL_CANCEL:
			return funcName ? "SCardCancel" : "SCARD_IOCTL_CANCEL";
		case SCARD_IOCTL_CONNECTA:
			return funcName ? "SCardConnectA" : "SCARD_IOCTL_CONNECTA";
		case SCARD_IOCTL_CONNECTW:
			return funcName ? "SCardConnectW" : "SCARD_IOCTL_CONNECTW";
		case SCARD_IOCTL_RECONNECT:
			return funcName ? "SCardReconnect" : "SCARD_IOCTL_RECONNECT";
		case SCARD_IOCTL_DISCONNECT:
			return funcName ? "SCardDisconnect" : "SCARD_IOCTL_DISCONNECT";
		case SCARD_IOCTL_BEGINTRANSACTION:
			return funcName ? "SCardBeginTransaction" : "SCARD_IOCTL_BEGINTRANSACTION";
		case SCARD_IOCTL_ENDTRANSACTION:
			return funcName ? "SCardEndTransaction" : "SCARD_IOCTL_ENDTRANSACTION";
		case SCARD_IOCTL_STATE:
			return funcName ? "SCardState" : "SCARD_IOCTL_STATE";
		case SCARD_IOCTL_STATUSA:
			return funcName ? "SCardStatusA" : "SCARD_IOCTL_STATUSA";
		case SCARD_IOCTL_STATUSW:
			return funcName ? "SCardStatusW" : "SCARD_IOCTL_STATUSW";
		case SCARD_IOCTL_TRANSMIT:
			return funcName ? "SCardTransmit" : "SCARD_IOCTL_TRANSMIT";
		case SCARD_IOCTL_CONTROL:
			return funcName ? "SCardControl" : "SCARD_IOCTL_CONTROL";
		case SCARD_IOCTL_GETATTRIB:
			return funcName ? "SCardGetAttrib" : "SCARD_IOCTL_GETATTRIB";
		case SCARD_IOCTL_SETATTRIB:
			return funcName ? "SCardSetAttrib" : "SCARD_IOCTL_SETATTRIB";
		case SCARD_IOCTL_ACCESSSTARTEDEVENT:
			return funcName ? "SCardAccessStartedEvent" : "SCARD_IOCTL_ACCESSSTARTEDEVENT";
		case SCARD_IOCTL_LOCATECARDSBYATRA:
			return funcName ? "SCardLocateCardsByATRA" : "SCARD_IOCTL_LOCATECARDSBYATRA";
		case SCARD_IOCTL_LOCATECARDSBYATRW:
			return funcName ? "SCardLocateCardsByATRB" : "SCARD_IOCTL_LOCATECARDSBYATRW";
		case SCARD_IOCTL_READCACHEA:
			return funcName ? "SCardReadCacheA" : "SCARD_IOCTL_READCACHEA";
		case SCARD_IOCTL_READCACHEW:
			return funcName ? "SCardReadCacheW" : "SCARD_IOCTL_READCACHEW";
		case SCARD_IOCTL_WRITECACHEA:
			return funcName ? "SCardWriteCacheA" : "SCARD_IOCTL_WRITECACHEA";
		case SCARD_IOCTL_WRITECACHEW:
			return funcName ? "SCardWriteCacheW" : "SCARD_IOCTL_WRITECACHEW";
		case SCARD_IOCTL_GETTRANSMITCOUNT:
			return funcName ? "SCardGetTransmitCount" : "SCARD_IOCTL_GETTRANSMITCOUNT";
		case SCARD_IOCTL_RELEASESTARTEDEVENT:
			return funcName ? "SCardReleaseStartedEvent" : "SCARD_IOCTL_RELEASESTARTEDEVENT";
		case SCARD_IOCTL_GETREADERICON:
			return funcName ? "SCardGetReaderIcon" : "SCARD_IOCTL_GETREADERICON";
		case SCARD_IOCTL_GETDEVICETYPEID:
			return funcName ? "SCardGetDeviceTypeId" : "SCARD_IOCTL_GETDEVICETYPEID";
		default:
			return funcName ? "SCardUnknown" : "SCARD_IOCTL_UNKNOWN";
	}

	return funcName ? "SCardUnknown" : "SCARD_IOCTL_UNKNOWN";
}

size_t smartcard_multi_string_length_a(const char* msz)
{
	char* p = (char*) msz;

	if (!p)
		return 0;

	while (p[0] || p[1])
		p++;

	return (p - msz);
}

size_t smartcard_multi_string_length_w(const WCHAR* msz)
{
	WCHAR* p = (WCHAR*) msz;

	if (!p)
		return 0;

	while (p[0] || p[1])
		p++;

	return (p - msz);
}

static UINT32 smartcard_EstablishContext(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	UINT32 status;
	SCARDCONTEXT hContext = -1;
	EstablishContext_Call call;
	EstablishContext_Return ret;

	status = smartcard_unpack_establish_context_call(smartcard, irp->input, &call);

	if (status)
		return status;

	status = ret.ReturnCode = SCardEstablishContext(call.dwScope, NULL, NULL, &hContext);

	smartcard->hContext = hContext;

	ret.Context.cbContext = sizeof(ULONG_PTR);
	ret.Context.pbContext = (UINT64) hContext;

	status = smartcard_pack_establish_context_return(smartcard, irp->output, &ret);

	if (status)
		return status;

	return ret.ReturnCode;
}

static UINT32 smartcard_ReleaseContext(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	UINT32 status;
	Context_Call call;
	Long_Return ret;
	SCARDCONTEXT hContext;

	status = smartcard_unpack_context_call(smartcard, irp->input, &call);

	if (status)
		return status;

	hContext = (ULONG_PTR) call.Context.pbContext;

	status = ret.ReturnCode = SCardReleaseContext(hContext);

	smartcard->hContext = 0;

	return ret.ReturnCode;
}

static UINT32 smartcard_IsValidContext(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	UINT32 status;
	Context_Call call;
	Long_Return ret;
	SCARDCONTEXT hContext;

	status = smartcard_unpack_context_call(smartcard, irp->input, &call);

	if (status)
		return status;

	hContext = (ULONG_PTR) call.Context.pbContext;

	status = ret.ReturnCode = SCardIsValidContext(hContext);

	return ret.ReturnCode;
}

static UINT32 smartcard_ListReadersA(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	UINT32 status;
	SCARDCONTEXT hContext;
	ListReaders_Call call;
	ListReaders_Return ret;
	LPSTR mszReaders = NULL;
	DWORD cchReaders = 0;

	status = smartcard_unpack_list_readers_call(smartcard, irp->input, &call);

	if (status)
		return status;

	hContext = (ULONG_PTR) call.Context.pbContext;

	cchReaders = SCARD_AUTOALLOCATE;

	status = ret.ReturnCode = SCardListReadersA(hContext, (LPCSTR) call.mszGroups, (LPSTR) &mszReaders, &cchReaders);

	if (status)
		return status;

	ret.msz = (BYTE*) mszReaders;
	ret.cBytes = smartcard_multi_string_length_a((char*) ret.msz) + 2;

	status = smartcard_pack_list_readers_return(smartcard, irp->output, &ret);

	if (status)
		return status;

	if (mszReaders)
		SCardFreeMemory(hContext, mszReaders);

	if (call.mszGroups)
		free(call.mszGroups);

	return ret.ReturnCode;
}

static UINT32 smartcard_ListReadersW(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	UINT32 status;
	SCARDCONTEXT hContext;
	ListReaders_Call call;
	ListReaders_Return ret;
	LPWSTR mszReaders = NULL;
	DWORD cchReaders = 0;

	status = smartcard_unpack_list_readers_call(smartcard, irp->input, &call);

	if (status)
		return status;

	hContext = (ULONG_PTR) call.Context.pbContext;

	cchReaders = SCARD_AUTOALLOCATE;

	status = ret.ReturnCode = SCardListReadersW(hContext, (LPCWSTR) call.mszGroups, (LPWSTR) &mszReaders, &cchReaders);

	if (status)
		return status;

	ret.msz = (BYTE*) mszReaders;
	ret.cBytes = (smartcard_multi_string_length_w((WCHAR*) ret.msz) + 2) * 2;

	status = smartcard_pack_list_readers_return(smartcard, irp->output, &ret);

	if (status)
		return status;

	if (mszReaders)
		SCardFreeMemory(hContext, mszReaders);

	if (call.mszGroups)
		free(call.mszGroups);

	return ret.ReturnCode;
}

static UINT32 smartcard_GetStatusChangeA(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	LONG status;
	UINT32 index;
	SCARDCONTEXT hContext;
	GetStatusChangeA_Call call;
	GetStatusChange_Return ret;
	ReaderStateA* readerState = NULL;
	ReaderState_Return* readerStateRet = NULL;
	LPSCARD_READERSTATEA rgReaderState = NULL;
	LPSCARD_READERSTATEA rgReaderStates = NULL;

	status = smartcard_unpack_get_status_change_a_call(smartcard, irp->input, &call);

	if (status)
		return status;

	hContext = (ULONG_PTR) call.Context.pbContext;

	rgReaderStates = (SCARD_READERSTATEA*) calloc(call.cReaders, sizeof(SCARD_READERSTATEA));

	for (index = 0; index < call.cReaders; index++)
	{
		rgReaderState = &rgReaderStates[index];
		rgReaderState->szReader = (LPCSTR) call.rgReaderStates[index].szReader;
		rgReaderState->dwCurrentState = call.rgReaderStates[index].Common.dwCurrentState;
		rgReaderState->dwEventState = call.rgReaderStates[index].Common.dwEventState;
		rgReaderState->cbAtr = call.rgReaderStates[index].Common.cbAtr;
		CopyMemory(&(rgReaderState->rgbAtr), &(call.rgReaderStates[index].Common.rgbAtr), 36);
	}

	status = ret.ReturnCode = SCardGetStatusChangeA(hContext, call.dwTimeOut, rgReaderStates, call.cReaders);

	if (status)
		return status;

	ret.cReaders = call.cReaders;
	ret.rgReaderStates = (ReaderState_Return*) calloc(ret.cReaders, sizeof(ReaderState_Return));

	for (index = 0; index < ret.cReaders; index++)
	{
		rgReaderState = &rgReaderStates[index];
		readerStateRet = &ret.rgReaderStates[index];
		readerStateRet->dwCurrentState = rgReaderState->dwCurrentState;
		readerStateRet->dwEventState = rgReaderState->dwEventState;
		readerStateRet->cbAtr = rgReaderState->cbAtr;
		CopyMemory(readerStateRet->rgbAtr, rgReaderState->rgbAtr, 36);
	}

	status = smartcard_pack_get_status_change_return(smartcard, irp->output, &ret);

	if (status)
		return status;

	if (call.rgReaderStates)
	{
		for (index = 0; index < call.cReaders; index++)
		{
			readerState = &call.rgReaderStates[index];

			if (readerState->szReader)
				free((void*) readerState->szReader);
			readerState->szReader = NULL;
		}
		free(call.rgReaderStates);
		free(rgReaderStates);
	}

	free(ret.rgReaderStates);

	return ret.ReturnCode;
}

static UINT32 smartcard_GetStatusChangeW(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	LONG status;
	UINT32 index;
	SCARDCONTEXT hContext;
	GetStatusChangeW_Call call;
	GetStatusChange_Return ret;
	ReaderStateW* readerState = NULL;
	ReaderState_Return* readerStateRet = NULL;
	LPSCARD_READERSTATEW rgReaderState = NULL;
	LPSCARD_READERSTATEW rgReaderStates = NULL;

	status = smartcard_unpack_get_status_change_w_call(smartcard, irp->input, &call);

	if (status)
		return status;

	hContext = (ULONG_PTR) call.Context.pbContext;

	rgReaderStates = (SCARD_READERSTATEW*) calloc(call.cReaders, sizeof(SCARD_READERSTATEW));

	for (index = 0; index < call.cReaders; index++)
	{
		rgReaderState = &rgReaderStates[index];
		rgReaderState->szReader = (LPCWSTR) call.rgReaderStates[index].szReader;
		rgReaderState->dwCurrentState = call.rgReaderStates[index].Common.dwCurrentState;
		rgReaderState->dwEventState = call.rgReaderStates[index].Common.dwEventState;
		rgReaderState->cbAtr = call.rgReaderStates[index].Common.cbAtr;
		CopyMemory(&(rgReaderState->rgbAtr), &(call.rgReaderStates[index].Common.rgbAtr), 36);
	}

	status = ret.ReturnCode = SCardGetStatusChangeW(hContext, call.dwTimeOut, rgReaderStates, call.cReaders);

	if (status)
		return status;

	ret.cReaders = call.cReaders;
	ret.rgReaderStates = (ReaderState_Return*) calloc(ret.cReaders, sizeof(ReaderState_Return));

	for (index = 0; index < ret.cReaders; index++)
	{
		rgReaderState = &rgReaderStates[index];
		readerStateRet = &ret.rgReaderStates[index];
		readerStateRet->dwCurrentState = rgReaderState->dwCurrentState;
		readerStateRet->dwEventState = rgReaderState->dwEventState;
		readerStateRet->cbAtr = rgReaderState->cbAtr;
		CopyMemory(readerStateRet->rgbAtr, rgReaderState->rgbAtr, 36);
	}

	status = smartcard_pack_get_status_change_return(smartcard, irp->output, &ret);

	if (status)
		return status;

	if (call.rgReaderStates)
	{
		for (index = 0; index < call.cReaders; index++)
		{
			readerState = &call.rgReaderStates[index];

			if (readerState->szReader)
				free((void*) readerState->szReader);
			readerState->szReader = NULL;
		}
		free(call.rgReaderStates);
		free(rgReaderStates);
	}

	free(ret.rgReaderStates);

	return ret.ReturnCode;
}

static UINT32 smartcard_Cancel(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	LONG status;
	SCARDCONTEXT hContext;
	Context_Call call;
	Long_Return ret;

	status = smartcard_unpack_context_call(smartcard, irp->input, &call);

	if (status)
		return status;

	hContext = (ULONG_PTR) call.Context.pbContext;

	status = ret.ReturnCode = SCardCancel(hContext);

	return ret.ReturnCode;
}

UINT32 smartcard_ConnectA(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	LONG status;
	SCARDHANDLE hCard;
	SCARDCONTEXT hContext;
	ConnectA_Call call;
	Connect_Return ret;

	call.szReader = NULL;

	status = smartcard_unpack_connect_a_call(smartcard, irp->input, &call);

	if (status)
		return status;

	hContext = (ULONG_PTR) call.Common.Context.pbContext;

	if ((call.Common.dwPreferredProtocols == SCARD_PROTOCOL_UNDEFINED) &&
			(call.Common.dwShareMode != SCARD_SHARE_DIRECT))
	{
		call.Common.dwPreferredProtocols = SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1;
	}

	status = ret.ReturnCode = SCardConnectA(hContext, (char*) call.szReader, call.Common.dwShareMode,
			call.Common.dwPreferredProtocols, &hCard, &ret.dwActiveProtocol);

	if (status)
		return status;

	smartcard->hCard = hCard;

	ret.hCard.Context.cbContext = sizeof(ULONG_PTR);
	ret.hCard.Context.pbContext = (UINT64) hContext;

	ret.hCard.cbHandle = sizeof(ULONG_PTR);
	ret.hCard.pbHandle = (UINT64) hCard;

	status = smartcard_pack_connect_return(smartcard, irp->output, &ret);

	if (status)
		return status;

	if (call.szReader)
		free(call.szReader);

	return ret.ReturnCode;
}

UINT32 smartcard_ConnectW(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	LONG status;
	SCARDCONTEXT hContext;
	SCARDHANDLE hCard;
	ConnectW_Call call;
	Connect_Return ret;

	call.szReader = NULL;

	status = smartcard_unpack_connect_w_call(smartcard, irp->input, &call);

	if (status)
		return status;

	hContext = (ULONG_PTR) call.Common.Context.pbContext;

	if ((call.Common.dwPreferredProtocols == SCARD_PROTOCOL_UNDEFINED) &&
			(call.Common.dwShareMode != SCARD_SHARE_DIRECT))
	{
		call.Common.dwPreferredProtocols = SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1;
	}

	status = ret.ReturnCode = SCardConnectW(hContext, (WCHAR*) call.szReader, call.Common.dwShareMode,
			call.Common.dwPreferredProtocols, &hCard, &ret.dwActiveProtocol);

	if (status)
		return status;

	smartcard->hCard = hCard;

	ret.hCard.Context.cbContext = sizeof(ULONG_PTR);
	ret.hCard.Context.pbContext = (UINT64) hContext;

	ret.hCard.cbHandle = sizeof(ULONG_PTR);
	ret.hCard.pbHandle = (UINT64) hCard;

	status = smartcard_pack_connect_return(smartcard, irp->output, &ret);

	if (status)
		return status;

	if (call.szReader)
		free(call.szReader);

	return ret.ReturnCode;
}

static UINT32 smartcard_Reconnect(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	LONG status;
	SCARDHANDLE hCard;
	SCARDCONTEXT hContext;
	Reconnect_Call call;
	Reconnect_Return ret;

	status = smartcard_unpack_reconnect_call(smartcard, irp->input, &call);

	if (status)
		return status;

	hCard = (ULONG_PTR) call.hCard.pbHandle;
	hContext = (ULONG_PTR) call.hCard.Context.pbContext;

	status = ret.ReturnCode = SCardReconnect(hCard, call.dwShareMode,
			call.dwPreferredProtocols, call.dwInitialization, &ret.dwActiveProtocol);

	if (status)
		return status;

	status = smartcard_pack_reconnect_return(smartcard, irp->output, &ret);

	if (status)
		return status;

	return ret.ReturnCode;
}

static UINT32 smartcard_Disconnect(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	LONG status;
	SCARDHANDLE hCard;
	SCARDCONTEXT hContext;
	HCardAndDisposition_Call call;
	Long_Return ret;

	status = smartcard_unpack_hcard_and_disposition_call(smartcard, irp->input, &call);

	if (status)
		return status;

	hCard = (ULONG_PTR) call.hCard.pbHandle;
	hContext = (ULONG_PTR) call.hCard.Context.pbContext;

	status = ret.ReturnCode = SCardDisconnect(hCard, (DWORD) call.dwDisposition);

	if (status)
		return status;

	smartcard->hCard = 0;

	return ret.ReturnCode;
}

static UINT32 smartcard_BeginTransaction(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	LONG status;
	SCARDHANDLE hCard;
	SCARDCONTEXT hContext;
	HCardAndDisposition_Call call;
	Long_Return ret;

	status = smartcard_unpack_hcard_and_disposition_call(smartcard, irp->input, &call);

	if (status)
		return status;

	hCard = (ULONG_PTR) call.hCard.pbHandle;
	hContext = (ULONG_PTR) call.hCard.Context.pbContext;

	status = ret.ReturnCode = SCardBeginTransaction(hCard);

	if (status)
		return status;

	return ret.ReturnCode;
}

static UINT32 smartcard_EndTransaction(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	LONG status;
	SCARDHANDLE hCard;
	SCARDCONTEXT hContext;
	HCardAndDisposition_Call call;
	Long_Return ret;

	status = smartcard_unpack_hcard_and_disposition_call(smartcard, irp->input, &call);

	if (status)
		return status;

	hCard = (ULONG_PTR) call.hCard.pbHandle;
	hContext = (ULONG_PTR) call.hCard.Context.pbContext;

	status = ret.ReturnCode = SCardEndTransaction(hCard, call.dwDisposition);

	if (status)
		return status;

	return ret.ReturnCode;
}

static UINT32 smartcard_State(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	LONG status;
	SCARDHANDLE hCard;
	SCARDCONTEXT hContext;
	State_Call call;
	State_Return ret;

	status = smartcard_unpack_state_call(smartcard, irp->input, &call);

	if (status)
		return status;

	hCard = (ULONG_PTR) call.hCard.pbHandle;
	hContext = (ULONG_PTR) call.hCard.Context.pbContext;

	ret.cbAtrLen = SCARD_ATR_LENGTH;

	status = ret.ReturnCode = SCardState(hCard, &ret.dwState, &ret.dwProtocol, (BYTE*) &ret.rgAtr, &ret.cbAtrLen);

	if (ret.ReturnCode)
	{
		Stream_Zero(irp->output, 256);
		return ret.ReturnCode;
	}

	status = smartcard_pack_state_return(smartcard, irp->output, &ret);

	if (status)
		return status;

	return ret.ReturnCode;
}

static DWORD smartcard_StatusA(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	LONG status;
	SCARDHANDLE hCard;
	SCARDCONTEXT hContext;
	Status_Call call;
	Status_Return ret;
	DWORD cchReaderLen = 0;
	LPSTR mszReaderNames = NULL;

	status = smartcard_unpack_status_call(smartcard, irp->input, &call);

	if (status)
		return status;

	hCard = (ULONG_PTR) call.hCard.pbHandle;
	hContext = (ULONG_PTR) call.hCard.Context.pbContext;

	if (call.cbAtrLen > 32)
		call.cbAtrLen = 32;

	ret.cbAtrLen = call.cbAtrLen;
	ZeroMemory(ret.pbAtr, 32);

	cchReaderLen = SCARD_AUTOALLOCATE;

	status = ret.ReturnCode = SCardStatusA(hCard, (LPSTR) &mszReaderNames, &cchReaderLen,
			&ret.dwState, &ret.dwProtocol, (BYTE*) &ret.pbAtr, &ret.cbAtrLen);

	if (ret.ReturnCode)
	{
		Stream_Zero(irp->output, 256);
		return ret.ReturnCode;
	}

	ret.mszReaderNames = (BYTE*) mszReaderNames;
	ret.cBytes = smartcard_multi_string_length_a((char*) ret.mszReaderNames) + 2;

	status = smartcard_pack_status_return(smartcard, irp->output, &ret);

	if (status)
		return status;

	if (mszReaderNames)
		SCardFreeMemory(hContext, mszReaderNames);

	return ret.ReturnCode;
}

static DWORD smartcard_StatusW(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	LONG status;
	SCARDHANDLE hCard;
	SCARDCONTEXT hContext;
	Status_Call call;
	Status_Return ret;
	DWORD cchReaderLen = 0;
	LPWSTR mszReaderNames = NULL;

	status = smartcard_unpack_status_call(smartcard, irp->input, &call);

	if (status)
		return status;

	hCard = (ULONG_PTR) call.hCard.pbHandle;
	hContext = (ULONG_PTR) call.hCard.Context.pbContext;

	if (call.cbAtrLen > 32)
		call.cbAtrLen = 32;

	ret.cbAtrLen = call.cbAtrLen;
	ZeroMemory(ret.pbAtr, 32);

	cchReaderLen = SCARD_AUTOALLOCATE;

	status = ret.ReturnCode = SCardStatusW(hCard, (LPWSTR) &mszReaderNames, &cchReaderLen,
			&ret.dwState, &ret.dwProtocol, (BYTE*) &ret.pbAtr, &ret.cbAtrLen);

	if (ret.ReturnCode)
	{
		Stream_Zero(irp->output, 256);
		return ret.ReturnCode;
	}

	ret.mszReaderNames = (BYTE*) mszReaderNames;
	ret.cBytes = (smartcard_multi_string_length_w((WCHAR*) ret.mszReaderNames) + 2) * 2;

	status = smartcard_pack_status_return(smartcard, irp->output, &ret);

	if (status)
		return status;

	if (mszReaderNames)
		SCardFreeMemory(hContext, mszReaderNames);

	return ret.ReturnCode;
}

static UINT32 smartcard_Transmit(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	LONG status;
	SCARDHANDLE hCard;
	SCARDCONTEXT hContext;
	Transmit_Call call;
	Transmit_Return ret;

	status = smartcard_unpack_transmit_call(smartcard, irp->input, &call);

	if (status)
		return status;

	hCard = (ULONG_PTR) call.hCard.pbHandle;
	hContext = (ULONG_PTR) call.hCard.Context.pbContext;

	ret.cbRecvLength = 0;
	ret.pbRecvBuffer = NULL;

	if (call.cbRecvLength && !call.fpbRecvBufferIsNULL)
	{
		if (call.cbRecvLength >= 66560)
			call.cbRecvLength = 66560;

		ret.cbRecvLength = call.cbRecvLength;
		ret.pbRecvBuffer = (BYTE*) malloc(ret.cbRecvLength);
	}

	ret.pioRecvPci = call.pioRecvPci;

	status = ret.ReturnCode = SCardTransmit(hCard, call.pioSendPci, call.pbSendBuffer,
			call.cbSendLength, ret.pioRecvPci, ret.pbRecvBuffer, &(ret.cbRecvLength));

	if (status)
		return status;

	status = smartcard_pack_transmit_return(smartcard, irp->output, &ret);

	if (status)
		return status;

	if (call.pbSendBuffer)
		free(call.pbSendBuffer);
	if (ret.pbRecvBuffer)
		free(ret.pbRecvBuffer);
	if (call.pioSendPci)
		free(call.pioSendPci);
	if (call.pioRecvPci)
		free(call.pioRecvPci);

	return ret.ReturnCode;
}

static UINT32 smartcard_Control(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	LONG status;
	SCARDHANDLE hCard;
	SCARDCONTEXT hContext;
	Control_Call call;
	Control_Return ret;
	UINT32 controlFunction;

	status = smartcard_unpack_control_call(smartcard, irp->input, &call);

	if (status)
		return status;

	hCard = (ULONG_PTR) call.hCard.pbHandle;
	hContext = (ULONG_PTR) call.hCard.Context.pbContext;

	if (DEVICE_TYPE_FROM_CTL_CODE(call.dwControlCode) == FILE_DEVICE_SMARTCARD)
	{
		controlFunction = FUNCTION_FROM_CTL_CODE(call.dwControlCode);
		call.dwControlCode = SCARD_CTL_CODE(controlFunction);
	}

	ret.cbOutBufferSize = call.cbOutBufferSize;
	ret.pvOutBuffer = (BYTE*) malloc(call.cbOutBufferSize);

	status = ret.ReturnCode = SCardControl(hCard,
			call.dwControlCode, call.pvInBuffer, call.cbInBufferSize,
			ret.pvOutBuffer, call.cbOutBufferSize, &ret.cbOutBufferSize);

	if (status)
		return status;

	status = smartcard_pack_control_return(smartcard, irp->output, &ret);

	if (status)
		return status;

	if (call.pvInBuffer)
		free(call.pvInBuffer);
	if (ret.pvOutBuffer)
		free(ret.pvOutBuffer);

	return ret.ReturnCode;
}

static UINT32 smartcard_GetAttrib(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	LONG status;
	DWORD cbAttrLen;
	SCARDHANDLE hCard;
	SCARDCONTEXT hContext;
	GetAttrib_Call call;
	GetAttrib_Return ret;

	status = smartcard_unpack_get_attrib_call(smartcard, irp->input, &call);

	if (status)
		return status;

	hCard = (ULONG_PTR) call.hCard.pbHandle;
	hContext = (ULONG_PTR) call.hCard.Context.pbContext;

	ret.pbAttr = NULL;
	cbAttrLen = (!call.cbAttrLen || call.fpbAttrIsNULL) ? 0 : SCARD_AUTOALLOCATE;

	status = ret.ReturnCode = SCardGetAttrib(hCard, call.dwAttrId,
			(cbAttrLen == 0) ? NULL : (BYTE*) &ret.pbAttr, &cbAttrLen);

	if (status)
		cbAttrLen = (call.cbAttrLen == 0) ? 0 : SCARD_AUTOALLOCATE;

	if ((call.dwAttrId == SCARD_ATTR_DEVICE_FRIENDLY_NAME_A) && (status == SCARD_E_UNSUPPORTED_FEATURE))
	{
		status = SCardGetAttrib(hCard, SCARD_ATTR_DEVICE_FRIENDLY_NAME_W,
				(cbAttrLen == 0) ? NULL : (BYTE*) &ret.pbAttr, &cbAttrLen);

		if (status)
			cbAttrLen = (call.cbAttrLen == 0) ? 0 : SCARD_AUTOALLOCATE;
	}

	if ((call.dwAttrId == SCARD_ATTR_DEVICE_FRIENDLY_NAME_W) && (status == SCARD_E_UNSUPPORTED_FEATURE))
	{
		status = SCardGetAttrib(hCard, SCARD_ATTR_DEVICE_FRIENDLY_NAME_A,
				(cbAttrLen == 0) ? NULL : (BYTE*) &ret.pbAttr, &cbAttrLen);

		if (status)
			cbAttrLen = (call.cbAttrLen == 0) ? 0 : SCARD_AUTOALLOCATE;
	}

	if ((cbAttrLen > call.cbAttrLen) && (ret.pbAttr))
		status = SCARD_E_INSUFFICIENT_BUFFER;

	ret.ReturnCode = status;

	call.cbAttrLen = cbAttrLen;
	ret.cbAttrLen = call.cbAttrLen;

	if (ret.ReturnCode)
	{
		Stream_Zero(irp->output, 256);
		return ret.ReturnCode;
	}

	status = smartcard_pack_get_attrib_return(smartcard, irp->output, &ret);

	if (status)
		return status;

	SCardFreeMemory(hContext, ret.pbAttr);

	return ret.ReturnCode;
}

static UINT32 smartcard_AccessStartedEvent(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	UINT32 status;
	Long_Return ret;

	if (Stream_GetRemainingLength(irp->input) < 4)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "AccessStartedEvent is too short: %d",
				(int) Stream_GetRemainingLength(irp->input));
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Seek(irp->input, 4); /* Unused (4 bytes) */
	
	status = ret.ReturnCode = SCARD_S_SUCCESS;

	return status;
}

void smartcard_irp_device_control_peek_io_control_code(SMARTCARD_DEVICE* smartcard, IRP* irp, UINT32* ioControlCode)
{
	*ioControlCode = 0;

	if (Stream_GetRemainingLength(irp->input) < 32)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "Device Control Request is too short: %d",
				(int) Stream_GetRemainingLength(irp->input));
		return;
	}

	Stream_Seek_UINT32(irp->input); /* OutputBufferLength (4 bytes) */
	Stream_Seek_UINT32(irp->input); /* InputBufferLength (4 bytes) */
	Stream_Read_UINT32(irp->input, *ioControlCode); /* IoControlCode (4 bytes) */
	Stream_Rewind(irp->input, (4 + 4 + 4));
}

void smartcard_irp_device_control(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	UINT32 result;
	UINT32 status;
	UINT32 ioControlCode;
	UINT32 outputBufferLength;
	UINT32 inputBufferLength;
	UINT32 objectBufferLength;

	/* Device Control Request */

	if (Stream_GetRemainingLength(irp->input) < 32)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "Device Control Request is too short: %d",
				(int) Stream_GetRemainingLength(irp->input));
		return;
	}

	Stream_Read_UINT32(irp->input, outputBufferLength); /* OutputBufferLength (4 bytes) */
	Stream_Read_UINT32(irp->input, inputBufferLength); /* InputBufferLength (4 bytes) */
	Stream_Read_UINT32(irp->input, ioControlCode); /* IoControlCode (4 bytes) */
	Stream_Seek(irp->input, 20); /* Padding (20 bytes) */

	if (Stream_Length(irp->input) != (Stream_GetPosition(irp->input) + inputBufferLength))
	{
		WLog_Print(smartcard->log, WLOG_WARN,
				"InputBufferLength mismatch: Actual: %d Expected: %d\n",
				Stream_Length(irp->input), Stream_GetPosition(irp->input) + inputBufferLength);
		return;
	}

	WLog_Print(smartcard->log, WLOG_DEBUG, "%s (0x%08X) FileId: %d CompletionId: %d",
			smartcard_get_ioctl_string(ioControlCode, TRUE), ioControlCode, irp->FileId, irp->CompletionId);

#if 1
	//if (/*(ioControlCode != SCARD_IOCTL_TRANSMIT) &&*/
	//	(ioControlCode != SCARD_IOCTL_GETSTATUSCHANGEA) &&
	//	(ioControlCode != SCARD_IOCTL_GETSTATUSCHANGEW))
	{
		printf("%s (0x%08X) FileId: %d CompletionId: %d\n",
			smartcard_get_ioctl_string(ioControlCode, TRUE), ioControlCode, irp->FileId, irp->CompletionId);
	}
#endif

	if ((ioControlCode != SCARD_IOCTL_ACCESSSTARTEDEVENT) &&
			(ioControlCode != SCARD_IOCTL_RELEASESTARTEDEVENT))
	{
		status = smartcard_unpack_common_type_header(smartcard, irp->input);

		if (status)
			return;

		status = smartcard_unpack_private_type_header(smartcard, irp->input);

		if (status)
			return;
	}

	/**
	 * [MS-RDPESC] 3.2.5.1: Sending Outgoing Messages:
	 * the output buffer length SHOULD be set to 2048
	 *
	 * Since it's a SHOULD and not a MUST, we don't care
	 * about it, but we still reserve at least 2048 bytes.
	 */
	Stream_EnsureRemainingCapacity(irp->output, 2048);

	/* Device Control Response */
	Stream_Seek_UINT32(irp->output); /* OutputBufferLength (4 bytes) */

	Stream_Seek(irp->output, SMARTCARD_COMMON_TYPE_HEADER_LENGTH); /* CommonTypeHeader (8 bytes) */
	Stream_Seek(irp->output, SMARTCARD_PRIVATE_TYPE_HEADER_LENGTH); /* PrivateTypeHeader (8 bytes) */

	Stream_Seek_UINT32(irp->output); /* Result (4 bytes) */

	switch (ioControlCode)
	{
		case SCARD_IOCTL_ESTABLISHCONTEXT:
			result = smartcard_EstablishContext(smartcard, irp);
			break;

		case SCARD_IOCTL_RELEASECONTEXT:
			result = smartcard_ReleaseContext(smartcard, irp);
			break;

		case SCARD_IOCTL_ISVALIDCONTEXT:
			result = smartcard_IsValidContext(smartcard, irp);
			break;

		case SCARD_IOCTL_LISTREADERGROUPSA:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_LISTREADERGROUPSW:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_LISTREADERSA:
			result = smartcard_ListReadersA(smartcard, irp);
			break;

		case SCARD_IOCTL_LISTREADERSW:
			result = smartcard_ListReadersW(smartcard, irp);
			break;

		case SCARD_IOCTL_INTRODUCEREADERGROUPA:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_INTRODUCEREADERGROUPW:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_FORGETREADERGROUPA:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_FORGETREADERGROUPW:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_INTRODUCEREADERA:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_INTRODUCEREADERW:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_FORGETREADERA:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_FORGETREADERW:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_ADDREADERTOGROUPA:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_ADDREADERTOGROUPW:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_REMOVEREADERFROMGROUPA:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_REMOVEREADERFROMGROUPW:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_LOCATECARDSA:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_LOCATECARDSW:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_GETSTATUSCHANGEA:
			result = smartcard_GetStatusChangeA(smartcard, irp);
			break;

		case SCARD_IOCTL_GETSTATUSCHANGEW:
			result = smartcard_GetStatusChangeW(smartcard, irp);
			break;

		case SCARD_IOCTL_CANCEL:
			result = smartcard_Cancel(smartcard, irp);
			break;

		case SCARD_IOCTL_CONNECTA:
			result = smartcard_ConnectA(smartcard, irp);
			break;

		case SCARD_IOCTL_CONNECTW:
			result = smartcard_ConnectW(smartcard, irp);
			break;

		case SCARD_IOCTL_RECONNECT:
			result = smartcard_Reconnect(smartcard, irp);
			break;

		case SCARD_IOCTL_DISCONNECT:
			result = smartcard_Disconnect(smartcard, irp);
			break;

		case SCARD_IOCTL_BEGINTRANSACTION:
			result = smartcard_BeginTransaction(smartcard, irp);
			break;

		case SCARD_IOCTL_ENDTRANSACTION:
			result = smartcard_EndTransaction(smartcard, irp);
			break;

		case SCARD_IOCTL_STATE:
			result = smartcard_State(smartcard, irp);
			break;

		case SCARD_IOCTL_STATUSA:
			result = smartcard_StatusA(smartcard, irp);
			break;

		case SCARD_IOCTL_STATUSW:
			result = smartcard_StatusW(smartcard, irp);
			break;

		case SCARD_IOCTL_TRANSMIT:
			result = smartcard_Transmit(smartcard, irp);
			break;

		case SCARD_IOCTL_CONTROL:
			result = smartcard_Control(smartcard, irp);
			break;

		case SCARD_IOCTL_GETATTRIB:
			result = smartcard_GetAttrib(smartcard, irp);
			break;

		case SCARD_IOCTL_SETATTRIB:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_ACCESSSTARTEDEVENT:
			result = smartcard_AccessStartedEvent(smartcard, irp);
			break;

		case SCARD_IOCTL_LOCATECARDSBYATRA:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_LOCATECARDSBYATRW:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_READCACHEA:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_READCACHEW:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_WRITECACHEA:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_WRITECACHEW:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_GETTRANSMITCOUNT:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_RELEASESTARTEDEVENT:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_GETREADERICON:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_GETDEVICETYPEID:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		default:
			result = STATUS_UNSUCCESSFUL;
			break;
	}

	if ((result != SCARD_S_SUCCESS) /* && (result != SCARD_E_TIMEOUT) */)
	{
		WLog_Print(smartcard->log, WLOG_WARN,
			"IRP failure: %s (0x%08X), status: %s (0x%08X)",
			smartcard_get_ioctl_string(ioControlCode, TRUE), ioControlCode,
			SCardGetErrorString(result), result);
	}

	irp->IoStatus = 0;

	if ((result & 0xC0000000) == 0xC0000000)
	{
		/* NTSTATUS error */

		irp->IoStatus = result;
		Stream_SetPosition(irp->output, RDPDR_DEVICE_IO_RESPONSE_LENGTH);

		WLog_Print(smartcard->log, WLOG_WARN,
			"IRP failure: %s (0x%08X), ntstatus: 0x%08X",
			smartcard_get_ioctl_string(ioControlCode, TRUE), ioControlCode, result);
	}

	/**
	 * [MS-RPCE] 2.2.6.3 Primitive Type Serialization
	 * The type MUST be aligned on an 8-byte boundary. If the size of the
	 * primitive type is not a multiple of 8 bytes, the data MUST be padded.
	 */

	smartcard_pack_write_size_align(smartcard, irp->output,
			Stream_GetPosition(irp->output) - (RDPDR_DEVICE_IO_RESPONSE_LENGTH + 4), 8);

	Stream_SealLength(irp->output);

	outputBufferLength = Stream_Length(irp->output) - RDPDR_DEVICE_IO_RESPONSE_LENGTH - 4;
	objectBufferLength = outputBufferLength - RDPDR_DEVICE_IO_RESPONSE_LENGTH;
	Stream_SetPosition(irp->output, RDPDR_DEVICE_IO_RESPONSE_LENGTH);

	/* Device Control Response */
	Stream_Write_UINT32(irp->output, outputBufferLength); /* OutputBufferLength (4 bytes) */

	smartcard_pack_common_type_header(smartcard, irp->output); /* CommonTypeHeader (8 bytes) */
	smartcard_pack_private_type_header(smartcard, irp->output, objectBufferLength); /* PrivateTypeHeader (8 bytes) */

	Stream_Write_UINT32(irp->output, result); /* Result (4 bytes) */

	Stream_SetPosition(irp->output, Stream_Length(irp->output));

	smartcard_complete_irp(smartcard, irp);
}
