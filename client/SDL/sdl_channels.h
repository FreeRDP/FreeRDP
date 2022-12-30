/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Client Channels
 *
 * Copyright 2022 Armin Novak <armin.novak@thincast.com>
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

#ifndef FREERDP_CLIENT_SDL_CHANNELS_H
#define FREERDP_CLIENT_SDL_CHANNELS_H

#include <freerdp/freerdp.h>
#include <freerdp/client/channels.h>

int sdl_on_channel_connected(freerdp* instance, const char* name, void* pInterface);
int sdl_on_channel_disconnected(freerdp* instance, const char* name, void* pInterface);

void sdl_OnChannelConnectedEventHandler(void* context, const ChannelConnectedEventArgs* e);
void sdl_OnChannelDisconnectedEventHandler(void* context, const ChannelDisconnectedEventArgs* e);

#endif /* FREERDP_CLIENT_SDL_CHANNELS_H */
