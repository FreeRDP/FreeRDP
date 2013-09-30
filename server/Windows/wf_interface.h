/**
* FreeRDP: A Remote Desktop Protocol Client
* FreeRDP Windows Server
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

#ifndef WF_INTERFACE_H
#define WF_INTERFACE_H

#include <winpr/windows.h>

#include <freerdp/api.h>
#include <freerdp/freerdp.h>
#include <freerdp/listener.h>

#include <freerdp/freerdp.h>
#include <freerdp/codec/rfx.h>

#include <freerdp/server/rdpsnd.h>

#define WF_SRV_CALLBACK_EVENT_CONNECT 1
#define WF_SRV_CALLBACK_EVENT_DISCONNECT 2
#define WF_SRV_CALLBACK_EVENT_ACTIVATE 4
#define WF_SRV_CALLBACK_EVENT_AUTH 8

typedef struct wf_info wfInfo;
typedef struct wf_peer_context wfPeerContext;

struct wf_info
{
	wStream* s;

	//screen and monitor info
	int screenID;
	int virtscreen_width;
	int virtscreen_height;
	int servscreen_width;
	int servscreen_height;
	int servscreen_xoffset;
	int servscreen_yoffset;
	//int width;
	//int height;

	int frame_idx;
	int bitsPerPixel;
	HDC driverDC;
	int peerCount;
	int activePeerCount;
	void* changeBuffer;
	int framesPerSecond;
	LPTSTR deviceKey;
	TCHAR deviceName[32];
	freerdp_peer** peers;
	BOOL mirrorDriverActive;
	UINT framesWaiting;

	HANDLE snd_mutex;
	BOOL snd_stop;
	AUDIO_FORMAT* agreed_format;

	RECT invalid;
	HANDLE mutex;
	BOOL updatePending;
	HANDLE updateEvent;
	HANDLE updateThread;
	HANDLE updateSemaphore;
	RFX_CONTEXT* rfx_context;
	unsigned long lastUpdate;
	unsigned long nextUpdate;
	SURFACE_BITS_COMMAND cmd;

	BOOL input_disabled;
	BOOL force_all_disconnect;
};

struct wf_peer_context
{
	rdpContext _p;

	wfInfo* info;
	int frame_idx;
	HANDLE updateEvent;
	BOOL socketClose;
	HANDLE socketEvent;
	HANDLE socketThread;
	HANDLE socketSemaphore;

	WTSVirtualChannelManager* vcm;
	RdpsndServerContext* rdpsnd;
};

struct wf_server
{
	DWORD port;
	HANDLE thread;
	freerdp_listener* instance;
};
typedef struct wf_server wfServer;

typedef void (__stdcall* cbCallback) (int, UINT32);

FREERDP_API int get_screen_info(int id, _TCHAR* name, int* w, int* h, int* b);
FREERDP_API void set_screen_id(int id);

FREERDP_API BOOL wfreerdp_server_start(wfServer* server);
FREERDP_API BOOL wfreerdp_server_stop(wfServer* server);

FREERDP_API wfServer* wfreerdp_server_new(void);
FREERDP_API void wfreerdp_server_free(wfServer* server);

FREERDP_API BOOL wfreerdp_server_is_running(wfServer* server);

FREERDP_API UINT32 wfreerdp_server_num_peers(void);
FREERDP_API UINT32 wfreerdp_server_get_peer_hostname(int pId, wchar_t * dstStr);
FREERDP_API BOOL wfreerdp_server_peer_is_local(int pId);
FREERDP_API BOOL wfreerdp_server_peer_is_connected(int pId);
FREERDP_API BOOL wfreerdp_server_peer_is_activated(int pId);
FREERDP_API BOOL wfreerdp_server_peer_is_authenticated(int pId);

FREERDP_API void wfreerdp_server_register_callback_event(cbCallback cb);

void wfreerdp_server_peer_callback_event(int pId, UINT32 eType);

#endif /* WF_INTERFACE_H */
