/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/tchar.h>
#include <winpr/windows.h>
#include <winpr/winsock.h>

#include <freerdp/freerdp.h>
#include <freerdp/listener.h>
#include <freerdp/constants.h>
#include <freerdp/channels/wtsvc.h>
#include <freerdp/channels/channels.h>
#include <freerdp/build-config.h>

#include "wf_peer.h"
#include "wf_settings.h"
#include "wf_info.h"

#include "wf_interface.h"

#define TAG SERVER_TAG("windows")

#define SERVER_KEY "Software\\"FREERDP_VENDOR_STRING"\\" \
		FREERDP_PRODUCT_STRING"\\Server"

cbCallback cbEvent;

int get_screen_info(int id, _TCHAR* name, int* width, int* height, int* bpp)
{
	DISPLAY_DEVICE dd;

	memset(&dd, 0, sizeof(DISPLAY_DEVICE));
	dd.cb = sizeof(DISPLAY_DEVICE);

	if (EnumDisplayDevices(NULL, id, &dd, 0) != 0)
	{
		HDC dc;

		if (name != NULL)
			_stprintf(name, _T("%s (%s)"), dd.DeviceName, dd.DeviceString);

		dc = CreateDC(dd.DeviceName, NULL, NULL, NULL);
		*width = GetDeviceCaps(dc, HORZRES);
		*height = GetDeviceCaps(dc, VERTRES);
		*bpp = GetDeviceCaps(dc, BITSPIXEL);
		//ReleaseDC(NULL, dc);
		DeleteDC(dc);

	}
	else
	{
		return 0;
	}

	return 1;
}

void set_screen_id(int id)
{
	wfInfo* wfi;

	wfi = wf_info_get_instance();
	if (!wfi)
		return;
	wfi->screenID = id;

	return;
}

static DWORD WINAPI wf_server_main_loop(LPVOID lpParam)
{
	int i, fds;
	int rcount;
	int max_fds;
	void* rfds[32];
	fd_set rfds_set;
	freerdp_listener* instance;
	wfInfo* wfi;

	wfi = wf_info_get_instance();
	if (!wfi)
	{
		WLog_ERR(TAG, "Failed to get instance");
		return -1;
	}

	wfi->force_all_disconnect = FALSE;

	ZeroMemory(rfds, sizeof(rfds));
	instance = (freerdp_listener*) lpParam;

	while(wfi->force_all_disconnect == FALSE)
	{
		rcount = 0;

		if (instance->GetFileDescriptor(instance, rfds, &rcount) != TRUE)
		{
			WLog_ERR(TAG, "Failed to get FreeRDP file descriptor");
			break;
		}

		max_fds = 0;
		FD_ZERO(&rfds_set);

		for (i = 0; i < rcount; i++)
		{
			fds = (int)(long)(rfds[i]);

			if (fds > max_fds)
				max_fds = fds;

			FD_SET(fds, &rfds_set);
		}

		if (max_fds == 0)
			break;


		select(max_fds + 1, &rfds_set, NULL, NULL, NULL);

		if (instance->CheckFileDescriptor(instance) != TRUE)
		{
			WLog_ERR(TAG, "Failed to check FreeRDP file descriptor");
			break;
		}
	}

	WLog_INFO(TAG, "wf_server_main_loop terminating");
	instance->Close(instance);

	return 0;
}

BOOL wfreerdp_server_start(wfServer* server)
{
	freerdp_listener* instance;

	server->instance = freerdp_listener_new();
	server->instance->PeerAccepted = wf_peer_accepted;
	instance = server->instance;

	wf_settings_read_dword(HKEY_LOCAL_MACHINE, SERVER_KEY,
				_T("DefaultPort"), &server->port);

	if (!instance->Open(instance, NULL, (UINT16) server->port))
		return FALSE;

	if (!(server->thread = CreateThread(NULL, 0, wf_server_main_loop, (void*) instance, 0, NULL)))
		return FALSE;

	return TRUE;
}

BOOL wfreerdp_server_stop(wfServer* server)
{
	wfInfo* wfi;

	wfi = wf_info_get_instance();
	if (!wfi)
		return FALSE;
	WLog_INFO(TAG, "Stopping server");
	wfi->force_all_disconnect = TRUE;
	server->instance->Close(server->instance);
	return TRUE;
}

wfServer* wfreerdp_server_new()
{
	WSADATA wsaData;
	wfServer* server;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		return NULL;

	server = (wfServer*) calloc(1, sizeof(wfServer));

	if (server)
	{
		server->port = 3389;
	}

	WTSRegisterWtsApiFunctionTable(FreeRDP_InitWtsApi());

	cbEvent = NULL;

	return server;
}

void wfreerdp_server_free(wfServer* server)
{
	free(server);

	WSACleanup();
}

BOOL wfreerdp_server_is_running(wfServer* server)
{
	DWORD tStatus;
	BOOL bRet;

	bRet = GetExitCodeThread(server->thread, &tStatus);
	if (bRet == 0)
	{
		WLog_ERR(TAG, "Error in call to GetExitCodeThread");
		return FALSE;
	}

	if (tStatus == STILL_ACTIVE)
		return TRUE;
	return FALSE;
}

UINT32 wfreerdp_server_num_peers()
{
	wfInfo* wfi;

	wfi = wf_info_get_instance();
	if (!wfi)
		return -1;
	return wfi->peerCount;
}

UINT32 wfreerdp_server_get_peer_hostname(int pId, wchar_t * dstStr)
{
	wfInfo* wfi;
	freerdp_peer* peer;

	wfi = wf_info_get_instance();
	if (!wfi)
		return 0;
	peer = wfi->peers[pId];

	if (peer)
	{
		UINT32 sLen;

		sLen = strnlen_s(peer->hostname, 50);
		swprintf(dstStr, 50, L"%hs", peer->hostname);
		return sLen;
	}
	else
	{
		WLog_WARN(TAG, "nonexistent peer id=%d", pId);
		return 0;
	}
}

BOOL wfreerdp_server_peer_is_local(int pId)
{
	wfInfo* wfi;
	freerdp_peer* peer;

	wfi = wf_info_get_instance();
	if (!wfi)
		return FALSE;
	peer = wfi->peers[pId];

	if (peer)
	{
		return peer->local;
	}
	else
	{
		return FALSE;
	}
}

BOOL wfreerdp_server_peer_is_connected(int pId)
{
	wfInfo* wfi;
	freerdp_peer* peer;

	wfi = wf_info_get_instance();
	if (!wfi)
		return FALSE;
	peer = wfi->peers[pId];


	if (peer)
	{
		return peer->connected;
	}
	else
	{
		return FALSE;
	}
}

BOOL wfreerdp_server_peer_is_activated(int pId)
{
	wfInfo* wfi;
	freerdp_peer* peer;

	wfi = wf_info_get_instance();
	if (!wfi)
		return FALSE;
	peer = wfi->peers[pId];

	if (peer)
	{
		return peer->activated;
	}
	else
	{
		return FALSE;
	}
}

BOOL wfreerdp_server_peer_is_authenticated(int pId)
{
	wfInfo* wfi;
	freerdp_peer* peer;

	wfi = wf_info_get_instance();
	if (!wfi)
		return FALSE;
	peer = wfi->peers[pId];

	if (peer)
	{
		return peer->authenticated;
	}
	else
	{
		return FALSE;
	}
}

void wfreerdp_server_register_callback_event(cbCallback cb)
{
	cbEvent = cb;
}

void wfreerdp_server_peer_callback_event(int pId, UINT32 eType)
{
	if (cbEvent)
		cbEvent(pId, eType);
}

