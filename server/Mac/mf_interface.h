/**
 * FreeRDP: A Remote Desktop Protocol Client
 * FreeRDP Mac OS X Server
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2012 Corey Clayton <can.of.tuna@gmail.com>
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

#ifndef MF_INTERFACE_H
#define MF_INTERFACE_H

#include <pthread.h>

#include <freerdp/codec/rfx.h>
#include <freerdp/codec/nsc.h>
#include <freerdp/listener.h>

#include <winpr/crt.h>

//#ifdef WITH_SERVER_CHANNELS
#include <freerdp/channels/wtsvc.h>
//#endif

//#ifdef CHANNEL_RDPSND_SERVER
#include <freerdp/server/rdpsnd.h>
//#include "mf_rdpsnd.h"
//#endif

//#ifdef CHANNEL_AUDIN_SERVER
#include <freerdp/server/audin.h>
//#include "mf_audin.h"
//#endif

typedef struct mf_info mfInfo;
typedef struct mf_peer_context mfPeerContext;

struct mf_peer_context
{
	rdpContext _p;
	
	mfInfo* info;
	wStream* s;
	BOOL activated;
	UINT32 frame_id;
	BOOL audin_open;
	RFX_CONTEXT* rfx_context;
	NSC_CONTEXT* nsc_context;
	
	//#ifdef WITH_SERVER_CHANNELS
	WTSVirtualChannelManager* vcm;
	//#endif
	//#ifdef CHANNEL_AUDIN_SERVER
	audin_server_context* audin;
	//#endif
	
	//#ifdef CHANNEL_RDPSND_SERVER
	RdpsndServerContext* rdpsnd;
	//#endif
};


struct mf_info
{
	//STREAM* s;
	
	//screen and monitor info
	UINT32 screenID;
	UINT32 virtscreen_width;
	UINT32 virtscreen_height;
	UINT32 servscreen_width;
	UINT32 servscreen_height;
	UINT32 servscreen_xoffset;
	UINT32 servscreen_yoffset;
	
	int bitsPerPixel;
	int peerCount;
	int activePeerCount;
	int framesPerSecond;
	freerdp_peer** peers;
	unsigned int framesWaiting;
	UINT32 scale;
	
	RFX_RECT invalid;
	pthread_mutex_t mutex;
	
	BOOL mouse_down_left;
	BOOL mouse_down_right;
	BOOL mouse_down_other;
	BOOL input_disabled;
	BOOL force_all_disconnect;
};

#endif
