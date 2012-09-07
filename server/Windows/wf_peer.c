/**
 * FreeRDP: A Remote Desktop Protocol Client
 * FreeRDP Windows Server
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

#include <winpr/tchar.h>
#include <winpr/windows.h>

#include <freerdp/listener.h>
#include <freerdp/utils/sleep.h>
#include <freerdp/codec/rfx.h>
#include <freerdp/utils/stream.h>

#include "wf_input.h"
#include "wf_mirage.h"
#include "wf_update.h"

#include "wf_peer.h"

void wf_peer_context_new(freerdp_peer* client, wfPeerContext* context)
{
	context->info = wf_info_get_instance();
	wf_info_peer_register(context->info, context);
}

void wf_peer_context_free(freerdp_peer* client, wfPeerContext* context)
{
	wf_info_peer_unregister(context->info, context);
}

void wf_peer_start_encoder_thread(freerdp_peer* client)
{
	wfInfo* wfi;
	
	wfi = ((wfPeerContext*) client->context)->info;

	wf_info_lock(wfi);

	wfi->updateThread = CreateThread(NULL, 0, wf_update_thread, wfi, 0, NULL);

	if (!wfi->updateThread)
	{
		_tprintf(_T("Failed to create update thread\n"));
	}

	wf_info_unlock(wfi);
}

void wf_peer_init(freerdp_peer* client)
{
	client->context_size = sizeof(wfPeerContext);
	client->ContextNew = (psPeerContextNew) wf_peer_context_new;
	client->ContextFree = (psPeerContextFree) wf_peer_context_free;
	
	freerdp_peer_context_new(client);
}

boolean wf_peer_post_connect(freerdp_peer* client)
{
	wfInfo* wfi;
	rdpSettings* settings;
	wfPeerContext* context = (wfPeerContext*) client->context;

	wfi = context->info;
	settings = client->settings;

	/**
	 * This callback is called when the entire connection sequence is done, i.e. we've received the
	 * Font List PDU from the client and sent out the Font Map PDU.
	 * The server may start sending graphics output and receiving keyboard/mouse input after this
	 * callback returns.
	 */

	if ((settings->width != wfi->width) || (settings->height != wfi->height))
	{
		printf("Client requested resolution %dx%d, but will resize to %dx%d\n",
			settings->width, settings->height, wfi->width, wfi->height);

		settings->width = wfi->width;
		settings->height = wfi->height;
		settings->color_depth = wfi->bitsPerPixel;

		client->update->DesktopResize(client->update->context);
	}

	wf_peer_start_encoder_thread(client);

	return true;
}

boolean wf_peer_activate(freerdp_peer* client)
{
	wfPeerContext* context = (wfPeerContext*) client->context;

	context->activated = true;

	return true;
}

void wf_peer_synchronize_event(rdpInput* input, uint32 flags)
{

}
