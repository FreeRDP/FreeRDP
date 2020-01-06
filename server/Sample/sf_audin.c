/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Sample Server (Audio Input)
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sfreerdp.h"

#include "sf_audin.h"

#include <freerdp/server/server-common.h>
#include <freerdp/log.h>
#define TAG SERVER_TAG("sample")

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT sf_peer_audin_opening(audin_server_context* context)
{
	WLog_DBG(TAG, "AUDIN opening.");
	/* Simply choose the first format supported by the client. */
	context->SelectFormat(context, 0);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT sf_peer_audin_open_result(audin_server_context* context, UINT32 result)
{
	/* TODO: Implement */
	WLog_WARN(TAG, "%s not implemented", __FUNCTION__);
	WLog_DBG(TAG, "AUDIN open result %" PRIu32 ".", result);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT sf_peer_audin_receive_samples(audin_server_context* context, const AUDIO_FORMAT* format,
                                          wStream* buf, size_t nframes)
{
	/* TODO: Implement */
	WLog_WARN(TAG, "%s not implemented", __FUNCTION__);
	WLog_DBG(TAG, "%s receive %" PRIdz " frames.", __FUNCTION__, nframes);
	return CHANNEL_RC_OK;
}

void sf_peer_audin_init(testPeerContext* context)
{
	context->audin = audin_server_context_new(context->vcm);
	context->audin->rdpcontext = &context->_p;
	context->audin->data = context;
	context->audin->num_server_formats = server_audin_get_formats(&context->audin->server_formats);

	if (context->audin->num_server_formats > 0)
		context->audin->dst_format = &context->audin->server_formats[0];

	context->audin->Opening = sf_peer_audin_opening;
	context->audin->OpenResult = sf_peer_audin_open_result;
	context->audin->ReceiveSamples = sf_peer_audin_receive_samples;
}

BOOL sf_peer_audin_start(testPeerContext* context)
{
	if (!context || !context->audin || !context->audin->Open)
		return FALSE;

	return context->audin->Open(context->audin);
}

BOOL sf_peer_audin_stop(testPeerContext* context)
{
	if (!context || !context->audin || !context->audin->Close)
		return FALSE;

	return context->audin->Close(context->audin);
}

BOOL sf_peer_audin_running(testPeerContext* context)
{
	if (!context || !context->audin || !context->audin->IsOpen)
		return FALSE;

	return context->audin->IsOpen(context->audin);
}

void sf_peer_audin_uninit(testPeerContext* context)
{
	if (context)
		audin_server_context_free(context->audin);
}
