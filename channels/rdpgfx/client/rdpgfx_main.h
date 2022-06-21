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

#include <freerdp/client/channels.h>
#include <freerdp/client/rdpgfx.h>
#include <freerdp/channels/log.h>
#include <freerdp/codec/zgfx.h>
#include <freerdp/cache/persistent.h>
#include <freerdp/freerdp.h>

typedef struct
{
	GENERIC_DYNVC_PLUGIN base;

	ZGFX_CONTEXT* zgfx;
	UINT32 UnacknowledgedFrames;
	UINT32 TotalDecodedFrames;
	UINT64 StartDecodingTime;
	BOOL suspendFrameAcks;
	BOOL sendFrameAcks;

	wHashTable* SurfaceTable;

	UINT16 MaxCacheSlots;
	void* CacheSlots[25600];
	rdpPersistentCache* persistent;

	rdpContext* rdpcontext;

	wLog* log;
	RDPGFX_CAPSET ConnectionCaps;
	RdpgfxClientContext* context;
} RDPGFX_PLUGIN;

#endif /* FREERDP_CHANNEL_RDPGFX_CLIENT_MAIN_H */
