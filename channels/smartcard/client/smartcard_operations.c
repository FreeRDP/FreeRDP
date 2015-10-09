/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Smartcard Device Service Virtual Channel
 *
 * Copyright (C) Alexi Volkov <alexi@myrealbox.com> 2006
 * Copyright 2011 O.S. Systems Software Ltda.
 * Copyright 2011 Anthony Tong <atong@trustedcs.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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

static LONG smartcard_EstablishContext_Decode(SMARTCARD_DEVICE* smartcard, SMARTCARD_OPERATION* operation, EstablishContext_Call* call)
{
	LONG status;
	IRP* irp = operation->irp;

	if (!call)
		return STATUS_NO_MEMORY;

	if ((status = smartcard_unpack_establish_context_call(smartcard, irp->input, call)))
	{
		WLog_ERR(TAG, "smartcard_unpack_establish_context_call failed with error %lu", status);
		return status;
	}
	smartcard_trace_establish_context_call(smartcard, call);
	return SCARD_S_SUCCESS;
}

static LONG smartcard_EstablishContext_Call(SMARTCARD_DEVICE* smartcard, SMARTCARD_OPERATION* operation, EstablishContext_Call* call)
{
	LONG status;
	SCARDCONTEXT hContext = -1;
	EstablishContext_Return ret;
	IRP* irp = operation->irp;
	status = ret.ReturnCode = SCardEstablishContext(call->dwScope, NULL, NULL, &hContext);

	if (ret.ReturnCode == SCARD_S_SUCCESS)
	{
		SMARTCARD_CONTEXT* pContext;
		void* key = (void*)(size_t) hContext;
		// TODO: handle return values
		pContext = smartcard_context_new(smartcard, hContext);
		if (!pContext)
		{
			WLog_ERR(TAG, "smartcard_context_new failed!");
			return STATUS_NO_MEMORY;
		}
		if (!ListDictionary_Add(smartcard->rgSCardContextList, key, (void*) pContext))
		{
			WLog_ERR(TAG, "ListDictionary_Add failed!");
			return STATUS_INTERNAL_ERROR;
		}
	}
	else
	{
		WLog_ERR(TAG, "SCardEstablishContext failed with error %lu", status);
		return status;
	}

	smartcard_scard_context_native_to_redir(smartcard, &(ret.hContext), hContext);
	smartcard_trace_establish_context_return(smartcard, &ret);
	if ((status = smartcard_pack_establish_context_return(smartcard, irp->output, &ret)))
	{
		WLog_ERR(TAG, "smartcard_pack_establish_context_return failed with error %lu", status);
		return status;
	}

	return ret.ReturnCode;
}

static LONG smartcard_ReleaseContext_Decode(SMARTCARD_DEVICE* smartcard, SMARTCARD_OPERATION* operation, Context_Call* call)
{
	LONG status;
	IRP* irp = operation->irp;

	if (!call)
		return STATUS_NO_MEMORY;

	if ((status = smartcard_unpack_context_call(smartcard, irp->input, call)))
		WLog_ERR(TAG, "smartcard_unpack_context_call failed with error %lu", status);
	smartcard_trace_context_call(smartcard, call, "ReleaseContext");
	operation->hContext = smartcard_scard_context_native_from_redir(smartcard, &(call->hContext));
	return status;
}

static LONG smartcard_ReleaseContext_Call(SMARTCARD_DEVICE* smartcard, SMARTCARD_OPERATION* operation, Context_Call* call)
{
	LONG status;
	Long_Return ret;

	status = ret.ReturnCode = SCardReleaseContext(operation->hContext);

	if (ret.ReturnCode == SCARD_S_SUCCESS)
	{
		SMARTCARD_CONTEXT* pContext;
		void* key = (void*)(size_t) operation->hContext;
		pContext = (SMARTCARD_CONTEXT*) ListDictionary_Remove(smartcard->rgSCardContextList, key);
		smartcard_context_free(pContext);
	}
	else
	{
		WLog_ERR(TAG, "SCardReleaseContext failed with error %lu", status);
		return status;
	}

	smartcard_trace_long_return(smartcard, &ret, "ReleaseContext");
	return ret.ReturnCode;
}

static LONG smartcard_IsValidContext_Decode(SMARTCARD_DEVICE* smartcard, SMARTCARD_OPERATION* operation, Context_Call* call)
{
	LONG status;
	IRP* irp = operation->irp;

	if (!call)
		return STATUS_NO_MEMORY;

	if ((status = smartcard_unpack_context_call(smartcard, irp->input, call)))
		WLog_ERR(TAG, "smartcard_unpack_context_call failed with error %lu", status);
	smartcard_trace_context_call(smartcard, call, "IsValidContext");
	operation->hContext = smartcard_scard_context_native_from_redir(smartcard, &(call->hContext));
	return status;
}

static LONG smartcard_IsValidContext_Call(SMARTCARD_DEVICE* smartcard, SMARTCARD_OPERATION* operation, Context_Call* call)
{
	LONG status;
	Long_Return ret;
	if ((status = ret.ReturnCode = SCardIsValidContext(operation->hContext)))
	{
		WLog_ERR(TAG, "SCardIsValidContext failed with error %lu", status);
		return status;
	}
	smartcard_trace_long_return(smartcard, &ret, "IsValidContext");
	return ret.ReturnCode;
}

static LONG smartcard_ListReadersA_Decode(SMARTCARD_DEVICE* smartcard, SMARTCARD_OPERATION* operation, ListReaders_Call* call)
{
	LONG status;
	IRP* irp = operation->irp;

	if (!call)
		return STATUS_NO_MEMORY;

	if ((status = smartcard_unpack_list_readers_call(smartcard, irp->input, call)))
		WLog_ERR(TAG, "smartcard_unpack_list_readers_call failed with error %lu", status);
	smartcard_trace_list_readers_call(smartcard, call, FALSE);
	operation->hContext = smartcard_scard_context_native_from_redir(smartcard, &(call->hContext));
	return status;
}

static LONG smartcard_ListReadersA_Call(SMARTCARD_DEVICE* smartcard, SMARTCARD_OPERATION* operation, ListReaders_Call* call)
{
	LONG status;
	ListReaders_Return ret;
	LPSTR mszReaders = NULL;
	DWORD cchReaders = 0;
	IRP* irp = operation->irp;

	cchReaders = SCARD_AUTOALLOCATE;

	status = ret.ReturnCode = SCardListReadersA(operation->hContext, (LPCSTR) call->mszGroups, (LPSTR) &mszReaders, &cchReaders);

	ret.msz = (BYTE*) mszReaders;
	ret.cBytes = cchReaders;

	if (call->mszGroups)
	{
		free(call->mszGroups);
		call->mszGroups = NULL;
	}

	if (status)
	{
		WLog_ERR(TAG, "SCardListReadersA failed with error %lu", status);
		return status;
	}

	smartcard_trace_list_readers_return(smartcard, &ret, FALSE);
	if ((status = smartcard_pack_list_readers_return(smartcard, irp->output, &ret)))
	{
		WLog_ERR(TAG, "smartcard_pack_list_readers_return failed with error %lu", status);
		return status;
	}

	if (mszReaders)
		SCardFreeMemory(operation->hContext, mszReaders);

	if (status != SCARD_S_SUCCESS)
		return status;

	return ret.ReturnCode;
}

static LONG smartcard_ListReadersW_Decode(SMARTCARD_DEVICE* smartcard, SMARTCARD_OPERATION* operation, ListReaders_Call* call)
{
	LONG status;
	IRP* irp = operation->irp;

	if (!call)
		return STATUS_NO_MEMORY;

	if ((status = smartcard_unpack_list_readers_call(smartcard, irp->input, call)))
		WLog_ERR(TAG, "smartcard_unpack_list_readers_call failed with error %lu", status);

	smartcard_trace_list_readers_call(smartcard, call, TRUE);
	operation->hContext = smartcard_scard_context_native_from_redir(smartcard, &(call->hContext));

	return status;
}

