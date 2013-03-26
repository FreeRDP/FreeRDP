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

#include "ndr_array.h"

#ifndef _WIN32

void NdrConformantArrayBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat)
{
	/**
	 * FC_CARRAY
	 * alignment<1>
	 * element_size<2>
	 * conformance_description<>
	 * [pointer_layout<>]
	 * element_description<>
	 * FC_END
	 */

	unsigned char type;
	unsigned char alignment;
	unsigned short element_size;

	type = pFormat[0];
	alignment = pFormat[1] + 1;
	element_size = *(unsigned short*) &pFormat[2];

	if (type != FC_CARRAY)
	{
		fprintf(stderr, "error: expected FC_CARRAY, got 0x%02X\n", type);
		return;
	}

	fprintf(stderr, "warning: NdrConformantArrayBufferSize unimplemented\n");
}

void NdrConformantVaryingArrayBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat)
{
	/**
	 * FC_CVARRAY
	 * alignment<1>
	 * element_size<2>
	 * conformance_description<>
	 * variance_description<>
	 * [pointer_layout<>]
	 * element_description<>
	 * FC_END
	 */

	fprintf(stderr, "warning: NdrConformantVaryingArrayBufferSize unimplemented\n");
}

void NdrFixedArrayBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat)
{
	/**
	 * FC_SMFARRAY
	 * alignment<1>
	 * total_size<2>
	 * [pointer_layout<>]
	 * element_description<>
	 * FC_END
	 */

	/**
	 * FC_LGFARRAY
	 * alignment<1>
	 * total_size<4>
	 * [pointer_layout<>]
	 * element_description<>
	 * FC_END
	 */

	fprintf(stderr, "warning: NdrFixedArrayBufferSize unimplemented\n");
}

void NdrVaryingArrayBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat)
{
	/**
	 * FC_SMVARRAY
	 * alignment<1>
	 * total_size<2>
	 * number_elements<2>
	 * element_size<2>
	 * variance_description<>
	 * [pointer_layout<>]
	 * element_description<>
	 * FC_END
	 */

	/**
	 * FC_LGVARRAY
	 * alignment<1>
	 * total_size<4>
	 * number_elements<4>
	 * element_size<2>
	 * variance_description<4>
	 * [pointer_layout<>]
	 * element_description<>
	 * FC_END
	 */

	fprintf(stderr, "warning: NdrVaryingArrayBufferSize unimplemented\n");
}

void NdrComplexArrayBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat)
{
	/**
	 * FC_BOGUS_ARRAY
	 * alignment<1>
	 * number_of_elements<2>
	 * conformance_description<>
	 * variance_description<>
	 * element_description<>
	 * FC_END
	 */

	fprintf(stderr, "warning: NdrComplexArrayBufferSize unimplemented\n");
}

#endif
