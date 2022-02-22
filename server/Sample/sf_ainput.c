/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Sample Server (Advanced Input)
 *
 * Copyright 2022 Armin Novak <armin.novak@thincast.com>
 * Copyright 2022 Thincast Technologies GmbH
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

#include "sf_ainput.h"

#include <freerdp/server/server-common.h>
#include <freerdp/server/ainput.h>

#include <freerdp/log.h>
#define TAG SERVER_TAG("sample.ainput")

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT sf_peer_ainput_mouse_event(ainput_server_context* context, UINT64 timestamp,
                                       UINT64 flags, INT32 x, INT32 y)
{
	/* TODO: Implement */
	WINPR_ASSERT(context);

	WLog_WARN(TAG, "%s not implemented: 0x%08" PRIx64 ", 0x%08" PRIx64 ", %" PRId32 "x%" PRId32,
	          __FUNCTION__, timestamp, flags, x, y);
	return CHANNEL_RC_OK;
}

void sf_peer_ainput_init(testPeerContext* context)
{
	WINPR_ASSERT(context);

	context->ainput = ainput_server_context_new(context->vcm);
	WINPR_ASSERT(context->ainput);

	context->ainput->rdpcontext = &context->_p;
	context->ainput->data = context;

	context->ainput->MouseEvent = sf_peer_ainput_mouse_event;
}

BOOL sf_peer_ainput_start(testPeerContext* context)
{
	if (!context || !context->ainput || !context->ainput->Open)
		return FALSE;

	return context->ainput->Open(context->ainput) == CHANNEL_RC_OK;
}

BOOL sf_peer_ainput_stop(testPeerContext* context)
{
	if (!context || !context->ainput || !context->ainput->Close)
		return FALSE;

	return context->ainput->Close(context->ainput) == CHANNEL_RC_OK;
}

BOOL sf_peer_ainput_running(testPeerContext* context)
{
	if (!context || !context->ainput || !context->ainput->IsOpen)
		return FALSE;

	return context->ainput->IsOpen(context->ainput);
}

void sf_peer_ainput_uninit(testPeerContext* context)
{
	ainput_server_context_free(context->ainput);
}
