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

void ndr_print_param_attributes(unsigned short attributes)
{
	if (attributes & PARAM_ATTRIBUTE_SERVER_ALLOC_SIZE)
		printf("ServerAllocSize, ");
	if (attributes & PARAM_ATTRIBUTE_SAVE_FOR_ASYNC_FINISH)
		printf("SaveForAsyncFinish, ");
	if (attributes & PARAM_ATTRIBUTE_IS_DONT_CALL_FREE_INST)
		printf("IsDontCallFreeInst, ");
	if (attributes & PARAM_ATTRIBUTE_IS_SIMPLE_REF)
		printf("IsSimpleRef, ");
	if (attributes & PARAM_ATTRIBUTE_IS_BY_VALUE)
		printf("IsByValue, ");
	if (attributes & PARAM_ATTRIBUTE_IS_BASE_TYPE)
		printf("IsBaseType, ");
	if (attributes & PARAM_ATTRIBUTE_IS_RETURN)
		printf("IsReturn, ");
	if (attributes & PARAM_ATTRIBUTE_IS_OUT)
		printf("IsOut, ");
	if (attributes & PARAM_ATTRIBUTE_IS_IN)
		printf("IsIn, ");
	if (attributes & PARAM_ATTRIBUTE_IS_PIPE)
		printf("IsPipe, ");
	if (attributes & PARAM_ATTRIBUTE_MUST_FREE)
		printf("MustFree, ");
	if (attributes & PARAM_ATTRIBUTE_MUST_SIZE)
		printf("MustSize, ");
}

void ndr_process_args(PMIDL_STUB_MESSAGE pStubMsg, PFORMAT_STRING pFormat, void** fpuArgs, unsigned short numberParams)
{
	unsigned int i;
	NDR_PARAM* params;
	PFORMAT_STRING fmt;
	unsigned char* arg;

	params = (NDR_PARAM*) pFormat;

	printf("Params = \n{\n");

	for (i = 0; i < numberParams; i++)
	{
		arg = pStubMsg->StackTop + params[i].StackOffset;
		fmt = (PFORMAT_STRING) &pStubMsg->StubDesc->pFormatTypes[params[i].Type.Offset];

		printf("\t#%d\t", i);

		ndr_print_param_attributes(params[i].Attributes);

		if (params[i].Attributes & PARAM_ATTRIBUTE_IS_IN)
		{

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
	unsigned char oiFlags;
	unsigned short procNum;
	unsigned short stackSize;
	unsigned char numberParams;
	NDR_PROC_HEADER* procHeader;
	MIDL_STUB_MESSAGE stubMsg;
	CLIENT_CALL_RETURN client_call_return;

	procNum = stackSize = numberParams = 0;
	procHeader = (NDR_PROC_HEADER*) &pFormat[0];

	client_call_return.Pointer = NULL;

	oiFlags = procHeader->OiFlags;
	procNum = procHeader->ProcNum;
	stackSize = procHeader->StackSize;
	pFormat += sizeof(NDR_PROC_HEADER);

	if (pStubDescriptor->Version >= 0x20000)
	{
		NDR_PROC_OI2_HEADER* procHeaderOi2 = (NDR_PROC_OI2_HEADER*) pFormat;

		oiFlags = procHeaderOi2->Oi2Flags;
		numberParams = procHeaderOi2->NumberParams;

		if (oiFlags & OI2_FLAG_HAS_EXTENSIONS)
		{
			NDR_PROC_HEADER_EXTS* extensions = (NDR_PROC_HEADER_EXTS*) pFormat;

			pFormat += extensions->Size;
		}
	}

	NdrClientInitializeNew(&rpcMsg, &stubMsg, pStubDescriptor, procNum);
	stubMsg.StackTop = (unsigned char*) stackTop;

	printf("ProcHeader: ProcNum:%d OiFlags: 0x%02X, handleType: 0x%02X StackSize: %d NumberParams: %d\n",
			procNum, oiFlags, procHeader->HandleType, stackSize, numberParams);

	ndr_process_args(&stubMsg, pFormat, fpuStack, numberParams);

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
