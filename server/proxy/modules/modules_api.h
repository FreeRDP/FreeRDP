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

#ifndef FREERDP_SERVER_PROXY_MODULES_API_H
#define FREERDP_SERVER_PROXY_MODULES_API_H

#include <freerdp/freerdp.h>
#include <winpr/winpr.h>

#include "../pf_context.h"

#define MODULE_TAG(module) "proxy.modules." module

/* hook callback. should return TRUE on success or FALSE on error. */
typedef BOOL (*proxyHookFn)(proxyData*);

/*
 * Filter callback:
 * 	It MUST return TRUE if the related event should be proxied,
 * 	or FALSE if it should be ignored.
 */
typedef BOOL (*proxyFilterFn)(proxyData*, void*);

/* describes a plugin: name, description and callbacks to execute. */
typedef struct proxy_plugin
{
	const char* name;        /* unique module name */
	const char* description; /* module description */

	BOOL (*PluginUnload)();

	/* proxy hooks. a module can set these function pointers to register hooks */
	proxyHookFn ClientPreConnect;
	proxyHookFn ClientLoginFailure;
	proxyHookFn ServerPostConnect;
	proxyHookFn ServerChannelsInit;
	proxyHookFn ServerChannelsFree;

	/* proxy filters. a module can set these function pointers to register filters */
	proxyFilterFn KeyboardEvent;
	proxyFilterFn MouseEvent;
	proxyFilterFn ClientChannelData; /* passthrough channels data */
	proxyFilterFn ServerChannelData; /* passthrough channels data */
} proxyPlugin;

/*
 * Main API for use by external modules.
 * Supports:
 *  - Registering a plugin.
 *  - Setting/getting plugin's per-session specific data.
 *  - Aborting a session.
 */
typedef struct proxy_plugins_manager
{
	/* used for registering a fresh new proxy plugin. */
	BOOL (*RegisterPlugin)(proxyPlugin* plugin);

	/* used for setting plugin's per-session info. */
	BOOL (*SetPluginData)(const char*, proxyData*, void*);

	/* used for getting plugin's per-session info. */
	void* (*GetPluginData)(const char*, proxyData*);

	/* used for aborting a session. */
	void (*AbortConnect)(proxyData*);
} proxyPluginsManager;

/* filter events parameters */
#define WINPR_PACK_PUSH
#include <winpr/pack.h>
typedef struct proxy_keyboard_event_info
{
	UINT16 flags;
	UINT16 rdp_scan_code;
} proxyKeyboardEventInfo;

typedef struct proxy_mouse_event_info
{
	UINT16 flags;
	UINT16 x;
	UINT16 y;
} proxyMouseEventInfo;

typedef struct channel_data_event_info
{
	/* channel metadata */
	const char* channel_name;
	UINT16 channel_id;

	/* actual data */
	const BYTE* data;
	int data_len;
} proxyChannelDataEventInfo;
#define WINPR_PACK_POP
#include <winpr/pack.h>

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API BOOL proxy_module_entry_point(proxyPluginsManager* plugins_manager);

#ifdef __cplusplus
};
#endif

#endif /* FREERDP_SERVER_PROXY_MODULES_API_H */
