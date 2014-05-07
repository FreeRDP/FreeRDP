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

static UINT32 smartcard_EstablishContext(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	UINT32 status;
	SCARDCONTEXT hContext = -1;
	EstablishContext_Call call;
	EstablishContext_Return ret = { 0 };

	status = smartcard_unpack_establish_context_call(smartcard, irp->input, &call);

	smartcard_trace_establish_context_call(smartcard, &call);

	if (status)
		return status;

	status = ret.ReturnCode = SCardEstablishContext(call.dwScope, NULL, NULL, &hContext);

	if (ret.ReturnCode == SCARD_S_SUCCESS)
	{
		void* key = (void*) (size_t) hContext;
		ListDictionary_Add(smartcard->rgSCardContextList, key, NULL);
	}

	smartcard_scard_context_native_to_redir(smartcard, &(ret.hContext), hContext);

	smartcard_trace_establish_context_return(smartcard, &ret);

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

	smartcard_trace_context_call(smartcard, &call, "ReleaseContext");

	if (status)
		return status;

	hContext = smartcard_scard_context_native_from_redir(smartcard, &(call.hContext));

	status = ret.ReturnCode = SCardReleaseContext(hContext);

	if (ret.ReturnCode == SCARD_S_SUCCESS)
	{
		void* key = (void*) (size_t) hContext;
		ListDictionary_Remove(smartcard->rgSCardContextList, key);
	}

	smartcard_trace_long_return(smartcard, &ret, "ReleaseContext");

	return ret.ReturnCode;
}

