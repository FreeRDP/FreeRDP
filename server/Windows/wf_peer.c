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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/tchar.h>
#include <winpr/stream.h>
#include <winpr/windows.h>

#include <freerdp/listener.h>
#include <freerdp/codec/rfx.h>
#include <freerdp/build-config.h>

#include "wf_info.h"
#include "wf_input.h"
#include "wf_mirage.h"
#include "wf_update.h"
#include "wf_settings.h"
#include "wf_rdpsnd.h"

#include "wf_peer.h"
#include <freerdp/peer.h>

#define SERVER_KEY "Software\\"FREERDP_VENDOR_STRING"\\" \
	FREERDP_PRODUCT_STRING

BOOL wf_peer_context_new(freerdp_peer* client, wfPeerContext* context)
{
	if (!(context->info = wf_info_get_instance()))
		return FALSE;

	context->vcm = WTSOpenServerA((LPSTR) client->context);

	if (!context->vcm || context->vcm == INVALID_HANDLE_VALUE)
		return FALSE;

	if (!wf_info_peer_register(context->info, context))
	{
		WTSCloseServer(context->vcm);
		context->vcm = NULL;
		return FALSE;
	}

	return TRUE;
}

void wf_peer_context_free(freerdp_peer* client, wfPeerContext* context)
{
	wf_info_peer_unregister(context->info, context);

	if (context->rdpsnd)
	{
		wf_rdpsnd_lock();
		context->info->snd_stop = TRUE;
		rdpsnd_server_context_free(context->rdpsnd);
		wf_rdpsnd_unlock();
	}

	WTSCloseServer(context->vcm);
}

BOOL wf_peer_init(freerdp_peer* client)
{
	client->ContextSize = sizeof(wfPeerContext);
	client->ContextNew = (psPeerContextNew) wf_peer_context_new;
	client->ContextFree = (psPeerContextFree) wf_peer_context_free;

	return freerdp_peer_context_new(client);
}

BOOL wf_peer_post_connect(freerdp_peer* client)
{
	int i;
	wfInfo* wfi;
	rdpSettings* settings;
	wfPeerContext* context = (wfPeerContext*) client->context;

	wfi = context->info;
	settings = client->settings;

	if ((get_screen_info(wfi->screenID, NULL, &wfi->servscreen_width, &wfi->servscreen_height, &wfi->bitsPerPixel) == 0) ||
		(wfi->servscreen_width == 0) ||
		(wfi->servscreen_height == 0) ||
		(wfi->bitsPerPixel == 0) )
	{
		WLog_ERR(TAG, "postconnect: error getting screen info for screen %d", wfi->screenID);
		WLog_ERR(TAG, "\t%dx%dx%d", wfi->servscreen_height, wfi->servscreen_width, wfi->bitsPerPixel);
		return FALSE;
	}

	if ((settings->DesktopWidth != wfi->servscreen_width) || (settings->DesktopHeight != wfi->servscreen_height))
	{
		/*
		WLog_DBG(TAG, "Client requested resolution %dx%d, but will resize to %dx%d",
			settings->DesktopWidth, settings->DesktopHeight, wfi->servscreen_width, wfi->servscreen_height);
			*/

		settings->DesktopWidth = wfi->servscreen_width;
		settings->DesktopHeight = wfi->servscreen_height;
		settings->ColorDepth = wfi->bitsPerPixel;

		client->update->DesktopResize(client->update->context);
	}

	if (WTSVirtualChannelManagerIsChannelJoined(context->vcm, "rdpsnd"))
	{
		wf_peer_rdpsnd_init(context); /* Audio Output */
	}

	return TRUE;
}

BOOL wf_peer_activate(freerdp_peer* client)
{
	wfInfo* wfi;
	wfPeerContext* context = (wfPeerContext*) client->context;

	wfi = context->info;
	client->activated = TRUE;
	wf_update_peer_activate(wfi, context);

	wfreerdp_server_peer_callback_event(((rdpContext*) context)->peer->pId, WF_SRV_CALLBACK_EVENT_ACTIVATE);

	return TRUE;
}

BOOL wf_peer_logon(freerdp_peer* client, SEC_WINNT_AUTH_IDENTITY* identity, BOOL automatic)
{
	wfreerdp_server_peer_callback_event(((rdpContext*) client->context)->peer->pId, WF_SRV_CALLBACK_EVENT_AUTH);
	return TRUE;
}

void wf_peer_synchronize_event(rdpInput* input, UINT32 flags)
{

}

BOOL wf_peer_accepted(freerdp_listener* instance, freerdp_peer* client)
{
	HANDLE hThread;

	if (!(hThread = CreateThread(NULL, 0, wf_peer_main_loop, client, 0, NULL)))
		return FALSE;

	CloseHandle(hThread);
	return TRUE;
}

