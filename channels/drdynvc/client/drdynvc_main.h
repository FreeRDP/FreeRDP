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

#ifndef __DRDYNVC_MAIN_H
#define __DRDYNVC_MAIN_H

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

#define MAX_PLUGINS 32

struct _DVCMAN
{
	IWTSVirtualChannelManager iface;

	drdynvcPlugin* drdynvc;

	int num_plugins;
	const char* plugin_names[MAX_PLUGINS];
	IWTSPlugin* plugins[MAX_PLUGINS];

	int num_listeners;
	IWTSListener* listeners[MAX_PLUGINS];

	wArrayList* channels;
	wStreamPool* pool;
};
typedef struct _DVCMAN DVCMAN;

struct _DVCMAN_LISTENER
{
	IWTSListener iface;

	DVCMAN* dvcman;
	char* channel_name;
	UINT32 flags;
	IWTSListenerCallback* listener_callback;
};
typedef struct _DVCMAN_LISTENER DVCMAN_LISTENER;

struct _DVCMAN_ENTRY_POINTS
{
	IDRDYNVC_ENTRY_POINTS iface;

	DVCMAN* dvcman;
	ADDIN_ARGV* args;
	rdpSettings* settings;
};
typedef struct _DVCMAN_ENTRY_POINTS DVCMAN_ENTRY_POINTS;

struct _DVCMAN_CHANNEL
{
	IWTSVirtualChannel iface;

	int status;
	DVCMAN* dvcman;
	void* pInterface;
	UINT32 channel_id;
	char* channel_name;
	IWTSVirtualChannelCallback* channel_callback;

	wStream* dvc_data;
	UINT32 dvc_data_length;
	CRITICAL_SECTION lock;
};
typedef struct _DVCMAN_CHANNEL DVCMAN_CHANNEL;

enum _DRDYNVC_STATE
{
	DRDYNVC_STATE_INITIAL,
	DRDYNVC_STATE_CAPABILITIES,
	DRDYNVC_STATE_READY,
	DRDYNVC_STATE_OPENING_CHANNEL,
	DRDYNVC_STATE_SEND_RECEIVE,
	DRDYNVC_STATE_FINAL
};
typedef enum _DRDYNVC_STATE DRDYNVC_STATE;

#define CREATE_REQUEST_PDU		0x01
#define DATA_FIRST_PDU			0x02
#define DATA_PDU			0x03
#define CLOSE_REQUEST_PDU		0x04
#define CAPABILITY_REQUEST_PDU		0x05

struct drdynvc_plugin
{
	CHANNEL_DEF channelDef;
	CHANNEL_ENTRY_POINTS_FREERDP channelEntryPoints;

	wLog* log;
	HANDLE thread;
	wStream* data_in;
	void* InitHandle;
	DWORD OpenHandle;
	wMessageQueue* queue;

	DRDYNVC_STATE state;
	DrdynvcClientContext* context;

	int version;
	int PriorityCharge0;
	int PriorityCharge1;
	int PriorityCharge2;
	int PriorityCharge3;
	rdpContext* rdpcontext;


	IWTSVirtualChannelManager* channel_mgr;
};

#endif
