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

#ifndef _FREERDP_SERVER_PROXY_SERVER_H
#define _FREERDP_SERVER_PROXY_SERVER_H

#include <winpr/collections.h>
#include <freerdp/listener.h>

#include <freerdp/server/proxy/proxy_config.h>
#include "proxy_modules.h"

struct proxy_server
{
	proxyModule* module;
	proxyConfig* config;

	freerdp_listener* listener;
	wArrayList* clients;        /* maintain a list of active sessions, for stats */
	wCountdownEvent* waitGroup; /* wait group used for gracefull shutdown */
	HANDLE stopEvent;           /* an event used to signal the main thread to stop */
};

#endif /* _FREERDP_SERVER_PROXY_SERVER_H */
