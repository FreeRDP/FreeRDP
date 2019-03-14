/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server
 *
 * Copyright 2019 Mati Shabtay <matishabtay@gmail.com>
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
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

#ifndef FREERDP_SERVER_PROXY_PFCONTEXT_H
#define FREERDP_SERVER_PROXY_PFCONTEXT_H

#include <freerdp/freerdp.h>
#include <freerdp/channels/wtsvc.h>
#include <freerdp/client/encomsp.h>
#include <freerdp/client/rdpei.h>
#include <freerdp/client/rdpgfx.h>
#include <freerdp/server/rdpgfx.h>

/**
 * Proxy context wraps a peer's context, and holds a reference to the other end
 * of the connection. This lets one side of the proxy to forward events to the
 * other.
 */
struct proxy_context
{
	/* Context of client or server */
	rdpContext _context;

	/**
	 * Context of peer to which the proxy is connected.
	 * Events from the first context are forwarded to this one.
	 */
	rdpContext* peerContext;

	HANDLE connectionClosed;
};
typedef struct proxy_context proxyContext;

/**
 * Context used for the client's connection to the proxy.
 */
struct client_to_proxy_context
{
	proxyContext _context;

	HANDLE vcm;
	HANDLE thread;

	RdpgfxServerContext* gfx;
};
typedef struct client_to_proxy_context clientToProxyContext;

BOOL client_to_proxy_context_new(freerdp_peer* client,
                                 clientToProxyContext* context);
void client_to_proxy_context_free(freerdp_peer* client,
                                  clientToProxyContext* context);

/**
 * Context used for the proxy's connection to the target server.
 */
struct proxy_to_server_context
{
	proxyContext _context;

	RdpeiClientContext* rdpei;
	RdpgfxClientContext* gfx;
	EncomspClientContext* encomsp;
};
typedef struct proxy_to_server_context proxyToServerContext;

rdpContext* proxy_to_server_context_create(rdpSettings* baseSettings, char* host,
        DWORD port, char* username, char* password);
void proxy_settings_mirror(rdpSettings* settings, rdpSettings* baseSettings);

#endif /* FREERDP_SERVER_PROXY_PFCONTEXT_H */