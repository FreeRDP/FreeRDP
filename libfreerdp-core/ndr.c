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

#include <freerdp/utils/memory.h>

#include "ndr.h"

/**
 * MSRPC NDR Types Technical Overview:
 * http://dvlabs.tippingpoint.com/blog/2007/11/24/msrpc-ndr-types/
 */

const char* FC_TYPE_STRINGS[] =
{
	"FC_ZERO",
	"FC_BYTE",
	"FC_CHAR",
	"FC_SMALL",
	"FC_USMALL",
	"FC_WCHAR",
	"FC_SHORT",
	"FC_USHORT",
	"FC_LONG",
	"FC_ULONG",
	"FC_FLOAT",
	"FC_HYPER",
	"FC_DOUBLE",
	"FC_ENUM16",
	"FC_ENUM32",
	"FC_IGNORE",
	"FC_ERROR_STATUS_T",
	"FC_RP",
	"FC_UP",
	"FC_OP",
	"FC_FP",
	"FC_STRUCT",
	"FC_PSTRUCT",
	"FC_CSTRUCT",
	"FC_CPSTRUCT",
	"FC_CVSTRUCT",
	"FC_BOGUS_STRUCT",
	"FC_CARRAY",
	"FC_CVARRAY",
	"FC_SMFARRAY",
	"FC_LGFARRAY",
	"FC_SMVARRAY",
	"FC_LGVARRAY",
	"FC_BOGUS_ARRAY",
	"FC_C_CSTRING",
	"FC_C_BSTRING",
	"FC_C_SSTRING",
	"FC_C_WSTRING",
	"FC_CSTRING",
	"FC_BSTRING",
	"FC_SSTRING",
	"FC_WSTRING",
	"FC_ENCAPSULATED_UNION",
	"FC_NON_ENCAPSULATED_UNION",
	"FC_BYTE_COUNT_POINTER",
	"FC_TRANSMIT_AS",
	"FC_REPRESENT_AS",
	"FC_IP",
	"FC_BIND_CONTEXT",
	"FC_BIND_GENERIC",
	"FC_BIND_PRIMITIVE",
	"FC_AUTO_HANDLE",
	"FC_CALLBACK_HANDLE",
	"FC_UNUSED1",
	"FC_POINTER",
	"FC_ALIGNM2",
	"FC_ALIGNM4",
	"FC_ALIGNM8",
	"FC_UNUSED2",
	"FC_UNUSED3",
	"FC_UNUSED4",
	"FC_STRUCTPAD1",
	"FC_STRUCTPAD2",
	"FC_STRUCTPAD3",
	"FC_STRUCTPAD4",
	"FC_STRUCTPAD5",
	"FC_STRUCTPAD6",
	"FC_STRUCTPAD7",
	"FC_STRING_SIZED",
	"FC_UNUSED5",
	"FC_NO_REPEAT",
	"FC_FIXED_REPEAT",
	"FC_VARIABLE_REPEAT",
	"FC_FIXED_OFFSET",
	"FC_VARIABLE_OFFSET",
	"FC_PP",
	"FC_EMBEDDED_COMPLEX",
	"FC_IN_PARAM",
	"FC_IN_PARAM_BASETYPE",
	"FC_IN_PARAM_NO_FREE_INST",
	"FC_IN_OUT_PARAM",
	"FC_OUT_PARAM",
	"FC_RETURN_PARAM",
	"FC_RETURN_PARAM_BASETYPE",
	"FC_DEREFERENCE",
	"FC_DIV_2",
	"FC_MULT_2",
	"FC_ADD_1",
	"FC_SUB_1",
	"FC_CALLBACK",
	"FC_CONSTANT_IID",
	"FC_END",
	"FC_PAD",
	"", "", "", "", "", "",
	"", "", "", "", "", "",
	"", "", "", "", "", "",
	"", "", "", "", "", "",
	"FC_SPLIT_DEREFERENCE",
	"FC_SPLIT_DIV_2",
	"FC_SPLIT_MULT_2",
	"FC_SPLIT_ADD_1",
	"FC_SPLIT_SUB_1",
	"FC_SPLIT_CALLBACK",
	"", "", "", "", "", "", "", "", "", "", "", "", "", "",
	"", "", "", "", "", "", "", "", "", "", "", "", "", "",
	"", "", "", "", "", "", "", "", "", "", "", "", "", "",
	"", "", "", "", "", "", "", "", "", "", "", "", "", "",
	"FC_HARD_STRUCT",
	"FC_TRANSMIT_AS_PTR",
	"FC_REPRESENT_AS_PTR",
	"FC_USER_MARSHAL",
	"FC_PIPE",
	"FC_BLKHOLE",
	"FC_RANGE",
	"FC_INT3264",
	"FC_UINT3264",
	"FC_END_OF_UNIVERSE",
};

