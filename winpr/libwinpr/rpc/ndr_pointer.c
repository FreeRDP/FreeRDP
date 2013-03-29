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

#include "ndr_pointer.h"
#include "ndr_private.h"

/**
 * Pointer Layout: http://msdn.microsoft.com/en-us/library/windows/desktop/aa374376/
 *
 * pointer_layout<>:
 *
 * FC_PP
 * FC_PAD
 * { pointer_instance_layout<> }*
 * FC_END
 *
 * pointer_instance<8>:
 *
 * offset_to_pointer_in_memory<2>
 * offset_to_pointer_in_buffer<2>
 * pointer_description<4>
 *
 */

PFORMAT_STRING NdrpSkipPointerLayout(PFORMAT_STRING pFormat)
{
	pFormat += 2;

	while (*pFormat != FC_END)
	{
		if (*pFormat == FC_NO_REPEAT)
		{
			/**
			 * FC_NO_REPEAT
			 * FC_PAD
			 * pointer_instance<8>
			 */

			pFormat += 10;
		}
		else if (*pFormat == FC_FIXED_REPEAT)
		{
			unsigned short number_of_pointers;

			/**
			 * FC_FIXED_REPEAT
			 * FC_PAD
			 * iterations<2>
			 * increment<2>
			 * offset_to_array<2>
			 * number_of_pointers<2>
			 * { pointer_instance<8> }*
			 */

			pFormat += 8;
			number_of_pointers = *(unsigned short*) pFormat;
			pFormat += 2 + (number_of_pointers * 8);
		}
		else if (*pFormat == FC_VARIABLE_REPEAT)
		{
			unsigned short number_of_pointers;

			/**
			 * FC_VARIABLE_REPEAT (FC_FIXED_OFFSET | FC_VARIABLE_OFFSET)
			 * FC_PAD ?!
			 * increment<2>
			 * offset_to_array<2>
			 * number_of_pointers<2>
			 * { pointer_instance<8> }*
			 */

			pFormat += 6;
			number_of_pointers = *(unsigned short*) pFormat;
			pFormat += 2 + (number_of_pointers * 8);
		}
		else
		{
			fprintf(stderr, "error: NdrpSkipPointerLayout unexpected 0x%02X\n", *pFormat);
			break;
		}
	}

	return pFormat + 1;
}

/* Pointers: http://msdn.microsoft.com/en-us/library/windows/desktop/hh802750/ */

/**
 * pointer_type<1>
 * pointer_attributes<1>
 * simple_type<1>
 * FC_PAD
 */

/**
 * pointer_type<1>
 * pointer_attributes<1>
 * offset_to_complex_description<2>
 */

void NdrpPointerBufferSize(unsigned char* pMemory, PFORMAT_STRING pFormat, PMIDL_STUB_MESSAGE pStubMsg)
{
	unsigned char type;
	unsigned char attributes;
	PFORMAT_STRING pNextFormat;
	NDR_TYPE_SIZE_ROUTINE pfnSizeRoutine;

	type = pFormat[0];
	attributes = pFormat[1];
	pFormat += 2;

	if (attributes & FC_SIMPLE_POINTER)
		pNextFormat = pFormat;
	else
		pNextFormat = pFormat + *(SHORT*) pFormat;

	switch (type)
	{
		case FC_RP: /* Reference Pointer */
			break;

		case FC_UP: /* Unique Pointer */
		case FC_OP: /* Unique Pointer in an object interface */

			if (!pMemory)
				return;

			break;

		case FC_FP: /* Full Pointer */
			fprintf(stderr, "warning: FC_FP unimplemented\n");
			break;
	}

	if (attributes & FC_POINTER_DEREF)
		pMemory = *(unsigned char**) pMemory;

	pfnSizeRoutine = pfnSizeRoutines[*pNextFormat];

	if (pfnSizeRoutine)
		pfnSizeRoutine(pStubMsg, pMemory, pNextFormat);
}

