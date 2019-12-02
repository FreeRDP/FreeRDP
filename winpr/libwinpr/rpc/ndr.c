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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <winpr/ndr.h>

#ifndef _WIN32

#include "ndr_array.h"
#include "ndr_context.h"
#include "ndr_pointer.h"
#include "ndr_simple.h"
#include "ndr_string.h"
#include "ndr_structure.h"
#include "ndr_union.h"

#include "ndr_private.h"

#include "../log.h"
#define TAG WINPR_TAG("rpc")

/**
 * MSRPC NDR Types Technical Overview:
 * http://dvlabs.tippingpoint.com/blog/2007/11/24/msrpc-ndr-types/
 */

static void NdrPrintParamAttributes(PARAM_ATTRIBUTES attributes)
{
	if (attributes.ServerAllocSize)
		WLog_INFO(TAG, "ServerAllocSize, ");

	if (attributes.SaveForAsyncFinish)
		WLog_INFO(TAG, "SaveForAsyncFinish, ");

	if (attributes.IsDontCallFreeInst)
		WLog_INFO(TAG, "IsDontCallFreeInst, ");

	if (attributes.IsSimpleRef)
		WLog_INFO(TAG, "IsSimpleRef, ");

	if (attributes.IsByValue)
		WLog_INFO(TAG, "IsByValue, ");

	if (attributes.IsBasetype)
		WLog_INFO(TAG, "IsBaseType, ");

	if (attributes.IsReturn)
		WLog_INFO(TAG, "IsReturn, ");

	if (attributes.IsOut)
		WLog_INFO(TAG, "IsOut, ");

	if (attributes.IsIn)
		WLog_INFO(TAG, "IsIn, ");

	if (attributes.IsPipe)
		WLog_INFO(TAG, "IsPipe, ");

	if (attributes.MustFree)
		WLog_INFO(TAG, "MustFree, ");

	if (attributes.MustSize)
		WLog_INFO(TAG, "MustSize, ");
}

