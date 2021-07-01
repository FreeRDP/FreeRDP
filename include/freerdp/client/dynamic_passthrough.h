/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Dynamic Virtual Channel Passthrough
 *
 * Copyright 2021 Alexandru Bagu <alexandru.bagu@gmail.com>
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

#ifndef FREERDP_CLIENT_DYNAMIC_PASSTHROUGH_H
#define FREERDP_CLIENT_DYNAMIC_PASSTHROUGH_H

#include <freerdp/dvc.h>

typedef struct _dynamic_passthrough_client_context DynamicPassthroughClientContext;

typedef UINT (*pcDynamicPassthroughClientSend)(DynamicPassthroughClientContext* context,
                                               const wStream* stream);

typedef UINT (*pcDynamicPassthroughClientOnReceive)(DynamicPassthroughClientContext* context,
                                                    const wStream* stream);

typedef struct _DYNAMIC_PASSTHROUGH_DVCMAN_CHANNEL DYNAMIC_PASSTHROUGH_DVCMAN_CHANNEL;
typedef UINT (*pcDynamicPassthroughDisconnect)(DYNAMIC_PASSTHROUGH_DVCMAN_CHANNEL* context);

struct _DYNAMIC_PASSTHROUGH_DVCMAN_CHANNEL
{
	void* dvcman_channel;
	IWTSVirtualChannel* iface;
	IWTSVirtualChannelCallback* channel_callback;

	pcDynamicPassthroughDisconnect disconnect;
};

struct _dynamic_passthrough_client_context
{
	void* custom;
	const char* channelname;
	void* server;
	DYNAMIC_PASSTHROUGH_DVCMAN_CHANNEL* dvcman_channel;

	pcDynamicPassthroughClientSend Send;
};

typedef struct _DYNAMIC_PASSTHROUGH_DVCMAN_CHANNEL_CALLLBACK DYNAMIC_PASSTHROUGH_DVCMAN_CHANNEL_CALLLBACK;
struct _DYNAMIC_PASSTHROUGH_DVCMAN_CHANNEL_CALLLBACK
{
	IWTSVirtualChannelCallback iface;

	IWTSPlugin* plugin;
	IWTSVirtualChannelManager* channel_mgr;
	IWTSVirtualChannel* channel;

  void* custom;
};
#endif /*FREERDP_CLIENT_DYNAMIC_PASSTHROUGH_H*/
