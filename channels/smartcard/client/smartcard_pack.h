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

#ifndef FREERDP_CHANNEL_SMARTCARD_CLIENT_PACK_H
#define FREERDP_CHANNEL_SMARTCARD_CLIENT_PACK_H

#include <winpr/crt.h>
#include <winpr/stream.h>
#include <winpr/smartcard.h>

#include "smartcard_main.h"

/* interface type_scard_pack */
/* [unique][version][uuid] */

typedef struct _REDIR_SCARDCONTEXT
{
	/* [range] */ DWORD cbContext;
	/* [size_is][unique] */ BYTE pbContext[8];
} REDIR_SCARDCONTEXT;

typedef struct _REDIR_SCARDHANDLE
{
	/* [range] */ DWORD cbHandle;
	/* [size_is] */ BYTE pbHandle[8];
} REDIR_SCARDHANDLE;

typedef struct _Long_Return
{
	LONG ReturnCode;
} 	Long_Return;

typedef struct _longAndMultiString_Return
{
	LONG ReturnCode;
	/* [range] */ DWORD cBytes;
	/* [size_is][unique] */ BYTE *msz;
} ListReaderGroups_Return;

typedef struct _longAndMultiString_Return ListReaders_Return;

typedef struct _Context_Call
{
	REDIR_SCARDCONTEXT hContext;
} Context_Call;

typedef struct _ContextAndStringA_Call
{
	REDIR_SCARDCONTEXT hContext;
	/* [string] */ unsigned char *sz;
} ContextAndStringA_Call;

typedef struct _ContextAndStringW_Call
{
	REDIR_SCARDCONTEXT hContext;
	/* [string] */ WCHAR *sz;
} ContextAndStringW_Call;

typedef struct _ContextAndTwoStringA_Call
{
	REDIR_SCARDCONTEXT hContext;
	/* [string] */ unsigned char *sz1;
	/* [string] */ unsigned char *sz2;
} ContextAndTwoStringA_Call;

typedef struct _ContextAndTwoStringW_Call
{
	REDIR_SCARDCONTEXT hContext;
	/* [string] */ WCHAR *sz1;
	/* [string] */ WCHAR *sz2;
} ContextAndTwoStringW_Call;

typedef struct _EstablishContext_Call
{
	DWORD dwScope;
} EstablishContext_Call;

typedef struct _EstablishContext_Return
{
	LONG ReturnCode;
	REDIR_SCARDCONTEXT hContext;
} EstablishContext_Return;

typedef struct _ListReaderGroups_Call
{
	REDIR_SCARDCONTEXT hContext;
	LONG fmszGroupsIsNULL;
	DWORD cchGroups;
} ListReaderGroups_Call;

typedef struct _ListReaders_Call
{
	REDIR_SCARDCONTEXT hContext;
	/* [range] */ DWORD cBytes;
	/* [size_is][unique] */ BYTE *mszGroups;
	LONG fmszReadersIsNULL;
	DWORD cchReaders;
} ListReaders_Call;

typedef struct _ReaderState_Common_Call
{
	DWORD dwCurrentState;
	DWORD dwEventState;
	/* [range] */ DWORD cbAtr;
	BYTE rgbAtr[36];
} ReaderState_Common_Call;

typedef struct _ReaderStateA
{
	/* [string] */ unsigned char *szReader;
	ReaderState_Common_Call Common;
} ReaderStateA;

typedef struct _ReaderStateW
{
	/* [string] */ WCHAR *szReader;
	ReaderState_Common_Call Common;
} ReaderStateW;

typedef struct _ReaderState_Return
{
	DWORD dwCurrentState;
	DWORD dwEventState;
	/* [range] */ DWORD cbAtr;
	BYTE rgbAtr[36];
} ReaderState_Return;

typedef struct _GetStatusChangeA_Call
{
	REDIR_SCARDCONTEXT hContext;
	DWORD dwTimeOut;
	/* [range] */ DWORD cReaders;
	/* [size_is] */ LPSCARD_READERSTATEA rgReaderStates;
} GetStatusChangeA_Call;

typedef struct _LocateCardsA_Call
{
	REDIR_SCARDCONTEXT hContext;
	/* [range] */ DWORD cBytes;
	/* [size_is] */ BYTE *mszCards;
	/* [range] */ DWORD cReaders;
	/* [size_is] */ ReaderStateA *rgReaderStates;
} LocateCardsA_Call;

