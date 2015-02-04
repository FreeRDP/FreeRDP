/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RPC Secure Context Binding
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

#ifndef FREERDP_CORE_RPC_BIND_H
#define FREERDP_CORE_RPC_BIND_H

#include "rpc.h"

#include <winpr/wtypes.h>

const p_uuid_t TSGU_UUID;
#define TSGU_SYNTAX_IF_VERSION	0x00030001

const p_uuid_t NDR_UUID;
#define NDR_SYNTAX_IF_VERSION	0x00000002

const p_uuid_t BTFN_UUID;
#define BTFN_SYNTAX_IF_VERSION	0x00000001

int rpc_send_bind_pdu(rdpRpc* rpc);
int rpc_recv_bind_ack_pdu(rdpRpc* rpc, BYTE* buffer, UINT32 length);
int rpc_send_rpc_auth_3_pdu(rdpRpc* rpc);

#endif /* FREERDP_CORE_RPC_BIND_H */
