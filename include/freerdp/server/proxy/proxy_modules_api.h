/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server
 *
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
 * Copyright 2019 Idan Freiberg <speidy@gmail.com>
 * Copyright 2021 Armin Novak <anovak@thincast.com>
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

#ifndef FREERDP_SERVER_PROXY_MODULES_API_H
#define FREERDP_SERVER_PROXY_MODULES_API_H

#include <freerdp/freerdp.h>
#include <winpr/winpr.h>
#include <winpr/sspi.h>

#define MODULE_TAG(module) "proxy.modules." module

typedef struct proxy_data proxyData;
typedef struct proxy_module proxyModule;
typedef struct proxy_plugin proxyPlugin;
typedef struct proxy_plugins_manager proxyPluginsManager;

/* hook callback. should return TRUE on success or FALSE on error. */
typedef BOOL (*proxyHookFn)(proxyPlugin*, proxyData*, void*);

/*
 * Filter callback:
 * 	It MUST return TRUE if the related event should be proxied,
 * 	or FALSE if it should be ignored.
 */
typedef BOOL (*proxyFilterFn)(proxyPlugin*, proxyData*, void*);

/* describes a plugin: name, description and callbacks to execute.
 *
 * This is public API, so always add new fields at the end of the struct to keep
 * some backward compatibility.
 */
struct proxy_plugin
{
	const char* name;        /* 0: unique module name */
	const char* description; /* 1: module description */

	UINT64 reserved1[32 - 2]; /* 2-32 */

	BOOL (*PluginUnload)(proxyPlugin* plugin); /* 33 */
	UINT64 reserved2[66 - 34];                 /* 34 - 65 */

	/* proxy hooks. a module can set these function pointers to register hooks */
	proxyHookFn ClientInitConnect;     /* 66 custom=rdpContext* */
	proxyHookFn ClientUninitConnect;   /* 67 custom=rdpContext* */
	proxyHookFn ClientPreConnect;      /* 68 custom=rdpContext* */
	proxyHookFn ClientPostConnect;     /* 69 custom=rdpContext* */
	proxyHookFn ClientPostDisconnect;  /* 70 custom=rdpContext* */
	proxyHookFn ClientX509Certificate; /* 71 custom=rdpContext* */
	proxyHookFn ClientLoginFailure;    /* 72 custom=rdpContext* */
	proxyHookFn ClientEndPaint;        /* 73 custom=rdpContext* */
	proxyHookFn ClientRedirect;        /* 74 custom=rdpContext* */
	proxyHookFn ClientLoadChannels;    /* 75 custom=rdpContext* */
	UINT64 reserved3[96 - 76];         /* 76-95 */

	proxyHookFn ServerPostConnect;       /* 96  custom=freerdp_peer* */
	proxyHookFn ServerPeerActivate;      /* 97  custom=freerdp_peer* */
	proxyHookFn ServerChannelsInit;      /* 98  custom=freerdp_peer* */
	proxyHookFn ServerChannelsFree;      /* 99  custom=freerdp_peer* */
	proxyHookFn ServerSessionEnd;        /* 100 custom=freerdp_peer* */
	proxyHookFn ServerSessionInitialize; /* 101 custom=freerdp_peer* */
	proxyHookFn ServerSessionStarted;    /* 102 custom=freerdp_peer* */

	UINT64 reserved4[128 - 103]; /* 103 - 127 */

	/* proxy filters. a module can set these function pointers to register filters */
	proxyFilterFn KeyboardEvent;         /* 128 */
	proxyFilterFn MouseEvent;            /* 129 */
	proxyFilterFn ClientChannelData;     /* 130 passthrough channels data */
	proxyFilterFn ServerChannelData;     /* 131 passthrough channels data */
	proxyFilterFn DynamicChannelCreate;  /* 132 passthrough drdynvc channel create data */
	proxyFilterFn ServerFetchTargetAddr; /* 133 */
	proxyFilterFn ServerPeerLogon;       /* 134 */
	proxyFilterFn ChannelCreate;         /* 135 passthrough drdynvc channel create data */
	proxyFilterFn UnicodeEvent;          /* 136 */
	proxyFilterFn MouseExEvent;          /* 137 */
	UINT64 reserved5[160 - 138];         /* 138-159 */

