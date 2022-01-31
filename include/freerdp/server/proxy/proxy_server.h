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
#ifndef FREERDP_SERVER_PROXY_SERVER_H
#define FREERDP_SERVER_PROXY_SERVER_H

#include <freerdp/api.h>
#include <freerdp/server/proxy/proxy_config.h>
#include <freerdp/server/proxy/proxy_modules_api.h>

typedef struct proxy_server proxyServer;

#ifdef __cplusplus
extern "C"
{
#endif

	/**
	 * @brief pf_server_new Creates a new proxy server instance
	 *
	 * @param config The proxy server configuration to use. Must NOT be NULL.
	 *
	 * @return A new proxy server instance or NULL on failure.
	 */
	FREERDP_API proxyServer* pf_server_new(const proxyConfig* config);

	/**
	 * @brief pf_server_free Cleans up a (stopped) proxy server instance.
	 *
	 * @param server The proxy server to clean up. Might be NULL.
	 */
	FREERDP_API void pf_server_free(proxyServer* server);

	/**
	 * @brief pf_server_add_module Allows registering proxy modules that are
	 *                             build-in instead of shipped as separate
	 *                             module loaded at runtime.
	 *
	 * @param server A proxy instance to add the module to. Must NOT be NULL
	 * @param ep     The proxy entry function to add. Must NOT be NULL
	 * @param userdata Custom data for the module. May be NULL
	 *
	 * @return TRUE for success, FALSE otherwise.
	 */
	FREERDP_API BOOL pf_server_add_module(proxyServer* server, proxyModuleEntryPoint ep,
	                                      void* userdata);

	/**
	 * @brief pf_server_start Starts the proxy, binding the configured port.
	 *
	 * @param server The server instance. Must NOT be NULL.
	 *
	 * @return TRUE for success, FALSE on error
	 */
	FREERDP_API BOOL pf_server_start(proxyServer* server);

	/**
	 * @brief pf_server_start_from_socket Starts the proxy using an existing bound socket
	 *
	 * @param server The server instance. Must NOT be NULL.
	 * @param socket The bound socket to wait for events on.
	 *
	 * @return TRUE for success, FALSE on error
	 */
	FREERDP_API BOOL pf_server_start_from_socket(proxyServer* server, int socket);

	/**
	 * @brief pf_server_start_with_peer_socket Use existing peer socket
	 *
	 * @param server The server instance. Must NOT be NULL.
	 * @param socket Ready to use peer socket
	 *
	 * @return TRUE for success, FALSE on error
	 */
	FREERDP_API BOOL pf_server_start_with_peer_socket(proxyServer* server, int socket);

	/**
	 * @brief pf_server_stop Stops a server instance asynchronously.
	 *        Can be called from any thread to stop a running server instance.
	 * @param server A pointer to the server instance to stop. May be NULL.
	 */
	FREERDP_API void pf_server_stop(proxyServer* server);

	/**
	 * @brief pf_server_run This (blocking) function runs the main loop of the
	 *                      proxy.
	 *
	 * @param server The server instance. Must NOT be NULL.
	 *
	 * @return TRUE for successful termination, FALSE otherwise.
	 */
	FREERDP_API BOOL pf_server_run(proxyServer* server);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_SERVER_PROXY_SERVER_H */
