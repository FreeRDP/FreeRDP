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

#ifndef WINPR_RPC_NDR_POINTER_H
#define WINPR_RPC_NDR_POINTER_H

#include <winpr/rpc.h>

#ifndef _WIN32

PFORMAT_STRING NdrpSkipPointerLayout(PFORMAT_STRING pFormat);

void NdrpPointerBufferSize(unsigned char* pMemory, PFORMAT_STRING pFormat, PMIDL_STUB_MESSAGE pStubMsg);

PFORMAT_STRING NdrpEmbeddedRepeatPointerBufferSize(PMIDL_STUB_MESSAGE pStubMsg,
		unsigned char* pMemory, PFORMAT_STRING pFormat, unsigned char** ppMemory);

PFORMAT_STRING NdrpEmbeddedPointerBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat);

void NdrPointerBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat);
void NdrByteCountPointerBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat);

#endif

#endif /* WINPR_RPC_NDR_POINTER_H */
