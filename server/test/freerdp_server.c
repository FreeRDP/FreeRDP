/**
 * FreeRDP: A Remote Desktop Protocol Client
 * FreeRDP Test Server
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2011 Vic Lee
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
#include <signal.h>
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

struct test_peer_info
{
	RFX_CONTEXT* context;
	STREAM* s;
	uint8* icon_data;
	uint8* bg_data;
	int icon_width;
	int icon_height;
	int icon_x;
	int icon_y;
};
typedef struct test_peer_info testPeerInfo;

static void test_peer_init(freerdp_peer* client)
{
	testPeerInfo* info;

	info = xnew(testPeerInfo);

	info->context = rfx_context_new();
	info->context->mode = RLGR3;
	info->context->width = client->settings->width;
	info->context->height = client->settings->height;
	rfx_context_set_pixel_format(info->context, RFX_PIXEL_FORMAT_RGB);

	info->s = stream_new(65536);

	info->icon_x = -1;
	info->icon_y = -1;

	client->param1 = info;
}

static void test_peer_uninit(freerdp_peer* client)
{
	testPeerInfo* info = (testPeerInfo*)client->param1;

	if (info)
	{
		stream_free(info->s);
		xfree(info->icon_data);
		xfree(info->bg_data);
		rfx_context_free(info->context);
		xfree(info);
	}
}

static STREAM* test_peer_stream_init(testPeerInfo* info)
{
	stream_clear(info->s);
	stream_set_pos(info->s, 0);
	return info->s;
}

static void test_peer_draw_background(freerdp_peer* client)
{
	testPeerInfo* info = (testPeerInfo*)client->param1;
	rdpUpdate* update = client->update;
	SURFACE_BITS_COMMAND* cmd = &update->surface_bits_command;
	STREAM* s;
	RFX_RECT rect;
	uint8* rgb_data;
	int size;

	if (!client->settings->rfx_codec)
		return;

	s = test_peer_stream_init(info);

	rect.x = 0;
	rect.y = 0;
	rect.width = client->settings->width;
	rect.height = client->settings->height;

	size = rect.width * rect.height * 3;
	rgb_data = xmalloc(size);
	memset(rgb_data, 0xA0, size);

	rfx_compose_message(info->context, s,
		&rect, 1, rgb_data, rect.width, rect.height, rect.width * 3);

	cmd->destLeft = 0;
	cmd->destTop = 0;
	cmd->destRight = rect.width;
	cmd->destBottom = rect.height;
	cmd->bpp = 32;
	cmd->codecID = client->settings->rfx_codec_id;
	cmd->width = rect.width;
	cmd->height = rect.height;
	cmd->bitmapDataLength = stream_get_length(s);
	cmd->bitmapData = stream_get_head(s);
	update->SurfaceBits(update, cmd);

	xfree(rgb_data);
}

static void test_peer_load_icon(freerdp_peer* client)
{
	testPeerInfo* info = (testPeerInfo*)client->param1;
	FILE* fp;
	int i;
	char line[50];
	uint8* rgb_data;
	int c;

	if (!client->settings->rfx_codec)
		return;

	if ((fp = fopen("test_icon.ppm", "r")) == NULL)
		return;

	/* P3 */
	fgets(line, sizeof(line), fp);
	/* Creater comment */
	fgets(line, sizeof(line), fp);
	/* width height */
	fgets(line, sizeof(line), fp);
	sscanf(line, "%d %d", &info->icon_width, &info->icon_height);
	/* Max */
	fgets(line, sizeof(line), fp);

	rgb_data = xmalloc(info->icon_width * info->icon_height * 3);

	for (i = 0; i < info->icon_width * info->icon_height * 3; i++)
	{
		if (fgets(line, sizeof(line), fp))
		{
			sscanf(line, "%d", &c);
			rgb_data[i] = (uint8)c;
		}
	}

	info->icon_data = rgb_data;

	/* background with same size, which will be used to erase the icon from old position */
	info->bg_data = xmalloc(info->icon_width * info->icon_height * 3);
	memset(info->bg_data, 0xA0, info->icon_width * info->icon_height * 3);
}

