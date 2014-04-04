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
	/* [range] */ unsigned long cbContext;
	/* [size_is][unique] */ BYTE *pbContext;
} REDIR_SCARDCONTEXT;

typedef struct _REDIR_SCARDHANDLE
{
	REDIR_SCARDCONTEXT Context;
	/* [range] */ unsigned long cbHandle;
	/* [size_is] */ BYTE *pbHandle;
} REDIR_SCARDHANDLE;

typedef struct _long_Return
{
	long ReturnCode;
} 	long_Return;

typedef struct _longAndMultiString_Return
{
	long ReturnCode;
	/* [range] */ unsigned long cBytes;
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
	unsigned long dwScope;
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
	unsigned long cchGroups;
} ListReaderGroups_Call;

typedef struct _ListReaders_Call
{
	REDIR_SCARDCONTEXT Context;
	/* [range] */ unsigned long cBytes;
	/* [size_is][unique] */ BYTE *mszGroups;
	long fmszReadersIsNULL;
	unsigned long cchReaders;
} ListReaders_Call;

typedef struct _ReaderState_Common_Call
{
	unsigned long dwCurrentState;
	unsigned long dwEventState;
	/* [range] */ unsigned long cbAtr;
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
	unsigned long dwCurrentState;
	unsigned long dwEventState;
	/* [range] */ unsigned long cbAtr;
	BYTE rgbAtr[ 36 ];
} ReaderState_Return;

typedef struct _GetStatusChangeA_Call
{
	REDIR_SCARDCONTEXT Context;
	/* [range] */ unsigned long cBytes;
	/* [size_is] */ BYTE *mszCards;
	/* [range] */ unsigned long cReaders;
	/* [size_is] */ ReaderStateA *rgReaderStates;
} GetStatusChangeA_Call;

typedef struct _LocateCardsA_Call
{
	REDIR_SCARDCONTEXT Context;
	/* [range] */ unsigned long cBytes;
	/* [size_is] */ BYTE *mszCards;
	/* [range] */ unsigned long cReaders;
	/* [size_is] */ ReaderStateA *rgReaderStates;
} LocateCardsA_Call;

typedef struct _LocateCardsW_Call
{
	REDIR_SCARDCONTEXT Context;
	/* [range] */ unsigned long cBytes;
	/* [size_is] */ BYTE *mszCards;
	/* [range] */ unsigned long cReaders;
	/* [size_is] */ ReaderStateW *rgReaderStates;
} LocateCardsW_Call;

typedef struct _LocateCards_ATRMask
{
	/* [range] */ unsigned long cbAtr;
	BYTE rgbAtr[ 36 ];
	BYTE rgbMask[ 36 ];
} LocateCards_ATRMask;

typedef struct _LocateCardsByATRA_Call
{
	REDIR_SCARDCONTEXT Context;
	/* [range] */ unsigned long cAtrs;
	/* [size_is] */ LocateCards_ATRMask *rgAtrMasks;
	/* [range] */ unsigned long cReaders;
	/* [size_is] */ ReaderStateA *rgReaderStates;
} LocateCardsByATRA_Call;

typedef struct _LocateCardsByATRW_Call
{
	REDIR_SCARDCONTEXT Context;
	/* [range] */ unsigned long cAtrs;
	/* [size_is] */ LocateCards_ATRMask *rgAtrMasks;
	/* [range] */ unsigned long cReaders;
	/* [size_is] */ ReaderStateW *rgReaderStates;
} LocateCardsByATRW_Call;

typedef struct _GetStatusChange_Return
{
	long ReturnCode;
	/* [range] */ unsigned long cReaders;
	/* [size_is] */ ReaderState_Return *rgReaderStates;
} LocateCards_Return;

typedef struct _GetStatusChange_Return GetStatusChange_Return;

typedef struct _GetStatusChangeW_Call
{
	REDIR_SCARDCONTEXT Context;
	unsigned long dwTimeOut;
	/* [range] */ unsigned long cReaders;
	/* [size_is] */ ReaderStateW *rgReaderStates;
} GetStatusChangeW_Call;

typedef struct _Connect_Common
{
	REDIR_SCARDCONTEXT Context;
	unsigned long dwShareMode;
	unsigned long dwPreferredProtocols;
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
	unsigned long dwActiveProtocol;
} Connect_Return;

typedef struct _Reconnect_Call
{
	REDIR_SCARDHANDLE hCard;
	unsigned long dwShareMode;
	unsigned long dwPreferredProtocols;
	unsigned long dwInitialization;
} Reconnect_Call;

typedef struct Reconnect_Return
{
	long ReturnCode;
	unsigned long dwActiveProtocol;
} Reconnect_Return;

