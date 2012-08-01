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

#ifndef WF_PEER_H
#define WF_PEER_H

#include "wfreerdp.h"

void wf_peer_context_new(freerdp_peer* client, wfPeerContext* context);
void wf_peer_context_free(freerdp_peer* client, wfPeerContext* context);

void wf_peer_init(freerdp_peer* client);

boolean wf_peer_post_connect(freerdp_peer* client);
boolean wf_peer_activate(freerdp_peer* client);

void wf_peer_synchronize_event(rdpInput* input, uint32 flags);

#endif /* WF_PEER_H */
