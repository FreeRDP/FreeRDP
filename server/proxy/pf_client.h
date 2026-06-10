/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server
 *
 * Copyright 2019 Mati Shabtay <matishabtay@gmail.com>
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
 * Copyright 2019 Idan Freiberg <speidy@gmail.com>
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

#ifndef FREERDP_SERVER_PROXY_PFCLIENT_H
#define FREERDP_SERVER_PROXY_PFCLIENT_H

#include <freerdp/freerdp.h>
#include <freerdp/server/proxy/proxy_context.h>
#include <winpr/wtypes.h>

/**
 * Wraps rdpContext and holds the state for the proxy's client.
 */

struct p_client_context
{
	rdpClientContext cctx;

	proxyData* pdata;

	/*
	 * In a case when freerdp_connect fails,
	 * Used for NLA fallback feature, to check if the server should close the connection.
	 * When it is set to TRUE, proxy's client knows it shouldn't signal the server thread to
	 * closed the connection when pf_client_post_disconnect is called, because it is trying to
	 * connect reconnect without NLA. It must be set to TRUE before the first try, and to FALSE
	 * after the connection fully established, to ensure graceful shutdown of the connection
	 * when it will be closed.
	 */
	BOOL allow_next_conn_failure;

	BOOL connected; /* Set after client post_connect. */

	pReceiveChannelData client_receive_channel_data_original;
	wQueue* cached_server_channel_data;
	WINPR_ATTR_NODISCARD BOOL (*sendChannelData)(pClientContext* pc,
	                                             const proxyChannelDataEventInfo* ev);

	/* X509 specific */
	char* remote_hostname;
	wStream* remote_pem;
	UINT16 remote_port;
	UINT32 remote_flags;

	BOOL input_state_sync_pending;
	UINT32 input_state;

	wHashTable* interceptContextMap;
	UINT32 computerNameLen;
	BOOL computerNameUnicode;
	union
	{
		WCHAR* wc;
		char* c;
		void* v;
	} computerName;
};

int RdpClientEntry(RDP_CLIENT_ENTRY_POINTS* pEntryPoints);
WINPR_ATTR_NODISCARD DWORD WINAPI pf_client_start(LPVOID arg);

#endif /* FREERDP_SERVER_PROXY_PFCLIENT_H */
