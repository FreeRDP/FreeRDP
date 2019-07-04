/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Server Channels
 *
 * Copyright 2011-2012 Vic Lee
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <freerdp/constants.h>
#include <freerdp/server/channels.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/stream.h>

#include "channels.h"

/**
 * this is a workaround to force importing symbols
 * will need to fix that later on cleanly
 */

#include <freerdp/server/audin.h>
#include <freerdp/server/rdpsnd.h>
#include <freerdp/server/cliprdr.h>
#include <freerdp/server/echo.h>
#include <freerdp/server/rdpdr.h>
#include <freerdp/server/rdpei.h>
#include <freerdp/server/drdynvc.h>
#include <freerdp/server/remdesk.h>
#include <freerdp/server/encomsp.h>
#include <freerdp/server/rail.h>
#include <freerdp/server/rdpgfx.h>
#include <freerdp/server/disp.h>

void freerdp_channels_dummy(void)
{
	audin_server_context* audin;
	RdpsndServerContext* rdpsnd;
	CliprdrServerContext* cliprdr;
	echo_server_context* echo;
	RdpdrServerContext* rdpdr;
	DrdynvcServerContext* drdynvc;
	RdpeiServerContext* rdpei;
	RemdeskServerContext* remdesk;
	EncomspServerContext* encomsp;
	RailServerContext* rail;
	RdpgfxServerContext* rdpgfx;
	DispServerContext* disp;
	audin = audin_server_context_new(NULL);
	audin_server_context_free(audin);
	rdpsnd = rdpsnd_server_context_new(NULL);
	rdpsnd_server_context_free(rdpsnd);
	cliprdr = cliprdr_server_context_new(NULL);
	cliprdr_server_context_free(cliprdr);
	echo = echo_server_context_new(NULL);
	echo_server_context_free(echo);
	rdpdr = rdpdr_server_context_new(NULL);
	rdpdr_server_context_free(rdpdr);
	drdynvc = drdynvc_server_context_new(NULL);
	drdynvc_server_context_free(drdynvc);
	rdpei = rdpei_server_context_new(NULL);
	rdpei_server_context_free(rdpei);
	remdesk = remdesk_server_context_new(NULL);
	remdesk_server_context_free(remdesk);
	encomsp = encomsp_server_context_new(NULL);
	encomsp_server_context_free(encomsp);
	rail = rail_server_context_new(NULL);
	rail_server_context_free(rail);
	rdpgfx = rdpgfx_server_context_new(NULL);
	rdpgfx_server_context_free(rdpgfx);
	disp = disp_server_context_new(NULL);
	disp_server_context_free(disp);
}

/**
 * end of ugly symbols import workaround
 */
