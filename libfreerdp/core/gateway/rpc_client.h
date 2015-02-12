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

RpcClientCall* rpc_client_call_find_by_id(rdpRpc* rpc, UINT32 CallId);

RpcClientCall* rpc_client_call_new(UINT32 CallId, UINT32 OpNum);
void rpc_client_call_free(RpcClientCall* client_call);

int rpc_in_channel_send_pdu(RpcInChannel* inChannel, BYTE* buffer, UINT32 length);

int rpc_client_in_channel_recv(rdpRpc* rpc);
int rpc_client_out_channel_recv(rdpRpc* rpc);

int rpc_client_receive_pipe_read(rdpRpc* rpc, BYTE* buffer, size_t length);

int rpc_client_write_call(rdpRpc* rpc, BYTE* data, int length, UINT16 opnum);

int rpc_client_new(rdpRpc* rpc);
void rpc_client_free(rdpRpc* rpc);

#endif /* FREERDP_CORE_RPC_CLIENT_H */
