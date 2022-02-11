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

#ifndef FREERDP_SERVER_PROXY_PFCONTEXT_H
#define FREERDP_SERVER_PROXY_PFCONTEXT_H

#include <freerdp/freerdp.h>
#include <freerdp/channels/wtsvc.h>

#include <freerdp/server/proxy/proxy_config.h>

#define PROXY_SESSION_ID_LENGTH 32

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct proxy_data proxyData;
	typedef struct proxy_module proxyModule;

	typedef struct s_InterceptContextMapEntry
	{
		void (*free)(struct s_InterceptContextMapEntry*);
	} InterceptContextMapEntry;

	/* All proxy interception channels derive from this base struct
	 * and set their cleanup function accordingly. */
	FREERDP_API void intercept_context_entry_free(void* obj);

	/** @brief how is handled a channel */
	typedef enum
	{
		PF_UTILS_CHANNEL_NOT_HANDLED,
		PF_UTILS_CHANNEL_BLOCK,
		PF_UTILS_CHANNEL_PASSTHROUGH,
		PF_UTILS_CHANNEL_INTERCEPT,
	} pf_utils_channel_mode;

	/** @brief per channel configuration */
	struct p_server_channel_context
	{
		char* channel_name;
		UINT32 channel_id;
		BOOL isDynamic;
		pf_utils_channel_mode channelMode;
	};
	typedef struct p_server_channel_context pServerChannelContext;

	void ChannelContext_free(pServerChannelContext* ctx);

	/**
	 * Wraps rdpContext and holds the state for the proxy's server.
	 */
	struct p_server_context
	{
		rdpContext context;

		proxyData* pdata;

		HANDLE vcm;
		HANDLE dynvcReady;

		wHashTable* interceptContextMap;
		wHashTable* channelsById;
	};
	typedef struct p_server_context pServerContext;

	pServerChannelContext* ChannelContext_new(pServerContext* ps, const char* name, UINT32 id);

	/**
	 * Wraps rdpContext and holds the state for the proxy's client.
	 */
	typedef struct p_client_context pClientContext;

	struct p_client_context
	{
		rdpContext context;

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
		BOOL (*sendChannelData)(pClientContext* pc, const proxyChannelDataEventInfo* ev);

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

	/**
	 * Holds data common to both sides of a proxy's session.
	 */
	struct proxy_data
	{
		proxyModule* module;
		const proxyConfig* config;

		pServerContext* ps;
		pClientContext* pc;

		HANDLE abort_event;
		HANDLE client_thread;
		HANDLE gfx_server_ready;

		char session_id[PROXY_SESSION_ID_LENGTH + 1];

		/* used to external modules to store per-session info */
		wHashTable* modules_info;
		psPeerReceiveChannelData server_receive_channel_data_original;
	};

	FREERDP_API BOOL pf_context_copy_settings(rdpSettings* dst, const rdpSettings* src);
	FREERDP_API BOOL pf_context_init_server_context(freerdp_peer* client);
	FREERDP_API pClientContext* pf_context_create_client_context(rdpSettings* clientSettings);

	FREERDP_API proxyData* proxy_data_new(void);
	FREERDP_API void proxy_data_set_client_context(proxyData* pdata, pClientContext* context);
	FREERDP_API void proxy_data_set_server_context(proxyData* pdata, pServerContext* context);
	FREERDP_API void proxy_data_free(proxyData* pdata);

	FREERDP_API BOOL proxy_data_shall_disconnect(proxyData* pdata);
	FREERDP_API void proxy_data_abort_connect(proxyData* pdata);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_SERVER_PROXY_PFCONTEXT_H */