typedef struct _LocateCardsW_Call
{
	REDIR_SCARDCONTEXT hContext;
	/* [range] */ DWORD cBytes;
	/* [size_is] */ BYTE *mszCards;
	/* [range] */ DWORD cReaders;
	/* [size_is] */ ReaderStateW *rgReaderStates;
} LocateCardsW_Call;

typedef struct _LocateCards_ATRMask
{
	/* [range] */ DWORD cbAtr;
	BYTE rgbAtr[ 36 ];
	BYTE rgbMask[ 36 ];
} LocateCards_ATRMask;

typedef struct _LocateCardsByATRA_Call
{
	REDIR_SCARDCONTEXT hContext;
	/* [range] */ DWORD cAtrs;
	/* [size_is] */ LocateCards_ATRMask *rgAtrMasks;
	/* [range] */ DWORD cReaders;
	/* [size_is] */ ReaderStateA *rgReaderStates;
} LocateCardsByATRA_Call;

typedef struct _LocateCardsByATRW_Call
{
	REDIR_SCARDCONTEXT hContext;
	/* [range] */ DWORD cAtrs;
	/* [size_is] */ LocateCards_ATRMask *rgAtrMasks;
	/* [range] */ DWORD cReaders;
	/* [size_is] */ ReaderStateW *rgReaderStates;
} LocateCardsByATRW_Call;

typedef struct _GetStatusChange_Return
{
	LONG ReturnCode;
	/* [range] */ DWORD cReaders;
	/* [size_is] */ ReaderState_Return *rgReaderStates;
} LocateCards_Return;

typedef struct _GetStatusChange_Return GetStatusChange_Return;

typedef struct _GetStatusChangeW_Call
{
	REDIR_SCARDCONTEXT hContext;
	DWORD dwTimeOut;
	/* [range] */ DWORD cReaders;
	/* [size_is] */ LPSCARD_READERSTATEW rgReaderStates;
} GetStatusChangeW_Call;

typedef struct _Connect_Common
{
	REDIR_SCARDCONTEXT hContext;
	DWORD dwShareMode;
	DWORD dwPreferredProtocols;
} Connect_Common;

typedef struct _ConnectA_Call
{
	/* [string] */ unsigned char *szReader;
	Connect_Common Common;
} ConnectA_Call;

typedef struct _ConnectW_Call
{
	/* [string] */ WCHAR *szReader;
	Connect_Common Common;
} ConnectW_Call;

typedef struct _Connect_Return
{
	LONG ReturnCode;
	REDIR_SCARDCONTEXT hContext;
	REDIR_SCARDHANDLE hCard;
	DWORD dwActiveProtocol;
} Connect_Return;

typedef struct _Reconnect_Call
{
	REDIR_SCARDCONTEXT hContext;
	REDIR_SCARDHANDLE hCard;
	DWORD dwShareMode;
	DWORD dwPreferredProtocols;
	DWORD dwInitialization;
} Reconnect_Call;

typedef struct Reconnect_Return
{
	LONG ReturnCode;
	DWORD dwActiveProtocol;
} Reconnect_Return;

typedef struct _HCardAndDisposition_Call
{
	REDIR_SCARDCONTEXT hContext;
	REDIR_SCARDHANDLE hCard;
	DWORD dwDisposition;
} HCardAndDisposition_Call;

typedef struct _State_Call
{
	REDIR_SCARDCONTEXT hContext;
	REDIR_SCARDHANDLE hCard;
	LONG fpbAtrIsNULL;
	DWORD cbAtrLen;
} State_Call;

typedef struct _State_Return
{
	LONG ReturnCode;
	DWORD dwState;
	DWORD dwProtocol;
	/* [range] */ DWORD cbAtrLen;
	/* [size_is][unique] */ BYTE rgAtr[36];
} State_Return;

typedef struct _Status_Call
{
	REDIR_SCARDCONTEXT hContext;
	REDIR_SCARDHANDLE hCard;
	LONG fmszReaderNamesIsNULL;
	DWORD cchReaderLen;
	DWORD cbAtrLen;
} Status_Call;

typedef struct _Status_Return
{
	LONG ReturnCode;
	/* [range] */ DWORD cBytes;
	/* [size_is][unique] */ BYTE *mszReaderNames;
	DWORD dwState;
	DWORD dwProtocol;
	BYTE pbAtr[32];
	/* [range] */ DWORD cbAtrLen;
} Status_Return;

typedef struct _SCardIO_Request
{
	DWORD dwProtocol;
	/* [range] */ DWORD cbExtraBytes;
	/* [size_is][unique] */ BYTE *pbExtraBytes;
} SCardIO_Request;

