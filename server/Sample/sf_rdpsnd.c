/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Sample Server (Audio Output)
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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

#include <freerdp/config.h>

#include <winpr/assert.h>

#include <freerdp/server/audin.h>

#include "sf_rdpsnd.h"

#include <freerdp/server/server-common.h>
#include <freerdp/log.h>
#define TAG SERVER_TAG("sample")

static void sf_peer_rdpsnd_activated(RdpsndServerContext* context)
{
	WINPR_UNUSED(context);
	WINPR_ASSERT(context);
	WLog_DBG(TAG, "RDPSND Activated");
}

BOOL sf_peer_rdpsnd_init(testPeerContext* context)
{
	WINPR_ASSERT(context);

	context->rdpsnd = rdpsnd_server_context_new(context->vcm);
	WINPR_ASSERT(context->rdpsnd);
	context->rdpsnd->rdpcontext = &context->_p;
	context->rdpsnd->data = context;
	context->rdpsnd->num_server_formats =
	    server_rdpsnd_get_formats(&context->rdpsnd->server_formats);

	if (context->rdpsnd->num_server_formats > 0)
		context->rdpsnd->src_format = &context->rdpsnd->server_formats[0];

	context->rdpsnd->Activated = sf_peer_rdpsnd_activated;

	WINPR_ASSERT(context->rdpsnd->Initialize);
	if (context->rdpsnd->Initialize(context->rdpsnd, TRUE) != CHANNEL_RC_OK)
	{
		return FALSE;
	}

	return TRUE;
}