static UINT32 smartcard_IsValidContext(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	UINT32 status;
	Context_Call call;
	Long_Return ret;
	SCARDCONTEXT hContext;

	status = smartcard_unpack_context_call(smartcard, irp->input, &call);

	smartcard_trace_context_call(smartcard, &call, "IsValidContext");

	if (status)
		return status;

	hContext = smartcard_scard_context_native_from_redir(smartcard, &(call.hContext));

	status = ret.ReturnCode = SCardIsValidContext(hContext);

	smartcard_trace_long_return(smartcard, &ret, "IsValidContext");

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

	smartcard_trace_list_readers_call(smartcard, &call, FALSE);

	if (status)
		return status;

	hContext = smartcard_scard_context_native_from_redir(smartcard, &(call.hContext));

	cchReaders = SCARD_AUTOALLOCATE;

	status = ret.ReturnCode = SCardListReadersA(hContext, (LPCSTR) call.mszGroups, (LPSTR) &mszReaders, &cchReaders);

	ret.msz = (BYTE*) mszReaders;
	ret.cBytes = cchReaders;

	if (status)
		return status;

	smartcard_trace_list_readers_return(smartcard, &ret, FALSE);

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

	smartcard_trace_list_readers_call(smartcard, &call, TRUE);

	if (status)
		return status;

	hContext = smartcard_scard_context_native_from_redir(smartcard, &(call.hContext));

	cchReaders = SCARD_AUTOALLOCATE;

	status = ret.ReturnCode = SCardListReadersW(hContext, (LPCWSTR) call.mszGroups, (LPWSTR) &mszReaders, &cchReaders);

	ret.msz = (BYTE*) mszReaders;
	ret.cBytes = cchReaders * 2;

	if (status)
		return status;

	smartcard_trace_list_readers_return(smartcard, &ret, TRUE);

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
	LPSCARD_READERSTATEA rgReaderState = NULL;

	status = smartcard_unpack_get_status_change_a_call(smartcard, irp->input, &call);

	smartcard_trace_get_status_change_a_call(smartcard, &call);

	if (status)
		return status;

	hContext = smartcard_scard_context_native_from_redir(smartcard, &(call.hContext));

	status = ret.ReturnCode = SCardGetStatusChangeA(hContext, call.dwTimeOut, call.rgReaderStates, call.cReaders);

	if (status && (status != SCARD_E_TIMEOUT) && (status != SCARD_E_CANCELLED))
		return status;

	ret.cReaders = call.cReaders;
	ret.rgReaderStates = (ReaderState_Return*) calloc(ret.cReaders, sizeof(ReaderState_Return));

	for (index = 0; index < ret.cReaders; index++)
	{
		ret.rgReaderStates[index].dwCurrentState = call.rgReaderStates[index].dwCurrentState;
		ret.rgReaderStates[index].dwEventState = call.rgReaderStates[index].dwEventState;
		ret.rgReaderStates[index].cbAtr = call.rgReaderStates[index].cbAtr;
		CopyMemory(&(ret.rgReaderStates[index].rgbAtr), &(call.rgReaderStates[index].rgbAtr), 32);
	}

	smartcard_trace_get_status_change_return(smartcard, &ret, FALSE);

	status = smartcard_pack_get_status_change_return(smartcard, irp->output, &ret);

	if (status)
		return status;

	if (call.rgReaderStates)
	{
		for (index = 0; index < call.cReaders; index++)
		{
			rgReaderState = &call.rgReaderStates[index];

			if (rgReaderState->szReader)
				free((void*) rgReaderState->szReader);
		}
		free(call.rgReaderStates);
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
	LPSCARD_READERSTATEW rgReaderState = NULL;

	status = smartcard_unpack_get_status_change_w_call(smartcard, irp->input, &call);

	smartcard_trace_get_status_change_w_call(smartcard, &call);

	if (status)
		return status;

	hContext = smartcard_scard_context_native_from_redir(smartcard, &(call.hContext));

	status = ret.ReturnCode = SCardGetStatusChangeW(hContext, call.dwTimeOut, call.rgReaderStates, call.cReaders);

	if (status && (status != SCARD_E_TIMEOUT) && (status != SCARD_E_CANCELLED))
		return status;

	ret.cReaders = call.cReaders;
	ret.rgReaderStates = (ReaderState_Return*) calloc(ret.cReaders, sizeof(ReaderState_Return));

	for (index = 0; index < ret.cReaders; index++)
	{
		ret.rgReaderStates[index].dwCurrentState = call.rgReaderStates[index].dwCurrentState;
		ret.rgReaderStates[index].dwEventState = call.rgReaderStates[index].dwEventState;
		ret.rgReaderStates[index].cbAtr = call.rgReaderStates[index].cbAtr;
		CopyMemory(&(ret.rgReaderStates[index].rgbAtr), &(call.rgReaderStates[index].rgbAtr), 32);
	}

	smartcard_trace_get_status_change_return(smartcard, &ret, TRUE);

	status = smartcard_pack_get_status_change_return(smartcard, irp->output, &ret);

	if (status)
		return status;

	if (call.rgReaderStates)
	{
		for (index = 0; index < call.cReaders; index++)
		{
			rgReaderState = &call.rgReaderStates[index];

			if (rgReaderState->szReader)
				free((void*) rgReaderState->szReader);
		}
		free(call.rgReaderStates);
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

	smartcard_trace_context_call(smartcard, &call, "Cancel");

	if (status)
		return status;

	hContext = smartcard_scard_context_native_from_redir(smartcard, &(call.hContext));

	status = ret.ReturnCode = SCardCancel(hContext);

	smartcard_trace_long_return(smartcard, &ret, "Cancel");

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

	smartcard_trace_connect_a_call(smartcard, &call);

	if (status)
		return status;

	hContext = smartcard_scard_context_native_from_redir(smartcard, &(call.Common.hContext));

	if ((call.Common.dwPreferredProtocols == SCARD_PROTOCOL_UNDEFINED) &&
			(call.Common.dwShareMode != SCARD_SHARE_DIRECT))
	{
		call.Common.dwPreferredProtocols = SCARD_PROTOCOL_Tx;
	}

	status = ret.ReturnCode = SCardConnectA(hContext, (char*) call.szReader, call.Common.dwShareMode,
			call.Common.dwPreferredProtocols, &hCard, &ret.dwActiveProtocol);

	if (status)
		return status;

	smartcard_scard_context_native_to_redir(smartcard, &(ret.hContext), hContext);
	smartcard_scard_handle_native_to_redir(smartcard, &(ret.hCard), hCard);

	smartcard_trace_connect_return(smartcard, &ret);

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

	smartcard_trace_connect_w_call(smartcard, &call);

	if (status)
		return status;

	hContext = smartcard_scard_context_native_from_redir(smartcard, &(call.Common.hContext));

	if ((call.Common.dwPreferredProtocols == SCARD_PROTOCOL_UNDEFINED) &&
			(call.Common.dwShareMode != SCARD_SHARE_DIRECT))
	{
		call.Common.dwPreferredProtocols = SCARD_PROTOCOL_Tx;
	}

	status = ret.ReturnCode = SCardConnectW(hContext, (WCHAR*) call.szReader, call.Common.dwShareMode,
			call.Common.dwPreferredProtocols, &hCard, &ret.dwActiveProtocol);

	if (status)
		return status;

	smartcard_scard_context_native_to_redir(smartcard, &(ret.hContext), hContext);
	smartcard_scard_handle_native_to_redir(smartcard, &(ret.hCard), hCard);

	smartcard_trace_connect_return(smartcard, &ret);

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

	smartcard_trace_reconnect_call(smartcard, &call);

	if (status)
		return status;

	hContext = smartcard_scard_context_native_from_redir(smartcard, &(call.hContext));
	hCard = smartcard_scard_handle_native_from_redir(smartcard, &(call.hCard));

	status = ret.ReturnCode = SCardReconnect(hCard, call.dwShareMode,
			call.dwPreferredProtocols, call.dwInitialization, &ret.dwActiveProtocol);

	if (status)
		return status;

	smartcard_trace_reconnect_return(smartcard, &ret);

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

	smartcard_trace_hcard_and_disposition_call(smartcard, &call, "Disconnect");

	if (status)
		return status;

	hContext = smartcard_scard_context_native_from_redir(smartcard, &(call.hContext));
	hCard = smartcard_scard_handle_native_from_redir(smartcard, &(call.hCard));

	status = ret.ReturnCode = SCardDisconnect(hCard, call.dwDisposition);

	smartcard_trace_long_return(smartcard, &ret, "Disconnect");

	if (status)
		return status;

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

	smartcard_trace_hcard_and_disposition_call(smartcard, &call, "BeginTransaction");

	if (status)
		return status;

	hContext = smartcard_scard_context_native_from_redir(smartcard, &(call.hContext));
	hCard = smartcard_scard_handle_native_from_redir(smartcard, &(call.hCard));

	status = ret.ReturnCode = SCardBeginTransaction(hCard);

	smartcard_trace_long_return(smartcard, &ret, "BeginTransaction");

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

	smartcard_trace_hcard_and_disposition_call(smartcard, &call, "EndTransaction");

	if (status)
		return status;

	hContext = smartcard_scard_context_native_from_redir(smartcard, &(call.hContext));
	hCard = smartcard_scard_handle_native_from_redir(smartcard, &(call.hCard));

	status = ret.ReturnCode = SCardEndTransaction(hCard, call.dwDisposition);

	smartcard_trace_long_return(smartcard, &ret, "EndTransaction");

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

	hContext = smartcard_scard_context_native_from_redir(smartcard, &(call.hContext));
	hCard = smartcard_scard_handle_native_from_redir(smartcard, &(call.hCard));

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
	Status_Return ret = { 0 };
	DWORD cchReaderLen = 0;
	LPSTR mszReaderNames = NULL;

	status = smartcard_unpack_status_call(smartcard, irp->input, &call);

	smartcard_trace_status_call(smartcard, &call, FALSE);

	if (status)
		return status;

	hContext = smartcard_scard_context_native_from_redir(smartcard, &(call.hContext));
	hCard = smartcard_scard_handle_native_from_redir(smartcard, &(call.hCard));

	if (call.cbAtrLen > 32)
		call.cbAtrLen = 32;

	ret.cbAtrLen = call.cbAtrLen;
	ZeroMemory(ret.pbAtr, 32);

	cchReaderLen = SCARD_AUTOALLOCATE;

	status = ret.ReturnCode = SCardStatusA(hCard, (LPSTR) &mszReaderNames, &cchReaderLen,
			&ret.dwState, &ret.dwProtocol, (BYTE*) &ret.pbAtr, &ret.cbAtrLen);

	if (status == SCARD_S_SUCCESS)
	{
		ret.mszReaderNames = (BYTE*) mszReaderNames;
		ret.cBytes = cchReaderLen;
	}

	smartcard_trace_status_return(smartcard, &ret, FALSE);

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

	smartcard_trace_status_call(smartcard, &call, TRUE);

	if (status)
		return status;

	hContext = smartcard_scard_context_native_from_redir(smartcard, &(call.hContext));
	hCard = smartcard_scard_handle_native_from_redir(smartcard, &(call.hCard));

	if (call.cbAtrLen > 32)
		call.cbAtrLen = 32;

	ret.cbAtrLen = call.cbAtrLen;
	ZeroMemory(ret.pbAtr, 32);

	cchReaderLen = SCARD_AUTOALLOCATE;

	status = ret.ReturnCode = SCardStatusW(hCard, (LPWSTR) &mszReaderNames, &cchReaderLen,
			&ret.dwState, &ret.dwProtocol, (BYTE*) &ret.pbAtr, &ret.cbAtrLen);

	ret.mszReaderNames = (BYTE*) mszReaderNames;
	ret.cBytes = cchReaderLen * 2;

	smartcard_trace_status_return(smartcard, &ret, TRUE);

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

	smartcard_trace_transmit_call(smartcard, &call);

	if (status)
		return status;

	hContext = smartcard_scard_context_native_from_redir(smartcard, &(call.hContext));
	hCard = smartcard_scard_handle_native_from_redir(smartcard, &(call.hCard));

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

	smartcard_trace_transmit_return(smartcard, &ret);

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

	status = smartcard_unpack_control_call(smartcard, irp->input, &call);

	smartcard_trace_control_call(smartcard, &call);

	if (status)
		return status;

	hContext = smartcard_scard_context_native_from_redir(smartcard, &(call.hContext));
	hCard = smartcard_scard_handle_native_from_redir(smartcard, &(call.hCard));

	ret.cbOutBufferSize = call.cbOutBufferSize;
	ret.pvOutBuffer = (BYTE*) malloc(call.cbOutBufferSize);

	status = ret.ReturnCode = SCardControl(hCard,
			call.dwControlCode, call.pvInBuffer, call.cbInBufferSize,
			ret.pvOutBuffer, call.cbOutBufferSize, &ret.cbOutBufferSize);

	if (status)
		return status;

	smartcard_trace_control_return(smartcard, &ret);

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

	smartcard_trace_get_attrib_call(smartcard, &call);

	if (status)
		return status;

	hContext = smartcard_scard_context_native_from_redir(smartcard, &(call.hContext));
	hCard = smartcard_scard_handle_native_from_redir(smartcard, &(call.hCard));

	ret.pbAttr = NULL;

	if (call.fpbAttrIsNULL)
		call.cbAttrLen = 0;

	if (call.cbAttrLen)
		ret.pbAttr = (BYTE*) malloc(call.cbAttrLen);

	cbAttrLen = call.cbAttrLen;

	status = ret.ReturnCode = SCardGetAttrib(hCard, call.dwAttrId, ret.pbAttr, &cbAttrLen);

	ret.cbAttrLen = cbAttrLen;

	smartcard_trace_get_attrib_return(smartcard, &ret, call.dwAttrId);

	if (ret.ReturnCode)
	{
		WLog_Print(smartcard->log, WLOG_WARN,
				"SCardGetAttrib: %s (0x%08X) cbAttrLen: %d\n",
				SCardGetAttributeString(call.dwAttrId), call.dwAttrId, call.cbAttrLen);

		Stream_Zero(irp->output, 256);
		return ret.ReturnCode;
	}

	status = smartcard_pack_get_attrib_return(smartcard, irp->output, &ret);

	if (status)
		return status;

	free(ret.pbAttr);

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
	UINT32 offset;
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

#if 0
	printf("%s (0x%08X) FileId: %d CompletionId: %d\n",
		smartcard_get_ioctl_string(ioControlCode, TRUE), ioControlCode, irp->FileId, irp->CompletionId);
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

	if ((ioControlCode != SCARD_IOCTL_ACCESSSTARTEDEVENT) &&
			(ioControlCode != SCARD_IOCTL_RELEASESTARTEDEVENT))
	{
		offset = (RDPDR_DEVICE_IO_REQUEST_LENGTH + RDPDR_DEVICE_IO_CONTROL_REQ_HDR_LENGTH);

		smartcard_unpack_read_size_align(smartcard, irp->input,
				Stream_GetPosition(irp->input) - offset, 8);
	}

	if ((result != SCARD_S_SUCCESS) && (result != SCARD_E_TIMEOUT) &&
			(result != SCARD_E_NO_READERS_AVAILABLE) && (result != SCARD_E_NO_SERVICE))
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

	if (((size_t) Stream_GetPosition(irp->input)) < Stream_Length(irp->input))
	{
		UINT32 difference;

		difference = (int) (Stream_Length(irp->input) - Stream_GetPosition(irp->input));

		WLog_Print(smartcard->log, WLOG_WARN,
			"IRP was not fully parsed %s (0x%08X): Actual: %d, Expected: %d, Difference: %d",
			smartcard_get_ioctl_string(ioControlCode, TRUE), ioControlCode,
			(int) Stream_GetPosition(irp->input), (int) Stream_Length(irp->input), difference);

		winpr_HexDump(Stream_Pointer(irp->input), difference);
	}

	if (((size_t) Stream_GetPosition(irp->input)) > Stream_Length(irp->input))
	{
		UINT32 difference;

		difference = (int) (Stream_GetPosition(irp->input) - Stream_Length(irp->input));

		WLog_Print(smartcard->log, WLOG_WARN,
			"IRP was parsed beyond its end %s (0x%08X): Actual: %d, Expected: %d, Difference: %d",
			smartcard_get_ioctl_string(ioControlCode, TRUE), ioControlCode,
			(int) Stream_GetPosition(irp->input), (int) Stream_Length(irp->input), difference);
	}

	/**
	 * [MS-RPCE] 2.2.6.3 Primitive Type Serialization
	 * The type MUST be aligned on an 8-byte boundary. If the size of the
	 * primitive type is not a multiple of 8 bytes, the data MUST be padded.
	 */

	if ((ioControlCode != SCARD_IOCTL_ACCESSSTARTEDEVENT) &&
			(ioControlCode != SCARD_IOCTL_RELEASESTARTEDEVENT))
	{
		offset = (RDPDR_DEVICE_IO_RESPONSE_LENGTH + RDPDR_DEVICE_IO_CONTROL_RSP_HDR_LENGTH);

		smartcard_pack_write_size_align(smartcard, irp->output,
				Stream_GetPosition(irp->output) - offset, 8);
	}

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
}
