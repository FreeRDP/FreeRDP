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

#ifndef WINPR_RPC_NDR_PRIVATE_H
#define WINPR_RPC_NDR_PRIVATE_H

#include <winpr/rpc.h>

#ifndef _WIN32

void NdrpAlignLength(ULONG* length, unsigned int alignment);
void NdrpIncrementLength(ULONG* length, unsigned int size);

extern const NDR_TYPE_SIZE_ROUTINE pfnSizeRoutines[];
extern const NDR_TYPE_MARSHALL_ROUTINE pfnMarshallRoutines[];
extern const NDR_TYPE_UNMARSHALL_ROUTINE pfnUnmarshallRoutines[];
extern const NDR_TYPE_FREE_ROUTINE pfnFreeRoutines[];

extern const char SimpleTypeAlignment[];
extern const char SimpleTypeBufferSize[];
extern const char SimpleTypeMemorySize[];

extern const char NdrTypeFlags[];
extern const char* FC_TYPE_STRINGS[];

#include "ndr_correlation.h"

#endif

#endif /* WINPR_RPC_NDR_PRIVATE_H */
