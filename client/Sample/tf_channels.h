/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Sample Client Channels
 *
 * Copyright 2018 Armin Novak <armin.novak@thincast.com>
 * Copyright 2018 Thincast Technologies GmbH
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

#ifndef FREERDP_CLIENT_SAMPLE_CHANNELS_H
#define FREERDP_CLIENT_SAMPLE_CHANNELS_H

#include <freerdp/freerdp.h>
#include <freerdp/client/channels.h>

int tf_on_channel_connected(freerdp* instance, const char* name, void* pInterface);
int tf_on_channel_disconnected(freerdp* instance, const char* name, void* pInterface);

void tf_OnChannelConnectedEventHandler(void* context, const ChannelConnectedEventArgs* e);
void tf_OnChannelDisconnectedEventHandler(void* context, const ChannelDisconnectedEventArgs* e);

#endif /* FREERDP_CLIENT_SAMPLE_CHANNELS_H */
