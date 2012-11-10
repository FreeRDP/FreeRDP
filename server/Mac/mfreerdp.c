/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Mac OS X Server
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>

#include <winpr/crt.h>

#include <freerdp/constants.h>
#include <freerdp/utils/sleep.h>
#include <freerdp/utils/memory.h>
#include <freerdp/server/rdpsnd.h>

#include "mf_audin.h"
#include "mf_rdpsnd.h"

#include "mfreerdp.h"

void mf_peer_context_new(freerdp_peer* client, mfPeerContext* context)
{
	context->rfx_context = rfx_context_new();
	context->rfx_context->mode = RLGR3;
	context->rfx_context->width = client->settings->DesktopWidth;
	context->rfx_context->height = client->settings->DesktopHeight;
	rfx_context_set_pixel_format(context->rfx_context, RDP_PIXEL_FORMAT_R8G8B8);

	context->nsc_context = nsc_context_new();
	nsc_context_set_pixel_format(context->nsc_context, RDP_PIXEL_FORMAT_R8G8B8);

	context->s = stream_new(0xFFFF);

	context->vcm = WTSCreateVirtualChannelManager(client);
}

void mf_peer_context_free(freerdp_peer* client, mfPeerContext* context)
{
	if (context)
	{
		stream_free(context->s);

		rfx_context_free(context->rfx_context);
		nsc_context_free(context->nsc_context);

		if (context->audin)
			audin_server_context_free(context->audin);

		if (context->rdpsnd)
			rdpsnd_server_context_free(context->rdpsnd);

		WTSDestroyVirtualChannelManager(context->vcm);
	}
}

static void mf_peer_init(freerdp_peer* client)
{
	client->context_size = sizeof(mfPeerContext);
	client->ContextNew = (psPeerContextNew) mf_peer_context_new;
	client->ContextFree = (psPeerContextFree) mf_peer_context_free;
	freerdp_peer_context_new(client);
}

BOOL mf_peer_post_connect(freerdp_peer* client)
{
	int i;
	mfPeerContext* context = (mfPeerContext*) client->context;

	printf("Client %s is activated\n", client->hostname);

	if (client->settings->AutoLogonEnabled)
	{
		printf(" and wants to login automatically as %s\\%s",
			client->settings->Domain ? client->settings->Domain : "",
			client->settings->Username);

		/* A real server may perform OS login here if NLA is not executed previously. */
	}
	printf("\n");

	printf("Client requested desktop: %dx%dx%d\n",
		client->settings->DesktopWidth, client->settings->DesktopHeight, client->settings->ColorDepth);

	/* Iterate all channel names requested by the client and activate those supported by the server */

	for (i = 0; i < client->settings->ChannelCount; i++)
	{
		if (client->settings->ChannelDefArray[i].joined)
		{
			if (strncmp(client->settings->ChannelDefArray[i].Name, "rdpsnd", 6) == 0)
			{
				mf_peer_rdpsnd_init(context); /* Audio Output */
			}
		}
	}

	/* Dynamic Virtual Channels */

	mf_peer_audin_init(context); /* Audio Input */

	return TRUE;
}

BOOL mf_peer_activate(freerdp_peer* client)
{
	mfPeerContext* context = (mfPeerContext*) client->context;

	rfx_context_reset(context->rfx_context);
	context->activated = TRUE;

	return TRUE;
}

void mf_peer_synchronize_event(rdpInput* input, UINT32 flags)
{
	printf("Client sent a synchronize event (flags:0x%08X)\n", flags);
}

void mf_peer_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code)
{
	printf("Client sent a keyboard event (flags:0x%04X code:0x%04X)\n", flags, code);
}

void mf_peer_unicode_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code)
{
	printf("Client sent a unicode keyboard event (flags:0x%04X code:0x%04X)\n", flags, code);
}

void mf_peer_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	//printf("Client sent a mouse event (flags:0x%04X pos: %d,%d)\n", flags, x, y);
}

void mf_peer_extended_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	//printf("Client sent an extended mouse event (flags:0x%04X pos: %d,%d)\n", flags, x, y);
}

static void mf_peer_refresh_rect(rdpContext* context, BYTE count, RECTANGLE_16* areas)
{
	BYTE i;

	printf("Client requested to refresh:\n");

	for (i = 0; i < count; i++)
	{
		printf("  (%d, %d) (%d, %d)\n", areas[i].left, areas[i].top, areas[i].right, areas[i].bottom);
	}
}

