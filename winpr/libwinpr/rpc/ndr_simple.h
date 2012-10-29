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

#ifndef WINPR_RPC_NDR_SIMPLE_H
#define WINPR_RPC_NDR_SIMPLE_H

#include <winpr/rpc.h>

#ifndef _WIN32

void NdrSimpleTypeBufferSize(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat);
void NdrSimpleTypeMarshall(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, unsigned char FormatChar);
void NdrSimpleTypeUnmarshall(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, unsigned char FormatChar);
void NdrSimpleTypeFree(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory, PFORMAT_STRING pFormat);

char NdrGetSimpleTypeBufferAlignment(unsigned char FormatChar);
char NdrGetSimpleTypeBufferSize(unsigned char FormatChar);
char NdrGetSimpleTypeMemorySize(unsigned char FormatChar);
int NdrGetTypeFlags(unsigned char FormatChar);

#endif

#endif /* WINPR_RPC_NDR_SIMPLE_H */
