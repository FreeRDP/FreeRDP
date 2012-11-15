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

#include "mf_interface.h"

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

void* mf_peer_main_loop(void* arg);

#endif /* MF_PEER_H */