void ndr_print_param_attributes(PARAM_ATTRIBUTES attributes)
{
	if (attributes.ServerAllocSize)
		printf("ServerAllocSize, ");
	if (attributes.SaveForAsyncFinish)
		printf("SaveForAsyncFinish, ");
	if (attributes.IsDontCallFreeInst)
		printf("IsDontCallFreeInst, ");
	if (attributes.IsSimpleRef)
		printf("IsSimpleRef, ");
	if (attributes.IsByValue)
		printf("IsByValue, ");
	if (attributes.IsBasetype)
		printf("IsBaseType, ");
	if (attributes.IsReturn)
		printf("IsReturn, ");
	if (attributes.IsOut)
		printf("IsOut, ");
	if (attributes.IsIn)
		printf("IsIn, ");
	if (attributes.IsPipe)
		printf("IsPipe, ");
	if (attributes.MustFree)
		printf("MustFree, ");
	if (attributes.MustSize)
		printf("MustSize, ");
}

void ndr_process_param(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, NDR_PARAM* param)
{
	unsigned char id;
	PFORMAT_STRING pFormat;

	/* Parameter Descriptors: http://msdn.microsoft.com/en-us/library/windows/desktop/aa374362/ */

	if (param->Attributes.IsBasetype)
	{
		pFormat = &param->Type.FormatChar;

		if (param->Attributes.IsSimpleRef)
			pMemory = *(unsigned char**) pMemory;
	}
	else
	{
		pFormat = &pStubMsg->StubDesc->pFormatTypes[param->Type.Offset];

		if (!(param->Attributes.IsByValue))
			pMemory = *(unsigned char**) pMemory;
	}

	id = (pFormat[0] & 0x7F);
}

void ndr_process_params(PMIDL_STUB_MESSAGE pStubMsg, PFORMAT_STRING pFormat, void** fpuArgs, unsigned short numberParams)
{
	unsigned int i;
	NDR_PARAM* params;
	PFORMAT_STRING fmt;
	unsigned char* arg;
	unsigned char type;

	params = (NDR_PARAM*) pFormat;

	printf("Params = \n{\n");

	for (i = 0; i < numberParams; i++)
	{
		float tmp;
		arg = pStubMsg->StackTop + params[i].StackOffset;
		fmt = (PFORMAT_STRING) &pStubMsg->StubDesc->pFormatTypes[params[i].Type.Offset];

#ifdef __x86_64__
		if ((params[i].Attributes.IsBasetype) &&
				!(params[i].Attributes.IsSimpleRef) &&
				((params[i].Type.FormatChar) == FC_FLOAT) && !fpuArgs)
		{
			tmp = *(double*) arg;
			arg = (unsigned char*) &tmp;
		}
#endif

		printf("\t#%d\t", i);

		type = (params[i].Attributes.IsBasetype) ? params[i].Type.FormatChar : *fmt;

		printf(" type %s (0x%02X) ", FC_TYPE_STRINGS[type & 0xBA], type);

		ndr_print_param_attributes(params[i].Attributes);

		if (params[i].Attributes.IsIn)
		{
			ndr_process_param(pStubMsg, arg, &params[i]);
		}

		printf("\n");
	}

	printf("}\n");
}

void NdrClientInitializeNew(PRPC_MESSAGE pRpcMessage, PMIDL_STUB_MESSAGE pStubMsg,
		PMIDL_STUB_DESC pStubDesc, unsigned int ProcNum)
{
	pRpcMessage->Handle = NULL;
	pRpcMessage->RpcFlags = 0;
	pRpcMessage->ProcNum = ProcNum;
	pRpcMessage->DataRepresentation = 0;
	pRpcMessage->ReservedForRuntime = NULL;
	pRpcMessage->RpcInterfaceInformation = pStubDesc->RpcInterfaceInformation;

	pStubMsg->RpcMsg = pRpcMessage;
	pStubMsg->BufferStart = NULL;
	pStubMsg->BufferEnd = NULL;
	pStubMsg->BufferLength = 0;
	pStubMsg->StackTop = NULL;
	pStubMsg->StubDesc = pStubDesc;
}

