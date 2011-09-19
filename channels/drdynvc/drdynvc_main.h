/**
 * FreeRDP: A Remote Desktop Protocol client.
 * Dynamic Virtual Channel
 *
 * Copyright 2010-2011 Vic Lee
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

#ifndef __DRDYNVC_MAIN_H
#define __DRDYNVC_MAIN_H

#include <freerdp/types.h>

typedef struct drdynvc_plugin drdynvcPlugin;

int drdynvc_write_data(drdynvcPlugin* plugin, uint32 ChannelId, uint8* data, uint32 data_size);
int drdynvc_push_event(drdynvcPlugin* plugin, RDP_EVENT* event);

#endif
