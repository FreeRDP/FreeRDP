/**
 * FreeRDP: A Remote Desktop Protocol Client
 * FreeRDP Test Server
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <freerdp/constants.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/thread.h>
#include <freerdp/rfx/rfx.h>
#include <freerdp/listener.h>

/* HL1, LH1, HH1, HL2, LH2, HH2, HL3, LH3, HH3, LL3 */
static const unsigned int test_quantization_values[] =
{
	6, 6, 6, 6, 7, 7, 8, 8, 8, 9
};

static const uint8 rgb_scanline_data[] =
{
	0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00,
	0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00,
	0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00,
	0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00,
	0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00,
	0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00,
	0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00,
	0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00,
	0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00,
	0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00,
	0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00,
	0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF,
	0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF,
	0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF,
	0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF,
	0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF,

	0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00,
	0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00,
	0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00,
	0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00,
	0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00,
	0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00,
	0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00,
	0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00,
	0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00,
	0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00,
	0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00,
	0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF,
	0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF,
	0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF,
	0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF,
	0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF
};

void test_peer_init_desktop(freerdp_peer* client)
{
	rdpUpdate* update = client->update;
	SURFACE_BITS_COMMAND* cmd = &update->surface_bits_command;
	RFX_CONTEXT* context;
	uint8* rgb_data;
	STREAM* s;
	int i;
	RFX_RECT rect = {0, 0, 100, 80};

	rgb_data = (uint8*) xmalloc(100 * 80 * 3);
	for (i = 0; i < 80; i++)
		memcpy(rgb_data + i * 100 * 3, rgb_scanline_data, 100 * 3);

	s = stream_new(65536);
	stream_clear(s);

	context = rfx_context_new();
	context->mode = RLGR3;
	context->width = 800;
	context->height = 600;
	rfx_context_set_pixel_format(context, RFX_PIXEL_FORMAT_RGB);

	/* In Video mode, the RemoteFX header should only be sent once */
	rfx_compose_message_header(context, s);

	rfx_compose_message_data(context, s,
		&rect, 1, rgb_data, 100, 80, 100 * 3);
	cmd->destLeft = 0;
	cmd->destTop = 0;
	cmd->destRight = 100;
	cmd->destBottom = 80;
	cmd->bpp = 32;
	cmd->codecID = CODEC_ID_REMOTEFX;
	cmd->width = 100;
	cmd->height = 80;
	cmd->bitmapDataLength = stream_get_length(s);
	cmd->bitmapData = stream_get_head(s);
	update->SurfaceBits(update, cmd);

	stream_free(s);
	rfx_context_free(context);
	xfree(rgb_data);
}

boolean test_peer_post_connect(freerdp_peer* client)
{
	/**
	 * This callback is called when the entire connection sequence is done, i.e. we've received the
	 * Font List PDU from the client and sent out the Font Map PDU.
	 * The server may start sending graphics output and receiving keyboard/mouse input after this
	 * callback returns.
	 */
	printf("Client %s is activated", client->settings->hostname);
	if (client->settings->autologon)
	{
		printf(" and wants to login automatically as %s\\%s",
			client->settings->domain ? client->settings->domain : "",
			client->settings->username);

		/* A real server may perform OS login here if NLA is not executed previously. */
	}
	printf("\n");

	printf("Client requested desktop: %dx%dx%d\n",
		client->settings->width, client->settings->height, client->settings->color_depth);

	/* A real server should tag the peer as activated here and start sending updates in mainloop. */
	test_peer_init_desktop(client);

	/* Return False here would stop the execution of the peer mainloop. */
	return True;
}

void test_peer_synchronize_event(rdpInput* input, uint32 flags)
{
	printf("Client sent a synchronize event (flags:0x%X)\n", flags);
}

void test_peer_keyboard_event(rdpInput* input, uint16 flags, uint16 code)
{
	printf("Client sent a keyboard event (flags:0x%X code:0x%X)\n", flags, code);
}

void test_peer_unicode_keyboard_event(rdpInput* input, uint16 code)
{
	printf("Client sent a unicode keyboard event (code:0x%X)\n", code);
}

void test_peer_mouse_event(rdpInput* input, uint16 flags, uint16 x, uint16 y)
{
	printf("Client sent a mouse event (flags:0x%X pos:%d,%d)\n", flags, x, y);
}

void test_peer_extended_mouse_event(rdpInput* input, uint16 flags, uint16 x, uint16 y)
{
	printf("Client sent an extended mouse event (flags:0x%X pos:%d,%d)\n", flags, x, y);
}

static void* test_peer_mainloop(void* arg)
{
	freerdp_peer* client = (freerdp_peer*)arg;
	int i;
	int fds;
	int max_fds;
	int rcount;
	void* rfds[32];
	fd_set rfds_set;

	memset(rfds, 0, sizeof(rfds));

	printf("We've got a client %s\n", client->settings->hostname);

	/* Initialize the real server settings here */
	client->settings->cert_file = xstrdup("server.crt");
	client->settings->privatekey_file = xstrdup("server.key");
	client->settings->nla_security = False;

	client->PostConnect = test_peer_post_connect;

	client->input->param1 = client;
	client->input->SynchronizeEvent = test_peer_synchronize_event;
	client->input->KeyboardEvent = test_peer_keyboard_event;
	client->input->UnicodeKeyboardEvent = test_peer_unicode_keyboard_event;
	client->input->MouseEvent = test_peer_mouse_event;
	client->input->ExtendedMouseEvent = test_peer_extended_mouse_event;

	client->Initialize(client);

	while (1)
	{
		rcount = 0;

		if (client->GetFileDescriptor(client, rfds, &rcount) != True)
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

		if (client->CheckFileDescriptor(client) != True)
			break;
	}

	printf("Client %s disconnected.\n", client->settings->hostname);

	client->Disconnect(client);
	freerdp_peer_free(client);

	return NULL;
}

static void test_peer_accepted(freerdp_listener* instance, freerdp_peer* client)
{
	pthread_t th;

	pthread_create(&th, 0, test_peer_mainloop, client);
	pthread_detach(th);
}

static void test_server_mainloop(freerdp_listener* instance)
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

		if (instance->GetFileDescriptor(instance, rfds, &rcount) != True)
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

		if (instance->CheckFileDescriptor(instance) != True)
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

	instance = freerdp_listener_new();

	instance->PeerAccepted = test_peer_accepted;

	/* Open the server socket and start listening. */
	if (instance->Open(instance, (argc > 1 ? argv[1] : NULL), 3389))
	{
		/* Entering the server main loop. In a real server the listener can be run in its own thread. */
		test_server_mainloop(instance);
	}

	freerdp_listener_free(instance);

	return 0;
}

