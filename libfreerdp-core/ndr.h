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

typedef struct
{
	unsigned char FullPtrUsed : 1;
	unsigned char RpcSsAllocUsed : 1;
	unsigned char ObjectProc : 1;
	unsigned char HasRpcFlags : 1;
	unsigned char IgnoreObjectException : 1;
	unsigned char HasCommOrFault : 1;
	unsigned char UseNewInitRoutines : 1;
	unsigned char Unused : 1;
} INTERPRETER_FLAGS, *PINTERPRETER_FLAGS;

typedef struct
{
	unsigned short MustSize : 1;
	unsigned short MustFree : 1;
	unsigned short IsPipe : 1;
	unsigned short IsIn : 1;
	unsigned short IsOut : 1;
	unsigned short IsReturn : 1;
	unsigned short IsBasetype : 1;
	unsigned short IsByValue : 1;
	unsigned short IsSimpleRef : 1;
	unsigned short IsDontCallFreeInst : 1;
	unsigned short SaveForAsyncFinish : 1;
	unsigned short Unused : 2;
	unsigned short ServerAllocSize : 3;
} PARAM_ATTRIBUTES, *PPARAM_ATTRIBUTES;

typedef struct
{
	unsigned char ServerMustSize : 1;
	unsigned char ClientMustSize : 1;
	unsigned char HasReturn : 1;
	unsigned char HasPipes : 1;
	unsigned char Unused : 1;
	unsigned char HasAsyncUuid : 1;
	unsigned char HasExtensions : 1;
	unsigned char HasAsyncHandle : 1;
} INTERPRETER_OPT_FLAGS, *PINTERPRETER_OPT_FLAGS;

typedef struct
{
	unsigned char HasNewCorrDesc : 1;
	unsigned char ClientCorrCheck : 1;
	unsigned char ServerCorrCheck : 1;
	unsigned char HasNotify : 1;
	unsigned char HasNotify2 : 1;
	unsigned char Unused : 3;
} INTERPRETER_OPT_FLAGS2, *PINTERPRETER_OPT_FLAGS2;

#define OI2_FLAG_SERVER_MUST_SIZE			0x01
#define OI2_FLAG_CLIENT_MUST_SIZE			0x02
#define OI2_FLAG_HAS_RETURN				0x04
#define OI2_FLAG_HAS_PIPES				0x08
#define OI2_FLAG_HAS_ASYNC_UUID				0x20
#define OI2_FLAG_HAS_EXTENSIONS				0x40
#define OI2_FLAG_HAS_ASYNC_HANDLE			0x80

#define OI2_EXT_FLAG_HAS_NEW_CORR_DESC			0x01
#define OI2_EXT_FLAG_CLIENT_CORR_CHECK			0x02
#define OI2_EXT_FLAG_SERVER_CORR_CHECK			0x04
#define OI2_EXT_FLAG_HAS_NOTIFY				0x08
#define OI2_EXT_FLAG_HAS_NOTIFY2			0x10

typedef struct
{
	PARAM_ATTRIBUTES Attributes;
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
	INTERPRETER_OPT_FLAGS2 Flags2;
	unsigned short ClientCorrHint;
	unsigned short ServerCorrHint;
	unsigned short NotifyIndex;
} NDR_PROC_HEADER_EXTS;

typedef struct _NDR_PROC_HEADER
{
	unsigned char HandleType;
	INTERPRETER_FLAGS OldOiFlags;
	unsigned short RpcFlagsLow;
	unsigned short RpcFlagsHi;
	unsigned short ProcNum;
	unsigned short StackSize;
} NDR_PROC_HEADER, *PNDR_PROC_HEADER;

typedef struct _NDR_OI2_PROC_HEADER
{
	unsigned short ClientBufferSize;
	unsigned short ServerBufferSize;
	INTERPRETER_OPT_FLAGS Oi2Flags;
	unsigned char NumberParams;
} NDR_OI2_PROC_HEADER, *PNDR_OI2_PROC_HEADER;

typedef enum _NDR_PHASE
{
	NDR_PHASE_SIZE,
	NDR_PHASE_MARSHALL,
	NDR_PHASE_UNMARSHALL,
	NDR_PHASE_FREE
} NDR_PHASE;

/* Type Format Strings: http://msdn.microsoft.com/en-us/library/windows/desktop/aa379093/ */

