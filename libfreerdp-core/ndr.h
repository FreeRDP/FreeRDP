/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Network Data Representation (NDR)
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_CORE_NDR_H
#define FREERDP_CORE_NDR_H

#include "rpc.h"

#include "config.h"

#include <freerdp/types.h>
#include <freerdp/wtypes.h>
#include <freerdp/utils/debug.h>
#include <freerdp/utils/stream.h>

#define __RPC_WIN32__			1
#define TARGET_IS_NT50_OR_LATER		1

typedef union _CLIENT_CALL_RETURN
{
	void* Pointer;
	LONG_PTR Simple;
} CLIENT_CALL_RETURN;

typedef void* RPC_IF_HANDLE;

typedef struct _RPC_VERSION
{
	unsigned short MajorVersion;
	unsigned short MinorVersion;
} RPC_VERSION;

typedef struct _RPC_SYNTAX_IDENTIFIER
{
	GUID SyntaxGUID;
	RPC_VERSION SyntaxVersion;
} RPC_SYNTAX_IDENTIFIER, PRPC_SYNTAX_IDENTIFIER;

#define RPC_MGR_EPV	void

typedef struct _RPC_MESSAGE
{
	RPC_BINDING_HANDLE Handle;
	unsigned long DataRepresentation;
	void* Buffer;
	unsigned int BufferLength;
	unsigned int ProcNum;
	PRPC_SYNTAX_IDENTIFIER TransferSyntax;
	void* RpcInterfaceInformation;
	void* ReservedForRuntime;
	RPC_MGR_EPV* ManagerEpv;
	void* ImportContext;
	unsigned long RpcFlags;
} RPC_MESSAGE, *PRPC_MESSAGE;

typedef void (*RPC_DISPATCH_FUNCTION)(PRPC_MESSAGE Message);

typedef struct
{
	unsigned int DispatchTableCount;
	RPC_DISPATCH_FUNCTION* DispatchTable;
	LONG_PTR Reserved;
} RPC_DISPATCH_TABLE, *PRPC_DISPATCH_TABLE;

typedef struct _RPC_PROTSEQ_ENDPOINT
{
	unsigned char* RpcProtocolSequence;
	unsigned char* Endpoint;
} RPC_PROTSEQ_ENDPOINT, * PRPC_PROTSEQ_ENDPOINT;

typedef struct _RPC_SERVER_INTERFACE
{
	unsigned int Length;
	RPC_SYNTAX_IDENTIFIER InterfaceId;
	RPC_SYNTAX_IDENTIFIER TransferSyntax;
	PRPC_DISPATCH_TABLE DispatchTable;
	unsigned int RpcProtseqEndpointCount;
	PRPC_PROTSEQ_ENDPOINT RpcProtseqEndpoint;
	RPC_MGR_EPV* DefaultManagerEpv;
	void const* InterpreterInfo;
	unsigned int Flags;
} RPC_SERVER_INTERFACE, *PRPC_SERVER_INTERFACE;

typedef struct _RPC_CLIENT_INTERFACE
{
	unsigned int Length;
	RPC_SYNTAX_IDENTIFIER InterfaceId;
	RPC_SYNTAX_IDENTIFIER TransferSyntax;
	PRPC_DISPATCH_TABLE DispatchTable;
	unsigned int RpcProtseqEndpointCount;
	PRPC_PROTSEQ_ENDPOINT RpcProtseqEndpoint;
	ULONG_PTR Reserved;
	void const* InterpreterInfo;
	unsigned int Flags;
} RPC_CLIENT_INTERFACE, *PRPC_CLIENT_INTERFACE;

typedef void* (*GENERIC_BINDING_ROUTINE)(void*);
typedef void (*GENERIC_UNBIND_ROUTINE)(void*, unsigned char*);

typedef struct _GENERIC_BINDING_ROUTINE_PAIR
{
	GENERIC_BINDING_ROUTINE pfnBind;
	GENERIC_UNBIND_ROUTINE pfnUnbind;
} GENERIC_BINDING_ROUTINE_PAIR, *PGENERIC_BINDING_ROUTINE_PAIR;

typedef struct __GENERIC_BINDING_INFO
{
	void* pObj;
	unsigned int Size;
	GENERIC_BINDING_ROUTINE pfnBind;
	GENERIC_UNBIND_ROUTINE pfnUnbind;
} GENERIC_BINDING_INFO, *PGENERIC_BINDING_INFO;

