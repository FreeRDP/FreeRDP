/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Sample Server (Audio Input)
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
 * Copyright 2023 Pascal Nowack <Pascal.Nowack@gmx.de>
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

#include "sfreerdp.h"

#include "sf_audin.h"

#include <freerdp/server/server-common.h>
#include <freerdp/log.h>
#define TAG SERVER_TAG("sample")

#if defined(CHANNEL_AUDIN_SERVER)

static UINT sf_peer_audin_data(audin_server_context* audin, const SNDIN_DATA* data)
{
	/* TODO: Implement */
	WINPR_ASSERT(audin);
	WINPR_ASSERT(data);

	WLog_WARN(TAG, "not implemented");
	WLog_DBG(TAG, "receive %" PRIdz " bytes.", Stream_Length(data->Data));
	return CHANNEL_RC_OK;
}

#endif

BOOL sf_peer_audin_init(testPeerContext* context)
{
	WINPR_ASSERT(context);
#if defined(CHANNEL_AUDIN_SERVER)
	context->audin = audin_server_context_new(context->vcm);
	WINPR_ASSERT(context->audin);

	context->audin->rdpcontext = &context->_p;
	context->audin->userdata = context;

	context->audin->Data = sf_peer_audin_data;

	return audin_server_set_formats(context->audin, -1, NULL);
#else
	return TRUE;
#endif
}

BOOL sf_peer_audin_start(testPeerContext* context)
{
#if defined(CHANNEL_AUDIN_SERVER)
	if (!context || !context->audin || !context->audin->Open)
		return FALSE;

	return context->audin->Open(context->audin);
#else
	return FALSE;
#endif
}

BOOL sf_peer_audin_stop(testPeerContext* context)
{
#if defined(CHANNEL_AUDIN_SERVER)
	if (!context || !context->audin || !context->audin->Close)
		return FALSE;

	return context->audin->Close(context->audin);
#else
	return FALSE;
#endif
}

BOOL sf_peer_audin_running(testPeerContext* context)
{
#if defined(CHANNEL_AUDIN_SERVER)
	if (!context || !context->audin || !context->audin->IsOpen)
		return FALSE;

	return context->audin->IsOpen(context->audin);
#else
	return FALSE;
#endif
}

void sf_peer_audin_uninit(testPeerContext* context)
{
	WINPR_ASSERT(context);

#if defined(CHANNEL_AUDIN_SERVER)
	audin_server_context_free(context->audin);
	context->audin = NULL;
#endif
}
