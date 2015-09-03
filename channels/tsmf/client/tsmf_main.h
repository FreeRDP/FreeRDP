/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Video Redirection Virtual Channel
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

#ifndef __TSMF_MAIN_H
#define __TSMF_MAIN_H

#include <freerdp/freerdp.h>

typedef struct _TSMF_LISTENER_CALLBACK TSMF_LISTENER_CALLBACK;

typedef struct _TSMF_CHANNEL_CALLBACK TSMF_CHANNEL_CALLBACK;

typedef struct _TSMF_PLUGIN TSMF_PLUGIN;

struct _TSMF_LISTENER_CALLBACK
{
	IWTSListenerCallback iface;

	IWTSPlugin* plugin;
	IWTSVirtualChannelManager* channel_mgr;
};

struct _TSMF_CHANNEL_CALLBACK
{
	IWTSVirtualChannelCallback iface;

	IWTSPlugin* plugin;
	IWTSVirtualChannelManager* channel_mgr;
	IWTSVirtualChannel* channel;

	BYTE presentation_id[GUID_SIZE];
	UINT32 stream_id;
};

struct _TSMF_PLUGIN
{
	IWTSPlugin iface;

	IWTSListener* listener;
	TSMF_LISTENER_CALLBACK* listener_callback;

	const char* decoder_name;
	const char* audio_name;
	const char* audio_device;

	rdpContext* rdpcontext;
};

BOOL tsmf_send_eos_response(IWTSVirtualChannelCallback* pChannelCallback, UINT32 message_id);
BOOL tsmf_playback_ack(IWTSVirtualChannelCallback* pChannelCallback,
		UINT32 message_id, UINT64 duration, UINT32 data_size);

#endif

