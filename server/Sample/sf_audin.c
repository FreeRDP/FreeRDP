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

#include <freerdp/log.h>
#define TAG SERVER_TAG("sample")

static const AUDIO_FORMAT test_audio_formats[] =
{
	{ WAVE_FORMAT_PCM, 2, 44100, 176400, 4, 16, 0, NULL },
	{ WAVE_FORMAT_ALAW, 2, 22050, 44100, 2, 8, 0, NULL }
};

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
	WLog_DBG(TAG, "AUDIN open result %d.", result);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT sf_peer_audin_receive_samples(audin_server_context* context, const void* buf, int nframes)
{
	WLog_DBG(TAG, "AUDIN receive %d frames.", nframes);
	return CHANNEL_RC_OK;
}

void sf_peer_audin_init(testPeerContext* context)
{
	context->audin = audin_server_context_new(context->vcm);
	context->audin->rdpcontext = &context->_p;
	context->audin->data = context;

	context->audin->server_formats = test_audio_formats;
	context->audin->num_server_formats =
			sizeof(test_audio_formats) / sizeof(test_audio_formats[0]);

	context->audin->dst_format.wFormatTag = 1;
	context->audin->dst_format.nChannels = 2;
	context->audin->dst_format.nSamplesPerSec = 44100;
	context->audin->dst_format.wBitsPerSample = 16;

	context->audin->Opening = sf_peer_audin_opening;
	context->audin->OpenResult = sf_peer_audin_open_result;
	context->audin->ReceiveSamples = sf_peer_audin_receive_samples;
}
