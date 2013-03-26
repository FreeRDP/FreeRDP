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

#ifndef FREERDP_CORE_NCACN_HTTP_H
#define FREERDP_CORE_NCACN_HTTP_H

#include <freerdp/types.h>
#include <freerdp/settings.h>

#include <freerdp/crypto/tls.h>
#include <freerdp/crypto/crypto.h>

#include <freerdp/utils/debug.h>

#include <winpr/stream.h>

#include "rpc.h"

BOOL rpc_ntlm_http_out_connect(rdpRpc* rpc);
BOOL rpc_ntlm_http_in_connect(rdpRpc* rpc);

void rpc_ntlm_http_init_channel(rdpRpc* rpc, rdpNtlmHttp* ntlm_http, TSG_CHANNEL channel);

rdpNtlmHttp* ntlm_http_new(void);
void ntlm_http_free(rdpNtlmHttp* ntlm_http);

#endif
