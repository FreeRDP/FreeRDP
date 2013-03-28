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

#include "ndr_context.h"
#include "ndr_private.h"

void NdrContextHandleBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat)
{
	unsigned char type = *pFormat;

	if (type == FC_BIND_PRIMITIVE)
	{
		/**
		 * FC_BIND_PRIMITIVE
		 * flag<1>
		 * offset<2>
		 */

		fprintf(stderr, "warning: NdrContextHandleBufferSize FC_BIND_PRIMITIVE unimplemented\n");
	}
	else if (type == FC_BIND_GENERIC)
	{
		/**
		 * FC_BIND_GENERIC
		 * flag_and_size<1>
		 * offset<2>
		 * binding_routine_pair_index<1>
		 * FC_PAD
		 */

		fprintf(stderr, "warning: NdrContextHandleBufferSize FC_BIND_GENERIC unimplemented\n");
	}
	else if (type == FC_BIND_CONTEXT)
	{
		/**
		 * FC_BIND_CONTEXT
		 * flags<1>
		 * offset<2>
		 * context_rundown_routine_index<1>
		 * param_num<1>
		 */

		NdrpAlignLength(&(pStubMsg->BufferLength), 4);
		NdrpIncrementLength(&(pStubMsg->BufferLength), 20);
	}
}

#endif
