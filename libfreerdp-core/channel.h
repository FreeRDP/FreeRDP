/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Virtual Channels
 *
 * Copyright 2011 Vic Lee
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

#ifndef __CHANNEL_H
#define __CHANNEL_H

boolean freerdp_channel_send(rdpRdp* rdp, uint16 channel_id, uint8* data, int size);
void freerdp_channel_process(freerdp* instance, STREAM* s, uint16 channel_id);
void freerdp_channel_peer_process(freerdp_peer* client, STREAM* s, uint16 channel_id);

#endif /* __CHANNEL_H */
