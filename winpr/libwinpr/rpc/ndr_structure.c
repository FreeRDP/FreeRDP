/**
 * WinPR: Windows Portable Runtime
 * Microsoft Remote Procedure Call (MSRPC)
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

#include <winpr/rpc.h>

#ifndef _WIN32

#include "ndr_private.h"
#include "ndr_pointer.h"
#include "ndr_structure.h"

/* Structures: http://msdn.microsoft.com/en-us/library/windows/desktop/aa378695/ */

void NdrSimpleStructBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory,
                               PFORMAT_STRING pFormat)
{
	/**
	 * FC_STRUCT
	 * alignment<1>
	 * memory_size<2>
	 * member_layout<>
	 * FC_END
	 */
	/**
	 * FC_PSTRUCT
	 * alignment<1>
	 * memory_size<2>
	 * pointer_layout<>
	 * member_layout<>
	 * FC_END
	 */
	unsigned char type;
	unsigned char alignment;
	unsigned short memory_size;
	type = pFormat[0];
	alignment = pFormat[1] + 1;
	memory_size = *(unsigned short*)&pFormat[2];
	NdrpAlignLength(&(pStubMsg->BufferLength), alignment);
	NdrpIncrementLength(&(pStubMsg->BufferLength), memory_size);
	pFormat += 4;

	if (*pFormat == FC_PSTRUCT)
		NdrpEmbeddedPointerBufferSize(pStubMsg, pMemory, pFormat);

	WLog_ERR(TAG, "warning: NdrSimpleStructBufferSize unimplemented");
}

void NdrConformantStructBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory,
                                   PFORMAT_STRING pFormat)
{
	/**
	 * FC_CSTRUCT alignment<1>
	 * memory_size<2>
	 * offset_to_array_description<2>
	 * member_layout<>
	 * FC_END
	 */
	/**
	 * FC_CPSTRUCT alignment<1>
	 * memory_size<2>
	 * offset_to_array_description<2>
	 * pointer_layout<>
	 * member_layout<> FC_END
	 */
	WLog_ERR(TAG, "warning: NdrConformantStructBufferSize unimplemented");
}

void NdrConformantVaryingStructBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory,
                                          PFORMAT_STRING pFormat)
{
	/**
	 * FC_CVSTRUCT alignment<1>
	 * memory_size<2>
	 * offset_to_array_description<2>
	 * [pointer_layout<>]
	 * layout<>
	 * FC_END
	 */
	WLog_ERR(TAG, "warning: NdrConformantVaryingStructBufferSize unimplemented");
}

static ULONG NdrComplexStructMemberSize(PMIDL_STUB_MESSAGE pStubMsg, PFORMAT_STRING pFormat)
{
	ULONG size = 0;

	while (*pFormat != FC_END)
	{
		switch (*pFormat)
		{
			case FC_BYTE:
			case FC_CHAR:
			case FC_SMALL:
			case FC_USMALL:
				size += sizeof(BYTE);
				break;

			case FC_WCHAR:
			case FC_SHORT:
			case FC_USHORT:
			case FC_ENUM16:
				size += sizeof(USHORT);
				break;

			case FC_LONG:
			case FC_ULONG:
			case FC_ENUM32:
				size += sizeof(ULONG);
				break;

			case FC_INT3264:
			case FC_UINT3264:
				size += sizeof(INT_PTR);
				break;

			case FC_FLOAT:
				size += sizeof(FLOAT);
				break;

			case FC_DOUBLE:
				size += sizeof(DOUBLE);
				break;

			case FC_HYPER:
				size += sizeof(ULONGLONG);
				break;

			case FC_ERROR_STATUS_T:
				size += sizeof(error_status_t);
				break;

			case FC_IGNORE:
				break;

			case FC_RP:
			case FC_UP:
			case FC_OP:
			case FC_FP:
			case FC_POINTER:
				size += sizeof(void*);

				if (*pFormat != FC_POINTER)
					pFormat += 4;

				break;

			case FC_ALIGNM2:
				NdrpAlignLength(&size, 2);
				break;

			case FC_ALIGNM4:
				NdrpAlignLength(&size, 4);
				break;

			case FC_ALIGNM8:
				NdrpAlignLength(&size, 8);
				break;

			case FC_STRUCTPAD1:
			case FC_STRUCTPAD2:
			case FC_STRUCTPAD3:
			case FC_STRUCTPAD4:
			case FC_STRUCTPAD5:
			case FC_STRUCTPAD6:
			case FC_STRUCTPAD7:
				size += *pFormat - FC_STRUCTPAD1 + 1;
				break;

			case FC_PAD:
				break;

			case FC_EMBEDDED_COMPLEX:
				WLog_ERR(TAG,
				         "warning: NdrComplexStructMemberSize FC_EMBEDDED_COMPLEX unimplemented");
				break;

			default:
				WLog_ERR(TAG, "warning: NdrComplexStructMemberSize 0x%02X unimplemented", *pFormat);
				break;
		}

		pFormat++;
	}

	return size;
}

void NdrComplexStructBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory,
                                PFORMAT_STRING pFormat)
{
	/**
	 * FC_BOGUS_STRUCT
	 * alignment<1>
	 * memory_size<2>
	 * offset_to_conformant_array_description<2>
	 * offset_to_pointer_layout<2>
	 * member_layout<>
	 * FC_END
	 * [pointer_layout<>]
	 */
	ULONG_PTR MaxCount;
	unsigned long Offset;
	unsigned long ActualCount;
	unsigned char* pMemoryCopy;
	unsigned char type;
	unsigned char alignment;
	unsigned short memory_size;
	unsigned char* pointer_layout;
	unsigned char* conformant_array_description;
	unsigned short offset_to_pointer_layout;
	unsigned short offset_to_conformant_array_description;
	type = pFormat[0];
	pMemoryCopy = pMemory;
	pointer_layout = conformant_array_description = NULL;

	if (type != FC_BOGUS_STRUCT)
	{
		WLog_ERR(TAG, "error: expected FC_BOGUS_STRUCT, got 0x%02X", type);
		return;
	}

	alignment = pFormat[1] + 1;
	memory_size = *(unsigned short*)&pFormat[2];
	NdrpAlignLength(&(pStubMsg->BufferLength), alignment);

	if (!pStubMsg->IgnoreEmbeddedPointers && !pStubMsg->PointerLength)
	{
		unsigned long BufferLengthCopy = pStubMsg->BufferLength;
		int IgnoreEmbeddedPointersCopy = pStubMsg->IgnoreEmbeddedPointers;
		pStubMsg->IgnoreEmbeddedPointers = 1;
		NdrComplexStructBufferSize(pStubMsg, pMemory, pFormat);
		pStubMsg->IgnoreEmbeddedPointers = IgnoreEmbeddedPointersCopy;
		pStubMsg->PointerLength = pStubMsg->BufferLength;
		pStubMsg->BufferLength = BufferLengthCopy;
	}

	pFormat += 4;
	offset_to_conformant_array_description = *(unsigned short*)&pFormat[0];

	if (offset_to_conformant_array_description)
		conformant_array_description =
		    (unsigned char*)pFormat + offset_to_conformant_array_description;

	pFormat += 2;
	offset_to_pointer_layout = *(unsigned short*)&pFormat[0];

	if (offset_to_pointer_layout)
		pointer_layout = (unsigned char*)pFormat + offset_to_pointer_layout;

	pFormat += 2;
	pStubMsg->Memory = pMemory;

	if (conformant_array_description)
	{
		ULONG size;
		unsigned char array_type;
		array_type = conformant_array_description[0];
		size = NdrComplexStructMemberSize(pStubMsg, pFormat);
		WLog_ERR(TAG, "warning: NdrComplexStructBufferSize array_type: 0x%02X unimplemented",
		         array_type);
		NdrpComputeConformance(pStubMsg, pMemory + size, conformant_array_description);
		NdrpComputeVariance(pStubMsg, pMemory + size, conformant_array_description);
		MaxCount = pStubMsg->MaxCount;
		ActualCount = pStubMsg->ActualCount;
		Offset = pStubMsg->Offset;
	}

	if (conformant_array_description)
	{
		unsigned char array_type;
		array_type = conformant_array_description[0];
		pStubMsg->MaxCount = MaxCount;
		pStubMsg->ActualCount = ActualCount;
		pStubMsg->Offset = Offset;
		WLog_ERR(TAG, "warning: NdrComplexStructBufferSize array_type: 0x%02X unimplemented",
		         array_type);
	}

	pStubMsg->Memory = pMemoryCopy;

	if (pStubMsg->PointerLength > 0)
	{
		pStubMsg->BufferLength = pStubMsg->PointerLength;
		pStubMsg->PointerLength = 0;
	}
}

#endif