typedef struct _Transmit_Call
{
	REDIR_SCARDCONTEXT hContext;
	REDIR_SCARDHANDLE hCard;
	LPSCARD_IO_REQUEST pioSendPci;
	/* [range] */ DWORD cbSendLength;
	/* [size_is] */ BYTE *pbSendBuffer;
	/* [unique] */ LPSCARD_IO_REQUEST pioRecvPci;
	LONG fpbRecvBufferIsNULL;
	DWORD cbRecvLength;
} Transmit_Call;

typedef struct _Transmit_Return
{
	LONG ReturnCode;
	/* [unique] */ LPSCARD_IO_REQUEST pioRecvPci;
	/* [range] */ DWORD cbRecvLength;
	/* [size_is][unique] */ BYTE *pbRecvBuffer;
} Transmit_Return;

typedef struct _GetTransmitCount_Call
{
	REDIR_SCARDCONTEXT hContext;
	REDIR_SCARDHANDLE hCard;
} GetTransmitCount_Call;

typedef struct _GetTransmitCount_Return
{
	LONG ReturnCode;
	DWORD cTransmitCount;
} GetTransmitCount_Return;

typedef struct _Control_Call
{
	REDIR_SCARDCONTEXT hContext;
	REDIR_SCARDHANDLE hCard;
	DWORD dwControlCode;
	/* [range] */ DWORD cbInBufferSize;
	/* [size_is][unique] */ BYTE *pvInBuffer;
	LONG fpvOutBufferIsNULL;
	DWORD cbOutBufferSize;
} Control_Call;

typedef struct _Control_Return
{
	LONG ReturnCode;
	/* [range] */ DWORD cbOutBufferSize;
	/* [size_is][unique] */ BYTE *pvOutBuffer;
} Control_Return;

typedef struct _GetAttrib_Call
{
	REDIR_SCARDCONTEXT hContext;
	REDIR_SCARDHANDLE hCard;
	DWORD dwAttrId;
	LONG fpbAttrIsNULL;
	DWORD cbAttrLen;
} GetAttrib_Call;

typedef struct _GetAttrib_Return
{
	LONG ReturnCode;
	/* [range] */ DWORD cbAttrLen;
	/* [size_is][unique] */ BYTE *pbAttr;
} GetAttrib_Return;

typedef struct _SetAttrib_Call
{
	REDIR_SCARDCONTEXT hContext;
	REDIR_SCARDHANDLE hCard;
	DWORD dwAttrId;
	/* [range] */ DWORD cbAttrLen;
	/* [size_is] */ BYTE *pbAttr;
} SetAttrib_Call;

typedef struct _ReadCache_Common
{
	REDIR_SCARDCONTEXT hContext;
	UUID *CardIdentifier;
	DWORD FreshnessCounter;
	LONG fPbDataIsNULL;
	DWORD cbDataLen;
} ReadCache_Common;

typedef struct _ReadCacheA_Call
{
	/* [string] */ unsigned char *szLookupName;
	ReadCache_Common Common;
} ReadCacheA_Call;

typedef struct _ReadCacheW_Call
{
	/* [string] */ WCHAR *szLookupName;
	ReadCache_Common Common;
} ReadCacheW_Call;

typedef struct _ReadCache_Return
{
	LONG ReturnCode;
	/* [range] */ DWORD cbDataLen;
	/* [size_is][unique] */ BYTE *pbData;
} ReadCache_Return;

typedef struct _WriteCache_Common
{
	REDIR_SCARDCONTEXT hContext;
	UUID *CardIdentifier;
	DWORD FreshnessCounter;
	/* [range] */ DWORD cbDataLen;
	/* [size_is][unique] */ BYTE *pbData;
} WriteCache_Common;

typedef struct _WriteCacheA_Call
{
	/* [string] */ unsigned char *szLookupName;
	WriteCache_Common Common;
} WriteCacheA_Call;

typedef struct _WriteCacheW_Call
{
	/* [string] */ WCHAR *szLookupName;
	WriteCache_Common Common;
} WriteCacheW_Call;

#define SMARTCARD_COMMON_TYPE_HEADER_LENGTH	8
#define SMARTCARD_PRIVATE_TYPE_HEADER_LENGTH	8

UINT32 smartcard_pack_write_size_align(SMARTCARD_DEVICE* smartcard, wStream* s, UINT32 size, UINT32 alignment);
UINT32 smartcard_unpack_read_size_align(SMARTCARD_DEVICE* smartcard, wStream* s, UINT32 size, UINT32 alignment);