typedef struct _HCardAndDisposition_Call
{
	REDIR_SCARDHANDLE hCard;
	unsigned long dwDisposition;
} HCardAndDisposition_Call;

typedef struct _State_Call
{
	REDIR_SCARDHANDLE hCard;
	long fpbAtrIsNULL;
	unsigned long cbAtrLen;
} State_Call;

typedef struct _State_Return
{
	long ReturnCode;
	unsigned long dwState;
	unsigned long dwProtocol;
	/* [range] */ unsigned long cbAtrLen;
	/* [size_is][unique] */ BYTE *rgAtr;
} State_Return;

typedef struct _Status_Call
{
	REDIR_SCARDHANDLE hCard;
	long fmszReaderNamesIsNULL;
	unsigned long cchReaderLen;
	unsigned long cbAtrLen;
} Status_Call;

typedef struct _Status_Return
{
	long ReturnCode;
	/* [range] */ unsigned long cBytes;
	/* [size_is][unique] */ BYTE *mszReaderNames;
	unsigned long dwState;
	unsigned long dwProtocol;
	BYTE pbAtr[ 32 ];
	/* [range] */ unsigned long cbAtrLen;
} Status_Return;

typedef struct _SCardIO_Request
{
	unsigned long dwProtocol;
	/* [range] */ unsigned long cbExtraBytes;
	/* [size_is][unique] */ BYTE *pbExtraBytes;
} SCardIO_Request;

typedef struct _Transmit_Call
{
	REDIR_SCARDHANDLE hCard;
	SCardIO_Request ioSendPci;
	/* [range] */ unsigned long cbSendLength;
	/* [size_is] */ BYTE *pbSendBuffer;
	/* [unique] */ SCardIO_Request *pioRecvPci;
	long fpbRecvBufferIsNULL;
	unsigned long cbRecvLength;
} Transmit_Call;

typedef struct _Transmit_Return
{
	long ReturnCode;
	/* [unique] */ SCardIO_Request *pioRecvPci;
	/* [range] */ unsigned long cbRecvLength;
	/* [size_is][unique] */ BYTE *pbRecvBuffer;
} Transmit_Return;

typedef struct _GetTransmitCount_Call
{
	REDIR_SCARDHANDLE hCard;
} GetTransmitCount_Call;

typedef struct _GetTransmitCount_Return
{
	long ReturnCode;
	unsigned long cTransmitCount;
} GetTransmitCount_Return;

typedef struct _Control_Call
{
	REDIR_SCARDHANDLE hCard;
	unsigned long dwControlCode;
	/* [range] */ unsigned long cbInBufferSize;
	/* [size_is][unique] */ BYTE *pvInBuffer;
	long fpvOutBufferIsNULL;
	unsigned long cbOutBufferSize;
} Control_Call;

typedef struct _Control_Return
{
	long ReturnCode;
	/* [range] */ unsigned long cbOutBufferSize;
	/* [size_is][unique] */ BYTE *pvOutBuffer;
} Control_Return;

typedef struct _GetAttrib_Call
{
	REDIR_SCARDHANDLE hCard;
	unsigned long dwAttrId;
	long fpbAttrIsNULL;
	unsigned long cbAttrLen;
} GetAttrib_Call;

typedef struct _GetAttrib_Return
{
	long ReturnCode;
	/* [range] */ unsigned long cbAttrLen;
	/* [size_is][unique] */ BYTE *pbAttr;
} GetAttrib_Return;

typedef struct _SetAttrib_Call
{
	REDIR_SCARDHANDLE hCard;
	unsigned long dwAttrId;
	/* [range] */ unsigned long cbAttrLen;
	/* [size_is] */ BYTE *pbAttr;
} SetAttrib_Call;

typedef struct _ReadCache_Common
{
	REDIR_SCARDCONTEXT Context;
	UUID *CardIdentifier;
	unsigned long FreshnessCounter;
	long fPbDataIsNULL;
	unsigned long cbDataLen;
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
	/* [range] */ unsigned long cbDataLen;
	/* [size_is][unique] */ BYTE *pbData;
} ReadCache_Return;

typedef struct _WriteCache_Common
{
	REDIR_SCARDCONTEXT Context;
	UUID *CardIdentifier;
	unsigned long FreshnessCounter;
	/* [range] */ unsigned long cbDataLen;
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

UINT32 smartcard_unpack_establish_context_call(SMARTCARD_DEVICE* smartcard, wStream* s, EstablishContext_Call* call);
UINT32 smartcard_unpack_list_readers_call(SMARTCARD_DEVICE* smartcard, wStream* s, ListReaders_Call* call);

#endif /* FREERDP_CHANNEL_SMARTCARD_CLIENT_PACK_H */
