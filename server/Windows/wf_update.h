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

#ifndef WF_UPDATE_H
#define WF_UPDATE_H

#include "wf_interface.h"

void wf_update_encode(wfInfo* wfi);
void wf_update_send(wfInfo* wfi);

DWORD WINAPI wf_update_thread(LPVOID lpParam);

void wf_update_begin(wfInfo* wfi);
void wf_update_peer_send(wfInfo* wfi, wfPeerContext* context);
void wf_update_end(wfInfo* wfi);

void wf_update_peer_activate(wfInfo* wfi, wfPeerContext* context);
void wf_update_peer_deactivate(wfInfo* wfi, wfPeerContext* context);

#endif /* WF_UPDATE_H */
