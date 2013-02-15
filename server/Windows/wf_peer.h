/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifndef WF_PEER_H
#define WF_PEER_H

#include "wf_interface.h"

#include <freerdp/listener.h>



void wf_peer_context_new(freerdp_peer* client, wfPeerContext* context);
void wf_peer_context_free(freerdp_peer* client, wfPeerContext* context);

void wf_peer_init(freerdp_peer* client);

void wf_dxgi_encode(freerdp_peer* client, UINT timeout);
void wf_rfx_encode(freerdp_peer* client);

BOOL wf_peer_post_connect(freerdp_peer* client);
BOOL wf_peer_activate(freerdp_peer* client);

void wf_peer_synchronize_event(rdpInput* input, UINT32 flags);

void wf_peer_send_changes(freerdp_peer* client);

void wf_detect_win_ver(void);

void wf_peer_accepted(freerdp_listener* instance, freerdp_peer* client);

DWORD WINAPI wf_peer_main_loop(LPVOID lpParam);


#endif /* WF_PEER_H */
