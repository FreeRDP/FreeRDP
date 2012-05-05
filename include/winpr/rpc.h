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

#ifndef WINPR_RPC_H
#define WINPR_RPC_H

#include <winpr/winpr.h>
#include <winpr/wtypes.h>

#define RPC_VAR_ENTRY	__cdecl

typedef long RPC_STATUS;

typedef void* I_RPC_HANDLE;
typedef I_RPC_HANDLE RPC_BINDING_HANDLE;
typedef RPC_BINDING_HANDLE handle_t;

typedef PCONTEXT_HANDLE PTUNNEL_CONTEXT_HANDLE_NOSERIALIZE;
typedef PCONTEXT_HANDLE PTUNNEL_CONTEXT_HANDLE_SERIALIZE;

typedef PCONTEXT_HANDLE PCHANNEL_CONTEXT_HANDLE_NOSERIALIZE;
typedef PCONTEXT_HANDLE PCHANNEL_CONTEXT_HANDLE_SERIALIZE;

#include <winpr/ndr.h>
#include <winpr/midl.h>

void RpcRaiseException(RPC_STATUS exception);

#endif /* WINPR_RPC_H */
