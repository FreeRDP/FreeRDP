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
	/* [size_is][unique] */ UINT64 pbContext;
} REDIR_SCARDCONTEXT;

typedef struct _REDIR_SCARDHANDLE
{
	REDIR_SCARDCONTEXT Context;
	/* [range] */ DWORD cbHandle;
	/* [size_is] */ UINT64 pbHandle;
} REDIR_SCARDHANDLE;

typedef struct _long_Return
{
	long ReturnCode;
} 	long_Return;

typedef struct _longAndMultiString_Return
{
	long ReturnCode;
	/* [range] */ DWORD cBytes;
	/* [size_is][unique] */ BYTE *msz;
} ListReaderGroups_Return;

typedef struct _longAndMultiString_Return ListReaders_Return;

typedef struct _Context_Call
{
	REDIR_SCARDCONTEXT Context;
} Context_Call;

typedef struct _ContextAndStringA_Call
{
	REDIR_SCARDCONTEXT Context;
	/* [string] */ unsigned char *sz;
} ContextAndStringA_Call;

typedef struct _ContextAndStringW_Call
{
	REDIR_SCARDCONTEXT Context;
	/* [string] */ WCHAR *sz;
} ContextAndStringW_Call;

typedef struct _ContextAndTwoStringA_Call
{
	REDIR_SCARDCONTEXT Context;
	/* [string] */ unsigned char *sz1;
	/* [string] */ unsigned char *sz2;
} ContextAndTwoStringA_Call;

typedef struct _ContextAndTwoStringW_Call
{
	REDIR_SCARDCONTEXT Context;
	/* [string] */ WCHAR *sz1;
	/* [string] */ WCHAR *sz2;
} ContextAndTwoStringW_Call;

typedef struct _EstablishContext_Call
{
	DWORD dwScope;
} EstablishContext_Call;

typedef struct _EstablishContext_Return
{
	long ReturnCode;
	REDIR_SCARDCONTEXT Context;
} EstablishContext_Return;

typedef struct _ListReaderGroups_Call
{
	REDIR_SCARDCONTEXT Context;
	long fmszGroupsIsNULL;
	DWORD cchGroups;
} ListReaderGroups_Call;

typedef struct _ListReaders_Call
{
	REDIR_SCARDCONTEXT Context;
	/* [range] */ DWORD cBytes;
	/* [size_is][unique] */ BYTE *mszGroups;
	long fmszReadersIsNULL;
	DWORD cchReaders;
} ListReaders_Call;

typedef struct _ReaderState_Common_Call
{
	DWORD dwCurrentState;
	DWORD dwEventState;
	/* [range] */ DWORD cbAtr;
	BYTE rgbAtr[ 36 ];
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
	BYTE rgbAtr[ 36 ];
} ReaderState_Return;

typedef struct _GetStatusChangeA_Call
{
	REDIR_SCARDCONTEXT Context;
	DWORD dwTimeOut;
	/* [range] */ DWORD cReaders;
	/* [size_is] */ ReaderStateA *rgReaderStates;
} GetStatusChangeA_Call;

typedef struct _LocateCardsA_Call
{
	REDIR_SCARDCONTEXT Context;
	/* [range] */ DWORD cBytes;
	/* [size_is] */ BYTE *mszCards;
	/* [range] */ DWORD cReaders;
	/* [size_is] */ ReaderStateA *rgReaderStates;
} LocateCardsA_Call;

typedef struct _LocateCardsW_Call
{
	REDIR_SCARDCONTEXT Context;
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
	REDIR_SCARDCONTEXT Context;
	/* [range] */ DWORD cAtrs;
	/* [size_is] */ LocateCards_ATRMask *rgAtrMasks;
	/* [range] */ DWORD cReaders;
	/* [size_is] */ ReaderStateA *rgReaderStates;
} LocateCardsByATRA_Call;

typedef struct _LocateCardsByATRW_Call
{
	REDIR_SCARDCONTEXT Context;
	/* [range] */ DWORD cAtrs;
	/* [size_is] */ LocateCards_ATRMask *rgAtrMasks;
	/* [range] */ DWORD cReaders;
	/* [size_is] */ ReaderStateW *rgReaderStates;
} LocateCardsByATRW_Call;

typedef struct _GetStatusChange_Return
{
	long ReturnCode;
	/* [range] */ DWORD cReaders;
	/* [size_is] */ ReaderState_Return *rgReaderStates;
} LocateCards_Return;

typedef struct _GetStatusChange_Return GetStatusChange_Return;

typedef struct _GetStatusChangeW_Call
{
	REDIR_SCARDCONTEXT Context;
	DWORD dwTimeOut;
	/* [range] */ DWORD cReaders;
	/* [size_is] */ ReaderStateW *rgReaderStates;
} GetStatusChangeW_Call;

typedef struct _Connect_Common
{
	REDIR_SCARDCONTEXT Context;
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
	long ReturnCode;
	REDIR_SCARDHANDLE hCard;
	DWORD dwActiveProtocol;
} Connect_Return;

typedef struct _Reconnect_Call
{
	REDIR_SCARDHANDLE hCard;
	DWORD dwShareMode;
	DWORD dwPreferredProtocols;
	DWORD dwInitialization;
} Reconnect_Call;

typedef struct Reconnect_Return
{
	long ReturnCode;
	DWORD dwActiveProtocol;
} Reconnect_Return;

typedef struct _HCardAndDisposition_Call
{
	REDIR_SCARDHANDLE hCard;
	DWORD dwDisposition;
} HCardAndDisposition_Call;

