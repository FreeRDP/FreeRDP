/**
 * FreeRDP: A Remote Desktop Protocol Client
 * X11 Peer
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freerdp/utils/sleep.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/thread.h>
#include <freerdp/common/color.h>

extern char* xf_pcap_file;

#include "xf_encode.h"

#include "xf_peer.h"

void xf_peer_init(freerdp_peer* client)
{
	xfPeer* info;

	info = xnew(xfPeer);

	info->context = rfx_context_new();
	info->context->mode = RLGR3;
	info->context->width = client->settings->width;
	info->context->height = client->settings->height;
	rfx_context_set_pixel_format(info->context, RFX_PIXEL_FORMAT_RGB);

	info->s = stream_new(65536);

	client->param1 = info;
}

void xf_peer_uninit(freerdp_peer* client)
{
	xfPeer* info = (xfPeer*) client->param1;

	if (info)
	{
		stream_free(info->s);
		rfx_context_free(info->context);
		xfree(info);
	}
}

STREAM* xf_peer_stream_init(xfPeer* info)
{
	stream_clear(info->s);
	stream_set_pos(info->s, 0);
	return info->s;
}

void xf_peer_live_rfx(freerdp_peer* client)
{
	int x, y;
	STREAM* s;
	int width;
	int height;
	int wrects;
	int hrects;
	int nrects;
	uint8* data;
	xfInfo* xfi;
	xfPeer* xfp;
	XImage* image;
	RFX_RECT* rects;
	uint32 seconds;
	uint32 useconds;
	rdpUpdate* update;
	SURFACE_BITS_COMMAND* cmd;

	seconds = 1;
	useconds = 0;
	update = client->update;
	xfi = (xfInfo*) client->info;
	xfp = (xfPeer*) client->param1;
	cmd = &update->surface_bits_command;

	wrects = 16;
	hrects = 12;

	nrects = wrects * hrects;
	rects = xmalloc(sizeof(RFX_RECT) * nrects);

	for (y = 0; y < hrects; y++)
	{
		for (x = 0; x < wrects; x++)
		{
			rects[(y * wrects) + x].x = x * 64;
			rects[(y * wrects) + x].y = y * 64;
			rects[(y * wrects) + x].width = 64;
			rects[(y * wrects) + x].height = 64;
		}
	}

	width = wrects * 64;
	height = hrects * 64;

	data = (uint8*) xmalloc(width * height * 3);

	while (1)
	{
		if (seconds > 0)
			freerdp_sleep(seconds);

		if (useconds > 0)
			freerdp_usleep(useconds);

		s = xf_peer_stream_init(xfp);

		image = xf_snapshot(xfi, 0, 0, width, height);
		freerdp_image_convert((uint8*) image->data, data, width, height, 32, 24, xfi->clrconv);
		rfx_compose_message(xfp->context, s, rects, nrects, data, width, height, 64 * wrects * 3);

		cmd->destLeft = 0;
		cmd->destTop = 0;
		cmd->destRight = width;
		cmd->destBottom = height;
		cmd->bpp = 32;
		cmd->codecID = client->settings->rfx_codec_id;
		cmd->width = width;
		cmd->height = height;
		cmd->bitmapDataLength = stream_get_length(s);
		cmd->bitmapData = stream_get_head(s);
		update->SurfaceBits(update, cmd);
	}
}

void xf_peer_dump_rfx(freerdp_peer* client)
{
	STREAM* s;
	uint32 seconds;
	uint32 useconds;
	rdpUpdate* update;
	rdpPcap* pcap_rfx;
	pcap_record record;

	s = stream_new(512);
	update = client->update;
	client->update->pcap_rfx = pcap_open(xf_pcap_file, False);
	pcap_rfx = client->update->pcap_rfx;

	seconds = useconds = 0;

	while (pcap_has_next_record(pcap_rfx))
	{
		pcap_get_next_record_header(pcap_rfx, &record);

		s->data = xrealloc(s->data, record.length);
		record.data = s->data;
		s->size = record.length;

		pcap_get_next_record_content(pcap_rfx, &record);
		s->p = s->data + s->size;

		seconds = record.header.ts_sec - seconds;
		useconds = record.header.ts_usec - useconds;

		if (seconds > 0)
			freerdp_sleep(seconds);

		if (useconds > 0)
			freerdp_usleep(useconds);

		update->SurfaceCommand(update, s);
	}
}

boolean xf_peer_post_connect(freerdp_peer* client)
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
	xf_peer_init(client);

	/* Return False here would stop the execution of the peer mainloop. */
	return True;
}

boolean xf_peer_activate(freerdp_peer* client)
{
	xfPeer* xfp = (xfPeer*) client->param1;

	rfx_context_reset(xfp->context);
	xfp->activated = True;

	if (xf_pcap_file != NULL)
	{
		client->update->dump_rfx = True;
		xf_peer_dump_rfx(client);
	}
	else
	{
		xf_peer_live_rfx(client);
	}

	return True;
}

void xf_peer_synchronize_event(rdpInput* input, uint32 flags)
{
	printf("Client sent a synchronize event (flags:0x%X)\n", flags);
}

void xf_peer_keyboard_event(rdpInput* input, uint16 flags, uint16 code)
{
	freerdp_peer* client = (freerdp_peer*) input->param1;
	rdpUpdate* update = client->update;
	xfPeer* xfp = (xfPeer*) client->param1;

	printf("Client sent a keyboard event (flags:0x%X code:0x%X)\n", flags, code);

	if ((flags & 0x4000) && code == 0x1F) /* 's' key */
	{
		if (client->settings->width != 800)
		{
			client->settings->width = 800;
			client->settings->height = 600;
		}
		else
		{
			client->settings->width = 640;
			client->settings->height = 480;
		}
		update->DesktopResize(update);
		xfp->activated = False;
	}
}

void xf_peer_unicode_keyboard_event(rdpInput* input, uint16 code)
{
	printf("Client sent a unicode keyboard event (code:0x%X)\n", code);
}

void xf_peer_mouse_event(rdpInput* input, uint16 flags, uint16 x, uint16 y)
{
	printf("Client sent a mouse event (flags:0x%X pos:%d,%d)\n", flags, x, y);
}

void xf_peer_extended_mouse_event(rdpInput* input, uint16 flags, uint16 x, uint16 y)
{
	printf("Client sent an extended mouse event (flags:0x%X pos:%d,%d)\n", flags, x, y);
}

void* xf_peer_main_loop(void* arg)
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

	client->PostConnect = xf_peer_post_connect;
	client->Activate = xf_peer_activate;

	client->input->param1 = client;
	client->input->SynchronizeEvent = xf_peer_synchronize_event;
	client->input->KeyboardEvent = xf_peer_keyboard_event;
	client->input->UnicodeKeyboardEvent = xf_peer_unicode_keyboard_event;
	client->input->MouseEvent = xf_peer_mouse_event;
	client->input->ExtendedMouseEvent = xf_peer_extended_mouse_event;

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
	xf_peer_uninit(client);
	freerdp_peer_free(client);

	return NULL;
}

void xf_peer_accepted(freerdp_listener* instance, freerdp_peer* client)
{
	pthread_t th;

	client->info = instance->info;
	pthread_create(&th, 0, xf_peer_main_loop, client);
	pthread_detach(th);
}