static LONG smartcard_ListReadersW_Call(SMARTCARD_DEVICE* smartcard, SMARTCARD_OPERATION* operation, ListReaders_Call* call)
{
	LONG status;
	ListReaders_Return ret;
	LPWSTR mszReaders = NULL;
	DWORD cchReaders = 0;
	IRP* irp = operation->irp;

	cchReaders = SCARD_AUTOALLOCATE;

	status = ret.ReturnCode = SCardListReadersW(operation->hContext,
			(LPCWSTR) call->mszGroups, (LPWSTR) &mszReaders, &cchReaders);

	ret.msz = (BYTE*) mszReaders;
	ret.cBytes = cchReaders * 2;

	if (call->mszGroups)
	{
		free(call->mszGroups);
		call->mszGroups = NULL;
	}

	if (status)
	{
		WLog_ERR(TAG, "SCardListReadersW failed with error %lu", status);
		return status;
	}

	smartcard_trace_list_readers_return(smartcard, &ret, TRUE);

	if ((status = smartcard_pack_list_readers_return(smartcard, irp->output, &ret)))
	{
		WLog_ERR(TAG, "smartcard_pack_list_readers_return failed with error %lu", status);
		return status;
	}

	if (mszReaders)
		SCardFreeMemory(operation->hContext, mszReaders);

	if (status != SCARD_S_SUCCESS)
		return status;

	return ret.ReturnCode;
}

static LONG smartcard_GetStatusChangeA_Decode(SMARTCARD_DEVICE* smartcard, SMARTCARD_OPERATION* operation, GetStatusChangeA_Call* call)
{
	LONG status;
	IRP* irp = operation->irp;

	if (!call)
		return STATUS_NO_MEMORY;

	if ((status = smartcard_unpack_get_status_change_a_call(smartcard, irp->input, call)))
	{
		WLog_ERR(TAG, "smartcard_unpack_get_status_change_a_call failed with error %lu", status);
		return status;
	}

	smartcard_trace_get_status_change_a_call(smartcard, call);
	operation->hContext = smartcard_scard_context_native_from_redir(smartcard, &(call->hContext));
	return status;
}

static LONG smartcard_GetStatusChangeA_Call(SMARTCARD_DEVICE* smartcard, SMARTCARD_OPERATION* operation, GetStatusChangeA_Call* call)
{
	LONG status;
	UINT32 index;
	GetStatusChange_Return ret;
	LPSCARD_READERSTATEA rgReaderState = NULL;
	IRP* irp = operation->irp;

	status = ret.ReturnCode = SCardGetStatusChangeA(operation->hContext,
			call->dwTimeOut, call->rgReaderStates, call->cReaders);

	if (status && (status != SCARD_E_TIMEOUT) && (status != SCARD_E_CANCELLED))
	{
		call->cReaders = 0;
	}

	ret.cReaders = call->cReaders;
	ret.rgReaderStates = NULL;
	if (ret.cReaders > 0)
		ret.rgReaderStates = (ReaderState_Return*) calloc(ret.cReaders, sizeof(ReaderState_Return));

	if (!ret.rgReaderStates)
		return STATUS_NO_MEMORY;

	for (index = 0; index < ret.cReaders; index++)
	{
		ret.rgReaderStates[index].dwCurrentState = call->rgReaderStates[index].dwCurrentState;
		ret.rgReaderStates[index].dwEventState = call->rgReaderStates[index].dwEventState;
		ret.rgReaderStates[index].cbAtr = call->rgReaderStates[index].cbAtr;
		CopyMemory(&(ret.rgReaderStates[index].rgbAtr), &(call->rgReaderStates[index].rgbAtr), 32);
	}

	smartcard_trace_get_status_change_return(smartcard, &ret, FALSE);
	if ((status = smartcard_pack_get_status_change_return(smartcard, irp->output, &ret)))
	{
		WLog_ERR(TAG, "smartcard_pack_get_status_change_return failed with error %lu", status);
		return status;
	}

	if (call->rgReaderStates)
	{
		for (index = 0; index < call->cReaders; index++)
		{
			rgReaderState = &call->rgReaderStates[index];
			free((void *)rgReaderState->szReader);
		}

		free(call->rgReaderStates);
	}

	free(ret.rgReaderStates);
	return ret.ReturnCode;
}

static LONG smartcard_GetStatusChangeW_Decode(SMARTCARD_DEVICE* smartcard, SMARTCARD_OPERATION* operation, GetStatusChangeW_Call* call)
{
	LONG status;
	IRP* irp = operation->irp;

	if (!call)
		return STATUS_NO_MEMORY;

	if ((status = smartcard_unpack_get_status_change_w_call(smartcard, irp->input, call)))
		WLog_ERR(TAG, "smartcard_unpack_get_status_change_w_call failed with error %lu", status);
	smartcard_trace_get_status_change_w_call(smartcard, call);
	operation->hContext = smartcard_scard_context_native_from_redir(smartcard, &(call->hContext));
	return status;
}

static LONG smartcard_GetStatusChangeW_Call(SMARTCARD_DEVICE* smartcard, SMARTCARD_OPERATION* operation, GetStatusChangeW_Call* call)
{
	LONG status;
	UINT32 index;
	GetStatusChange_Return ret;
	LPSCARD_READERSTATEW rgReaderState = NULL;
	IRP* irp = operation->irp;
	status = ret.ReturnCode = SCardGetStatusChangeW(operation->hContext, call->dwTimeOut, call->rgReaderStates, call->cReaders);

	if (status && (status != SCARD_E_TIMEOUT) && (status != SCARD_E_CANCELLED))
	{
		call->cReaders=0;
	}

	ret.cReaders = call->cReaders;
	ret.rgReaderStates = NULL;
	if (ret.cReaders > 0)
		ret.rgReaderStates = (ReaderState_Return*) calloc(ret.cReaders, sizeof(ReaderState_Return));

	if (!ret.rgReaderStates)
		return STATUS_NO_MEMORY;

	for (index = 0; index < ret.cReaders; index++)
	{
		ret.rgReaderStates[index].dwCurrentState = call->rgReaderStates[index].dwCurrentState;
		ret.rgReaderStates[index].dwEventState = call->rgReaderStates[index].dwEventState;
		ret.rgReaderStates[index].cbAtr = call->rgReaderStates[index].cbAtr;
		CopyMemory(&(ret.rgReaderStates[index].rgbAtr), &(call->rgReaderStates[index].rgbAtr), 32);
	}

	smartcard_trace_get_status_change_return(smartcard, &ret, TRUE);
	if ((status = smartcard_pack_get_status_change_return(smartcard, irp->output, &ret)))
	{
		WLog_ERR(TAG, "smartcard_pack_get_status_change_return failed with error %lu", status);
		return status;
	}

	if (call->rgReaderStates)
	{
		for (index = 0; index < call->cReaders; index++)
		{
			rgReaderState = &call->rgReaderStates[index];
			free((void *)rgReaderState->szReader);
		}

		free(call->rgReaderStates);
	}

	free(ret.rgReaderStates);
	return ret.ReturnCode;
}

static LONG smartcard_Cancel_Decode(SMARTCARD_DEVICE* smartcard, SMARTCARD_OPERATION* operation, Context_Call* call)
{
	LONG status;
	IRP* irp = operation->irp;

	if (!call)
		return STATUS_NO_MEMORY;

	if ((status = smartcard_unpack_context_call(smartcard, irp->input, call)))
		WLog_ERR(TAG, "smartcard_unpack_context_call failed with error %lu", status);
	smartcard_trace_context_call(smartcard, call, "Cancel");
	operation->hContext = smartcard_scard_context_native_from_redir(smartcard, &(call->hContext));
	return status;
}

static LONG smartcard_Cancel_Call(SMARTCARD_DEVICE* smartcard, SMARTCARD_OPERATION* operation, Context_Call* call)
{
	LONG status;
	Long_Return ret;

	if ((status = ret.ReturnCode = SCardCancel(operation->hContext)))
	{
		WLog_ERR(TAG, "SCardCancel failed with error %lu", status);
		return status;
	}
	smartcard_trace_long_return(smartcard, &ret, "Cancel");
	return ret.ReturnCode;
}

