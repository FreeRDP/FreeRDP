/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server
 *
 * Copyright 2021 Armin Novak <armin.novak@thincast.com>
 * Copyright 2021 Thincast Technologies GmbH
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

#ifndef FREERDP_SERVER_PROXY_SCARD_H
#define FREERDP_SERVER_PROXY_SCARD_H

#include <winpr/wlog.h>
#include <freerdp/server/proxy/proxy_context.h>

typedef UINT (*pf_scard_send_fkt_t)(wLog* log, pClientContext*, wStream*);

BOOL pf_channel_smartcard_client_new(pClientContext* pc);
void pf_channel_smartcard_client_free(pClientContext* pc);

BOOL pf_channel_smartcard_client_reset(pClientContext* pc);
BOOL pf_channel_smartcard_client_emulate(pClientContext* pc);

BOOL pf_channel_smartcard_client_handle(wLog* log, pClientContext* pc, wStream* s, wStream* out,
                                        pf_scard_send_fkt_t fkt);
BOOL pf_channel_smartcard_server_handle(pServerContext* ps, wStream* s);

#endif /* FREERDP_SERVER_PROXY_SCARD_H */