static void test_peer_draw_icon(freerdp_peer* client, int x, int y)
{
	testPeerInfo* info = (testPeerInfo*)client->param1;
	rdpUpdate* update = client->update;
	SURFACE_BITS_COMMAND* cmd = &update->surface_bits_command;
	RFX_RECT rect;
	STREAM* s;

	if (!client->settings->rfx_codec || !info)
		return;
	if (info->icon_width < 1)
		return;

	rect.x = 0;
	rect.y = 0;
	rect.width = info->icon_width;
	rect.height = info->icon_height;

	if (info->icon_x >= 0)
	{
		s = test_peer_stream_init(info);
		rfx_compose_message(info->context, s,
			&rect, 1, info->bg_data, rect.width, rect.height, rect.width * 3);

		cmd->destLeft = info->icon_x;
		cmd->destTop = info->icon_y;
		cmd->destRight = info->icon_x + info->icon_width;
		cmd->destBottom = info->icon_y + info->icon_height;
		cmd->bpp = 32;
		cmd->codecID = client->settings->rfx_codec_id;
		cmd->width = info->icon_width;
		cmd->height = info->icon_height;
		cmd->bitmapDataLength = stream_get_length(s);
		cmd->bitmapData = stream_get_head(s);
		update->SurfaceBits(update, cmd);
	}

	s = test_peer_stream_init(info);
	rfx_compose_message(info->context, s,
		&rect, 1, info->icon_data, rect.width, rect.height, rect.width * 3);

	cmd->destLeft = x;
	cmd->destTop = y;
	cmd->destRight = x + info->icon_width;
	cmd->destBottom = y + info->icon_height;
	cmd->bpp = 32;
	cmd->codecID = client->settings->rfx_codec_id;
	cmd->width = info->icon_width;
	cmd->height = info->icon_height;
	cmd->bitmapDataLength = stream_get_length(s);
	cmd->bitmapData = stream_get_head(s);
	update->SurfaceBits(update, cmd);

	info->icon_x = x;
	info->icon_y = y;
}

void test_peer_dump_rfx(freerdp_peer* client)
{
	STREAM* s;
	rdpUpdate* update;
	rdpPcap* pcap_rfx;
	pcap_record record;

	s = stream_new(512);
	update = client->update;
	client->update->pcap_rfx = pcap_open("rfx_test.pcap", False);
	pcap_rfx = client->update->pcap_rfx;

	while (pcap_has_next_record(pcap_rfx))
	{
		pcap_get_next_record_header(pcap_rfx, &record);

		s->data = xrealloc(s->data, record.length);
		record.data = s->data;
		s->size = record.length;

		pcap_get_next_record_content(pcap_rfx, &record);
		s->p = s->data + s->size;

		update->SurfaceCommand(update, s);
	}
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
	test_peer_init(client);
	test_peer_draw_background(client);
	test_peer_load_icon(client);

	client->update->dump_rfx = True;

	if (client->update->dump_rfx)
	{
		test_peer_dump_rfx(client);
	}

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

	test_peer_draw_icon(input->param1, x + 10, y);
}

void test_peer_extended_mouse_event(rdpInput* input, uint16 flags, uint16 x, uint16 y)
{
	printf("Client sent an extended mouse event (flags:0x%X pos:%d,%d)\n", flags, x, y);
}

static void* test_peer_mainloop(void* arg)
{
	int i;
	int fds;
	int max_fds;
	int rcount;
	void* rfds[32];
	fd_set rfds_set;
	freerdp_peer* client = (freerdp_peer*) arg;

	memset(rfds, 0, sizeof(rfds));

	printf("We've got a client %s\n", client->settings->hostname);

	/* Initialize the real server settings here */
	client->settings->cert_file = xstrdup("server.crt");
	client->settings->privatekey_file = xstrdup("server.key");
	client->settings->nla_security = False;
	client->settings->rfx_codec = True;

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
	test_peer_uninit(client);
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

	/* Ignore SIGPIPE, otherwise an SSL_write failure could crash your server */
	signal(SIGPIPE, SIG_IGN);

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