static LONG smartcard_ConnectA_Decode(SMARTCARD_DEVICE* smartcard, SMARTCARD_OPERATION* operation, ConnectA_Call* call)
{
	LONG status;
	IRP* irp = operation->irp;

	if (!call)
		return STATUS_NO_MEMORY;

	if ((status = smartcard_unpack_connect_a_call(smartcard, irp->input, call)))
		WLog_ERR(TAG, "smartcard_unpack_connect_a_call failed with error %lu", status);
	smartcard_trace_connect_a_call(smartcard, call);
	operation->hContext = smartcard_scard_context_native_from_redir(smartcard, &(call->Common.hContext));
	return status;
}

static LONG smartcard_ConnectA_Call(SMARTCARD_DEVICE* smartcard, SMARTCARD_OPERATION* operation, ConnectA_Call* call)
{
	LONG status;
	SCARDHANDLE hCard = 0;
	Connect_Return ret = { 0 };
	IRP* irp = operation->irp;

	if ((call->Common.dwPreferredProtocols == SCARD_PROTOCOL_UNDEFINED) &&
			(call->Common.dwShareMode != SCARD_SHARE_DIRECT))
	{
		call->Common.dwPreferredProtocols = SCARD_PROTOCOL_Tx;
	}

	status = ret.ReturnCode = SCardConnectA(operation->hContext, (char*) call->szReader, call->Common.dwShareMode,
						call->Common.dwPreferredProtocols, &hCard, &ret.dwActiveProtocol);

	smartcard_scard_context_native_to_redir(smartcard, &(ret.hContext), operation->hContext);
	smartcard_scard_handle_native_to_redir(smartcard, &(ret.hCard), hCard);
	smartcard_trace_connect_return(smartcard, &ret);

	if (status)
	{
		WLog_ERR(TAG, "SCardConnectA failed with error %lu", status);
		return status;
	}


	if ((status = smartcard_pack_connect_return(smartcard, irp->output, &ret)))
	{
		WLog_ERR(TAG, "smartcard_pack_connect_return failed with error %lu", status);
		return status;
	}

	free(call->szReader);

	return ret.ReturnCode;
}

static LONG smartcard_ConnectW_Decode(SMARTCARD_DEVICE* smartcard, SMARTCARD_OPERATION* operation, ConnectW_Call* call)
{
	LONG status;
	IRP* irp = operation->irp;

	if (!call)
		return STATUS_NO_MEMORY;

	if ((status = smartcard_unpack_connect_w_call(smartcard, irp->input, call)))
		WLog_ERR(TAG, "smartcard_unpack_connect_w_call failed with error %lu", status);

	smartcard_trace_connect_w_call(smartcard, call);
	operation->hContext = smartcard_scard_context_native_from_redir(smartcard, &(call->Common.hContext));

	return status;
}

static LONG smartcard_ConnectW_Call(SMARTCARD_DEVICE* smartcard, SMARTCARD_OPERATION* operation, ConnectW_Call* call)
{
	LONG status;
	SCARDHANDLE hCard = 0;
	Connect_Return ret = { 0 };
	IRP* irp = operation->irp;

	if ((call->Common.dwPreferredProtocols == SCARD_PROTOCOL_UNDEFINED) &&
			(call->Common.dwShareMode != SCARD_SHARE_DIRECT))
	{
		call->Common.dwPreferredProtocols = SCARD_PROTOCOL_Tx;
	}

	status = ret.ReturnCode = SCardConnectW(operation->hContext, (WCHAR*) call->szReader, call->Common.dwShareMode,
							call->Common.dwPreferredProtocols, &hCard, &ret.dwActiveProtocol);

	smartcard_scard_context_native_to_redir(smartcard, &(ret.hContext), operation->hContext);
	smartcard_scard_handle_native_to_redir(smartcard, &(ret.hCard), hCard);
	smartcard_trace_connect_return(smartcard, &ret);

	if (status)
	{
		WLog_ERR(TAG, "SCardConnectW failed with error %lu", status);
		return status;
	}

	if ((status = smartcard_pack_connect_return(smartcard, irp->output, &ret)))
	{
		WLog_ERR(TAG, "smartcard_pack_connect_return failed with error %lu", status);
		return status;
	}

	free(call->szReader);

	return ret.ReturnCode;
}

static LONG smartcard_Reconnect_Decode(SMARTCARD_DEVICE* smartcard, SMARTCARD_OPERATION* operation, Reconnect_Call* call)
{
	LONG status;
	IRP* irp = operation->irp;

	if (!call)
		return STATUS_NO_MEMORY;

	if ((status = smartcard_unpack_reconnect_call(smartcard, irp->input, call)))
		WLog_ERR(TAG, "smartcard_unpack_reconnect_call failed with error %lu", status);
	smartcard_trace_reconnect_call(smartcard, call);
	operation->hContext = smartcard_scard_context_native_from_redir(smartcard, &(call->hContext));
	operation->hCard = smartcard_scard_handle_native_from_redir(smartcard, &(call->hCard));
	return status;
}

static LONG smartcard_Reconnect_Call(SMARTCARD_DEVICE* smartcard, SMARTCARD_OPERATION* operation, Reconnect_Call* call)
{
	LONG status;
	Reconnect_Return ret;
	IRP* irp = operation->irp;
	status = ret.ReturnCode = SCardReconnect(operation->hCard, call->dwShareMode,
							  call->dwPreferredProtocols, call->dwInitialization, &ret.dwActiveProtocol);
	smartcard_trace_reconnect_return(smartcard, &ret);
	if ((status = smartcard_pack_reconnect_return(smartcard, irp->output, &ret)))
	{
		WLog_ERR(TAG, "smartcard_pack_reconnect_return failed with error %lu", status);
		return status;
	}

	return ret.ReturnCode;
}

static LONG smartcard_Disconnect_Decode(SMARTCARD_DEVICE* smartcard, SMARTCARD_OPERATION* operation, HCardAndDisposition_Call* call)
{
	LONG status;
	IRP* irp = operation->irp;

	if (!call)
		return STATUS_NO_MEMORY;

	if ((status = smartcard_unpack_hcard_and_disposition_call(smartcard, irp->input, call)))
		WLog_ERR(TAG, "smartcard_unpack_hcard_and_disposition_call failed with error %lu", status);
	smartcard_trace_hcard_and_disposition_call(smartcard, call, "Disconnect");
	operation->hContext = smartcard_scard_context_native_from_redir(smartcard, &(call->hContext));
	operation->hCard = smartcard_scard_handle_native_from_redir(smartcard, &(call->hCard));
	return status;
}

static LONG smartcard_Disconnect_Call(SMARTCARD_DEVICE* smartcard, SMARTCARD_OPERATION* operation, HCardAndDisposition_Call* call)
{
	LONG status;
	Long_Return ret;
	if ((status = ret.ReturnCode = SCardDisconnect(operation->hCard, call->dwDisposition)))
	{
		WLog_ERR(TAG, "SCardDisconnect failed with error %lu", status);
		return status;
	}
	smartcard_trace_long_return(smartcard, &ret, "Disconnect");

	return ret.ReturnCode;
}

static LONG smartcard_BeginTransaction_Decode(SMARTCARD_DEVICE* smartcard, SMARTCARD_OPERATION* operation, HCardAndDisposition_Call* call)
{
	LONG status;
	IRP* irp = operation->irp;

	if (!call)
		return STATUS_NO_MEMORY;

	if ((status = smartcard_unpack_hcard_and_disposition_call(smartcard, irp->input, call)))
		WLog_ERR(TAG, "smartcard_unpack_hcard_and_disposition_call failed with error %lu", status);
	smartcard_trace_hcard_and_disposition_call(smartcard, call, "BeginTransaction");
	operation->hContext = smartcard_scard_context_native_from_redir(smartcard, &(call->hContext));
	operation->hCard = smartcard_scard_handle_native_from_redir(smartcard, &(call->hCard));
	return status;
}

