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
#include <freerdp/utils/tcp.h>
#include <freerdp\listener.h>

#include "wf_peer.h"
#include "wf_settings.h"
#include "wf_info.h"

#include "wf_interface.h"

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
	wfi->screenID = id;

	return;
}

DWORD WINAPI wf_server_main_loop(LPVOID lpParam)
{
	int i, fds;
	int rcount;
	int max_fds;
	void* rfds[32];
	fd_set rfds_set;
	freerdp_listener* instance;
	wfInfo* wfi;

	wfi = wf_info_get_instance();
	wfi->force_all_disconnect = FALSE;

	ZeroMemory(rfds, sizeof(rfds));
	instance = (freerdp_listener*) lpParam;

	while(wfi->force_all_disconnect == FALSE)
	{
		rcount = 0;

		if (instance->GetFileDescriptor(instance, rfds, &rcount) != TRUE)
		{
			printf("Failed to get FreeRDP file descriptor\n");
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
			printf("Failed to check FreeRDP file descriptor\n");
			break;
		}
	}

	printf("wf_server_main_loop terminating\n");

	instance->Close(instance);

	return 0;
}

BOOL wfreerdp_server_start(wfServer* server)
{
	freerdp_listener* instance;

	server->instance = freerdp_listener_new();
	server->instance->PeerAccepted = wf_peer_accepted;
	instance = server->instance;

	wf_settings_read_dword(HKEY_LOCAL_MACHINE, _T("Software\\FreeRDP\\Server"), _T("DefaultPort"), &server->port);

	if (instance->Open(instance, NULL, (UINT16) server->port))
	{
		server->thread = CreateThread(NULL, 0, wf_server_main_loop, (void*) instance, 0, NULL);
		return TRUE;
	}

	return FALSE;
}

BOOL wfreerdp_server_stop(wfServer* server)
{
	wfInfo* wfi;

	wfi = wf_info_get_instance();

	printf("Stopping server\n");
	wfi->force_all_disconnect = TRUE;
	server->instance->Close(server->instance);
	return TRUE;
}

wfServer* wfreerdp_server_new()
{
	wfServer* server;

	server = (wfServer*) malloc(sizeof(wfServer));
	ZeroMemory(server, sizeof(wfServer));

	freerdp_wsa_startup();

	if (server)
	{
		server->port = 3389;
	}

	cbEvent = NULL;

	return server;
}

void wfreerdp_server_free(wfServer* server)
{
	if (server)
	{
		free(server);
	}

	freerdp_wsa_cleanup();
}


FREERDP_API BOOL wfreerdp_server_is_running(wfServer* server)
{
	DWORD tStatus;
	BOOL bRet;

	bRet = GetExitCodeThread(server->thread, &tStatus);
	if (bRet == 0)
	{
		printf("Error in call to GetExitCodeThread\n");
		return FALSE;
	}

	if (tStatus == STILL_ACTIVE)
		return TRUE;
	return FALSE;
}

FREERDP_API UINT32 wfreerdp_server_num_peers()
{
	wfInfo* wfi;

	wfi = wf_info_get_instance();
	return wfi->peerCount;
}

FREERDP_API UINT32 wfreerdp_server_get_peer_hostname(int pId, wchar_t * dstStr)
{
	wfInfo* wfi;
	freerdp_peer* peer;

	wfi = wf_info_get_instance();
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
		printf("nonexistent peer id=%d\n", pId);
		return 0;
	}

}

FREERDP_API BOOL wfreerdp_server_peer_is_local(int pId)
{
	wfInfo* wfi;
	freerdp_peer* peer;

	wfi = wf_info_get_instance();
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
FREERDP_API BOOL wfreerdp_server_peer_is_connected(int pId)
{
	wfInfo* wfi;
	freerdp_peer* peer;

	wfi = wf_info_get_instance();
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

FREERDP_API BOOL wfreerdp_server_peer_is_activated(int pId)
{
	wfInfo* wfi;
	freerdp_peer* peer;

	wfi = wf_info_get_instance();
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

FREERDP_API BOOL wfreerdp_server_peer_is_authenticated(int pId)
{
	wfInfo* wfi;
	freerdp_peer* peer;

	wfi = wf_info_get_instance();
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

FREERDP_API void wfreerdp_server_register_callback_event(cbCallback cb)
{
	cbEvent = cb;
}

void wfreerdp_server_peer_callback_event(int pId, UINT32 eType)
{
	if (cbEvent)
		cbEvent(pId, eType);
}