CLIENT_CALL_RETURN ndr_client_call(PMIDL_STUB_DESC pStubDescriptor, PFORMAT_STRING pFormat, void** stackTop, void** fpuStack)
{
	RPC_MESSAGE rpcMsg;
	unsigned short procNum;
	unsigned short stackSize;
	unsigned char numberParams;
	unsigned char handleType;
	MIDL_STUB_MESSAGE stubMsg;
	INTERPRETER_FLAGS flags;
	INTERPRETER_OPT_FLAGS optFlags;
	INTERPRETER_OPT_FLAGS2 extFlags;
	NDR_PROC_HEADER* procHeader;
	NDR_OI2_PROC_HEADER* oi2ProcHeader;
	CLIENT_CALL_RETURN client_call_return;

	procNum = stackSize = numberParams = 0;
	procHeader = (NDR_PROC_HEADER*) &pFormat[0];

	client_call_return.Pointer = NULL;

	handleType = procHeader->HandleType;
	flags = procHeader->OldOiFlags;
	procNum = procHeader->ProcNum;
	stackSize = procHeader->StackSize;
	pFormat += sizeof(NDR_PROC_HEADER);

	/* The Header: http://msdn.microsoft.com/en-us/library/windows/desktop/aa378707/ */
	/* Procedure Header Descriptor: http://msdn.microsoft.com/en-us/library/windows/desktop/aa374387/ */
	/* Handles: http://msdn.microsoft.com/en-us/library/windows/desktop/aa373932/ */

	printf("Oi Header: HandleType: 0x%02X OiFlags: 0x%02X ProcNum: %d StackSize: 0x%04X\n",
			handleType, *((unsigned char*) &flags),
			(unsigned short) procNum, (unsigned short) stackSize);

	if (handleType > 0)
	{
		/* implicit handle */
		printf("Implicit Handle\n");
		oi2ProcHeader = (NDR_OI2_PROC_HEADER*) &pFormat[0];
		pFormat += sizeof(NDR_OI2_PROC_HEADER);
	}
	else
	{
		/* explicit handle */
		printf("Explicit Handle\n");
		oi2ProcHeader = (NDR_OI2_PROC_HEADER*) &pFormat[6];
		pFormat += sizeof(NDR_OI2_PROC_HEADER) + 6;
	}

	optFlags = oi2ProcHeader->Oi2Flags;
	numberParams = oi2ProcHeader->NumberParams;

	printf("Oi2 Header: Oi2Flags: 0x%02X, NumberParams: %d\n",
			*((unsigned char*) &optFlags),
			(unsigned char) numberParams);

	NdrClientInitializeNew(&rpcMsg, &stubMsg, pStubDescriptor, procNum);

	if (optFlags.HasExtensions)
	{
		NDR_PROC_HEADER_EXTS* extensions = (NDR_PROC_HEADER_EXTS*) pFormat;

		pFormat += extensions->Size;
		extFlags = extensions->Flags2;

		printf("Extensions: Size: %d, flags2: 0x%02X\n",
				extensions->Size, *((unsigned char*) &extensions->Flags2));

#ifdef __x86_64__
		if (extensions->Size > sizeof(*extensions) && fpuStack)
		{
			int i;
			unsigned short fpuMask = *(unsigned short*) (extensions + 1);

			for (i = 0; i < 4; i++, fpuMask >>= 2)
			{
				switch (fpuMask & 3)
				{
					case 1: *(float*) &stackTop[i] = *(float*) &fpuStack[i];
						break;

					case 2: *(double*) &stackTop[i] = *(double*) &fpuStack[i];
						break;
				}
			}
		}
#endif
	}

	stubMsg.StackTop = (unsigned char*) stackTop;

	if (extFlags.HasNewCorrDesc)
	{
		printf("HasNewCorrDesc\n");
	}

	ndr_process_params(&stubMsg, pFormat, fpuStack, numberParams);

	return client_call_return;
}

CLIENT_CALL_RETURN NdrClientCall2(PMIDL_STUB_DESC pStubDescriptor, PFORMAT_STRING pFormat, ...)
{
	va_list args;
	CLIENT_CALL_RETURN client_call_return;

	va_start(args, pFormat);
	client_call_return = ndr_client_call(pStubDescriptor, pFormat, va_arg(args, void**), NULL);
	va_end(args);

	return client_call_return;
}

void* MIDL_user_allocate(size_t cBytes)
{
    return (xmalloc(cBytes));
}

void MIDL_user_free(void* p)
{
	xfree(p);
}
