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

#ifndef FREERDP_LIB_CORE_GATEWAY_RPC_CLIENT_H
#define FREERDP_LIB_CORE_GATEWAY_RPC_CLIENT_H

#include <winpr/wtypes.h>
#include <winpr/stream.h>
#include <freerdp/api.h>

#include "rpc.h"

FREERDP_LOCAL RpcClientCall* rpc_client_call_find_by_id(RpcClient* client, UINT32 CallId);

FREERDP_LOCAL RpcClientCall* rpc_client_call_new(UINT32 CallId, UINT32 OpNum);
FREERDP_LOCAL void rpc_client_call_free(RpcClientCall* client_call);

FREERDP_LOCAL int rpc_in_channel_send_pdu(RpcInChannel* inChannel, const BYTE* buffer,
                                          size_t length);

FREERDP_LOCAL int rpc_client_in_channel_recv(rdpRpc* rpc);
FREERDP_LOCAL int rpc_client_out_channel_recv(rdpRpc* rpc);

FREERDP_LOCAL int rpc_client_receive_pipe_read(RpcClient* client, BYTE* buffer, size_t length);

FREERDP_LOCAL BOOL rpc_client_write_call(rdpRpc* rpc, wStream* s, UINT16 opnum);

FREERDP_LOCAL RpcClient* rpc_client_new(rdpContext* context, UINT32 max_recv_frag);
FREERDP_LOCAL void rpc_client_free(RpcClient* client);

#endif /* FREERDP_LIB_CORE_GATEWAY_RPC_CLIENT_H */
