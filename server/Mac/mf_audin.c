/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Mac OS X Server (Audio Input)
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

#include "mfreerdp.h"

#include "mf_audin.h"
#include "mf_interface.h"

#include <freerdp/server/server-common.h>
#include <freerdp/log.h>
#define TAG SERVER_TAG("mac")

static UINT mf_peer_audin_data(audin_server_context* audin, const SNDIN_DATA* data)
{
	/* TODO: Implement */
	WINPR_ASSERT(audin);
	WINPR_ASSERT(data);

	WLog_WARN(TAG, "not implemented");
	WLog_DBG(TAG, "receive %" PRIdz " bytes.", Stream_Length(data->Data));
	return CHANNEL_RC_OK;
}

BOOL mf_peer_audin_init(mfPeerContext* context)
{
	WINPR_ASSERT(context);

	context->audin = audin_server_context_new(context->vcm);
	context->audin->rdpcontext = &context->_p;
	context->audin->userdata = context;

	context->audin->Data = mf_peer_audin_data;

	return audin_server_set_formats(context->audin, -1, NULL);
}

void mf_peer_audin_uninit(mfPeerContext* context)
{
	WINPR_ASSERT(context);

	audin_server_context_free(context->audin);
	context->audin = NULL;
}
