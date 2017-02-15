/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef __WF_CHANNELS_H
#define __WF_CHANNELS_H

#include <freerdp/freerdp.h>
#include <freerdp/client/channels.h>
#include <freerdp/client/rdpei.h>
#include <freerdp/client/rdpgfx.h>
#include <freerdp/client/encomsp.h>
#include <freerdp/client/cliprdr.h>

#include "wf_client.h"

void wf_OnChannelConnectedEventHandler(rdpContext* context,
                                       ChannelConnectedEventArgs* e);
void wf_OnChannelDisconnectedEventHandler(rdpContext* context,
        ChannelDisconnectedEventArgs* e);

#endif
