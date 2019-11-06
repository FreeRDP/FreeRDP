/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RPC over HTTP (ncacn_http)
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

#ifndef FREERDP_LIB_CORE_GATEWAY_NCACN_HTTP_H
#define FREERDP_LIB_CORE_GATEWAY_NCACN_HTTP_H

#include <freerdp/api.h>

#include "rpc.h"
#include "http.h"

FREERDP_LOCAL BOOL rpc_ncacn_http_ntlm_init(rdpContext* context, RpcChannel* channel);
FREERDP_LOCAL void rpc_ncacn_http_ntlm_uninit(RpcChannel* channel);

FREERDP_LOCAL BOOL rpc_ncacn_http_send_in_channel_request(RpcChannel* inChannel);
FREERDP_LOCAL BOOL rpc_ncacn_http_recv_in_channel_response(RpcChannel* inChannel,
                                                           HttpResponse* response);

FREERDP_LOCAL BOOL rpc_ncacn_http_send_out_channel_request(RpcChannel* outChannel,
                                                           BOOL replacement);
FREERDP_LOCAL BOOL rpc_ncacn_http_recv_out_channel_response(RpcChannel* outChannel,
                                                            HttpResponse* response);

#endif /* FREERDP_LIB_CORE_GATEWAY_NCACN_HTTP_H */