static void NdrProcessParam(PMIDL_STUB_MESSAGE pStubMsg, NDR_PHASE phase, unsigned char* pMemory,
                            NDR_PARAM* param)
{
	unsigned char type;
	PFORMAT_STRING pFormat;

	/* Parameter Descriptors: http://msdn.microsoft.com/en-us/library/windows/desktop/aa374362/ */

	if (param->Attributes.IsBasetype)
	{
		pFormat = &param->Type.FormatChar;

		if (param->Attributes.IsSimpleRef)
			pMemory = *(unsigned char**)pMemory;
	}
	else
	{
		pFormat = &pStubMsg->StubDesc->pFormatTypes[param->Type.Offset];

		if (!(param->Attributes.IsByValue))
			pMemory = *(unsigned char**)pMemory;
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

static void NdrProcessParams(PMIDL_STUB_MESSAGE pStubMsg, PFORMAT_STRING pFormat, NDR_PHASE phase,
                             void** fpuArgs, unsigned short numberParams)
{
	unsigned int i;
	NDR_PARAM* params;
	PFORMAT_STRING fmt;
	unsigned char* arg;
	unsigned char type;
	params = (NDR_PARAM*)pFormat;
	WLog_INFO(TAG, "Params = ");

	for (i = 0; i < numberParams; i++)
	{
#ifdef __x86_64__
		float tmp;
#endif
		arg = pStubMsg->StackTop + params[i].StackOffset;
		fmt = (PFORMAT_STRING)&pStubMsg->StubDesc->pFormatTypes[params[i].Type.Offset];
#ifdef __x86_64__

		if ((params[i].Attributes.IsBasetype) && !(params[i].Attributes.IsSimpleRef) &&
		    ((params[i].Type.FormatChar) == FC_FLOAT) && !fpuArgs)
		{
			tmp = *(double*)arg;
			arg = (unsigned char*)&tmp;
		}

#endif
		type = (params[i].Attributes.IsBasetype) ? params[i].Type.FormatChar : *fmt;
		WLog_INFO(TAG, "'\t#%u\ttype %s (0x%02X) ", i, FC_TYPE_STRINGS[type], type);
		NdrPrintParamAttributes(params[i].Attributes);

		if (params[i].Attributes.IsIn)
		{
			NdrProcessParam(pStubMsg, phase, arg, &params[i]);
		}
	}
}

static void NdrClientInitializeNew(PRPC_MESSAGE pRpcMessage, PMIDL_STUB_MESSAGE pStubMsg,
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

static void NdrPrintOptFlags(INTERPRETER_OPT_FLAGS optFlags)
{
	if (optFlags.ClientMustSize)
		WLog_INFO(TAG, "ClientMustSize, ");

	if (optFlags.ServerMustSize)
		WLog_INFO(TAG, "ServerMustSize, ");

	if (optFlags.HasAsyncUuid)
		WLog_INFO(TAG, "HasAsyncUiid, ");

	if (optFlags.HasAsyncHandle)
		WLog_INFO(TAG, "HasAsyncHandle, ");

	if (optFlags.HasReturn)
		WLog_INFO(TAG, "HasReturn, ");

	if (optFlags.HasPipes)
		WLog_INFO(TAG, "HasPipes, ");

	if (optFlags.HasExtensions)
		WLog_INFO(TAG, "HasExtensions, ");
}

static void NdrPrintExtFlags(INTERPRETER_OPT_FLAGS2 extFlags)
{
	if (extFlags.HasNewCorrDesc)
		WLog_INFO(TAG, "HasNewCorrDesc, ");

	if (extFlags.ClientCorrCheck)
		WLog_INFO(TAG, "ClientCorrCheck, ");

	if (extFlags.ServerCorrCheck)
		WLog_INFO(TAG, "ServerCorrCheck, ");

	if (extFlags.HasNotify)
		WLog_INFO(TAG, "HasNotify, ");

	if (extFlags.HasNotify2)
		WLog_INFO(TAG, "HasNotify2, ");
}

CLIENT_CALL_RETURN NdrClientCall(PMIDL_STUB_DESC pStubDescriptor, PFORMAT_STRING pFormat,
                                 void** stackTop, void** fpuStack)
{
	RPC_MESSAGE rpcMsg;
	unsigned short procNum;
	unsigned short stackSize;
	unsigned char numberParams;
	unsigned char handleType;
	MIDL_STUB_MESSAGE stubMsg;
	INTERPRETER_FLAGS flags;
	INTERPRETER_OPT_FLAGS optFlags;
	NDR_PROC_HEADER* procHeader;
	NDR_OI2_PROC_HEADER* oi2ProcHeader;
	CLIENT_CALL_RETURN client_call_return;
	procNum = stackSize = numberParams = 0;
	procHeader = (NDR_PROC_HEADER*)&pFormat[0];
	client_call_return.Pointer = NULL;
	handleType = procHeader->HandleType;
	flags = procHeader->OldOiFlags;
	procNum = procHeader->ProcNum;
	stackSize = procHeader->StackSize;
	pFormat += sizeof(NDR_PROC_HEADER);
	/* The Header: http://msdn.microsoft.com/en-us/library/windows/desktop/aa378707/ */
	/* Procedure Header Descriptor:
	 * http://msdn.microsoft.com/en-us/library/windows/desktop/aa374387/ */
	/* Handles: http://msdn.microsoft.com/en-us/library/windows/desktop/aa373932/ */
	WLog_DBG(TAG, "Oi Header: HandleType: 0x%02X OiFlags: 0x%02X ProcNum: %hu StackSize: 0x%04X",
	         handleType, *((unsigned char*)&flags), procNum, stackSize);

	if (handleType > 0)
	{
		/* implicit handle */
		WLog_INFO(TAG, "Implicit Handle");
		oi2ProcHeader = (NDR_OI2_PROC_HEADER*)&pFormat[0];
		pFormat += sizeof(NDR_OI2_PROC_HEADER);
	}
	else
	{
		/* explicit handle */
		WLog_INFO(TAG, "Explicit Handle");
		oi2ProcHeader = (NDR_OI2_PROC_HEADER*)&pFormat[6];
		pFormat += sizeof(NDR_OI2_PROC_HEADER) + 6;
	}

	optFlags = oi2ProcHeader->Oi2Flags;
	numberParams = oi2ProcHeader->NumberParams;
	WLog_DBG(TAG,
	         "Oi2 Header: Oi2Flags: 0x%02X, NumberParams: %u ClientBufferSize: %hu "
	         "ServerBufferSize: %hu",
	         *((unsigned char*)&optFlags), numberParams, oi2ProcHeader->ClientBufferSize,
	         oi2ProcHeader->ServerBufferSize);
	WLog_INFO(TAG, "Oi2Flags: ");
	NdrPrintOptFlags(optFlags);
	NdrClientInitializeNew(&rpcMsg, &stubMsg, pStubDescriptor, procNum);

	if (optFlags.HasExtensions)
	{
		INTERPRETER_OPT_FLAGS2 extFlags;
		NDR_PROC_HEADER_EXTS* extensions = (NDR_PROC_HEADER_EXTS*)pFormat;
		pFormat += extensions->Size;
		extFlags = extensions->Flags2;
		WLog_DBG(TAG, "Extensions: Size: %hhu, flags2: 0x%02X", extensions->Size,
		         *((unsigned char*)&extensions->Flags2));
#ifdef __x86_64__

		if (extensions->Size > sizeof(*extensions) && fpuStack)
		{
			int i;
			unsigned short fpuMask = *(unsigned short*)(extensions + 1);

			for (i = 0; i < 4; i++, fpuMask >>= 2)
			{
				switch (fpuMask & 3)
				{
					case 1:
						*(float*)&stackTop[i] = *(float*)&fpuStack[i];
						break;

					case 2:
						*(double*)&stackTop[i] = *(double*)&fpuStack[i];
						break;
				}
			}
		}

#endif
		WLog_INFO(TAG, "ExtFlags: ");
		NdrPrintExtFlags(extFlags);
	}

	stubMsg.StackTop = (unsigned char*)stackTop;
	NdrProcessParams(&stubMsg, pFormat, NDR_PHASE_SIZE, fpuStack, numberParams);
	WLog_DBG(TAG, "stubMsg BufferLength: %" PRIu32 "", stubMsg.BufferLength);
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

#endif
