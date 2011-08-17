/**
 * FreeRDP: A Remote Desktop Protocol client.
 * Dynamic Virtual Channel Manager
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

#ifndef __DVCMAN_H
#define __DVCMAN_H

#include <freerdp/dvc.h>
#include "drdynvc_main.h"

IWTSVirtualChannelManager* dvcman_new(drdynvcPlugin* plugin);
int dvcman_load_plugin(IWTSVirtualChannelManager* pChannelMgr, RDP_PLUGIN_DATA* data);
void dvcman_free(IWTSVirtualChannelManager* pChannelMgr);
int dvcman_init(IWTSVirtualChannelManager* pChannelMgr);
int dvcman_create_channel(IWTSVirtualChannelManager* pChannelMgr, uint32 ChannelId, const char* ChannelName);
int dvcman_close_channel(IWTSVirtualChannelManager* pChannelMgr, uint32 ChannelId);
int dvcman_receive_channel_data_first(IWTSVirtualChannelManager* pChannelMgr, uint32 ChannelId, uint32 length);
int dvcman_receive_channel_data(IWTSVirtualChannelManager* pChannelMgr, uint32 ChannelId, uint8* data, uint32 data_size);

#endif
