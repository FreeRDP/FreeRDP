/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RPC Client
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

#ifndef FREERDP_CORE_RPC_CLIENT_H
#define FREERDP_CORE_RPC_CLIENT_H

#include "rpc.h"

#include <winpr/interlocked.h>

wStream* rpc_client_fragment_pool_take(rdpRpc* rpc);
int rpc_client_fragment_pool_return(rdpRpc* rpc, wStream* fragment);

RPC_PDU* rpc_client_receive_pool_take(rdpRpc* rpc);
int rpc_client_receive_pool_return(rdpRpc* rpc, RPC_PDU* pdu);

RpcClientCall* rpc_client_call_find_by_id(rdpRpc* rpc, UINT32 CallId);

RpcClientCall* rpc_client_call_new(UINT32 CallId, UINT32 OpNum);
void rpc_client_call_free(RpcClientCall* client_call);

int rpc_send_enqueue_pdu(rdpRpc* rpc, BYTE* buffer, UINT32 length);
int rpc_send_dequeue_pdu(rdpRpc* rpc);

int rpc_recv_enqueue_pdu(rdpRpc* rpc);
RPC_PDU* rpc_recv_dequeue_pdu(rdpRpc* rpc);
RPC_PDU* rpc_recv_peek_pdu(rdpRpc* rpc);

int rpc_client_new(rdpRpc* rpc);
int rpc_client_start(rdpRpc* rpc);
int rpc_client_stop(rdpRpc* rpc);
int rpc_client_free(rdpRpc* rpc);

#endif /* FREERDP_CORE_RPC_CLIENT_H */