typedef struct _State_Call
{
	REDIR_SCARDHANDLE hCard;
	long fpbAtrIsNULL;
	DWORD cbAtrLen;
} State_Call;

typedef struct _State_Return
{
	long ReturnCode;
	DWORD dwState;
	DWORD dwProtocol;
	/* [range] */ DWORD cbAtrLen;
	/* [size_is][unique] */ BYTE *rgAtr;
} State_Return;

typedef struct _Status_Call
{
	REDIR_SCARDHANDLE hCard;
	long fmszReaderNamesIsNULL;
	DWORD cchReaderLen;
	DWORD cbAtrLen;
} Status_Call;

typedef struct _Status_Return
{
	long ReturnCode;
	/* [range] */ DWORD cBytes;
	/* [size_is][unique] */ BYTE *mszReaderNames;
	DWORD dwState;
	DWORD dwProtocol;
	BYTE pbAtr[ 32 ];
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
	REDIR_SCARDHANDLE hCard;
	SCardIO_Request ioSendPci;
	/* [range] */ DWORD cbSendLength;
	/* [size_is] */ BYTE *pbSendBuffer;
	/* [unique] */ SCardIO_Request *pioRecvPci;
	long fpbRecvBufferIsNULL;
	DWORD cbRecvLength;
} Transmit_Call;

typedef struct _Transmit_Return
{
	long ReturnCode;
	/* [unique] */ SCardIO_Request *pioRecvPci;
	/* [range] */ DWORD cbRecvLength;
	/* [size_is][unique] */ BYTE *pbRecvBuffer;
} Transmit_Return;

typedef struct _GetTransmitCount_Call
{
	REDIR_SCARDHANDLE hCard;
} GetTransmitCount_Call;

typedef struct _GetTransmitCount_Return
{
	long ReturnCode;
	DWORD cTransmitCount;
} GetTransmitCount_Return;

typedef struct _Control_Call
{
	REDIR_SCARDHANDLE hCard;
	DWORD dwControlCode;
	/* [range] */ DWORD cbInBufferSize;
	/* [size_is][unique] */ BYTE *pvInBuffer;
	long fpvOutBufferIsNULL;
	DWORD cbOutBufferSize;
} Control_Call;

typedef struct _Control_Return
{
	long ReturnCode;
	/* [range] */ DWORD cbOutBufferSize;
	/* [size_is][unique] */ BYTE *pvOutBuffer;
} Control_Return;

typedef struct _GetAttrib_Call
{
	REDIR_SCARDHANDLE hCard;
	DWORD dwAttrId;
	long fpbAttrIsNULL;
	DWORD cbAttrLen;
} GetAttrib_Call;

typedef struct _GetAttrib_Return
{
	long ReturnCode;
	/* [range] */ DWORD cbAttrLen;
	/* [size_is][unique] */ BYTE *pbAttr;
} GetAttrib_Return;

typedef struct _SetAttrib_Call
{
	REDIR_SCARDHANDLE hCard;
	DWORD dwAttrId;
	/* [range] */ DWORD cbAttrLen;
	/* [size_is] */ BYTE *pbAttr;
} SetAttrib_Call;

typedef struct _ReadCache_Common
{
	REDIR_SCARDCONTEXT Context;
	UUID *CardIdentifier;
	DWORD FreshnessCounter;
	long fPbDataIsNULL;
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
	long ReturnCode;
	/* [range] */ DWORD cbDataLen;
	/* [size_is][unique] */ BYTE *pbData;
} ReadCache_Return;

typedef struct _WriteCache_Common
{
	REDIR_SCARDCONTEXT Context;
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

UINT32 smartcard_unpack_common_type_header(SMARTCARD_DEVICE* smartcard, wStream* s);
UINT32 smartcard_pack_common_type_header(SMARTCARD_DEVICE* smartcard, wStream* s);

UINT32 smartcard_unpack_private_type_header(SMARTCARD_DEVICE* smartcard, wStream* s);
UINT32 smartcard_pack_private_type_header(SMARTCARD_DEVICE* smartcard, wStream* s, UINT32 objectBufferLength);

UINT32 smartcard_unpack_establish_context_call(SMARTCARD_DEVICE* smartcard, wStream* s, EstablishContext_Call* call);

UINT32 smartcard_unpack_list_readers_call(SMARTCARD_DEVICE* smartcard, wStream* s, ListReaders_Call* call);

UINT32 smartcard_unpack_connect_a_call(SMARTCARD_DEVICE* smartcard, wStream* s, ConnectA_Call* call);
UINT32 smartcard_unpack_connect_w_call(SMARTCARD_DEVICE* smartcard, wStream* s, ConnectW_Call* call);

UINT32 smartcard_unpack_reconnect_call(SMARTCARD_DEVICE* smartcard, wStream* s, Reconnect_Call* call);

UINT32 smartcard_unpack_hcard_and_disposition_call(SMARTCARD_DEVICE* smartcard, wStream* s, HCardAndDisposition_Call* call);

UINT32 smartcard_unpack_get_status_change_a_call(SMARTCARD_DEVICE* smartcard, wStream* s, GetStatusChangeA_Call* call);
UINT32 smartcard_unpack_get_status_change_w_call(SMARTCARD_DEVICE* smartcard, wStream* s, GetStatusChangeW_Call* call);

#endif /* FREERDP_CHANNEL_SMARTCARD_CLIENT_PACK_H */
