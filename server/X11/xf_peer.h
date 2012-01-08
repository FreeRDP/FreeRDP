/**
 * FreeRDP: A Remote Desktop Protocol Client
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

#include <freerdp/gdi/gdi.h>
#include <freerdp/gdi/dc.h>
#include <freerdp/gdi/region.h>
#include <freerdp/codec/rfx.h>
#include <freerdp/listener.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/stopwatch.h>

#include "xfreerdp.h"

struct xf_peer_context
{
	rdpContext _p;

	HGDI_DC hdc;
	STREAM* s;
	xfInfo* info;
	int pipe_fd[2];
	boolean activated;
	RFX_CONTEXT* rfx_context;
	uint8* capture_buffer;
	pthread_t thread;
	int activations;
	STOPWATCH* stopwatch;
};
typedef struct xf_peer_context xfPeerContext;

void xf_peer_accepted(freerdp_listener* instance, freerdp_peer* client);

#endif /* __XF_PEER_H */