static LONG smartcard_BeginTransaction_Call(SMARTCARD_DEVICE* smartcard, SMARTCARD_OPERATION* operation, HCardAndDisposition_Call* call)
{
	Long_Return ret;
	if ((ret.ReturnCode = SCardBeginTransaction(operation->hCard)))
	{
		WLog_ERR(TAG, "SCardBeginTransaction failed with error %lu", ret.ReturnCode);
		return ret.ReturnCode;
	}
	smartcard_trace_long_return(smartcard, &ret, "BeginTransaction");
	return ret.ReturnCode;
}

static LONG smartcard_EndTransaction_Decode(SMARTCARD_DEVICE* smartcard, SMARTCARD_OPERATION* operation, HCardAndDisposition_Call* call)
{
	LONG status;
	IRP* irp = operation->irp;

	if (!call)
		return STATUS_NO_MEMORY;

	if ((status = smartcard_unpack_hcard_and_disposition_call(smartcard, irp->input, call)))
		WLog_ERR(TAG, "smartcard_unpack_hcard_and_disposition_call failed with error %lu", status);
	smartcard_trace_hcard_and_disposition_call(smartcard, call, "EndTransaction");
	operation->hContext = smartcard_scard_context_native_from_redir(smartcard, &(call->hContext));
	operation->hCard = smartcard_scard_handle_native_from_redir(smartcard, &(call->hCard));
	return status;
}

static LONG smartcard_EndTransaction_Call(SMARTCARD_DEVICE* smartcard, SMARTCARD_OPERATION* operation, HCardAndDisposition_Call* call)
{
	Long_Return ret;
	if ((ret.ReturnCode = SCardEndTransaction(operation->hCard, call->dwDisposition)))
	{
		WLog_ERR(TAG, "SCardEndTransaction failed with error %lu", ret.ReturnCode);
		return ret.ReturnCode;
	}
	smartcard_trace_long_return(smartcard, &ret, "EndTransaction");
	return ret.ReturnCode;
}

static LONG smartcard_State_Decode(SMARTCARD_DEVICE* smartcard, SMARTCARD_OPERATION* operation, State_Call* call)
{
	LONG status;
	IRP* irp = operation->irp;

	if (!call)
		return STATUS_NO_MEMORY;

	if ((status = smartcard_unpack_state_call(smartcard, irp->input, call)))
		WLog_ERR(TAG, "smartcard_unpack_state_call failed with error %lu", status);
	operation->hContext = smartcard_scard_context_native_from_redir(smartcard, &(call->hContext));
	operation->hCard = smartcard_scard_handle_native_from_redir(smartcard, &(call->hCard));
	return status;
}

static LONG smartcard_State_Call(SMARTCARD_DEVICE* smartcard, SMARTCARD_OPERATION* operation, State_Call* call)
{
	LONG status;
	State_Return ret;
	IRP* irp = operation->irp;
	ret.cbAtrLen = SCARD_ATR_LENGTH;
	ret.ReturnCode = SCardState(operation->hCard, &ret.dwState, &ret.dwProtocol, (BYTE*) &ret.rgAtr, &ret.cbAtrLen);
	if ((status = smartcard_pack_state_return(smartcard, irp->output, &ret)))
	{
		WLog_ERR(TAG, "smartcard_pack_state_return failed with error %lu", status);
		return status;
	}

	return ret.ReturnCode;
}

static LONG smartcard_StatusA_Decode(SMARTCARD_DEVICE* smartcard, SMARTCARD_OPERATION* operation, Status_Call* call)
{
	LONG status;
	IRP* irp = operation->irp;

	if (!call)
		return STATUS_NO_MEMORY;

	if ((status = smartcard_unpack_status_call(smartcard, irp->input, call)))
		WLog_ERR(TAG, "smartcard_unpack_status_call failed with error %lu", status);
	smartcard_trace_status_call(smartcard, call, FALSE);
	operation->hContext = smartcard_scard_context_native_from_redir(smartcard, &(call->hContext));
	operation->hCard = smartcard_scard_handle_native_from_redir(smartcard, &(call->hCard));
	return status;
}

static LONG smartcard_StatusA_Call(SMARTCARD_DEVICE* smartcard, SMARTCARD_OPERATION* operation, Status_Call* call)
{
	LONG status;
	Status_Return ret = { 0 };
	DWORD cchReaderLen = 0;
	LPSTR mszReaderNames = NULL;
	IRP* irp = operation->irp;

	ret.cbAtrLen = 32;
	ZeroMemory(ret.pbAtr, 32);
	cchReaderLen = SCARD_AUTOALLOCATE;

	status = ret.ReturnCode = SCardStatusA(operation->hCard, (LPSTR) &mszReaderNames, &cchReaderLen,
					&ret.dwState, &ret.dwProtocol, (BYTE*) &ret.pbAtr, &ret.cbAtrLen);

	if (status == SCARD_S_SUCCESS)
	{
		ret.mszReaderNames = (BYTE*) mszReaderNames;
		ret.cBytes = cchReaderLen;
	}

	smartcard_trace_status_return(smartcard, &ret, FALSE);
	if ((status = smartcard_pack_status_return(smartcard, irp->output, &ret)))
	{
		WLog_ERR(TAG, "smartcard_pack_status_return failed with error %lu", status);
		return status;
	}

	if (mszReaderNames)
		SCardFreeMemory(operation->hContext, mszReaderNames);

	return ret.ReturnCode;
}

static LONG smartcard_StatusW_Decode(SMARTCARD_DEVICE* smartcard, SMARTCARD_OPERATION* operation, Status_Call* call)
{
	LONG status;
	IRP* irp = operation->irp;

	if (!call)
		return STATUS_NO_MEMORY;

	if ((status = smartcard_unpack_status_call(smartcard, irp->input, call)))
		WLog_ERR(TAG, "smartcard_unpack_status_call failed with error %lu", status);
	smartcard_trace_status_call(smartcard, call, TRUE);
	operation->hContext = smartcard_scard_context_native_from_redir(smartcard, &(call->hContext));
	operation->hCard = smartcard_scard_handle_native_from_redir(smartcard, &(call->hCard));
	return status;
}

static LONG smartcard_StatusW_Call(SMARTCARD_DEVICE* smartcard, SMARTCARD_OPERATION* operation, Status_Call* call)
{
	LONG status;
	Status_Return ret;
	DWORD cchReaderLen = 0;
	LPWSTR mszReaderNames = NULL;
	IRP* irp = operation->irp;

	ret.cbAtrLen = 32;
	ZeroMemory(ret.pbAtr, 32);
	cchReaderLen = SCARD_AUTOALLOCATE;

	status = ret.ReturnCode = SCardStatusW(operation->hCard, (LPWSTR) &mszReaderNames, &cchReaderLen,
						&ret.dwState, &ret.dwProtocol, (BYTE*) &ret.pbAtr, &ret.cbAtrLen);

	ret.mszReaderNames = (BYTE*) mszReaderNames;
	ret.cBytes = cchReaderLen * 2;
	smartcard_trace_status_return(smartcard, &ret, TRUE);
	if ((status = smartcard_pack_status_return(smartcard, irp->output, &ret)))
	{
		WLog_ERR(TAG, "smartcard_pack_status_return failed with error %lu", status);
		return status;
	}

	if (mszReaderNames)
		SCardFreeMemory(operation->hContext, mszReaderNames);

	return ret.ReturnCode;
}

static LONG smartcard_Transmit_Decode(SMARTCARD_DEVICE* smartcard, SMARTCARD_OPERATION* operation, Transmit_Call* call)
{
	LONG status;
	IRP* irp = operation->irp;

	if (!call)
		return STATUS_NO_MEMORY;

	if ((status = smartcard_unpack_transmit_call(smartcard, irp->input, call)))
		WLog_ERR(TAG, "smartcard_unpack_transmit_call failed with error %lu", status);

	smartcard_trace_transmit_call(smartcard, call);

	operation->hContext = smartcard_scard_context_native_from_redir(smartcard, &(call->hContext));
	operation->hCard = smartcard_scard_handle_native_from_redir(smartcard, &(call->hCard));

	return status;
}

