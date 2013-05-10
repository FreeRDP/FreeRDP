/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Peer
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef __XF_PEER_H
#define __XF_PEER_H

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/stream.h>
#include <winpr/collections.h>

#include <freerdp/gdi/gdi.h>
#include <freerdp/gdi/dc.h>
#include <freerdp/gdi/region.h>
#include <freerdp/codec/rfx.h>
#include <freerdp/listener.h>
#include <freerdp/utils/stopwatch.h>

typedef struct xf_peer_context xfPeerContext;

#include "xfreerdp.h"

#define PeerEvent_Base						0

#define PeerEvent_Class						(PeerEvent_Base + 1)

#define PeerEvent_EncodeRegion					1

struct xf_peer_context
{
	rdpContext _p;

	int fps;
	wStream* s;
	xfInfo* info;
	HANDLE mutex;
	BOOL activated;
	HANDLE monitorThread;
	HANDLE updateReadyEvent;
	HANDLE updateSentEvent;
	RFX_CONTEXT* rfx_context;
};

void xf_peer_accepted(freerdp_listener* instance, freerdp_peer* client);

#endif /* __XF_PEER_H */
