/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Smartcard Redirection Virtual Channel
 *
 * Copyright 2021 Armin Novak <anovak@thincast.com>
 * Copyright 2021 Thincast Technologies GmbH
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

#ifndef FREERDP_CHANNEL_SCARD_H
#define FREERDP_CHANNEL_SCARD_H

#include <winpr/crt.h>
#include <winpr/smartcard.h>

#define RDP_SCARD_CTL_CODE(code) \
	CTL_CODE(FILE_DEVICE_FILE_SYSTEM, (code), METHOD_BUFFERED, FILE_ANY_ACCESS)

#define SCARD_IOCTL_ESTABLISHCONTEXT RDP_SCARD_CTL_CODE(5)       /* SCardEstablishContext */
#define SCARD_IOCTL_RELEASECONTEXT RDP_SCARD_CTL_CODE(6)         /* SCardReleaseContext */
#define SCARD_IOCTL_ISVALIDCONTEXT RDP_SCARD_CTL_CODE(7)         /* SCardIsValidContext */
#define SCARD_IOCTL_LISTREADERGROUPSA RDP_SCARD_CTL_CODE(8)      /* SCardListReaderGroupsA */
#define SCARD_IOCTL_LISTREADERGROUPSW RDP_SCARD_CTL_CODE(9)      /* SCardListReaderGroupsW */
#define SCARD_IOCTL_LISTREADERSA RDP_SCARD_CTL_CODE(10)          /* SCardListReadersA */
#define SCARD_IOCTL_LISTREADERSW RDP_SCARD_CTL_CODE(11)          /* SCardListReadersW */
#define SCARD_IOCTL_INTRODUCEREADERGROUPA RDP_SCARD_CTL_CODE(20) /* SCardIntroduceReaderGroupA */
#define SCARD_IOCTL_INTRODUCEREADERGROUPW RDP_SCARD_CTL_CODE(21) /* SCardIntroduceReaderGroupW */
#define SCARD_IOCTL_FORGETREADERGROUPA RDP_SCARD_CTL_CODE(22)    /* SCardForgetReaderGroupA */
#define SCARD_IOCTL_FORGETREADERGROUPW RDP_SCARD_CTL_CODE(23)    /* SCardForgetReaderGroupW */
#define SCARD_IOCTL_INTRODUCEREADERA RDP_SCARD_CTL_CODE(24)      /* SCardIntroduceReaderA */
#define SCARD_IOCTL_INTRODUCEREADERW RDP_SCARD_CTL_CODE(25)      /* SCardIntroduceReaderW */
#define SCARD_IOCTL_FORGETREADERA RDP_SCARD_CTL_CODE(26)         /* SCardForgetReaderA */
#define SCARD_IOCTL_FORGETREADERW RDP_SCARD_CTL_CODE(27)         /* SCardForgetReaderW */
#define SCARD_IOCTL_ADDREADERTOGROUPA RDP_SCARD_CTL_CODE(28)     /* SCardAddReaderToGroupA */
#define SCARD_IOCTL_ADDREADERTOGROUPW RDP_SCARD_CTL_CODE(29)     /* SCardAddReaderToGroupW */
#define SCARD_IOCTL_REMOVEREADERFROMGROUPA                \
	RDP_SCARD_CTL_CODE(30) /* SCardRemoveReaderFromGroupA \
	                        */
#define SCARD_IOCTL_REMOVEREADERFROMGROUPW                                                   \
	RDP_SCARD_CTL_CODE(31)                                    /* SCardRemoveReaderFromGroupW \
	                                                           */