static LONG smartcard_Transmit_Call(SMARTCARD_DEVICE* smartcard, SMARTCARD_OPERATION* operation, Transmit_Call* call)
{
	LONG status;
	Transmit_Return ret;
	IRP* irp = operation->irp;
	ret.cbRecvLength = 0;
	ret.pbRecvBuffer = NULL;

	if (call->cbRecvLength && !call->fpbRecvBufferIsNULL)
	{
		if (call->cbRecvLength >= 66560)
			call->cbRecvLength = 66560;

		ret.cbRecvLength = call->cbRecvLength;
		ret.pbRecvBuffer = (BYTE*) malloc(ret.cbRecvLength);

		if (!ret.pbRecvBuffer)
			return STATUS_NO_MEMORY;
	}

	ret.pioRecvPci = call->pioRecvPci;

	ret.ReturnCode = SCardTransmit(operation->hCard, call->pioSendPci, call->pbSendBuffer,
				call->cbSendLength, ret.pioRecvPci, ret.pbRecvBuffer, &(ret.cbRecvLength));

	smartcard_trace_transmit_return(smartcard, &ret);
	if ((status = smartcard_pack_transmit_return(smartcard, irp->output, &ret)))
	{
		WLog_ERR(TAG, "smartcard_pack_transmit_return failed with error %lu", status);
		return status;
	}

	free(call->pbSendBuffer);
	free(ret.pbRecvBuffer);
	free(call->pioSendPci);
	free(call->pioRecvPci);

	return ret.ReturnCode;
}

static LONG smartcard_Control_Decode(SMARTCARD_DEVICE* smartcard, SMARTCARD_OPERATION* operation, Control_Call* call)
{
	LONG status;
	IRP* irp = operation->irp;

	if (!call)
		return STATUS_NO_MEMORY;

	if ((status = smartcard_unpack_control_call(smartcard, irp->input, call)))
		WLog_ERR(TAG, "smartcard_unpack_control_call failed with error %lu", status);
	smartcard_trace_control_call(smartcard, call);
	operation->hContext = smartcard_scard_context_native_from_redir(smartcard, &(call->hContext));
	operation->hCard = smartcard_scard_handle_native_from_redir(smartcard, &(call->hCard));
	return status;
}

static LONG smartcard_Control_Call(SMARTCARD_DEVICE* smartcard, SMARTCARD_OPERATION* operation, Control_Call* call)
{
	LONG status;
	Control_Return ret;
	IRP* irp = operation->irp;
	ret.cbOutBufferSize = call->cbOutBufferSize;
	ret.pvOutBuffer = (BYTE*) malloc(call->cbOutBufferSize);

	if (!ret.pvOutBuffer)
		return SCARD_E_NO_MEMORY;

	status = ret.ReturnCode = SCardControl(operation->hCard,
			call->dwControlCode, call->pvInBuffer, call->cbInBufferSize,
			ret.pvOutBuffer, call->cbOutBufferSize, &ret.cbOutBufferSize);

	smartcard_trace_control_return(smartcard, &ret);
	if ((status = smartcard_pack_control_return(smartcard, irp->output, &ret)))
	{
		WLog_ERR(TAG, "smartcard_pack_control_return failed with error %lu", status);
		return status;
	}

	free(call->pvInBuffer);
	free(ret.pvOutBuffer);

	return ret.ReturnCode;
}

static LONG smartcard_GetAttrib_Decode(SMARTCARD_DEVICE* smartcard, SMARTCARD_OPERATION* operation, GetAttrib_Call* call)
{
	LONG status;
	IRP* irp = operation->irp;

	if (!call)
		return STATUS_NO_MEMORY;

	if ((status = smartcard_unpack_get_attrib_call(smartcard, irp->input, call)))
		WLog_ERR(TAG, "smartcard_unpack_get_attrib_call failed with error %lu", status);
	smartcard_trace_get_attrib_call(smartcard, call);
	operation->hContext = smartcard_scard_context_native_from_redir(smartcard, &(call->hContext));
	operation->hCard = smartcard_scard_handle_native_from_redir(smartcard, &(call->hCard));
	return status;
}

static LONG smartcard_GetAttrib_Call(SMARTCARD_DEVICE* smartcard, SMARTCARD_OPERATION* operation, GetAttrib_Call* call)
{
	LONG status;
	DWORD cbAttrLen;
	BOOL autoAllocate;
	GetAttrib_Return ret;
	IRP* irp = operation->irp;

	ret.pbAttr = NULL;

	if (call->fpbAttrIsNULL)
		call->cbAttrLen = 0;

	autoAllocate = (call->cbAttrLen == SCARD_AUTOALLOCATE) ? TRUE : FALSE;

	if (call->cbAttrLen && !autoAllocate)
	{
		ret.pbAttr = (BYTE*) malloc(call->cbAttrLen);

		if (!ret.pbAttr)
			return SCARD_E_NO_MEMORY;
	}

	cbAttrLen = call->cbAttrLen;

	status = ret.ReturnCode = SCardGetAttrib(operation->hCard, call->dwAttrId,
			autoAllocate ? (LPBYTE) &(ret.pbAttr) : ret.pbAttr, &cbAttrLen);

	ret.cbAttrLen = cbAttrLen;

	smartcard_trace_get_attrib_return(smartcard, &ret, call->dwAttrId);

	if (ret.ReturnCode)
	{
		WLog_WARN(TAG, "SCardGetAttrib: %s (0x%08X) cbAttrLen: %d",
				SCardGetAttributeString(call->dwAttrId), call->dwAttrId, call->cbAttrLen);
		Stream_Zero(irp->output, 256);

		free(ret.pbAttr);
		return ret.ReturnCode;
	}

	if ((status = smartcard_pack_get_attrib_return(smartcard, irp->output, &ret)))
	{
		WLog_ERR(TAG, "smartcard_pack_get_attrib_return failed with error %lu", status);
		return status;
	}

	free(ret.pbAttr);
	return ret.ReturnCode;
}

