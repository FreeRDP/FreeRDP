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

void freerdp_channels_dummy() 
{
	audin_server_context_new(NULL);
	audin_server_context_free(NULL);

	rdpsnd_server_context_new(NULL);
	rdpsnd_server_context_free(NULL);

	cliprdr_server_context_new(NULL);
	cliprdr_server_context_free(NULL);

	echo_server_context_new(NULL);
	echo_server_context_free(NULL);

	rdpdr_server_context_new(NULL);
	rdpdr_server_context_free(NULL);

	drdynvc_server_context_new(NULL);
	drdynvc_server_context_free(NULL);

	rdpei_server_context_new(NULL);
	rdpei_server_context_free(NULL);

	remdesk_server_context_new(NULL);
	remdesk_server_context_free(NULL);

	encomsp_server_context_new(NULL);
	encomsp_server_context_free(NULL);

}

/**
 * end of ugly symbols import workaround
 */
