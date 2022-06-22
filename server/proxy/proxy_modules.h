/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server
 *
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

#ifndef FREERDP_SERVER_PROXY_MODULES_H
#define FREERDP_SERVER_PROXY_MODULES_H

#include <winpr/wtypes.h>
#include <winpr/collections.h>

#include <freerdp/server/proxy/proxy_modules_api.h>

typedef enum
{
	FILTER_TYPE_KEYBOARD,                              /* proxyKeyboardEventInfo */
	FILTER_TYPE_MOUSE,                                 /* proxyMouseEventInfo */
	FILTER_TYPE_CLIENT_PASSTHROUGH_CHANNEL_DATA,       /* proxyChannelDataEventInfo */
	FILTER_TYPE_SERVER_PASSTHROUGH_CHANNEL_DATA,       /* proxyChannelDataEventInfo */
	FILTER_TYPE_CLIENT_PASSTHROUGH_DYN_CHANNEL_CREATE, /* proxyChannelDataEventInfo */
	FILTER_TYPE_SERVER_FETCH_TARGET_ADDR,              /* proxyFetchTargetEventInfo */
	FILTER_TYPE_SERVER_PEER_LOGON,                     /* proxyServerPeerLogon */
	FILTER_TYPE_CLIENT_PASSTHROUGH_CHANNEL_CREATE,     /* proxyChannelDataEventInfo */

	FILTER_LAST
} PF_FILTER_TYPE;

typedef enum
{
	HOOK_TYPE_CLIENT_INIT_CONNECT,
	HOOK_TYPE_CLIENT_UNINIT_CONNECT,
	HOOK_TYPE_CLIENT_PRE_CONNECT,
	HOOK_TYPE_CLIENT_POST_CONNECT,
	HOOK_TYPE_CLIENT_POST_DISCONNECT,
	HOOK_TYPE_CLIENT_REDIRECT,
	HOOK_TYPE_CLIENT_VERIFY_X509,
	HOOK_TYPE_CLIENT_LOGIN_FAILURE,
	HOOK_TYPE_CLIENT_END_PAINT,
	HOOK_TYPE_CLIENT_LOAD_CHANNELS,

	HOOK_TYPE_SERVER_POST_CONNECT,
	HOOK_TYPE_SERVER_ACTIVATE,
	HOOK_TYPE_SERVER_CHANNELS_INIT,
	HOOK_TYPE_SERVER_CHANNELS_FREE,
	HOOK_TYPE_SERVER_SESSION_END,
	HOOK_TYPE_SERVER_SESSION_INITIALIZE,
	HOOK_TYPE_SERVER_SESSION_STARTED,

	HOOK_LAST
} PF_HOOK_TYPE;

#ifdef __cplusplus
extern "C"
{
#endif

	proxyModule* pf_modules_new(const char* root_dir, const char** modules, size_t count);

	/**
	 * @brief pf_modules_add Registers a new plugin
	 * @param ep A module entry point function, must NOT be NULL
	 * @return TRUE for success, FALSE otherwise
	 */
	BOOL pf_modules_add(proxyModule* module, proxyModuleEntryPoint ep, void* userdata);

	BOOL pf_modules_is_plugin_loaded(proxyModule* module, const char* plugin_name);
	void pf_modules_list_loaded_plugins(proxyModule* module);

	BOOL pf_modules_run_filter(proxyModule* module, PF_FILTER_TYPE type, proxyData* pdata,
	                           void* param);
	BOOL pf_modules_run_hook(proxyModule* module, PF_HOOK_TYPE type, proxyData* pdata,
	                         void* custom);

	void pf_modules_free(proxyModule* module);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_SERVER_PROXY_MODULES_H */