static LONG smartcard_AccessStartedEvent_Decode(SMARTCARD_DEVICE* smartcard, SMARTCARD_OPERATION* operation, Long_Call* call)
{
	IRP* irp = operation->irp;

	if (!call)
		return STATUS_NO_MEMORY;

	if (Stream_GetRemainingLength(irp->input) < 4)
	{
		WLog_WARN(TAG, "AccessStartedEvent is too short: %d",
				   (int) Stream_GetRemainingLength(irp->input));
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Read_UINT32(irp->input, call->LongValue); /* Unused (4 bytes) */
	return SCARD_S_SUCCESS;
}

static LONG smartcard_AccessStartedEvent_Call(SMARTCARD_DEVICE* smartcard, SMARTCARD_OPERATION* operation, Long_Call* call)
{
	if (!smartcard->StartedEvent)
		smartcard->StartedEvent = SCardAccessStartedEvent();

	if (!smartcard->StartedEvent)
		return SCARD_E_NO_SERVICE;

	return SCARD_S_SUCCESS;
}

static LONG smartcard_LocateCardsByATRA_Decode(SMARTCARD_DEVICE* smartcard, SMARTCARD_OPERATION* operation, LocateCardsByATRA_Call* call)
{
	LONG status;
	IRP* irp = operation->irp;

	if (!call)
		return STATUS_NO_MEMORY;

	if ((status = smartcard_unpack_locate_cards_by_atr_a_call(smartcard, irp->input, call)))
		WLog_ERR(TAG, "smartcard_unpack_locate_cards_by_atr_a_call failed with error %lu", status);
	smartcard_trace_locate_cards_by_atr_a_call(smartcard, call);
	operation->hContext = smartcard_scard_context_native_from_redir(smartcard, &(call->hContext));
	return status;
}

static LONG smartcard_LocateCardsByATRA_Call(SMARTCARD_DEVICE* smartcard, SMARTCARD_OPERATION* operation, LocateCardsByATRA_Call* call)
{
	LONG status;
	BOOL equal;
	DWORD i, j, k;
	GetStatusChange_Return ret;
	LPSCARD_READERSTATEA state = NULL;
	LPSCARD_READERSTATEA states = NULL;
	IRP* irp = operation->irp;

	states = (LPSCARD_READERSTATEA) calloc(call->cReaders, sizeof(SCARD_READERSTATEA));

	if (!states)
		return STATUS_NO_MEMORY;

	for (i = 0; i < call->cReaders; i++)
	{
		states[i].szReader = (LPCSTR) call->rgReaderStates[i].szReader;
		states[i].dwCurrentState = call->rgReaderStates[i].Common.dwCurrentState;
		states[i].dwEventState = call->rgReaderStates[i].Common.dwEventState;
		states[i].cbAtr = call->rgReaderStates[i].Common.cbAtr;
		CopyMemory(&(states[i].rgbAtr), &(call->rgReaderStates[i].Common.rgbAtr), 36);
	}

	status = ret.ReturnCode = SCardGetStatusChangeA(operation->hContext, 0x000001F4, states, call->cReaders);

	if (status && (status != SCARD_E_TIMEOUT) && (status != SCARD_E_CANCELLED))
	{
		call->cReaders = 0;
	}

	for (i = 0; i < call->cAtrs; i++)
	{
		for (j = 0; j < call->cReaders; j++)
		{
			equal = TRUE;

			for (k = 0; k < call->rgAtrMasks[i].cbAtr; k++)
			{
				if ((call->rgAtrMasks[i].rgbAtr[k] & call->rgAtrMasks[i].rgbMask[k]) !=
				    (states[j].rgbAtr[k] & call->rgAtrMasks[i].rgbMask[k]))
				{
					equal = FALSE;
					break;
				}
				if (equal)
				{
					states[j].dwEventState |= SCARD_STATE_ATRMATCH;
				}
			}
		}
	}

	ret.cReaders = call->cReaders;
	ret.rgReaderStates = NULL;
	if (ret.cReaders > 0)
		ret.rgReaderStates = (ReaderState_Return*) calloc(ret.cReaders, sizeof(ReaderState_Return));

	if (!ret.rgReaderStates)
		return STATUS_NO_MEMORY;

	for (i = 0; i < ret.cReaders; i++)
	{
		state = &states[i];
		ret.rgReaderStates[i].dwCurrentState = state->dwCurrentState;
		ret.rgReaderStates[i].dwEventState = state->dwEventState;
		ret.rgReaderStates[i].cbAtr = state->cbAtr;
		CopyMemory(&(ret.rgReaderStates[i].rgbAtr), &(state->rgbAtr), 32);
	}
	free(states);

	smartcard_trace_get_status_change_return(smartcard, &ret, FALSE);
	if ((status = smartcard_pack_get_status_change_return(smartcard, irp->output, &ret)))
	{
		WLog_ERR(TAG, "smartcard_pack_get_status_change_return failed with error %lu", status);
		return status;
	}

	if (call->rgReaderStates)
	{
		for (i = 0; i < call->cReaders; i++)
		{
			state = (LPSCARD_READERSTATEA) &call->rgReaderStates[i];

			if (state->szReader) {
				free((void*) state->szReader);
				state->szReader = NULL;
			}
		}

		free(call->rgReaderStates);
		call->rgReaderStates = NULL;
	}

	free(ret.rgReaderStates);
	return ret.ReturnCode;
}

LONG smartcard_irp_device_control_decode(SMARTCARD_DEVICE* smartcard, SMARTCARD_OPERATION* operation)
{
	LONG status;
	UINT32 offset;
	void* call = NULL;
	UINT32 ioControlCode;
	UINT32 outputBufferLength;
	UINT32 inputBufferLength;
	IRP* irp = operation->irp;

	/* Device Control Request */

	if (Stream_GetRemainingLength(irp->input) < 32)
	{
		WLog_WARN(TAG, "Device Control Request is too short: %d",
				   (int) Stream_GetRemainingLength(irp->input));
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Read_UINT32(irp->input, outputBufferLength); /* OutputBufferLength (4 bytes) */
	Stream_Read_UINT32(irp->input, inputBufferLength); /* InputBufferLength (4 bytes) */
	Stream_Read_UINT32(irp->input, ioControlCode); /* IoControlCode (4 bytes) */
	Stream_Seek(irp->input, 20); /* Padding (20 bytes) */
	operation->ioControlCode = ioControlCode;

	if (Stream_Length(irp->input) != (Stream_GetPosition(irp->input) + inputBufferLength))
	{
		WLog_WARN(TAG, "InputBufferLength mismatch: Actual: %d Expected: %d",
				Stream_Length(irp->input), Stream_GetPosition(irp->input) + inputBufferLength);
		return SCARD_F_INTERNAL_ERROR;
	}

	WLog_DBG(TAG, "%s (0x%08X) FileId: %d CompletionId: %d",
		smartcard_get_ioctl_string(ioControlCode, TRUE),
		ioControlCode, irp->FileId, irp->CompletionId);

	if ((ioControlCode != SCARD_IOCTL_ACCESSSTARTEDEVENT) &&
			(ioControlCode != SCARD_IOCTL_RELEASESTARTEDEVENT))
	{
		if ((status = smartcard_unpack_common_type_header(smartcard, irp->input)))
		{
			WLog_ERR(TAG, "smartcard_unpack_common_type_header failed with error %lu", status);
			return SCARD_F_INTERNAL_ERROR;
		}

		if ((status = smartcard_unpack_private_type_header(smartcard, irp->input)))
		{
			WLog_ERR(TAG, "smartcard_unpack_common_type_header failed with error %lu", status);
			return SCARD_F_INTERNAL_ERROR;
		}
	}

	/* Decode */

	switch (ioControlCode)
	{
		case SCARD_IOCTL_ESTABLISHCONTEXT:
			if (!(call = calloc(1, sizeof(EstablishContext_Call))))
			{
				WLog_ERR(TAG, "calloc failed!");
				return SCARD_E_NO_MEMORY;
			}
			status = smartcard_EstablishContext_Decode(smartcard, operation, (EstablishContext_Call*) call);
			break;

		case SCARD_IOCTL_RELEASECONTEXT:
			if (!(call = calloc(1, sizeof(Context_Call))))
			{
				WLog_ERR(TAG, "calloc failed!");
				return SCARD_E_NO_MEMORY;
			}
			status = smartcard_ReleaseContext_Decode(smartcard, operation, (Context_Call*) call);
			break;

		case SCARD_IOCTL_ISVALIDCONTEXT:
			if (!(call = calloc(1, sizeof(Context_Call))))
			{
				WLog_ERR(TAG, "calloc failed!");
				return SCARD_E_NO_MEMORY;
			}
			status = smartcard_IsValidContext_Decode(smartcard, operation, (Context_Call*) call);
			break;

		case SCARD_IOCTL_LISTREADERGROUPSA:
			status = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_LISTREADERGROUPSW:
			status = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_LISTREADERSA:
			if (!(call = calloc(1, sizeof(ListReaders_Call))))
			{
				WLog_ERR(TAG, "calloc failed!");
				return SCARD_E_NO_MEMORY;
			}
			status = smartcard_ListReadersA_Decode(smartcard, operation, (ListReaders_Call*) call);
			break;

		case SCARD_IOCTL_LISTREADERSW:
			if (!(call = calloc(1, sizeof(ListReaders_Call))))
			{
				WLog_ERR(TAG, "calloc failed!");
				return SCARD_E_NO_MEMORY;
			}
			status = smartcard_ListReadersW_Decode(smartcard, operation, (ListReaders_Call*) call);
			break;

		case SCARD_IOCTL_INTRODUCEREADERGROUPA:
			status = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_INTRODUCEREADERGROUPW:
			status = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_FORGETREADERGROUPA:
			status = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_FORGETREADERGROUPW:
			status = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_INTRODUCEREADERA:
			status = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_INTRODUCEREADERW:
			status = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_FORGETREADERA:
			status = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_FORGETREADERW:
			status = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_ADDREADERTOGROUPA:
			status = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_ADDREADERTOGROUPW:
			status = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_REMOVEREADERFROMGROUPA:
			status = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_REMOVEREADERFROMGROUPW:
			status = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_LOCATECARDSA:
			status = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_LOCATECARDSW:
			status = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_GETSTATUSCHANGEA:
			if (!(call = calloc(1, sizeof(GetStatusChangeA_Call))))
			{
				WLog_ERR(TAG, "calloc failed!");
				return SCARD_E_NO_MEMORY;
			}
			status = smartcard_GetStatusChangeA_Decode(smartcard, operation, (GetStatusChangeA_Call*) call);
			break;

		case SCARD_IOCTL_GETSTATUSCHANGEW:
			if (!(call = calloc(1, sizeof(GetStatusChangeW_Call))))
			{
				WLog_ERR(TAG, "calloc failed!");
				return SCARD_E_NO_MEMORY;
			}
			status = smartcard_GetStatusChangeW_Decode(smartcard, operation, (GetStatusChangeW_Call*) call);
			break;

		case SCARD_IOCTL_CANCEL:
			if (!(call = calloc(1, sizeof(Context_Call))))
			{
				WLog_ERR(TAG, "calloc failed!");
				return SCARD_E_NO_MEMORY;
			}
			status = smartcard_Cancel_Decode(smartcard, operation, (Context_Call*) call);
			break;

		case SCARD_IOCTL_CONNECTA:
			if (!(call = calloc(1, sizeof(ConnectA_Call))))
			{
				WLog_ERR(TAG, "calloc failed!");
				return SCARD_E_NO_MEMORY;
			}
			status = smartcard_ConnectA_Decode(smartcard, operation, (ConnectA_Call*) call);
			break;

		case SCARD_IOCTL_CONNECTW:
			if (!(call = calloc(1, sizeof(ConnectW_Call))))
			{
				WLog_ERR(TAG, "calloc failed!");
				return SCARD_E_NO_MEMORY;
			}
			status = smartcard_ConnectW_Decode(smartcard, operation, (ConnectW_Call*) call);
			break;

		case SCARD_IOCTL_RECONNECT:
			if (!(call = calloc(1, sizeof(Reconnect_Call))))
			{
				WLog_ERR(TAG, "calloc failed!");
				return SCARD_E_NO_MEMORY;
			}
			status = smartcard_Reconnect_Decode(smartcard, operation, (Reconnect_Call*) call);
			break;

		case SCARD_IOCTL_DISCONNECT:
			if (!(call = calloc(1, sizeof(HCardAndDisposition_Call))))
			{
				WLog_ERR(TAG, "calloc failed!");
				return SCARD_E_NO_MEMORY;
			}
			status = smartcard_Disconnect_Decode(smartcard, operation, (HCardAndDisposition_Call*) call);
			break;

		case SCARD_IOCTL_BEGINTRANSACTION:
			if (!(call = calloc(1, sizeof(HCardAndDisposition_Call))))
			{
				WLog_ERR(TAG, "calloc failed!");
				return SCARD_E_NO_MEMORY;
			}
			status = smartcard_BeginTransaction_Decode(smartcard, operation, (HCardAndDisposition_Call*) call);
			break;

		case SCARD_IOCTL_ENDTRANSACTION:
			if (!(call = calloc(1, sizeof(HCardAndDisposition_Call))))
			{
				WLog_ERR(TAG, "calloc failed!");
				return SCARD_E_NO_MEMORY;
			}
			status = smartcard_EndTransaction_Decode(smartcard, operation, (HCardAndDisposition_Call*) call);
			break;

		case SCARD_IOCTL_STATE:
			if (!(call = calloc(1, sizeof(State_Call))))
			{
				WLog_ERR(TAG, "calloc failed!");
				return SCARD_E_NO_MEMORY;
			}
			status = smartcard_State_Decode(smartcard, operation, (State_Call*) call);
			break;

		case SCARD_IOCTL_STATUSA:
			if (!(call = calloc(1, sizeof(Status_Call))))
			{
				WLog_ERR(TAG, "calloc failed!");
				return SCARD_E_NO_MEMORY;
			}
			status = smartcard_StatusA_Decode(smartcard, operation, (Status_Call*) call);
			break;

		case SCARD_IOCTL_STATUSW:
			if (!(call = calloc(1, sizeof(Status_Call))))
			{
				WLog_ERR(TAG, "calloc failed!");
				return SCARD_E_NO_MEMORY;
			}
			status = smartcard_StatusW_Decode(smartcard, operation, (Status_Call*) call);
			break;

		case SCARD_IOCTL_TRANSMIT:
			if (!(call = calloc(1, sizeof(Transmit_Call))))
			{
				WLog_ERR(TAG, "calloc failed!");
				return SCARD_E_NO_MEMORY;
			}
			status = smartcard_Transmit_Decode(smartcard, operation, (Transmit_Call*) call);
			break;

		case SCARD_IOCTL_CONTROL:
			if (!(call = calloc(1, sizeof(Control_Call))))
			{
				WLog_ERR(TAG, "calloc failed!");
				return SCARD_E_NO_MEMORY;
			}
			status = smartcard_Control_Decode(smartcard, operation, (Control_Call*) call);
			break;

		case SCARD_IOCTL_GETATTRIB:
			if (!(call = calloc(1, sizeof(GetAttrib_Call))))
			{
				WLog_ERR(TAG, "calloc failed!");
				return SCARD_E_NO_MEMORY;
			}
			status = smartcard_GetAttrib_Decode(smartcard, operation, (GetAttrib_Call*) call);
			break;

		case SCARD_IOCTL_SETATTRIB:
			status = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_ACCESSSTARTEDEVENT:
			if (!(call = calloc(1, sizeof(Long_Call))))
			{
				WLog_ERR(TAG, "calloc failed!");
				return SCARD_E_NO_MEMORY;
			}
			status = smartcard_AccessStartedEvent_Decode(smartcard, operation, (Long_Call*) call);
			break;

		case SCARD_IOCTL_LOCATECARDSBYATRA:
			if (!(call = calloc(1, sizeof(LocateCardsByATRA_Call))))
			{
				WLog_ERR(TAG, "calloc failed!");
				return SCARD_E_NO_MEMORY;
			}
			status = smartcard_LocateCardsByATRA_Decode(smartcard, operation, (LocateCardsByATRA_Call*) call);
			break;

		case SCARD_IOCTL_LOCATECARDSBYATRW:
			status = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_READCACHEA:
			status = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_READCACHEW:
			status = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_WRITECACHEA:
			status = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_WRITECACHEW:
			status = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_GETTRANSMITCOUNT:
			status = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_RELEASESTARTEDEVENT:
			status = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_GETREADERICON:
			status = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_GETDEVICETYPEID:
			status = SCARD_F_INTERNAL_ERROR;
			break;

		default:
			status = SCARD_F_INTERNAL_ERROR;
			break;
	}

	if ((ioControlCode != SCARD_IOCTL_ACCESSSTARTEDEVENT) &&
			(ioControlCode != SCARD_IOCTL_RELEASESTARTEDEVENT))
	{
		offset = (RDPDR_DEVICE_IO_REQUEST_LENGTH + RDPDR_DEVICE_IO_CONTROL_REQ_HDR_LENGTH);
		smartcard_unpack_read_size_align(smartcard, irp->input,
					Stream_GetPosition(irp->input) - offset, 8);
	}

	if (((size_t) Stream_GetPosition(irp->input)) < Stream_Length(irp->input))
	{
		UINT32 difference;
		difference = (int)(Stream_Length(irp->input) - Stream_GetPosition(irp->input));
		WLog_WARN(TAG, "IRP was not fully parsed %s (0x%08X): Actual: %d, Expected: %d, Difference: %d",
				smartcard_get_ioctl_string(ioControlCode, TRUE), ioControlCode,
				(int) Stream_GetPosition(irp->input), (int) Stream_Length(irp->input), difference);
		winpr_HexDump(TAG, WLOG_WARN, Stream_Pointer(irp->input), difference);
	}

	if (((size_t) Stream_GetPosition(irp->input)) > Stream_Length(irp->input))
	{
		UINT32 difference;
		difference = (int)(Stream_GetPosition(irp->input) - Stream_Length(irp->input));
		WLog_WARN(TAG, "IRP was parsed beyond its end %s (0x%08X): Actual: %d, Expected: %d, Difference: %d",
				smartcard_get_ioctl_string(ioControlCode, TRUE), ioControlCode,
				(int) Stream_GetPosition(irp->input), (int) Stream_Length(irp->input), difference);
	}

	if (status != SCARD_S_SUCCESS)
	{
		free(call);
		call = NULL;
	}

	operation->call = call;
	return status;
}

LONG smartcard_irp_device_control_call(SMARTCARD_DEVICE* smartcard, SMARTCARD_OPERATION* operation)
{
	IRP* irp;
	LONG result;
	UINT32 offset;
	ULONG_PTR* call;
	UINT32 ioControlCode;
	UINT32 outputBufferLength;
	UINT32 objectBufferLength;
	irp = operation->irp;
	call = operation->call;
	ioControlCode = operation->ioControlCode;

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

	/* Call */

	switch (ioControlCode)
	{
		case SCARD_IOCTL_ESTABLISHCONTEXT:
			result = smartcard_EstablishContext_Call(smartcard, operation, (EstablishContext_Call*) call);
			break;

		case SCARD_IOCTL_RELEASECONTEXT:
			result = smartcard_ReleaseContext_Call(smartcard, operation, (Context_Call*) call);
			break;

		case SCARD_IOCTL_ISVALIDCONTEXT:
			result = smartcard_IsValidContext_Call(smartcard, operation, (Context_Call*) call);
			break;

		case SCARD_IOCTL_LISTREADERGROUPSA:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_LISTREADERGROUPSW:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_LISTREADERSA:
			result = smartcard_ListReadersA_Call(smartcard, operation, (ListReaders_Call*) call);
			break;

		case SCARD_IOCTL_LISTREADERSW:
			result = smartcard_ListReadersW_Call(smartcard, operation, (ListReaders_Call*) call);
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
			result = smartcard_GetStatusChangeA_Call(smartcard, operation, (GetStatusChangeA_Call*) call);
			break;

		case SCARD_IOCTL_GETSTATUSCHANGEW:
			result = smartcard_GetStatusChangeW_Call(smartcard, operation, (GetStatusChangeW_Call*) call);
			break;

		case SCARD_IOCTL_CANCEL:
			result = smartcard_Cancel_Call(smartcard, operation, (Context_Call*) call);
			break;

		case SCARD_IOCTL_CONNECTA:
			result = smartcard_ConnectA_Call(smartcard, operation, (ConnectA_Call*) call);
			break;

		case SCARD_IOCTL_CONNECTW:
			result = smartcard_ConnectW_Call(smartcard, operation, (ConnectW_Call*) call);
			break;

		case SCARD_IOCTL_RECONNECT:
			result = smartcard_Reconnect_Call(smartcard, operation, (Reconnect_Call*) call);
			break;

		case SCARD_IOCTL_DISCONNECT:
			result = smartcard_Disconnect_Call(smartcard, operation, (HCardAndDisposition_Call*) call);
			break;

		case SCARD_IOCTL_BEGINTRANSACTION:
			result = smartcard_BeginTransaction_Call(smartcard, operation, (HCardAndDisposition_Call*) call);
			break;

		case SCARD_IOCTL_ENDTRANSACTION:
			result = smartcard_EndTransaction_Call(smartcard, operation, (HCardAndDisposition_Call*) call);
			break;

		case SCARD_IOCTL_STATE:
			result = smartcard_State_Call(smartcard, operation, (State_Call*) call);
			break;

		case SCARD_IOCTL_STATUSA:
			result = smartcard_StatusA_Call(smartcard, operation, (Status_Call*) call);
			break;

		case SCARD_IOCTL_STATUSW:
			result = smartcard_StatusW_Call(smartcard, operation, (Status_Call*) call);
			break;

		case SCARD_IOCTL_TRANSMIT:
			result = smartcard_Transmit_Call(smartcard, operation, (Transmit_Call*) call);
			break;

		case SCARD_IOCTL_CONTROL:
			result = smartcard_Control_Call(smartcard, operation, (Control_Call*) call);
			break;

		case SCARD_IOCTL_GETATTRIB:
			result = smartcard_GetAttrib_Call(smartcard, operation, (GetAttrib_Call*) call);
			break;

		case SCARD_IOCTL_SETATTRIB:
			result = SCARD_F_INTERNAL_ERROR;
			break;

		case SCARD_IOCTL_ACCESSSTARTEDEVENT:
			result = smartcard_AccessStartedEvent_Call(smartcard, operation, (Long_Call*) call);
			break;

		case SCARD_IOCTL_LOCATECARDSBYATRA:
			result = smartcard_LocateCardsByATRA_Call(smartcard, operation, (LocateCardsByATRA_Call*) call);
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

	free(call);

	/**
	 * [MS-RPCE] 2.2.6.3 Primitive Type Serialization
	 * The type MUST be aligned on an 8-byte boundary. If the size of the
	 * primitive type is not a multiple of 8 bytes, the data MUST be padded.
	 */

	if ((ioControlCode != SCARD_IOCTL_ACCESSSTARTEDEVENT) &&
			(ioControlCode != SCARD_IOCTL_RELEASESTARTEDEVENT))
	{
		offset = (RDPDR_DEVICE_IO_RESPONSE_LENGTH + RDPDR_DEVICE_IO_CONTROL_RSP_HDR_LENGTH);
		smartcard_pack_write_size_align(smartcard, irp->output, Stream_GetPosition(irp->output) - offset, 8);
	}

	if ((result != SCARD_S_SUCCESS) && (result != SCARD_E_TIMEOUT) &&
			(result != SCARD_E_NO_READERS_AVAILABLE) && (result != SCARD_E_NO_SERVICE))
	{
		WLog_WARN(TAG, "IRP failure: %s (0x%08X), status: %s (0x%08X)",
			smartcard_get_ioctl_string(ioControlCode, TRUE), ioControlCode,
			SCardGetErrorString(result), result);
	}

	irp->IoStatus = 0;

	if ((result & 0xC0000000) == 0xC0000000)
	{
		/* NTSTATUS error */
		irp->IoStatus = (UINT32)result;
		Stream_SetPosition(irp->output, RDPDR_DEVICE_IO_RESPONSE_LENGTH);
		WLog_WARN(TAG, "IRP failure: %s (0x%08X), ntstatus: 0x%08X",
			smartcard_get_ioctl_string(ioControlCode, TRUE), ioControlCode, result);
	}

	Stream_SealLength(irp->output);
	outputBufferLength = Stream_Length(irp->output) - RDPDR_DEVICE_IO_RESPONSE_LENGTH - 4;
	objectBufferLength = outputBufferLength - RDPDR_DEVICE_IO_RESPONSE_LENGTH;
	Stream_SetPosition(irp->output, RDPDR_DEVICE_IO_RESPONSE_LENGTH);

	/* Device Control Response */
	Stream_Write_UINT32(irp->output, outputBufferLength); /* OutputBufferLength (4 bytes) */
	if ((result = smartcard_pack_common_type_header(smartcard, irp->output))) /* CommonTypeHeader (8 bytes) */
	{
		WLog_ERR(TAG, "smartcard_pack_common_type_header failed with error %lu", result);
		return result;
	}
	if ((result = smartcard_pack_private_type_header(smartcard, irp->output, objectBufferLength))) /* PrivateTypeHeader (8 bytes) */
	{
		WLog_ERR(TAG, "smartcard_pack_private_type_header failed with error %lu", result);
		return result;
	}

	Stream_Write_UINT32(irp->output, result); /* Result (4 bytes) */
	Stream_SetPosition(irp->output, Stream_Length(irp->output));

	return SCARD_S_SUCCESS;
}