#define SCARD_IOCTL_LOCATECARDSA RDP_SCARD_CTL_CODE(38)       /* SCardLocateCardsA */
#define SCARD_IOCTL_LOCATECARDSW RDP_SCARD_CTL_CODE(39)       /* SCardLocateCardsW */
#define SCARD_IOCTL_GETSTATUSCHANGEA RDP_SCARD_CTL_CODE(40)   /* SCardGetStatusChangeA */
#define SCARD_IOCTL_GETSTATUSCHANGEW RDP_SCARD_CTL_CODE(41)   /* SCardGetStatusChangeW */
#define SCARD_IOCTL_CANCEL RDP_SCARD_CTL_CODE(42)             /* SCardCancel */
#define SCARD_IOCTL_CONNECTA RDP_SCARD_CTL_CODE(43)           /* SCardConnectA */
#define SCARD_IOCTL_CONNECTW RDP_SCARD_CTL_CODE(44)           /* SCardConnectW */
#define SCARD_IOCTL_RECONNECT RDP_SCARD_CTL_CODE(45)          /* SCardReconnect */
#define SCARD_IOCTL_DISCONNECT RDP_SCARD_CTL_CODE(46)         /* SCardDisconnect */
#define SCARD_IOCTL_BEGINTRANSACTION RDP_SCARD_CTL_CODE(47)   /* SCardBeginTransaction */
#define SCARD_IOCTL_ENDTRANSACTION RDP_SCARD_CTL_CODE(48)     /* SCardEndTransaction */
#define SCARD_IOCTL_STATE RDP_SCARD_CTL_CODE(49)              /* SCardState */
#define SCARD_IOCTL_STATUSA RDP_SCARD_CTL_CODE(50)            /* SCardStatusA */
#define SCARD_IOCTL_STATUSW RDP_SCARD_CTL_CODE(51)            /* SCardStatusW */
#define SCARD_IOCTL_TRANSMIT RDP_SCARD_CTL_CODE(52)           /* SCardTransmit */
#define SCARD_IOCTL_CONTROL RDP_SCARD_CTL_CODE(53)            /* SCardControl */
#define SCARD_IOCTL_GETATTRIB RDP_SCARD_CTL_CODE(54)          /* SCardGetAttrib */
#define SCARD_IOCTL_SETATTRIB RDP_SCARD_CTL_CODE(55)          /* SCardSetAttrib */
#define SCARD_IOCTL_ACCESSSTARTEDEVENT RDP_SCARD_CTL_CODE(56) /* SCardAccessStartedEvent */
#define SCARD_IOCTL_RELEASETARTEDEVENT RDP_SCARD_CTL_CODE(57) /* SCardReleaseStartedEvent */
#define SCARD_IOCTL_LOCATECARDSBYATRA RDP_SCARD_CTL_CODE(58)  /* SCardLocateCardsByATRA */
#define SCARD_IOCTL_LOCATECARDSBYATRW RDP_SCARD_CTL_CODE(59)  /* SCardLocateCardsByATRW */
#define SCARD_IOCTL_READCACHEA RDP_SCARD_CTL_CODE(60)         /* SCardReadCacheA */
#define SCARD_IOCTL_READCACHEW RDP_SCARD_CTL_CODE(61)         /* SCardReadCacheW */
#define SCARD_IOCTL_WRITECACHEA RDP_SCARD_CTL_CODE(62)        /* SCardWriteCacheA */
#define SCARD_IOCTL_WRITECACHEW RDP_SCARD_CTL_CODE(63)        /* SCardWriteCacheW */
#define SCARD_IOCTL_GETTRANSMITCOUNT RDP_SCARD_CTL_CODE(64)   /* SCardGetTransmitCount */
#define SCARD_IOCTL_GETREADERICON RDP_SCARD_CTL_CODE(65)      /* SCardGetReaderIconA */
#define SCARD_IOCTL_GETDEVICETYPEID RDP_SCARD_CTL_CODE(66)    /* SCardGetDeviceTypeIdA */

#pragma pack(push, 1)

/* interface type_scard_pack */
/* [unique][version][uuid] */

typedef struct
{
	/* [range] */ DWORD cbContext;
	/* [size_is][unique] */ BYTE pbContext[8];
} REDIR_SCARDCONTEXT;

typedef struct
{
	/* [range] */ DWORD cbHandle;
	/* [size_is] */ BYTE pbHandle[8];
} REDIR_SCARDHANDLE;

typedef struct
{
	LONG ReturnCode;
} Long_Return;

typedef struct
{
	LONG ReturnCode;
	/* [range] */ DWORD cBytes;
	/* [size_is][unique] */ BYTE* msz;
} ListReaderGroups_Return, ListReaders_Return;

typedef struct
{
	LONG ReturnCode;
	REDIR_SCARDCONTEXT hContext;
} EstablishContext_Return;

typedef struct
{
	DWORD dwCurrentState;
	DWORD dwEventState;
	/* [range] */ DWORD cbAtr;
	BYTE rgbAtr[36];
} ReaderState_Return;

typedef struct
{
	/* [range] */ DWORD cbAtr;
	BYTE rgbAtr[36];
	BYTE rgbMask[36];
} LocateCards_ATRMask;

typedef struct
{
	LONG ReturnCode;
	/* [range] */ DWORD cReaders;
	/* [size_is] */ ReaderState_Return* rgReaderStates;
} LocateCards_Return, GetStatusChange_Return;

typedef struct
{
	LONG ReturnCode;
	ULONG cbDataLen;
	BYTE* pbData;
} GetReaderIcon_Return;

