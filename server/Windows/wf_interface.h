/**
 * FreeRDP: A Remote Desktop Protocol Client
 * FreeRDP Windows Server
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef WF_INTERFACE_H
#define WF_INTERFACE_H

#include "wfreerdp.h"

#include <winpr/windows.h>
#include <freerdp/freerdp.h>
#include <freerdp/listener.h>

struct wf_server
{
	DWORD port;
	HANDLE thread;
	freerdp_listener* instance;
};
typedef struct wf_server wfServer;

BOOL wfreerdp_server_start(wfServer* server);
BOOL wfreerdp_server_stop(wfServer* server);

wfServer* wfreerdp_server_new();
void wfreerdp_server_free(wfServer* server);

#endif /* WF_INTERFACE_H */
