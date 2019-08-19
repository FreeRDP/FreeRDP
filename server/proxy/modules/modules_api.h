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

#define PROXY_API FREERDP_API

typedef struct module_operations moduleOperations;

/* used for filtering */
typedef BOOL (*proxyFilterFn)(moduleOperations*, rdpContext*, void*);

/* used for hooks */
typedef BOOL (*proxyHookFn)(moduleOperations*, rdpContext*);

/*
 * used for per-session info.
 *
 * each module is allowed to store data per session.
 * it is useful, for example, when a module wants to create a server channel in runtime,
 * or to save information about a session (for example, the count of mouse clicks in the last
 * minute), and then do something with this information (maybe abort the connection).
 */
typedef BOOL (*moduleSetSessionData)(moduleOperations*, rdpContext*, void*);
typedef void* (*moduleGetSessionData)(moduleOperations*, rdpContext*);

/*
 * used for connection management. when a module wants to forcibly close a connection, it should
 * call this method.
 */
typedef void (*moduleAbortConnect)(moduleOperations*, rdpContext*);

typedef struct connection_info connectionInfo;
typedef struct proxy_keyboard_event_info proxyKeyboardEventInfo;
typedef struct proxy_mouse_event_info proxyMouseEventInfo;

/* represents a set of operations that a module can do */
struct module_operations
{
	/* per-session API. a module must not change these function pointers. */
	moduleSetSessionData SetSessionData;
	moduleGetSessionData GetSessionData;
	moduleAbortConnect AbortConnect;

	/* proxy hooks. a module can set these function pointers to register hooks. */
	proxyHookFn ClientPreConnect;
	proxyHookFn ServerChannelsInit;
	proxyHookFn ServerChannelsFree;

	/* proxy filters a module can set these function pointers to register filters. */
	proxyFilterFn KeyboardEvent;
	proxyFilterFn MouseEvent;
};

/* filter events parameters */
#define WINPR_PACK_PUSH
#include <winpr/pack.h>
struct proxy_keyboard_event_info
{
	UINT16 flags;
	UINT16 rdp_scan_code;
};

struct proxy_mouse_event_info
{
	UINT16 flags;
	UINT16 x;
	UINT16 y;
};
#define WINPR_PACK_POP
#include <winpr/pack.h>

/*
 * these two functions must be implemented by any proxy module.
 * module_init: used for module initialization, hooks and filters registration.
 */
PROXY_API BOOL module_init(moduleOperations* module);
PROXY_API BOOL module_exit(moduleOperations* module);

#endif /* FREERDP_SERVER_PROXY_MODULES_API_H */