typedef struct
{
	LONG ReturnCode;
	ULONG dwDeviceId;
} GetDeviceTypeId_Return;

typedef struct
{
	LONG ReturnCode;
	REDIR_SCARDCONTEXT hContext;
	REDIR_SCARDHANDLE hCard;
	DWORD dwActiveProtocol;
} Connect_Return;

typedef struct
{
	LONG ReturnCode;
	DWORD dwActiveProtocol;
} Reconnect_Return;

typedef struct
{
	LONG ReturnCode;
	DWORD dwState;
	DWORD dwProtocol;
	/* [range] */ DWORD cbAtrLen;
	/* [size_is][unique] */ BYTE rgAtr[36];
} State_Return;

typedef struct
{
	LONG ReturnCode;
	/* [range] */ DWORD cBytes;
	/* [size_is][unique] */ BYTE* mszReaderNames;
	DWORD dwState;
	DWORD dwProtocol;
	BYTE pbAtr[32];
	/* [range] */ DWORD cbAtrLen;
} Status_Return;

typedef struct
{
	DWORD dwProtocol;
	/* [range] */ DWORD cbExtraBytes;
	/* [size_is][unique] */ BYTE* pbExtraBytes;
} SCardIO_Request;

typedef struct
{
	LONG ReturnCode;
	/* [unique] */ LPSCARD_IO_REQUEST pioRecvPci;
	/* [range] */ DWORD cbRecvLength;
	/* [size_is][unique] */ BYTE* pbRecvBuffer;
} Transmit_Return;

typedef struct
{
	LONG ReturnCode;
	DWORD cTransmitCount;
} GetTransmitCount_Return;

typedef struct
{
	LONG ReturnCode;
	/* [range] */ DWORD cbOutBufferSize;
	/* [size_is][unique] */ BYTE* pvOutBuffer;
} Control_Return;

typedef struct
{
	LONG ReturnCode;
	/* [range] */ DWORD cbAttrLen;
	/* [size_is][unique] */ BYTE* pbAttr;
} GetAttrib_Return;

typedef struct
{
	LONG ReturnCode;
	/* [range] */ DWORD cbDataLen;
	/* [size_is][unique] */ BYTE* pbData;
} ReadCache_Return;
#pragma pack(pop)

typedef struct
{
	REDIR_SCARDCONTEXT hContext;
	REDIR_SCARDHANDLE hCard;
} Handles_Call;

typedef struct
{
	Handles_Call handles;
	LONG fmszGroupsIsNULL;
	DWORD cchGroups;
} ListReaderGroups_Call;

typedef struct
{
	Handles_Call handles;
	/* [range] */ DWORD cBytes;
	/* [size_is][unique] */ BYTE* mszGroups;
	LONG fmszReadersIsNULL;
	DWORD cchReaders;
} ListReaders_Call;

typedef struct
{
	Handles_Call handles;
	DWORD dwTimeOut;
	/* [range] */ DWORD cReaders;
	/* [size_is] */ LPSCARD_READERSTATEA rgReaderStates;
} GetStatusChangeA_Call;

typedef struct
{
	Handles_Call handles;
	/* [range] */ DWORD cBytes;
	/* [size_is] */ CHAR* mszCards;
	/* [range] */ DWORD cReaders;
	/* [size_is] */ LPSCARD_READERSTATEA rgReaderStates;
} LocateCardsA_Call;

typedef struct
{
	Handles_Call handles;
	/* [range] */ DWORD cBytes;
	/* [size_is] */ WCHAR* mszCards;
	/* [range] */ DWORD cReaders;
	/* [size_is] */ LPSCARD_READERSTATEW rgReaderStates;
} LocateCardsW_Call;

typedef struct
{
	Handles_Call handles;
	/* [range] */ DWORD cAtrs;
	/* [size_is] */ LocateCards_ATRMask* rgAtrMasks;
	/* [range] */ DWORD cReaders;
	/* [size_is] */ LPSCARD_READERSTATEA rgReaderStates;
} LocateCardsByATRA_Call;

typedef struct
{
	Handles_Call handles;
	/* [range] */ DWORD cAtrs;
	/* [size_is] */ LocateCards_ATRMask* rgAtrMasks;
	/* [range] */ DWORD cReaders;
	/* [size_is] */ LPSCARD_READERSTATEW rgReaderStates;
} LocateCardsByATRW_Call;

typedef struct
{
	Handles_Call handles;
	DWORD dwTimeOut;
	/* [range] */ DWORD cReaders;
	/* [size_is] */ LPSCARD_READERSTATEW rgReaderStates;
} GetStatusChangeW_Call;

