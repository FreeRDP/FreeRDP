/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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
#include <freerdp/addin.h>

#include <winpr/collections.h>

#include "drdynvc_main.h"

#define MAX_PLUGINS 32

struct _DVCMAN
{
	IWTSVirtualChannelManager iface;

	drdynvcPlugin* drdynvc;

	const char* plugin_names[MAX_PLUGINS];
	IWTSPlugin* plugins[MAX_PLUGINS];
	int num_plugins;

	IWTSListener* listeners[MAX_PLUGINS];
	int num_listeners;

	wArrayList* channels;
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
	HANDLE dvc_chan_mutex;
};
typedef struct _DVCMAN_CHANNEL DVCMAN_CHANNEL;

IWTSVirtualChannelManager* dvcman_new(drdynvcPlugin* plugin);
int dvcman_load_addin(IWTSVirtualChannelManager* pChannelMgr, ADDIN_ARGV* args);
void dvcman_free(IWTSVirtualChannelManager* pChannelMgr);
int dvcman_init(IWTSVirtualChannelManager* pChannelMgr);
int dvcman_create_channel(IWTSVirtualChannelManager* pChannelMgr, UINT32 ChannelId, const char* ChannelName);
int dvcman_close_channel(IWTSVirtualChannelManager* pChannelMgr, UINT32 ChannelId);
int dvcman_receive_channel_data_first(IWTSVirtualChannelManager* pChannelMgr, UINT32 ChannelId, UINT32 length);
int dvcman_receive_channel_data(IWTSVirtualChannelManager* pChannelMgr, UINT32 ChannelId, BYTE* data, UINT32 data_size);

void* dvcman_get_channel_interface_by_name(IWTSVirtualChannelManager* pChannelMgr, const char* ChannelName);

#endif

