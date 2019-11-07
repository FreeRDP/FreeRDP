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

#ifndef WINPR_RPC_NDR_CORRELATION_H
#define WINPR_RPC_NDR_CORRELATION_H

#include <winpr/rpc.h>

#ifndef _WIN32

PFORMAT_STRING NdrpComputeCount(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory,
                                PFORMAT_STRING pFormat, ULONG_PTR* pCount);
PFORMAT_STRING NdrpComputeConformance(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory,
                                      PFORMAT_STRING pFormat);
PFORMAT_STRING NdrpComputeVariance(PMIDL_STUB_MESSAGE pStubMsg, unsigned char* pMemory,
                                   PFORMAT_STRING pFormat);

#endif

#endif /* WINPR_RPC_NDR_CORRELATION_H */
