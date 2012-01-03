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
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <sys/select.h>
#include <freerdp/utils/sleep.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/thread.h>
#include <freerdp/codec/color.h>

extern char* xf_pcap_file;
extern boolean xf_pcap_dump_realtime;

#include "xf_encode.h"

#include "xf_peer.h"

#ifdef WITH_XDAMAGE

void xf_xdamage_init(xfInfo* xfi)
{
	int damage_event;
	int damage_error;
	int major, minor;
	XGCValues values;

	if (XDamageQueryExtension(xfi->display, &damage_event, &damage_error) == 0)
	{
		printf("XDamageQueryExtension failed\n");
		return;
	}

	XDamageQueryVersion(xfi->display, &major, &minor);

	if (XDamageQueryVersion(xfi->display, &major, &minor) == 0)
	{
		printf("XDamageQueryVersion failed\n");
		return;
	}
	else if (major < 1)
	{
		printf("XDamageQueryVersion failed: major:%d minor:%d\n", major, minor);
		return;
	}

	xfi->xdamage_notify_event = damage_event + XDamageNotify;
	xfi->xdamage = XDamageCreate(xfi->display, DefaultRootWindow(xfi->display), XDamageReportDeltaRectangles);

	if (xfi->xdamage == None)
	{
		printf("XDamageCreate failed\n");
		return;
	}

#ifdef WITH_XFIXES
	xfi->xdamage_region = XFixesCreateRegion(xfi->display, NULL, 0);

	if (xfi->xdamage_region == None)
	{
		printf("XFixesCreateRegion failed\n");
		XDamageDestroy(xfi->display, xfi->xdamage);
		xfi->xdamage = None;
		return;
	}
#endif

	values.subwindow_mode = IncludeInferiors;
	xfi->xdamage_gc = XCreateGC(xfi->display, DefaultRootWindow(xfi->display), GCSubwindowMode, &values);
}

#endif

xfInfo* xf_info_init()
{
	int i;
	xfInfo* xfi;
	int pf_count;
	int vi_count;
	XVisualInfo* vi;
	XVisualInfo* vis;
	XVisualInfo template;
	XPixmapFormatValues* pf;
	XPixmapFormatValues* pfs;

	xfi = xnew(xfInfo);

	xfi->display = XOpenDisplay(NULL);

	if (xfi->display == NULL)
	{
		printf("failed to open display: %s\n", XDisplayName(NULL));
		exit(1);
	}

	xfi->number = DefaultScreen(xfi->display);
	xfi->screen = ScreenOfDisplay(xfi->display, xfi->number);
	xfi->depth = DefaultDepthOfScreen(xfi->screen);
	xfi->width = WidthOfScreen(xfi->screen);
	xfi->height = HeightOfScreen(xfi->screen);

	pfs = XListPixmapFormats(xfi->display, &pf_count);

	if (pfs == NULL)
	{
		printf("XListPixmapFormats failed\n");
		exit(1);
	}

	for (i = 0; i < pf_count; i++)
	{
		pf = pfs + i;

		if (pf->depth == xfi->depth)
		{
			xfi->bpp = pf->bits_per_pixel;
			xfi->scanline_pad = pf->scanline_pad;
			break;
		}
	}
	XFree(pfs);

	memset(&template, 0, sizeof(template));
	template.class = TrueColor;
	template.screen = xfi->number;

	vis = XGetVisualInfo(xfi->display, VisualClassMask | VisualScreenMask, &template, &vi_count);

	if (vis == NULL)
	{
		printf("XGetVisualInfo failed\n");
		exit(1);
	}

	for (i = 0; i < vi_count; i++)
	{
		vi = vis + i;

		if (vi->depth == xfi->depth)
		{
			xfi->visual = vi->visual;
			break;
		}
	}
	XFree(vis);

	xfi->clrconv = (HCLRCONV) xnew(HCLRCONV);
	xfi->clrconv->invert = 1;
	xfi->clrconv->alpha = 1;

	XSelectInput(xfi->display, DefaultRootWindow(xfi->display), SubstructureNotifyMask);

#ifdef WITH_XDAMAGE
	xf_xdamage_init(xfi);
#endif 

	return xfi;
}

void xf_peer_context_new(freerdp_peer* client, xfPeerContext* context)
{
	context->info = xf_info_init();
	context->rfx_context = rfx_context_new();
	context->rfx_context->mode = RLGR3;
	context->rfx_context->width = context->info->width;
	context->rfx_context->height = context->info->height;
	rfx_context_set_pixel_format(context->rfx_context, RFX_PIXEL_FORMAT_RGB);

	context->s = stream_new(65536);
}

void xf_peer_context_free(freerdp_peer* client, xfPeerContext* context)
{
	if (context)
	{
		stream_free(context->s);
		rfx_context_free(context->rfx_context);
		xfree(context);
	}
}

void xf_peer_init(freerdp_peer* client)
{
	client->context_size = sizeof(xfPeerContext);
	client->ContextNew = (psPeerContextNew) xf_peer_context_new;
	client->ContextFree = (psPeerContextFree) xf_peer_context_free;
	freerdp_peer_context_new(client);
}

STREAM* xf_peer_stream_init(xfPeerContext* context)
{
	stream_clear(context->s);
	stream_set_pos(context->s, 0);
	return context->s;
}

