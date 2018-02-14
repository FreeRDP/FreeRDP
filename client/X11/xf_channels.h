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

#ifndef FREERDP_CLIENT_X11_CHANNELS_H
#define FREERDP_CLIENT_X11_CHANNELS_H

#include <freerdp/freerdp.h>
#include <freerdp/client/channels.h>
#include <freerdp/client/rdpei.h>
#include <freerdp/client/tsmf.h>
#include <freerdp/client/rail.h>
#include <freerdp/client/cliprdr.h>
#include <freerdp/client/rdpgfx.h>
#include <freerdp/client/encomsp.h>
#include <freerdp/client/disp.h>
#include <freerdp/client/geometry.h>
#include <freerdp/client/video.h>

int xf_on_channel_connected(freerdp* instance, const char* name, void* pInterface);
int xf_on_channel_disconnected(freerdp* instance, const char* name, void* pInterface);

void xf_OnChannelConnectedEventHandler(void* context, ChannelConnectedEventArgs* e);
void xf_OnChannelDisconnectedEventHandler(void* context, ChannelDisconnectedEventArgs* e);

#endif /* FREERDP_CLIENT_X11_CHANNELS_H */
