/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2011-2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/select.h>
#include <sys/signal.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "x11_shadow.h"

x11ShadowServer* x11_shadow_server_new(rdpShadowServer* rdp)
{
	x11ShadowServer* server;

	server = (x11ShadowServer*) calloc(1, sizeof(x11ShadowServer));

	if (!server)
		return NULL;

	signal(SIGPIPE, SIG_IGN);

	return server;
}

void x11_shadow_server_free(x11ShadowServer* server)
{
	if (!server)
		return;

	free(server);
}
