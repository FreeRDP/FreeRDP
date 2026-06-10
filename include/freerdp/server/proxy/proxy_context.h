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

#include <freerdp/api.h>
#include <freerdp/types.h>

#include <freerdp/freerdp.h>
#include <freerdp/channels/wtsvc.h>

#include <freerdp/server/proxy/proxy_config.h>
#include <freerdp/server/proxy/proxy_types.h>

#define PROXY_SESSION_ID_LENGTH 32

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct proxy_data proxyData;
	typedef struct proxy_module proxyModule;
	typedef struct p_server_static_channel_context pServerStaticChannelContext;

	typedef struct s_InterceptContextMapEntry
	{
		void (*free)(struct s_InterceptContextMapEntry*);
	} InterceptContextMapEntry;

	/* All proxy interception channels derive from this base struct
	 * and set their cleanup function accordingly. */
	FREERDP_API void intercept_context_entry_free(void* obj);

	typedef PfChannelResult (*proxyChannelDataFn)(proxyData* pdata,
	                                              const pServerStaticChannelContext* channel,
	                                              const BYTE* xdata, size_t xsize, UINT32 flags,
	                                              size_t totalSizepServer);
	typedef void (*proxyChannelContextDtor)(void* context);

	/** @brief per channel configuration */
	struct p_server_static_channel_context
	{
		char* channel_name;
		UINT32 front_channel_id;
		UINT32 back_channel_id;
		pf_utils_channel_mode channelMode;
		WINPR_ATTR_NODISCARD proxyChannelDataFn onFrontData;
		WINPR_ATTR_NODISCARD proxyChannelDataFn onBackData;
		proxyChannelContextDtor contextDtor;
		void* context;
	};

	FREERDP_API void StaticChannelContext_free(pServerStaticChannelContext* ctx);

	/**
	 * Wraps rdpContext and holds the state for the proxy's server.
	 */
	typedef struct p_server_context pServerContext;

	WINPR_ATTR_MALLOC(StaticChannelContext_free, 1)
	WINPR_ATTR_NODISCARD
	pServerStaticChannelContext* StaticChannelContext_new(pServerContext* ps, const char* name,
	                                                      UINT32 id);

	typedef struct p_client_context pClientContext;

	/**
	 * Holds data common to both sides of a proxy's session.
	 */
	struct proxy_data
	{
		proxyModule* module;
		const proxyConfig* config;

		rdpContext* ps; /* actual type is pServerContext */
		rdpContext* pc; /* actual type is pClientContext */

		HANDLE abort_event;
		HANDLE client_thread;
		HANDLE gfx_server_ready;

		char session_id[PROXY_SESSION_ID_LENGTH + 1];

		/* used to external modules to store per-session info */
		wHashTable* modules_info;
		psPeerReceiveChannelData server_receive_channel_data_original;
	};

	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL pf_context_copy_settings(rdpSettings* dst, const rdpSettings* src);

	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL pf_context_init_server_context(freerdp_peer* client);

	WINPR_ATTR_MALLOC(freerdp_client_context_free, 1)
	FREERDP_API pClientContext* pf_context_create_client_context(const rdpSettings* clientSettings);

	FREERDP_API void proxy_data_free(proxyData* pdata);

	WINPR_ATTR_MALLOC(proxy_data_free, 1)
	FREERDP_API proxyData* proxy_data_new(void);
	FREERDP_API void proxy_data_set_client_context(proxyData* pdata, pClientContext* context);

	/**
	 * @brief getter for proxy RDP client context
	 * @param pdata Pointer to the proxy data structure, must not be nullptr
	 * @return A pointer to the client context structure or nullptr in case of failure
	 * @since version 3.27.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API pClientContext* proxy_data_get_client_context(proxyData* pdata);

	FREERDP_API void proxy_data_set_server_context(proxyData* pdata, pServerContext* context);

	/**
	 * @brief getter for proxy RDP server context
	 * @param pdata Pointer to the proxy data structure, must not be nullptr
	 * @return A pointer to the server context structure or nullptr in case of failure
	 * @since version 3.27.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API pServerContext* proxy_data_get_server_context(proxyData* pdata);

	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL proxy_data_shall_disconnect(proxyData* pdata);
	FREERDP_API void proxy_data_abort_connect(proxyData* pdata);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_SERVER_PROXY_PFCONTEXT_H */
