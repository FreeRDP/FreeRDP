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

#include <freerdp/config.h>

#include <stdio.h>
#include <stdlib.h>

#include <winpr/windows.h>
#include <freerdp/server/server-common.h>

#include "wf_rdpsnd.h"
#include "wf_info.h"

#ifdef WITH_RDPSND_DSOUND

#include "wf_directsound.h"

#else

#include "wf_wasapi.h"

#endif

#include <freerdp/log.h>
#define TAG SERVER_TAG("windows")

static void wf_peer_rdpsnd_activated(RdpsndServerContext* context)
{
	wfInfo* wfi;
	size_t i;
	wfi = wf_info_get_instance();
	wfi->agreed_format = NULL;
	WLog_DBG(TAG, "Client supports the following %d formats:", context->num_client_formats);

	for (i = 0; i < context->num_client_formats; i++)
	{
		size_t j;
		// TODO: improve the way we agree on a format
		for (j = 0; j < context->num_server_formats; j++)
		{
			if ((context->client_formats[i].wFormatTag == context->server_formats[j].wFormatTag) &&
			    (context->client_formats[i].nChannels == context->server_formats[j].nChannels) &&
			    (context->client_formats[i].nSamplesPerSec ==
			     context->server_formats[j].nSamplesPerSec))
			{
				WLog_DBG(TAG, "agreed on format!");
				wfi->agreed_format = (AUDIO_FORMAT*)&context->server_formats[j];
				break;
			}
		}

		if (wfi->agreed_format != NULL)
			break;
	}

	if (wfi->agreed_format == NULL)
	{
		WLog_ERR(TAG, "Could not agree on a audio format with the server");
		return;
	}

	context->SelectFormat(context, i);
	context->SetVolume(context, 0x7FFF, 0x7FFF);
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
			WLog_ERR(TAG, "wf_rdpsnd_lock failed with 0x%08lX", GetLastError());
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
		WLog_DBG(TAG, "wf_rdpsnd_unlock failed with 0x%08lX", GetLastError());
		return -1;
	}

	return TRUE;
}

BOOL wf_peer_rdpsnd_init(wfPeerContext* context)
{
	wfInfo* wfi = wf_info_get_instance();

	if (!wfi)
		return FALSE;

	if (!(wfi->snd_mutex = CreateMutex(NULL, FALSE, NULL)))
		return FALSE;

	context->rdpsnd = rdpsnd_server_context_new(context->vcm);
	context->rdpsnd->rdpcontext = &context->_p;
	context->rdpsnd->data = context;
	context->rdpsnd->num_server_formats =
	    server_rdpsnd_get_formats(&context->rdpsnd->server_formats);

	if (context->rdpsnd->num_server_formats > 0)
		context->rdpsnd->src_format = &context->rdpsnd->server_formats[0];

	context->rdpsnd->Activated = wf_peer_rdpsnd_activated;
	context->rdpsnd->Initialize(context->rdpsnd, TRUE);
	wf_rdpsnd_set_latest_peer(context);
	wfi->snd_stop = FALSE;
	return TRUE;
}