SCARDCONTEXT smartcard_scard_context_native_from_redir(SMARTCARD_DEVICE* smartcard, REDIR_SCARDCONTEXT* context);
void smartcard_scard_context_native_to_redir(SMARTCARD_DEVICE* smartcard, REDIR_SCARDCONTEXT* context, SCARDCONTEXT hContext);

SCARDHANDLE smartcard_scard_handle_native_from_redir(SMARTCARD_DEVICE* smartcard, REDIR_SCARDHANDLE* handle);
void smartcard_scard_handle_native_to_redir(SMARTCARD_DEVICE* smartcard, REDIR_SCARDHANDLE* handle, SCARDHANDLE hCard);

UINT32 smartcard_unpack_common_type_header(SMARTCARD_DEVICE* smartcard, wStream* s);
UINT32 smartcard_pack_common_type_header(SMARTCARD_DEVICE* smartcard, wStream* s);

UINT32 smartcard_unpack_private_type_header(SMARTCARD_DEVICE* smartcard, wStream* s);
UINT32 smartcard_pack_private_type_header(SMARTCARD_DEVICE* smartcard, wStream* s, UINT32 objectBufferLength);

UINT32 smartcard_unpack_redir_scard_context(SMARTCARD_DEVICE* smartcard, wStream* s, REDIR_SCARDCONTEXT* context);
UINT32 smartcard_pack_redir_scard_context(SMARTCARD_DEVICE* smartcard, wStream* s, REDIR_SCARDCONTEXT* context);

UINT32 smartcard_unpack_redir_scard_context_ref(SMARTCARD_DEVICE* smartcard, wStream* s, REDIR_SCARDCONTEXT* context);
UINT32 smartcard_pack_redir_scard_context_ref(SMARTCARD_DEVICE* smartcard, wStream* s, REDIR_SCARDCONTEXT* context);

UINT32 smartcard_unpack_redir_scard_handle(SMARTCARD_DEVICE* smartcard, wStream* s, REDIR_SCARDHANDLE* handle);
UINT32 smartcard_pack_redir_scard_handle(SMARTCARD_DEVICE* smartcard, wStream* s, REDIR_SCARDHANDLE* handle);

UINT32 smartcard_unpack_redir_scard_handle_ref(SMARTCARD_DEVICE* smartcard, wStream* s, REDIR_SCARDHANDLE* handle);
UINT32 smartcard_pack_redir_scard_handle_ref(SMARTCARD_DEVICE* smartcard, wStream* s, REDIR_SCARDHANDLE* handle);

UINT32 smartcard_unpack_establish_context_call(SMARTCARD_DEVICE* smartcard, wStream* s, EstablishContext_Call* call);
void smartcard_trace_establish_context_call(SMARTCARD_DEVICE* smartcard, EstablishContext_Call* call);

UINT32 smartcard_pack_establish_context_return(SMARTCARD_DEVICE* smartcard, wStream* s, EstablishContext_Return* ret);
void smartcard_trace_establish_context_return(SMARTCARD_DEVICE* smartcard, EstablishContext_Return* ret);

UINT32 smartcard_unpack_context_call(SMARTCARD_DEVICE* smartcard, wStream* s, Context_Call* call);
void smartcard_trace_context_call(SMARTCARD_DEVICE* smartcard, Context_Call* call, const char* name);

void smartcard_trace_long_return(SMARTCARD_DEVICE* smartcard, Long_Return* ret, const char* name);

UINT32 smartcard_unpack_list_readers_call(SMARTCARD_DEVICE* smartcard, wStream* s, ListReaders_Call* call);
void smartcard_trace_list_readers_call(SMARTCARD_DEVICE* smartcard, ListReaders_Call* call, BOOL unicode);

UINT32 smartcard_pack_list_readers_return(SMARTCARD_DEVICE* smartcard, wStream* s, ListReaders_Return* ret);
void smartcard_trace_list_readers_return(SMARTCARD_DEVICE* smartcard, ListReaders_Return* ret, BOOL unicode);

UINT32 smartcard_unpack_connect_a_call(SMARTCARD_DEVICE* smartcard, wStream* s, ConnectA_Call* call);
void smartcard_trace_connect_a_call(SMARTCARD_DEVICE* smartcard, ConnectA_Call* call);

UINT32 smartcard_unpack_connect_w_call(SMARTCARD_DEVICE* smartcard, wStream* s, ConnectW_Call* call);
void smartcard_trace_connect_w_call(SMARTCARD_DEVICE* smartcard, ConnectW_Call* call);

