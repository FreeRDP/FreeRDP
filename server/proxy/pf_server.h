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

#ifndef FREERDP_SERVER_PROXY_SERVER_H
#define FREERDP_SERVER_PROXY_SERVER_H

#include <winpr/collections.h>
#include <freerdp/listener.h>

#include "pf_config.h"

typedef struct proxy_server
{
	proxyConfig* config;

	freerdp_listener* listener;
	wArrayList* clients;        /* maintain a list of active sessions, for stats */
	wCountdownEvent* waitGroup; /* wait group used for gracefull shutdown */
	HANDLE thread;              /* main server thread - freerdp listener thread */
	HANDLE stopEvent;           /* an event used to signal the main thread to stop */
} proxyServer;

proxyServer* pf_server_new(proxyConfig* config);
void pf_server_free(proxyServer* server);

BOOL pf_server_start(proxyServer* server);
void pf_server_stop(proxyServer* server);

#endif /* FREERDP_SERVER_PROXY_SERVER_H */