	/* Runtime data fields */
	proxyPluginsManager* mgr; /* 160 */ /** Set during plugin registration */
	void* userdata; /* 161 */           /** Custom data provided with RegisterPlugin, memory managed
	                              outside of plugin. */
	void* custom; /* 162 */ /** Custom configuration data, must be allocated in RegisterPlugin and
	                  freed in PluginUnload */

	UINT64 reserved6[192 - 163]; /* 163-191 Add some filler data to allow for new callbacks or
	                              * fields without breaking API */
};

/*
 * Main API for use by external modules.
 * Supports:
 *  - Registering a plugin.
 *  - Setting/getting plugin's per-session specific data.
 *  - Aborting a session.
 */
struct proxy_plugins_manager
{
	/* 0 used for registering a fresh new proxy plugin. */
	BOOL (*RegisterPlugin)(struct proxy_plugins_manager* mgr, const proxyPlugin* plugin);

	/* 1 used for setting plugin's per-session info. */
	BOOL (*SetPluginData)(struct proxy_plugins_manager* mgr, const char*, proxyData*, void*);

	/* 2 used for getting plugin's per-session info. */
	void* (*GetPluginData)(struct proxy_plugins_manager* mgr, const char*, proxyData*);

	/* 3 used for aborting a session. */
	void (*AbortConnect)(struct proxy_plugins_manager* mgr, proxyData*);

	UINT64 reserved[128 - 4]; /* 4-127 reserved fields */
};

typedef BOOL (*proxyModuleEntryPoint)(proxyPluginsManager* plugins_manager, void* userdata);

/* filter events parameters */
#define WINPR_PACK_PUSH
#include <winpr/pack.h>
typedef struct proxy_keyboard_event_info
{
	UINT16 flags;
	UINT16 rdp_scan_code;
} proxyKeyboardEventInfo;

typedef struct proxy_unicode_event_info
{
	UINT16 flags;
	UINT16 code;
} proxyUnicodeEventInfo;

typedef struct proxy_mouse_event_info
{
	UINT16 flags;
	UINT16 x;
	UINT16 y;
} proxyMouseEventInfo;

typedef struct proxy_mouse_ex_event_info
{
	UINT16 flags;
	UINT16 x;
	UINT16 y;
} proxyMouseExEventInfo;

typedef struct
{
	/* channel metadata */
	const char* channel_name;
	UINT16 channel_id;

	/* actual data */
	const BYTE* data;
	size_t data_len;
	size_t total_size;
	UINT32 flags;
} proxyChannelDataEventInfo;

typedef enum
{
	PROXY_FETCH_TARGET_METHOD_DEFAULT,
	PROXY_FETCH_TARGET_METHOD_CONFIG,
	PROXY_FETCH_TARGET_METHOD_LOAD_BALANCE_INFO,
	PROXY_FETCH_TARGET_USE_CUSTOM_ADDR
} ProxyFetchTargetMethod;

typedef struct
{
	/* out values */
	char* target_address;
	UINT16 target_port;

	/*
	 * If this value is set to true by a plugin, target info will be fetched from config and proxy
	 * will connect any client to the same remote server.
	 */
	ProxyFetchTargetMethod fetch_method;
} proxyFetchTargetEventInfo;

typedef struct server_peer_logon
{
	const SEC_WINNT_AUTH_IDENTITY* identity;
	BOOL automatic;
} proxyServerPeerLogon;
#define WINPR_PACK_POP
#include <winpr/pack.h>

#endif /* FREERDP_SERVER_PROXY_MODULES_API_H */