void xf_peer_live_rfx(freerdp_peer* client)
{
	STREAM* s;
	int width;
	int height;
	uint8* data;
	xfInfo* xfi;
	XImage* image;
	RFX_RECT rect;
	uint32 seconds;
	uint32 useconds;
	rdpUpdate* update;
	xfPeerContext* xfp;
	SURFACE_BITS_COMMAND* cmd;

	seconds = 1;
	useconds = 0;
	update = client->update;
	xfp = (xfPeerContext*) client->context;
	xfi = (xfInfo*) xfp->info;
	cmd = &update->surface_bits_command;

	width = xfi->width;
	height = xfi->height;
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

		rect.x = 0;
		rect.y = 0;
		rect.width = width;
		rect.height = height;
		rfx_compose_message(xfp->rfx_context, s, &rect, 1, data, width, height, width * 3);

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
		update->SurfaceBits(update->context, cmd);
	}
}

static boolean xf_peer_sleep_tsdiff(uint32 *old_sec, uint32 *old_usec, uint32 new_sec, uint32 new_usec)
{
	sint32 sec, usec;

	if (*old_sec == 0 && *old_usec == 0)
	{
		*old_sec = new_sec;
		*old_usec = new_usec;
		return true;
	}

	sec = new_sec - *old_sec;
	usec = new_usec - *old_usec;

	if (sec < 0 || (sec == 0 && usec < 0))
	{
		printf("Invalid time stamp detected.\n");
		return false;
	}

	*old_sec = new_sec;
	*old_usec = new_usec;

	while (usec < 0)
	{
		usec += 1000000;
		sec--;
	}

	if (sec > 0)
		freerdp_sleep(sec);

	if (usec > 0)
		freerdp_usleep(usec);

	return true;
}

void xf_peer_dump_rfx(freerdp_peer* client)
{
	STREAM* s;
	uint32 prev_seconds;
	uint32 prev_useconds;
	rdpUpdate* update;
	rdpPcap* pcap_rfx;
	pcap_record record;

	s = stream_new(512);
	update = client->update;
	client->update->pcap_rfx = pcap_open(xf_pcap_file, false);
	pcap_rfx = client->update->pcap_rfx;

	if (pcap_rfx == NULL)
		return;

	prev_seconds = prev_useconds = 0;

	while (pcap_has_next_record(pcap_rfx))
	{
		pcap_get_next_record_header(pcap_rfx, &record);

		s->data = xrealloc(s->data, record.length);
		record.data = s->data;
		s->size = record.length;

		pcap_get_next_record_content(pcap_rfx, &record);
		s->p = s->data + s->size;

		if (xf_pcap_dump_realtime && xf_peer_sleep_tsdiff(&prev_seconds, &prev_useconds, record.header.ts_sec, record.header.ts_usec) == false)
                        break;

		update->SurfaceCommand(update->context, s);
	}
}

boolean xf_peer_capabilities(freerdp_peer* client)
{
	return true;
}

boolean xf_peer_post_connect(freerdp_peer* client)
{
	xfInfo* xfi;
	xfPeerContext* xfp;

	xfp = (xfPeerContext*) client->context;
	xfi = (xfInfo*) xfp->info;

	/**
	 * This callback is called when the entire connection sequence is done, i.e. we've received the
	 * Font List PDU from the client and sent out the Font Map PDU.
	 * The server may start sending graphics output and receiving keyboard/mouse input after this
	 * callback returns.
	 */
	printf("Client %s is activated", client->hostname);
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

	client->settings->width = xfi->width;
	client->settings->height = xfi->height;
	client->update->DesktopResize(client->update->context);
	xfp->activated = false;

	/* Return false here would stop the execution of the peer mainloop. */
	return true;
}

boolean xf_peer_activate(freerdp_peer* client)
{
	xfPeerContext* xfp = (xfPeerContext*) client->context;

	rfx_context_reset(xfp->rfx_context);
	xfp->activated = true;

	if (xf_pcap_file != NULL)
	{
		client->update->dump_rfx = true;
		xf_peer_dump_rfx(client);
	}
	else
	{
		xf_peer_live_rfx(client);
	}

	return true;
}

void xf_peer_synchronize_event(rdpInput* input, uint32 flags)
{
	printf("Client sent a synchronize event (flags:0x%X)\n", flags);
}

void xf_peer_keyboard_event(rdpInput* input, uint16 flags, uint16 code)
{
	freerdp_peer* client = (freerdp_peer*) input->context->peer;
	rdpUpdate* update = client->update;
	xfPeerContext* xfp = (xfPeerContext*) input->context;

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
		update->DesktopResize(update->context);
		xfp->activated = false;
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

	printf("We've got a client %s\n", client->hostname);

	xf_peer_init(client);

	/* Initialize the real server settings here */
	client->settings->cert_file = xstrdup("server.crt");
	client->settings->privatekey_file = xstrdup("server.key");
	client->settings->nla_security = false;
	client->settings->rfx_codec = true;

	client->Capabilities = xf_peer_capabilities;
	client->PostConnect = xf_peer_post_connect;
	client->Activate = xf_peer_activate;

	client->input->SynchronizeEvent = xf_peer_synchronize_event;
	client->input->KeyboardEvent = xf_peer_keyboard_event;
	client->input->UnicodeKeyboardEvent = xf_peer_unicode_keyboard_event;
	client->input->MouseEvent = xf_peer_mouse_event;
	client->input->ExtendedMouseEvent = xf_peer_extended_mouse_event;

	client->Initialize(client);

	while (1)
	{
		rcount = 0;

		if (client->GetFileDescriptor(client, rfds, &rcount) != true)
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

		if (client->CheckFileDescriptor(client) != true)
			break;
	}

	printf("Client %s disconnected.\n", client->hostname);

	client->Disconnect(client);
	freerdp_peer_context_free(client);
	freerdp_peer_free(client);

	return NULL;
}

void xf_peer_accepted(freerdp_listener* instance, freerdp_peer* client)
{
	pthread_t th;

	pthread_create(&th, 0, xf_peer_main_loop, client);
	pthread_detach(th);
}
