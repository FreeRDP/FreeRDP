/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Mac OS X Server
 *
 * Copyright 2012 Corey Clayton <can.of.tuna@gmail.com>
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

#ifndef WF_PEER_H
#define WF_PEER_H

#include <freerdp/codec/nsc.h>

#include <freerdp/listener.h>

#ifdef WITH_SERVER_CHANNELS
#include <freerdp/channels/wtsvc.h>
#endif

#ifdef CHANNEL_RDPSND_SERVER
#include <freerdp/server/rdpsnd.h>
#include "mf_rdpsnd.h"
#endif

#ifdef CHANNEL_AUDIN_SERVER
#include "mf_audin.h"
#endif


struct mf_peer_context
{
	rdpContext _p;
    
	STREAM* s;
	BOOL activated;
	UINT32 frame_id;
	BOOL audin_open;
	RFX_CONTEXT* rfx_context;
	NSC_CONTEXT* nsc_context;

#ifdef WITH_SERVER_CHANNELS
	WTSVirtualChannelManager* vcm;
#endif
#ifdef CHANNEL_AUDIN_SERVER
	audin_server_context* audin;
#endif

#ifdef CHANNEL_RDPSND_SERVER
	rdpsnd_server_context* rdpsnd;
#endif
};
typedef struct mf_peer_context mfPeerContext;

BOOL mf_peer_get_fds(freerdp_peer* client, void** rfds, int* rcount);
BOOL mf_peer_check_fds(freerdp_peer* client);

void mf_peer_rfx_update(freerdp_peer* client);

void mf_peer_context_new(freerdp_peer* client, mfPeerContext* context);
void mf_peer_context_free(freerdp_peer* client, mfPeerContext* context);

void mf_peer_init(freerdp_peer* client);

BOOL mf_peer_post_connect(freerdp_peer* client);
BOOL mf_peer_activate(freerdp_peer* client);

void mf_peer_synchronize_event(rdpInput* input, UINT32 flags);

void mf_peer_accepted(freerdp_listener* instance, freerdp_peer* client);

static void* mf_peer_main_loop(void* arg);

#endif /* MF_PEER_H */
