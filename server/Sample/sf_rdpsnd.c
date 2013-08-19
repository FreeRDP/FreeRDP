/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Sample Server (Audio Output)
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <freerdp/server/audin.h>

#include "sf_rdpsnd.h"

static const AUDIO_FORMAT test_audio_formats[] =
{
	{ WAVE_FORMAT_PCM, 2, 44100, 176400, 4, 16, 0, NULL },
	{ WAVE_FORMAT_ALAW, 2, 22050, 44100, 2, 8, 0, NULL }
};

static void sf_peer_rdpsnd_activated(RdpsndServerContext* context)
{
	printf("RDPSND Activated\n");
}

BOOL sf_peer_rdpsnd_init(testPeerContext* context)
{
	context->rdpsnd = rdpsnd_server_context_new(context->vcm);
	context->rdpsnd->data = context;

	context->rdpsnd->server_formats = test_audio_formats;
	context->rdpsnd->num_server_formats =
			sizeof(test_audio_formats) / sizeof(test_audio_formats[0]);

	context->rdpsnd->src_format.wFormatTag = 1;
	context->rdpsnd->src_format.nChannels = 2;
	context->rdpsnd->src_format.nSamplesPerSec = 44100;
	context->rdpsnd->src_format.wBitsPerSample = 16;

	context->rdpsnd->Activated = sf_peer_rdpsnd_activated;

	context->rdpsnd->Initialize(context->rdpsnd);

	return TRUE;
}
