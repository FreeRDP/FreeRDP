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

#include "ndr_correlation.h"
#include "ndr_private.h"

/*
 * Correlation Descriptors: http://msdn.microsoft.com/en-us/library/windows/desktop/aa373607/
 *
 * correlation_type<1>
 * correlation_operator<1>
 * offset<2>
 * [robust_flags<2>]
 *
 */

PFORMAT_STRING NdrpComputeCount(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat, ULONG_PTR* pCount)
{
	LPVOID ptr = NULL;
	ULONG_PTR data = 0;
	unsigned char type;
	unsigned short offset;
	unsigned char conformance;
	unsigned char correlation_type;
	unsigned char correlation_operator;
	correlation_type = pFormat[0];
	type = correlation_type & 0x0F;
	conformance = correlation_type & 0xF0;
	correlation_operator = pFormat[1];
	offset = *(unsigned short*) & pFormat[2];

	if (conformance == FC_NORMAL_CONFORMANCE)
	{
		ptr = pMemory;
	}
	else if (conformance == FC_POINTER_CONFORMANCE)
	{
		ptr = pStubMsg->Memory;
	}
	else if (conformance == FC_TOP_LEVEL_CONFORMANCE)
	{
		ptr = pStubMsg->StackTop;
	}
	else if (conformance == FC_CONSTANT_CONFORMANCE)
	{
		data = offset | ((DWORD) pFormat[1] << 16);
		*pCount = data;
	}
	else if (conformance == FC_TOP_LEVEL_MULTID_CONFORMANCE)
	{
		if (pStubMsg->StackTop)
			ptr = pStubMsg->StackTop;
	}
	else
		return pFormat;

	switch (correlation_operator)
	{
		case FC_DEREFERENCE:
			ptr = *(LPVOID*)((char*) ptr + offset);
			break;

		case FC_DIV_2:
			ptr = (char*) ptr + offset;
			break;

		case FC_MULT_2:
			ptr = (char*) ptr + offset;
			break;

		case FC_SUB_1:
			ptr = (char*) ptr + offset;
			break;

		case FC_ADD_1:
			ptr = (char*) ptr + offset;
			break;

		case FC_CALLBACK:
			{
				WLog_ERR(TAG, "warning: NdrpComputeConformance FC_CALLBACK unimplemented");
			}
			break;
	}

	if (!ptr)
		return pFormat;

	switch (type)
	{
		case FC_LONG:
			data = *(LONG*) ptr;
			break;

		case FC_ULONG:
			data = *(ULONG*) ptr;
			break;

		case FC_SHORT:
			data = *(SHORT*) ptr;
			break;

		case FC_USHORT:
			data = *(USHORT*) ptr;
			break;

		case FC_CHAR:
		case FC_SMALL:
			data = *(CHAR*) ptr;
			break;

		case FC_BYTE:
		case FC_USMALL:
			data = *(BYTE*) ptr;
			break;

		case FC_HYPER:
			data = (ULONG_PTR) *(ULONGLONG*) ptr;
			break;
	}

	switch (correlation_operator)
	{
		case FC_ZERO:
		case FC_DEREFERENCE:
			*pCount = data;
			break;

		case FC_DIV_2:
			*pCount = data / 1;
			break;

		case FC_MULT_2:
			*pCount = data * 1;
			break;

		case FC_SUB_1:
			*pCount = data - 1;
			break;

		case FC_ADD_1:
			*pCount = data + 1;
			break;

		case FC_CALLBACK:
			break;
	}

	if (pStubMsg->fHasNewCorrDesc)
		pFormat += 6;
	else
		pFormat += 4;

	return pFormat;
}

PFORMAT_STRING NdrpComputeConformance(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat)
{
	return NdrpComputeCount(pStubMsg, pMemory, pFormat, &pStubMsg->MaxCount);
}

PFORMAT_STRING NdrpComputeVariance(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat)
{
	ULONG_PTR ActualCount = pStubMsg->ActualCount;
	pFormat = NdrpComputeCount(pStubMsg, pMemory, pFormat, &ActualCount);
	pStubMsg->ActualCount = (ULONG) ActualCount;
	return pFormat;
}

#endif