typedef void (*NDR_RUNDOWN)(void* context);
typedef void (*NDR_NOTIFY_ROUTINE)(void);

typedef const unsigned char* PFORMAT_STRING;

typedef struct _MIDL_STUB_DESC MIDL_STUB_DESC;
typedef MIDL_STUB_DESC* PMIDL_STUB_DESC;

typedef struct _MIDL_STUB_MESSAGE
{
	PRPC_MESSAGE RpcMsg;
	unsigned char* Buffer;
	unsigned char* BufferStart;
	unsigned char* BufferEnd;
	unsigned char* BufferMark;
	unsigned long BufferLength;
	unsigned long MemorySize;
	unsigned char* Memory;
	unsigned char* StackTop;
	PMIDL_STUB_DESC StubDesc;
} MIDL_STUB_MESSAGE, *PMIDL_STUB_MESSAGE;

typedef struct _MIDL_STUB_MESSAGE MIDL_STUB_MESSAGE, *PMIDL_STUB_MESSAGE;

typedef void (*EXPR_EVAL)(struct _MIDL_STUB_MESSAGE*);

typedef void (*XMIT_HELPER_ROUTINE)(PMIDL_STUB_MESSAGE);

typedef struct _XMIT_ROUTINE_QUINTUPLE
{
	XMIT_HELPER_ROUTINE pfnTranslateToXmit;
	XMIT_HELPER_ROUTINE pfnTranslateFromXmit;
	XMIT_HELPER_ROUTINE pfnFreeXmit;
	XMIT_HELPER_ROUTINE pfnFreeInst;
} XMIT_ROUTINE_QUINTUPLE, *PXMIT_ROUTINE_QUINTUPLE;

typedef unsigned long (*USER_MARSHAL_SIZING_ROUTINE)(unsigned long*, unsigned long, void*);
typedef unsigned char* (*USER_MARSHAL_MARSHALLING_ROUTINE)(unsigned long*, unsigned char*, void*);
typedef unsigned char* (*USER_MARSHAL_UNMARSHALLING_ROUTINE)(unsigned long*, unsigned char*, void*);
typedef void (*USER_MARSHAL_FREEING_ROUTINE)(unsigned long*, void*);

typedef struct _USER_MARSHAL_ROUTINE_QUADRUPLE
{
	USER_MARSHAL_SIZING_ROUTINE pfnBufferSize;
	USER_MARSHAL_MARSHALLING_ROUTINE pfnMarshall;
	USER_MARSHAL_UNMARSHALLING_ROUTINE pfnUnmarshall;
	USER_MARSHAL_FREEING_ROUTINE pfnFree;
} USER_MARSHAL_ROUTINE_QUADRUPLE;

typedef struct _MALLOC_FREE_STRUCT
{
	void* (*pfnAllocate)(size_t);
	void (*pfnFree)(void*);
} MALLOC_FREE_STRUCT;

typedef struct _COMM_FAULT_OFFSETS
{
	short CommOffset;
	short FaultOffset;
} COMM_FAULT_OFFSETS;

typedef void* NDR_CS_ROUTINES;
typedef void* NDR_EXPR_DESC;

struct _MIDL_STUB_DESC
{
	void* RpcInterfaceInformation;
	void* (*pfnAllocate)(size_t);
	void (*pfnFree)(void*);

	union
	{
		handle_t* pAutoHandle;
		handle_t* pPrimitiveHandle;
		PGENERIC_BINDING_INFO pGenericBindingInfo;
        } IMPLICIT_HANDLE_INFO;

        const NDR_RUNDOWN* apfnNdrRundownRoutines;
        const GENERIC_BINDING_ROUTINE_PAIR* aGenericBindingRoutinePairs;
        const EXPR_EVAL* apfnExprEval;
        const XMIT_ROUTINE_QUINTUPLE* aXmitQuintuple;
        const unsigned char* pFormatTypes;

        int fCheckBounds;
        unsigned long Version;
        MALLOC_FREE_STRUCT* pMallocFreeStruct;

        long MIDLVersion;
        const COMM_FAULT_OFFSETS* CommFaultOffsets;
        const USER_MARSHAL_ROUTINE_QUADRUPLE* aUserMarshalQuadruple;