static void mf_peer_suppress_output(rdpContext* context, BYTE allow, RECTANGLE_16* area)
{
	if (allow > 0)
	{
		printf("Client restore output (%d, %d) (%d, %d).\n", area->left, area->top, area->right, area->bottom);
	}
	else
	{
		printf("Client minimized and suppress output.\n");
	}
}

static void* mf_peer_main_loop(void* arg)
{
	int i;
	int fds;
	int max_fds;
	int rcount;
	void* rfds[32];
	fd_set rfds_set;
	mfPeerContext* context;
	freerdp_peer* client = (freerdp_peer*) arg;

	memset(rfds, 0, sizeof(rfds));

	mf_peer_init(client);

	/* Initialize the real server settings here */
	client->settings->CertificateFile = _strdup("server.crt");
	client->settings->PrivateKeyFile = _strdup("server.key");
	client->settings->NlaSecurity = FALSE;
	client->settings->RemoteFxCodec = TRUE;
	client->settings->SuppressOutput = TRUE;
	client->settings->RefreshRect = TRUE;

	client->PostConnect = mf_peer_post_connect;
	client->Activate = mf_peer_activate;

	client->input->SynchronizeEvent = mf_peer_synchronize_event;
	client->input->KeyboardEvent = mf_peer_keyboard_event;
	client->input->UnicodeKeyboardEvent = mf_peer_unicode_keyboard_event;
	client->input->MouseEvent = mf_peer_mouse_event;
	client->input->ExtendedMouseEvent = mf_peer_extended_mouse_event;

	client->update->RefreshRect = mf_peer_refresh_rect;
	client->update->SuppressOutput = mf_peer_suppress_output;

	client->Initialize(client);
	context = (mfPeerContext*) client->context;

	printf("We've got a client %s\n", client->local ? "(local)" : client->hostname);

	while (1)
	{
		rcount = 0;

		if (client->GetFileDescriptor(client, rfds, &rcount) != TRUE)
		{
			printf("Failed to get FreeRDP file descriptor\n");
			break;
		}
		WTSVirtualChannelManagerGetFileDescriptor(context->vcm, rfds, &rcount);

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

		if (select(max_fds + 1, &rfds_set, NULL, NULL, NULL) == -1)
		{
			/* these are not really errors */
			if (!((errno == EAGAIN) ||
				(errno == EWOULDBLOCK) ||
				(errno == EINPROGRESS) ||
				(errno == EINTR))) /* signal occurred */
			{
				printf("select failed\n");
				break;
			}
		}

		if (client->CheckFileDescriptor(client) != TRUE)
			break;
		if (WTSVirtualChannelManagerCheckFileDescriptor(context->vcm) != TRUE)
			break;
	}

	printf("Client %s disconnected.\n", client->local ? "(local)" : client->hostname);

	client->Disconnect(client);
	freerdp_peer_context_free(client);
	freerdp_peer_free(client);

	return NULL;
}

static void mf_peer_accepted(freerdp_listener* instance, freerdp_peer* client)
{
	pthread_t th;

	pthread_create(&th, 0, mf_peer_main_loop, client);
	pthread_detach(th);
}

static void mf_server_main_loop(freerdp_listener* instance)
{
	int i;
	int fds;
	int max_fds;
	int rcount;
	void* rfds[32];
	fd_set rfds_set;

	memset(rfds, 0, sizeof(rfds));

	while (1)
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

		if (select(max_fds + 1, &rfds_set, NULL, NULL, NULL) == -1)
		{
			/* these are not really errors */
			if (!((errno == EAGAIN) ||
				(errno == EWOULDBLOCK) ||
				(errno == EINPROGRESS) ||
				(errno == EINTR))) /* signal occurred */
			{
				printf("select failed\n");
				break;
			}
		}

		if (instance->CheckFileDescriptor(instance) != TRUE)
		{
			printf("Failed to check FreeRDP file descriptor\n");
			break;
		}
	}

	instance->Close(instance);
}

int main(int argc, char* argv[])
{
	freerdp_listener* instance;

	signal(SIGPIPE, SIG_IGN);

	instance = freerdp_listener_new();

	instance->PeerAccepted = mf_peer_accepted;

	if (instance->Open(instance, NULL, 3389))
	{
		mf_server_main_loop(instance);
	}

	freerdp_listener_free(instance);

	return 0;
}

