/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Windows Server (Audio Output)
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2013 Corey Clayton <can.of.tuna@gmail.com>
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

#include <winpr/windows.h>

#include "wf_rdpsnd.h"
#include "wf_info.h"

#ifdef WITH_RDPSND_DSOUND

#include "wf_directsound.h"

#else

#include "wf_wasapi.h"

#endif


static const AUDIO_FORMAT test_audio_formats[] =
{
	{ 0x11, 2, 22050, 1024, 4, 0, NULL }, /* IMA ADPCM, 22050 Hz, 2 channels */
	{ 0x11, 1, 22050, 512, 4, 0, NULL }, /* IMA ADPCM, 22050 Hz, 1 channels */
	{ 0x01, 2, 22050, 4, 16, 0, NULL }, /* PCM, 22050 Hz, 2 channels, 16 bits */
	{ 0x01, 1, 22050, 2, 16, 0, NULL }, /* PCM, 22050 Hz, 1 channels, 16 bits */
	{ 0x01, 2, 44100, 4, 16, 0, NULL }, /* PCM, 44100 Hz, 2 channels, 16 bits */
	{ 0x01, 1, 44100, 2, 16, 0, NULL }, /* PCM, 44100 Hz, 1 channels, 16 bits */
	{ 0x01, 2, 11025, 4, 16, 0, NULL }, /* PCM, 11025 Hz, 2 channels, 16 bits */
	{ 0x01, 1, 11025, 2, 16, 0, NULL }, /* PCM, 11025 Hz, 1 channels, 16 bits */
	{ 0x01, 2, 8000, 4, 16, 0, NULL }, /* PCM, 8000 Hz, 2 channels, 16 bits */
	{ 0x01, 1, 8000, 2, 16, 0, NULL } /* PCM, 8000 Hz, 1 channels, 16 bits */
};

static void wf_peer_rdpsnd_activated(rdpsnd_server_context* context)
{
#ifdef WITH_RDPSND_DSOUND

	wf_directsound_activate(context);

#else

	wf_wasapi_activate(context);

#endif
	
}

int wf_rdpsnd_lock()
{
	DWORD dRes;
	wfInfo* wfi;

	wfi = wf_info_get_instance();

	dRes = WaitForSingleObject(wfi->snd_mutex, INFINITE);

	switch (dRes)
	{
	case WAIT_ABANDONED:
	case WAIT_OBJECT_0:
		return TRUE;
		break;

	case WAIT_TIMEOUT:
		return FALSE;
		break;

	case WAIT_FAILED:
		printf("wf_rdpsnd_lock failed with 0x%08X\n", GetLastError());
		return -1;
		break;
	}

	return -1;
}

int wf_rdpsnd_unlock()
{
	wfInfo* wfi;

	wfi = wf_info_get_instance();

	if (ReleaseMutex(wfi->snd_mutex) == 0)
	{
		printf("wf_rdpsnd_unlock failed with 0x%08X\n", GetLastError());
		return -1;
	}

	return TRUE;
}

BOOL wf_peer_rdpsnd_init(wfPeerContext* context)
{
	wfInfo* wfi;
	
	wfi = wf_info_get_instance();

	wfi->snd_mutex = CreateMutex(NULL, FALSE, NULL);
	context->rdpsnd = rdpsnd_server_context_new(context->vcm);
	context->rdpsnd->data = context;

	context->rdpsnd->server_formats = test_audio_formats;
	context->rdpsnd->num_server_formats =
			sizeof(test_audio_formats) / sizeof(test_audio_formats[0]);

	context->rdpsnd->src_format.wFormatTag = 1;
	context->rdpsnd->src_format.nChannels = 2;
	context->rdpsnd->src_format.nSamplesPerSec = 44100;
	context->rdpsnd->src_format.wBitsPerSample = 16;

	context->rdpsnd->Activated = wf_peer_rdpsnd_activated;

	context->rdpsnd->Initialize(context->rdpsnd);

	wf_rdpsnd_set_latest_peer(context);

	wfi->snd_stop = FALSE;
	return TRUE;
}