typedef struct
{
	Handles_Call handles;
	WCHAR* szReaderName;
} GetReaderIcon_Call;

typedef struct
{
	Handles_Call handles;
	WCHAR* szReaderName;
} GetDeviceTypeId_Call;

typedef struct
{
	Handles_Call handles;
	DWORD dwShareMode;
	DWORD dwPreferredProtocols;
} Connect_Common_Call;

typedef struct
{
	Connect_Common_Call Common;
	/* [string] */ CHAR* szReader;
} ConnectA_Call;

typedef struct
{
	Connect_Common_Call Common;
	/* [string] */ WCHAR* szReader;
} ConnectW_Call;

typedef struct
{
	Handles_Call handles;
	DWORD dwShareMode;
	DWORD dwPreferredProtocols;
	DWORD dwInitialization;
} Reconnect_Call;

typedef struct
{
	Handles_Call handles;
	DWORD dwDisposition;
} HCardAndDisposition_Call;

typedef struct
{
	Handles_Call handles;
	LONG fpbAtrIsNULL;
	DWORD cbAtrLen;
} State_Call;

typedef struct
{
	Handles_Call handles;
	LONG fmszReaderNamesIsNULL;
	DWORD cchReaderLen;
	DWORD cbAtrLen;
} Status_Call;

typedef struct
{
	Handles_Call handles;
	LPSCARD_IO_REQUEST pioSendPci;
	/* [range] */ DWORD cbSendLength;
	/* [size_is] */ BYTE* pbSendBuffer;
	/* [unique] */ LPSCARD_IO_REQUEST pioRecvPci;
	LONG fpbRecvBufferIsNULL;
	DWORD cbRecvLength;
} Transmit_Call;

typedef struct
{
	Handles_Call handles;
	LONG LongValue;
} Long_Call;

typedef struct
{
	Handles_Call handles;
} Context_Call;

typedef struct
{
	Handles_Call handles;
	/* [string] */ char* sz;
} ContextAndStringA_Call;

typedef struct
{
	Handles_Call handles;
	/* [string] */ WCHAR* sz;
} ContextAndStringW_Call;

typedef struct
{
	Handles_Call handles;
	/* [string] */ char* sz1;
	/* [string] */ char* sz2;
} ContextAndTwoStringA_Call;

typedef struct
{
	Handles_Call handles;
	/* [string] */ WCHAR* sz1;
	/* [string] */ WCHAR* sz2;
} ContextAndTwoStringW_Call;

typedef struct
{
	Handles_Call handles;
	DWORD dwScope;
} EstablishContext_Call;

typedef struct
{
	Handles_Call handles;
} GetTransmitCount_Call;

typedef struct
{
	Handles_Call handles;
	DWORD dwControlCode;
	/* [range] */ DWORD cbInBufferSize;
	/* [size_is][unique] */ BYTE* pvInBuffer;
	LONG fpvOutBufferIsNULL;
	DWORD cbOutBufferSize;
} Control_Call;

typedef struct
{
	Handles_Call handles;
	DWORD dwAttrId;
	LONG fpbAttrIsNULL;
	DWORD cbAttrLen;
} GetAttrib_Call;

typedef struct
{
	Handles_Call handles;
	DWORD dwAttrId;
	/* [range] */ DWORD cbAttrLen;
	/* [size_is] */ BYTE* pbAttr;
} SetAttrib_Call;

typedef struct
{
	Handles_Call handles;
	UUID* CardIdentifier;
	DWORD FreshnessCounter;
	LONG fPbDataIsNULL;
	DWORD cbDataLen;
} ReadCache_Common;

typedef struct
{
	ReadCache_Common Common;
	/* [string] */ char* szLookupName;
} ReadCacheA_Call;

typedef struct
{
	ReadCache_Common Common;
	/* [string] */ WCHAR* szLookupName;
} ReadCacheW_Call;

typedef struct
{
	Handles_Call handles;
	UUID* CardIdentifier;
	DWORD FreshnessCounter;
	/* [range] */ DWORD cbDataLen;
	/* [size_is][unique] */ BYTE* pbData;
} WriteCache_Common;

typedef struct
{
	WriteCache_Common Common;
	/* [string] */ char* szLookupName;
} WriteCacheA_Call;

typedef struct
{
	WriteCache_Common Common;
	/* [string] */ WCHAR* szLookupName;
} WriteCacheW_Call;

#endif /* FREERDP_CHANNEL_SCARD_H */
