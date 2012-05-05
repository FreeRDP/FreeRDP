/**
 * WinPR: Windows Portable Runtime
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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <winpr/ndr.h>

#include "ndr_array.h"
#include "ndr_context.h"
#include "ndr_pointer.h"
#include "ndr_simple.h"
#include "ndr_string.h"
#include "ndr_structure.h"
#include "ndr_union.h"

#include "ndr_private.h"

/**
 * MSRPC NDR Types Technical Overview:
 * http://dvlabs.tippingpoint.com/blog/2007/11/24/msrpc-ndr-types/
 */

void NdrPrintParamAttributes(PARAM_ATTRIBUTES attributes)
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

void NdrProcessParam(PMIDL_STUB_MESSAGE pStubMsg, NDR_PHASE phase, unsigned char* pMemory, NDR_PARAM* param)
{
	unsigned char type;
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

	type = (pFormat[0] & 0x7F);

	if (type > FC_PAD)
		return;

	if (phase == NDR_PHASE_SIZE)
	{
		NDR_TYPE_SIZE_ROUTINE pfnSizeRoutine = pfnSizeRoutines[type];

		if (pfnSizeRoutine)
			pfnSizeRoutine(pStubMsg, pMemory, pFormat);
	}
	else if (phase == NDR_PHASE_MARSHALL)
	{
		NDR_TYPE_MARSHALL_ROUTINE pfnMarshallRoutine = pfnMarshallRoutines[type];

		if (pfnMarshallRoutine)
			pfnMarshallRoutine(pStubMsg, pMemory, *pFormat);
	}
	else if (phase == NDR_PHASE_UNMARSHALL)
	{
		NDR_TYPE_UNMARSHALL_ROUTINE pfnUnmarshallRoutine = pfnUnmarshallRoutines[type];

		if (pfnUnmarshallRoutine)
			pfnUnmarshallRoutine(pStubMsg, pMemory, *pFormat);
	}
	else if (phase == NDR_PHASE_FREE)
	{
		NDR_TYPE_FREE_ROUTINE pfnFreeRoutine = pfnFreeRoutines[type];

		if (pfnFreeRoutine)
			pfnFreeRoutine(pStubMsg, pMemory, pFormat);
	}
}

void NdrProcessParams(PMIDL_STUB_MESSAGE pStubMsg, PFORMAT_STRING pFormat, NDR_PHASE phase, void** fpuArgs, unsigned short numberParams)
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
		arg = pStubMsg->StackTop + params[i].StackOffset;
		fmt = (PFORMAT_STRING) &pStubMsg->StubDesc->pFormatTypes[params[i].Type.Offset];

#ifdef __x86_64__
		if ((params[i].Attributes.IsBasetype) &&
				!(params[i].Attributes.IsSimpleRef) &&
				((params[i].Type.FormatChar) == FC_FLOAT) && !fpuArgs)
		{
			float tmp = *(double*) arg;
			arg = (unsigned char*) &tmp;
		}
#endif

		printf("\t#%d\t", i);

		type = (params[i].Attributes.IsBasetype) ? params[i].Type.FormatChar : *fmt;

		printf(" type %s (0x%02X) ", FC_TYPE_STRINGS[type], type);

		NdrPrintParamAttributes(params[i].Attributes);

		if (params[i].Attributes.IsIn)
		{
			NdrProcessParam(pStubMsg, phase, arg, &params[i]);
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
	pStubMsg->IgnoreEmbeddedPointers = 0;
	pStubMsg->PointerLength = 0;
}

void NdrPrintOptFlags(INTERPRETER_OPT_FLAGS optFlags)
{
	if (optFlags.ClientMustSize)
		printf("ClientMustSize, ");
	if (optFlags.ServerMustSize)
		printf("ServerMustSize, ");
	if (optFlags.HasAsyncUuid)
		printf("HasAsyncUiid, ");
	if (optFlags.HasAsyncHandle)
		printf("HasAsyncHandle, ");
	if (optFlags.HasReturn)
		printf("HasReturn, ");
	if (optFlags.HasPipes)
		printf("HasPipes, ");
	if (optFlags.HasExtensions)
		printf("HasExtensions, ");
}

void NdrPrintExtFlags(INTERPRETER_OPT_FLAGS2 extFlags)
{
	if (extFlags.HasNewCorrDesc)
		printf("HasNewCorrDesc, ");
	if (extFlags.ClientCorrCheck)
		printf("ClientCorrCheck, ");
	if (extFlags.ServerCorrCheck)
		printf("ServerCorrCheck, ");
	if (extFlags.HasNotify)
		printf("HasNotify, ");
	if (extFlags.HasNotify2)
		printf("HasNotify2, ");
}

CLIENT_CALL_RETURN NdrClientCall(PMIDL_STUB_DESC pStubDescriptor, PFORMAT_STRING pFormat, void** stackTop, void** fpuStack)
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

	printf("Oi2 Header: Oi2Flags: 0x%02X, NumberParams: %d ClientBufferSize: %d ServerBufferSize: %d\n",
			*((unsigned char*) &optFlags),
			(unsigned char) numberParams,
			oi2ProcHeader->ClientBufferSize,
			oi2ProcHeader->ServerBufferSize);

	printf("Oi2Flags: ");
	NdrPrintOptFlags(optFlags);
	printf("\n");

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

	printf("ExtFlags: ");
	NdrPrintExtFlags(extFlags);
	printf("\n");

	NdrProcessParams(&stubMsg, pFormat, NDR_PHASE_SIZE, fpuStack, numberParams);

	printf("stubMsg BufferLength: %d\n", (int) stubMsg.BufferLength);

	return client_call_return;
}

CLIENT_CALL_RETURN NdrClientCall2(PMIDL_STUB_DESC pStubDescriptor, PFORMAT_STRING pFormat, ...)
{
	va_list args;
	CLIENT_CALL_RETURN client_call_return;

	va_start(args, pFormat);
	client_call_return = NdrClientCall(pStubDescriptor, pFormat, va_arg(args, void**), NULL);
	va_end(args);

	return client_call_return;
}