UINT32 smartcard_pack_connect_return(SMARTCARD_DEVICE* smartcard, wStream* s, Connect_Return* ret);
void smartcard_trace_connect_return(SMARTCARD_DEVICE* smartcard, Connect_Return* ret);

UINT32 smartcard_unpack_reconnect_call(SMARTCARD_DEVICE* smartcard, wStream* s, Reconnect_Call* call);
void smartcard_trace_reconnect_call(SMARTCARD_DEVICE* smartcard, Reconnect_Call* call);

UINT32 smartcard_pack_reconnect_return(SMARTCARD_DEVICE* smartcard, wStream* s, Reconnect_Return* ret);
void smartcard_trace_reconnect_return(SMARTCARD_DEVICE* smartcard, Reconnect_Return* ret);

UINT32 smartcard_unpack_hcard_and_disposition_call(SMARTCARD_DEVICE* smartcard, wStream* s, HCardAndDisposition_Call* call);
void smartcard_trace_hcard_and_disposition_call(SMARTCARD_DEVICE* smartcard, HCardAndDisposition_Call* call, const char* name);

UINT32 smartcard_unpack_get_status_change_a_call(SMARTCARD_DEVICE* smartcard, wStream* s, GetStatusChangeA_Call* call);
void smartcard_trace_get_status_change_a_call(SMARTCARD_DEVICE* smartcard, GetStatusChangeA_Call* call);

UINT32 smartcard_unpack_get_status_change_w_call(SMARTCARD_DEVICE* smartcard, wStream* s, GetStatusChangeW_Call* call);
void smartcard_trace_get_status_change_w_call(SMARTCARD_DEVICE* smartcard, GetStatusChangeW_Call* call);

UINT32 smartcard_pack_get_status_change_return(SMARTCARD_DEVICE* smartcard, wStream* s, GetStatusChange_Return* ret);
void smartcard_trace_get_status_change_return(SMARTCARD_DEVICE* smartcard, GetStatusChange_Return* ret, BOOL unicode);

UINT32 smartcard_unpack_state_call(SMARTCARD_DEVICE* smartcard, wStream* s, State_Call* call);
UINT32 smartcard_pack_state_return(SMARTCARD_DEVICE* smartcard, wStream* s, State_Return* ret);

UINT32 smartcard_unpack_status_call(SMARTCARD_DEVICE* smartcard, wStream* s, Status_Call* call);
void smartcard_trace_status_call(SMARTCARD_DEVICE* smartcard, Status_Call* call, BOOL unicode);

UINT32 smartcard_pack_status_return(SMARTCARD_DEVICE* smartcard, wStream* s, Status_Return* ret);
void smartcard_trace_status_return(SMARTCARD_DEVICE* smartcard, Status_Return* ret, BOOL unicode);

UINT32 smartcard_unpack_get_attrib_call(SMARTCARD_DEVICE* smartcard, wStream* s, GetAttrib_Call* call);
void smartcard_trace_get_attrib_call(SMARTCARD_DEVICE* smartcard, GetAttrib_Call* call);

UINT32 smartcard_pack_get_attrib_return(SMARTCARD_DEVICE* smartcard, wStream* s, GetAttrib_Return* ret);
void smartcard_trace_get_attrib_return(SMARTCARD_DEVICE* smartcard, GetAttrib_Return* ret, DWORD dwAttrId);

UINT32 smartcard_unpack_control_call(SMARTCARD_DEVICE* smartcard, wStream* s, Control_Call* call);
void smartcard_trace_control_call(SMARTCARD_DEVICE* smartcard, Control_Call* call);

UINT32 smartcard_pack_control_return(SMARTCARD_DEVICE* smartcard, wStream* s, Control_Return* ret);
void smartcard_trace_control_return(SMARTCARD_DEVICE* smartcard, Control_Return* ret);

UINT32 smartcard_unpack_transmit_call(SMARTCARD_DEVICE* smartcard, wStream* s, Transmit_Call* call);
void smartcard_trace_transmit_call(SMARTCARD_DEVICE* smartcard, Transmit_Call* call);

UINT32 smartcard_pack_transmit_return(SMARTCARD_DEVICE* smartcard, wStream* s, Transmit_Return* ret);
void smartcard_trace_transmit_return(SMARTCARD_DEVICE* smartcard, Transmit_Return* ret);

#endif /* FREERDP_CHANNEL_SMARTCARD_CLIENT_PACK_H */
