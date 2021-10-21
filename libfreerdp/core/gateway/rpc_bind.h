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

#ifndef FREERDP_LIB_CORE_GATEWAY_RPC_BIND_H
#define FREERDP_LIB_CORE_GATEWAY_RPC_BIND_H

#include "rpc.h"

#include <winpr/wtypes.h>
#include <freerdp/api.h>

FREERDP_LOCAL extern const p_uuid_t TSGU_UUID;
#define TSGU_SYNTAX_IF_VERSION 0x00030001

FREERDP_LOCAL extern const p_uuid_t NDR_UUID;
#define NDR_SYNTAX_IF_VERSION 0x00000002

FREERDP_LOCAL extern const p_uuid_t BTFN_UUID;
#define BTFN_SYNTAX_IF_VERSION 0x00000001

FREERDP_LOCAL int rpc_send_bind_pdu(rdpRpc* rpc);
FREERDP_LOCAL BOOL rpc_recv_bind_ack_pdu(rdpRpc* rpc, wStream* s);
FREERDP_LOCAL int rpc_send_rpc_auth_3_pdu(rdpRpc* rpc);

#endif /* FREERDP_LIB_CORE_GATEWAY_RPC_BIND_H */