DWORD WINAPI wf_peer_socket_listener(LPVOID lpParam)
{
	int i, fds;
	int rcount;
	int max_fds;
	void* rfds[32];
	fd_set rfds_set;
	wfPeerContext* context;
	freerdp_peer* client = (freerdp_peer*) lpParam;

	ZeroMemory(rfds, sizeof(rfds));
	context = (wfPeerContext*) client->context;

	while (1)
	{
		rcount = 0;

		if (client->GetFileDescriptor(client, rfds, &rcount) != TRUE)
		{
			WLog_ERR(TAG, "Failed to get peer file descriptor");
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

		SetEvent(context->socketEvent);
		WaitForSingleObject(context->socketSemaphore, INFINITE);

		if (context->socketClose)
			break;
	}

	return 0;
}

BOOL wf_peer_read_settings(freerdp_peer* client)
{
	if (!wf_settings_read_string_ascii(HKEY_LOCAL_MACHINE, SERVER_KEY,
			_T("CertificateFile"), &(client->settings->CertificateFile)))
	{
		client->settings->CertificateFile = _strdup("server.crt");
		if (!client->settings->CertificateFile)
			return FALSE;
	}

	if (!wf_settings_read_string_ascii(HKEY_LOCAL_MACHINE, SERVER_KEY,
			_T("PrivateKeyFile"), &(client->settings->PrivateKeyFile)))
	{
		client->settings->PrivateKeyFile = _strdup("server.key");
		if (!client->settings->PrivateKeyFile)
			return FALSE;
	}

	return TRUE;
}

DWORD WINAPI wf_peer_main_loop(LPVOID lpParam)
{
	wfInfo* wfi;
	DWORD nCount;
	DWORD status;
	HANDLE handles[32];
	rdpSettings* settings;
	wfPeerContext* context;
	freerdp_peer* client = (freerdp_peer*) lpParam;

	if (!getenv("HOME"))
	{
		char home[MAX_PATH * 2] = "HOME=";
		strcat(home, getenv("HOMEDRIVE"));
		strcat(home, getenv("HOMEPATH"));
		_putenv(home);
	}

	if (!wf_peer_init(client))
		goto fail_peer_init;

	settings = client->settings;
	settings->RemoteFxCodec = TRUE;
	settings->ColorDepth = 32;
	settings->NSCodec = FALSE;
	settings->JpegCodec = FALSE;
	if (!wf_peer_read_settings(client))
		goto fail_peer_init;

	client->PostConnect = wf_peer_post_connect;
	client->Activate = wf_peer_activate;
	client->Logon = wf_peer_logon;

	client->input->SynchronizeEvent = wf_peer_synchronize_event;
	client->input->KeyboardEvent = wf_peer_keyboard_event;
	client->input->UnicodeKeyboardEvent = wf_peer_unicode_keyboard_event;
	client->input->MouseEvent = wf_peer_mouse_event;
	client->input->ExtendedMouseEvent = wf_peer_extended_mouse_event;

	if (!client->Initialize(client))
		goto fail_client_initialize;

	if (context->socketClose)
		goto fail_socked_closed;

	context = (wfPeerContext*) client->context;

	wfi = context->info;

	if (wfi->input_disabled)
	{
		WLog_INFO(TAG, "client input is disabled");
		client->input->KeyboardEvent = wf_peer_keyboard_event_dummy;
		client->input->UnicodeKeyboardEvent = wf_peer_unicode_keyboard_event_dummy;
		client->input->MouseEvent = wf_peer_mouse_event_dummy;
		client->input->ExtendedMouseEvent = wf_peer_extended_mouse_event_dummy;
	}

	if (!(context->socketEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
		goto fail_socket_event;

	if (!(context->socketSemaphore = CreateSemaphore(NULL, 0, 1, NULL)))
		goto fail_socket_semaphore;

	if (!(context->socketThread = CreateThread(NULL, 0, wf_peer_socket_listener, client, 0, NULL)))
		goto fail_socket_thread;

	WLog_INFO(TAG, "We've got a client %s", client->local ? "(local)" : client->hostname);
	nCount = 0;
	handles[nCount++] = context->updateEvent;
	handles[nCount++] = context->socketEvent;

	while (1)
	{
		status = WaitForMultipleObjects(nCount, handles, FALSE, INFINITE);

		if ((status == WAIT_FAILED) || (status == WAIT_TIMEOUT))
		{
			WLog_ERR(TAG, "WaitForMultipleObjects failed");
			break;
		}

		if (WaitForSingleObject(context->updateEvent, 0) == 0)
		{
			if (client->activated)
				wf_update_peer_send(wfi, context);

			ResetEvent(context->updateEvent);
			ReleaseSemaphore(wfi->updateSemaphore, 1, NULL);
		}

		if (WaitForSingleObject(context->socketEvent, 0) == 0)
		{
			if (client->CheckFileDescriptor(client) != TRUE)
			{
				WLog_ERR(TAG, "Failed to check peer file descriptor");
				context->socketClose = TRUE;
			}

			ResetEvent(context->socketEvent);
			ReleaseSemaphore(context->socketSemaphore, 1, NULL);

			if (context->socketClose)
				break;
		}

		//force disconnect
		if (wfi->force_all_disconnect == TRUE)
		{
			WLog_INFO(TAG, "Forcing Disconnect -> ");
			break;
		}

		/* FIXME: we should wait on this, instead of calling it every time */
		if (WTSVirtualChannelManagerCheckFileDescriptor(context->vcm) != TRUE)
			break;
	}

	WLog_INFO(TAG, "Client %s disconnected.", client->local ? "(local)" : client->hostname);

	if (WaitForSingleObject(context->updateEvent, 0) == 0)
	{
		ResetEvent(context->updateEvent);
		ReleaseSemaphore(wfi->updateSemaphore, 1, NULL);
	}

	wf_update_peer_deactivate(wfi, context);

	client->Disconnect(client);

fail_socket_thread:
	CloseHandle(context->socketSemaphore);
	context->socketSemaphore = NULL;
fail_socket_semaphore:
	CloseHandle(context->socketEvent);
	context->socketEvent = NULL;
fail_socket_event:
fail_socked_closed:
fail_client_initialize:
	freerdp_peer_context_free(client);
fail_peer_init:
	freerdp_peer_free(client);

	return 0;
}
