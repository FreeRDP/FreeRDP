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

CLIENT_CALL_RETURN NdrClientCall2(PMIDL_STUB_DESC pStubDescriptor, PFORMAT_STRING pFormat, ...)
{
	unsigned char oiFlags;
	unsigned short procNum;
	unsigned short stackSize;
	unsigned char numberParams;
	NDR_PROC_HEADER* procHeader;
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

	printf("ProcHeader: ProcNum:%d OiFlags: 0x%02X, handleType: 0x%02X StackSize: %d NumberParams: %d\n",
			procNum, oiFlags, procHeader->HandleType, stackSize, numberParams);

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
