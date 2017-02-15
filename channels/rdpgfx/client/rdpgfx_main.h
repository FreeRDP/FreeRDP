/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Graphics Pipeline Extension
 *
 * Copyright 2013-2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_CHANNEL_RDPGFX_CLIENT_MAIN_H
#define FREERDP_CHANNEL_RDPGFX_CLIENT_MAIN_H

#include <freerdp/dvc.h>
#include <freerdp/types.h>
#include <freerdp/addin.h>

#include <winpr/wlog.h>
#include <winpr/collections.h>

#include <freerdp/client/rdpgfx.h>
#include <freerdp/channels/log.h>
#include <freerdp/codec/zgfx.h>
#include <freerdp/freerdp.h>

struct _RDPGFX_CHANNEL_CALLBACK
{
	IWTSVirtualChannelCallback iface;

	IWTSPlugin* plugin;
	IWTSVirtualChannelManager* channel_mgr;
	IWTSVirtualChannel* channel;
};
typedef struct _RDPGFX_CHANNEL_CALLBACK RDPGFX_CHANNEL_CALLBACK;

struct _RDPGFX_LISTENER_CALLBACK
{
	IWTSListenerCallback iface;

	IWTSPlugin* plugin;
	IWTSVirtualChannelManager* channel_mgr;
	RDPGFX_CHANNEL_CALLBACK* channel_callback;
};
typedef struct _RDPGFX_LISTENER_CALLBACK RDPGFX_LISTENER_CALLBACK;

struct _RDPGFX_PLUGIN
{
	IWTSPlugin iface;

	IWTSListener* listener;
	RDPGFX_LISTENER_CALLBACK* listener_callback;

	rdpSettings* settings;

	BOOL ThinClient;
	BOOL SmallCache;
	BOOL Progressive;
	BOOL ProgressiveV2;
	BOOL H264;
	BOOL AVC444;

	ZGFX_CONTEXT* zgfx;
	UINT32 UnacknowledgedFrames;
	UINT32 TotalDecodedFrames;
	BOOL suspendFrameAcks;

	wHashTable* SurfaceTable;

	UINT16 MaxCacheSlot;
	void* CacheSlots[25600];
	rdpContext* rdpcontext;
};
typedef struct _RDPGFX_PLUGIN RDPGFX_PLUGIN;

#endif /* FREERDP_CHANNEL_RDPGFX_CLIENT_MAIN_H */

