/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Dynamic Virtual Channel
 *
 * Copyright 2010-2011 Vic Lee
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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

#ifndef FREERDP_CHANNEL_DRDYNVC_CLIENT_MAIN_H
#define FREERDP_CHANNEL_DRDYNVC_CLIENT_MAIN_H

#include <winpr/wlog.h>
#include <winpr/synch.h>
#include <freerdp/settings.h>
#include <winpr/collections.h>

#include <freerdp/api.h>
#include <freerdp/svc.h>
#include <freerdp/dvc.h>
#include <freerdp/addin.h>
#include <freerdp/channels/log.h>
#include <freerdp/client/drdynvc.h>
#include <freerdp/freerdp.h>

typedef struct drdynvc_plugin drdynvcPlugin;

typedef struct
{
	IWTSVirtualChannelManager iface;

	drdynvcPlugin* drdynvc;

	wArrayList* plugin_names;
	wArrayList* plugins;

	wHashTable* listeners;
	wHashTable* channelsById;
	wStreamPool* pool;
} DVCMAN;

typedef struct
{
	IWTSListener iface;

	DVCMAN* dvcman;
	char* channel_name;
	UINT32 flags;
	IWTSListenerCallback* listener_callback;
} DVCMAN_LISTENER;

typedef struct
{
	IDRDYNVC_ENTRY_POINTS iface;

	DVCMAN* dvcman;
	const ADDIN_ARGV* args;
	rdpContext* context;
} DVCMAN_ENTRY_POINTS;

typedef enum
{
	DVC_CHANNEL_INIT,
	DVC_CHANNEL_RUNNING,
	DVC_CHANNEL_CLOSED
} DVC_CHANNEL_STATE;

typedef struct
{
	IWTSVirtualChannel iface;

	volatile LONG refCounter;
	DVC_CHANNEL_STATE state;
	DVCMAN* dvcman;
	void* pInterface;
	UINT32 channel_id;
	char* channel_name;
	IWTSVirtualChannelCallback* channel_callback;

	wStream* dvc_data;
	UINT32 dvc_data_length;
	CRITICAL_SECTION lock;
} DVCMAN_CHANNEL;

typedef enum
{
	DRDYNVC_STATE_INITIAL,
	DRDYNVC_STATE_CAPABILITIES,
	DRDYNVC_STATE_READY,
	DRDYNVC_STATE_OPENING_CHANNEL,
	DRDYNVC_STATE_SEND_RECEIVE,
	DRDYNVC_STATE_FINAL
} DRDYNVC_STATE;

struct drdynvc_plugin
{
	CHANNEL_DEF channelDef;
	CHANNEL_ENTRY_POINTS_FREERDP_EX channelEntryPoints;

	wLog* log;
	HANDLE thread;
	BOOL async;
	wStream* data_in;
	void* InitHandle;
	DWORD OpenHandle;
	wMessageQueue* queue;

	DRDYNVC_STATE state;
	DrdynvcClientContext* context;

	UINT16 version;
	int PriorityCharge0;
	int PriorityCharge1;
	int PriorityCharge2;
	int PriorityCharge3;
	rdpContext* rdpcontext;

	IWTSVirtualChannelManager* channel_mgr;
};

#endif /* FREERDP_CHANNEL_DRDYNVC_CLIENT_MAIN_H */