PFORMAT_STRING NdrpEmbeddedRepeatPointerBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat, unsigned char** ppMemory)
{
	ULONG_PTR MaxCount;
	unsigned char* Memory;
	unsigned char* MemoryCopy;
	unsigned char* MemoryPointer;
	PFORMAT_STRING pFormatNext;
	PFORMAT_STRING pFormatPointers;
	unsigned short increment;
	unsigned short pointer_count;
	unsigned short offset_to_array;
	unsigned short number_of_pointers;

	Memory = pStubMsg->Memory;
	MemoryCopy = pStubMsg->Memory;

	if (*pFormat == FC_FIXED_REPEAT)
	{
		pFormat += 2;
		MaxCount = *(unsigned short*) pFormat;
	}
	else
	{
		if (*pFormat != FC_VARIABLE_REPEAT)
		{
			RpcRaiseException(1766);
			return pFormat;
		}

		MaxCount = pStubMsg->MaxCount;

		if (pFormat[1] == FC_VARIABLE_OFFSET)
		{
			pMemory += pStubMsg->Offset * *((unsigned short*) &pFormat[1]);
		}
	}

	pFormat += 2;
	increment = *(unsigned short*) pFormat;

	pFormat += 2;
	offset_to_array = *(unsigned short*) pFormat;
	pStubMsg->Memory = Memory + offset_to_array;

	pFormat += 2;
	number_of_pointers = *(unsigned short*) pFormat;

	pFormat += 2;

	pFormatPointers = pFormat;

	if (MaxCount)
	{
		do
		{
			MaxCount--;
			pFormatNext = pFormatPointers;
			pointer_count = number_of_pointers;

			if (number_of_pointers)
			{
				do
				{
					pointer_count--;
					MemoryPointer = &pMemory[*(unsigned short*) pFormatNext];
					NdrpPointerBufferSize(MemoryPointer, pFormatNext + 4, pStubMsg);
					pFormatNext += 8;
				}
				while (pointer_count);
			}

			pMemory += increment;
			pStubMsg->Memory += increment;
		}
		while (MaxCount);

		Memory = MemoryCopy;
	}

	pFormat = pFormatPointers + (number_of_pointers * 8);
	pStubMsg->Memory = Memory;

	return pFormat;
}

PFORMAT_STRING NdrpEmbeddedPointerBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat)
{
	ULONG_PTR MaxCount;
	unsigned long Offset;
	unsigned char* Memory;
	char PointerLengthSet;
	PFORMAT_STRING pFormatCopy;
	unsigned long BufferLength;
	unsigned long BufferLengthCopy = 0;
	unsigned long PointerLength;
	unsigned char* pMemoryPtr = NULL;

	pFormatCopy = pFormat;

	if (!pStubMsg->IgnoreEmbeddedPointers)
	{
		PointerLength = pStubMsg->PointerLength;
		PointerLengthSet = (PointerLength != 0);

		if (PointerLengthSet)
		{
			BufferLength = pStubMsg->BufferLength;
			pStubMsg->PointerLength = 0;
			BufferLengthCopy = BufferLength;
			pStubMsg->BufferLength = PointerLength;
		}

		MaxCount = pStubMsg->MaxCount;
		Offset = pStubMsg->Offset;
		Memory = pStubMsg->Memory;
		pStubMsg->Memory = pMemory;
		pFormat = pFormatCopy + 2;

		while (*pFormat != FC_END)
		{
			if (*pFormat == FC_NO_REPEAT)
			{
				NdrpPointerBufferSize(&pMemory[pFormat[2]], &pFormat[6], pStubMsg);
				pFormat += 10;
			}

			pStubMsg->Offset = Offset;
			pStubMsg->MaxCount = MaxCount;

			NdrpEmbeddedRepeatPointerBufferSize(pStubMsg, pMemory, pFormat, &pMemoryPtr);
		}

		pStubMsg->Memory = Memory;

		if (PointerLengthSet)
		{
			pStubMsg->PointerLength = pStubMsg->BufferLength;
			pStubMsg->BufferLength = BufferLengthCopy;
		}
	}

	return pFormat;
}

void NdrPointerBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat)
{
	if (*pFormat != FC_RP)
	{
		NdrpAlignLength((&pStubMsg->BufferLength), 4);
		NdrpIncrementLength((&pStubMsg->BufferLength), 4);
	}

	NdrpPointerBufferSize(pMemory, pFormat, pStubMsg);
}

void NdrByteCountPointerBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat)
{
	fprintf(stderr, "warning: NdrByteCountPointerBufferSize unimplemented\n");
}

#endif