        const NDR_NOTIFY_ROUTINE* NotifyRoutineTable;
        ULONG_PTR mFlags;
	const NDR_CS_ROUTINES* CsRoutineTables;
	void* ProxyServerInfo;
	const NDR_EXPR_DESC* pExprInfo;
};

#define OI2_FLAG_SERVER_MUST_SIZE			0x01
#define OI2_FLAG_CLIENT_MUST_SIZE			0x02
#define OI2_FLAG_HAS_RETURN				0x04
#define OI2_FLAG_HAS_PIPES				0x08
#define OI2_FLAG_HAS_ASYNC_UUID				0x20
#define OI2_FLAG_HAS_EXTENSIONS				0x40
#define OI2_FLAG_HAS_ASYNC_HANDLE			0x80

#define PARAM_ATTRIBUTE_SERVER_ALLOC_SIZE		0xE000
#define PARAM_ATTRIBUTE_SAVE_FOR_ASYNC_FINISH		0x0400
#define PARAM_ATTRIBUTE_IS_DONT_CALL_FREE_INST		0x0200
#define PARAM_ATTRIBUTE_IS_SIMPLE_REF			0x0100
#define PARAM_ATTRIBUTE_IS_BY_VALUE			0x0080
#define PARAM_ATTRIBUTE_IS_BASE_TYPE			0x0040
#define PARAM_ATTRIBUTE_IS_RETURN			0x0020
#define PARAM_ATTRIBUTE_IS_OUT				0x0010
#define PARAM_ATTRIBUTE_IS_IN				0x0008
#define PARAM_ATTRIBUTE_IS_PIPE				0x0004
#define PARAM_ATTRIBUTE_MUST_FREE			0x0002
#define PARAM_ATTRIBUTE_MUST_SIZE			0x0001

typedef struct
{
	unsigned short Attributes;
	unsigned short StackOffset;

	union
	{
		unsigned char FormatChar;
		unsigned short Offset;
	} Type;

} NDR_PARAM;

typedef struct
{
	unsigned char Size;
	unsigned char Flags2;
	unsigned short ClientCorrHint;
	unsigned short ServerCorrHint;
	unsigned short NotifyIndex;
} NDR_PROC_HEADER_EXTS;

typedef struct _NDR_PROC_HEADER
{
	unsigned char HandleType;
	unsigned char OiFlags;
	unsigned short RpcFlagsLow;
	unsigned short RpcFlagsHi;
	unsigned short ProcNum;
	unsigned short StackSize;
} NDR_PROC_HEADER;

typedef struct _NDR_PROC_OI2_HEADER
{
	unsigned short ClientBufferSize;
	unsigned short ServerBufferSize;
	unsigned int Oi2Flags;
	unsigned char NumberParams;
} NDR_PROC_OI2_HEADER;

/* Type Format Strings: http://msdn.microsoft.com/en-us/library/windows/desktop/aa379093/ */

#define FC_BYTE				0x01
#define FC_CHAR				0x02
#define FC_SMALL			0x03
#define FC_USMALL			0x04
#define FC_WCHAR			0x05
#define FC_SHORT			0x06
#define FC_USHORT			0x07
#define FC_LONG				0x08
#define FC_ULONG			0x09
#define FC_FLOAT			0x0A
#define FC_HYPER			0x0B
#define FC_DOUBLE			0x0C
#define FC_ENUM16			0x0D
#define FC_ENUM32			0x0E
#define FC_ERROR_STATUS_T		0x10
#define FC_INT3264			0xB8
#define FC_UINT3264			0xB9

#define FC_BIND_CONTEXT			0x30
#define FC_BIND_GENERIC			0x31
#define FC_BIND_PRIMITIVE		0x32
#define FC_AUTO_HANDLE			0x33
#define FC_CALLBACK_HANDLE		0x34

#define NdrFcShort(s)	(byte)(s & 0xFF), (byte)(s >> 8)

#define NdrFcLong(s)	(byte)(s & 0xFF), (byte)((s & 0x0000FF00) >> 8), \
                        (byte)((s & 0x00FF0000) >> 16), (byte)(s >> 24)

CLIENT_CALL_RETURN NdrClientCall2(PMIDL_STUB_DESC pStubDescriptor, PFORMAT_STRING pFormat, ...);

void* MIDL_user_allocate(size_t cBytes);
void MIDL_user_free(void* p);

#endif /* FREERDP_CORE_NDR_H */