#define FC_ZERO				0x00
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
#define FC_IGNORE			0x0F
#define FC_ERROR_STATUS_T		0x10
#define FC_RP				0x11
#define FC_UP				0x12
#define FC_OP				0x13
#define FC_FP				0x14
#define FC_STRUCT			0x15
#define FC_PSTRUCT			0x16
#define FC_CSTRUCT			0x17
#define FC_CPSTRUCT			0x18
#define FC_CVSTRUCT			0x19
#define FC_BOGUS_STRUCT			0x1A
#define FC_CARRAY			0x1B
#define FC_CVARRAY			0x1C
#define FC_SMFARRAY			0x1D
#define FC_LGFARRAY			0x1E
#define FC_SMVARRAY			0x1F
#define FC_LGVARRAY			0x20
#define FC_BOGUS_ARRAY			0x21
#define FC_C_CSTRING			0x22
#define FC_C_BSTRING			0x23
#define FC_C_SSTRING			0x24
#define FC_C_WSTRING			0x25
#define FC_CSTRING			0x26
#define FC_BSTRING			0x27
#define FC_SSTRING			0x28
#define FC_WSTRING			0x29
#define FC_ENCAPSULATED_UNION		0x2A
#define FC_NON_ENCAPSULATED_UNION	0x2B
#define FC_BYTE_COUNT_POINTER		0x2C
#define FC_TRANSMIT_AS			0x2D
#define FC_REPRESENT_AS			0x2E
#define FC_IP				0x2F
#define FC_BIND_CONTEXT			0x30
#define FC_BIND_GENERIC			0x31
#define FC_BIND_PRIMITIVE		0x32
#define FC_AUTO_HANDLE			0x33
#define FC_CALLBACK_HANDLE		0x34
#define FC_UNUSED1			0x35
#define FC_POINTER			0x36
#define FC_ALIGNM2			0x37
#define FC_ALIGNM4			0x38
#define FC_ALIGNM8			0x39
#define FC_UNUSED2			0x3A
#define FC_UNUSED3			0x3B
#define FC_UNUSED4			0x3C
#define FC_STRUCTPAD1			0x3D
#define FC_STRUCTPAD2			0x3E
#define FC_STRUCTPAD3			0x3F
#define FC_STRUCTPAD4			0x40
#define FC_STRUCTPAD5			0x41
#define FC_STRUCTPAD6			0x42
#define FC_STRUCTPAD7			0x43
#define FC_STRING_SIZED			0x44
#define FC_UNUSED5			0x45
#define FC_NO_REPEAT			0x46
#define FC_FIXED_REPEAT			0x47
#define FC_VARIABLE_REPEAT		0x48
#define FC_FIXED_OFFSET			0x49
#define FC_VARIABLE_OFFSET		0x4A
#define FC_PP				0x4B
#define FC_EMBEDDED_COMPLEX		0x4C
#define FC_IN_PARAM			0x4D
#define FC_IN_PARAM_BASETYPE		0x4E
#define FC_IN_PARAM_NO_FREE_INST	0x4F
#define FC_IN_OUT_PARAM			0x50
#define FC_OUT_PARAM			0x51
#define FC_RETURN_PARAM			0x52
#define FC_RETURN_PARAM_BASETYPE	0x53
#define FC_DEREFERENCE			0x54
#define FC_DIV_2			0x55
#define FC_MULT_2			0x56
#define FC_ADD_1			0x57
#define FC_SUB_1			0x58
#define FC_CALLBACK			0x59
#define FC_CONSTANT_IID			0x5A
#define FC_END				0x5B
#define FC_PAD				0x5C
#define FC_SPLIT_DEREFERENCE		0x74
#define FC_SPLIT_DIV_2			0x75
#define FC_SPLIT_MULT_2			0x76
#define FC_SPLIT_ADD_1			0x77
#define FC_SPLIT_SUB_1			0x78
#define FC_SPLIT_CALLBACK		0x79
#define FC_HARD_STRUCT			0xB1
#define FC_TRANSMIT_AS_PTR		0xB2
#define FC_REPRESENT_AS_PTR		0xB3
#define FC_USER_MARSHAL			0xB4
#define FC_PIPE				0xB5
#define FC_BLKHOLE			0xB6
#define FC_RANGE			0xB7
#define FC_INT3264			0xB8
#define FC_UINT3264			0xB9
#define FC_END_OF_UNIVERSE		0xBA

#define NdrFcShort(s)	(byte)(s & 0xFF), (byte)(s >> 8)

#define NdrFcLong(s)	(byte)(s & 0xFF), (byte)((s & 0x0000FF00) >> 8), \
                        (byte)((s & 0x00FF0000) >> 16), (byte)(s >> 24)

CLIENT_CALL_RETURN NdrClientCall2(PMIDL_STUB_DESC pStubDescriptor, PFORMAT_STRING pFormat, ...);

void* MIDL_user_allocate(size_t cBytes);
void MIDL_user_free(void* p);

#endif /* FREERDP_CORE_NDR_H */
