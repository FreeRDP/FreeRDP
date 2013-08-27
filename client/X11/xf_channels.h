/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Client Channels
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef __XF_CHANNELS_H
#define __XF_CHANNELS_H

#include <freerdp/freerdp.h>
#include <freerdp/client/channels.h>
#include <freerdp/client/rdpei.h>

int xf_on_channel_connected(freerdp* instance, const char* name, void* pInterface);
int xf_on_channel_disconnected(freerdp* instance, const char* name, void* pInterface);

void xf_OnChannelConnectedEventHandler(rdpContext* context, ChannelConnectedEventArgs* e);
void xf_OnChannelDisconnectedEventHandler(rdpContext* context, ChannelDisconnectedEventArgs* e);

#endif
