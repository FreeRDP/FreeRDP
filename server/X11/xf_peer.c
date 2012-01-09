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
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <sys/select.h>
#include <freerdp/kbd/kbd.h>
#include <freerdp/codec/color.h>
#include <freerdp/utils/sleep.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/thread.h>

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

	XInitThreads();

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

	freerdp_kbd_init(xfi->display, 0);

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
	xfPeerContext* xfp;

	client->context_size = sizeof(xfPeerContext);
	client->ContextNew = (psPeerContextNew) xf_peer_context_new;
	client->ContextFree = (psPeerContextFree) xf_peer_context_free;
	freerdp_peer_context_new(client);

	xfp = (xfPeerContext*) client->context;

	xfp->pipe_fd[0] = -1;
	xfp->pipe_fd[1] = -1;

	if (pipe(xfp->pipe_fd) < 0)
		printf("xf_peer_init: pipe failed\n");

	xfp->thread = 0;
	xfp->activations = 0;

	xfp->stopwatch = stopwatch_create();

	xfp->hdc = gdi_GetDC();

	xfp->hdc->hwnd = (HGDI_WND) malloc(sizeof(GDI_WND));
	xfp->hdc->hwnd->invalid = gdi_CreateRectRgn(0, 0, 0, 0);
	xfp->hdc->hwnd->invalid->null = 1;

	xfp->hdc->hwnd->count = 32;
	xfp->hdc->hwnd->cinvalid = (HGDI_RGN) malloc(sizeof(GDI_RGN) * xfp->hdc->hwnd->count);
	xfp->hdc->hwnd->ninvalid = 0;

	pthread_mutex_init(&(xfp->mutex), NULL);
}

STREAM* xf_peer_stream_init(xfPeerContext* context)
{
	stream_clear(context->s);
	stream_set_pos(context->s, 0);
	return context->s;
}

int xf_is_event_set(xfPeerContext* xfp)
{
	fd_set rfds;
	int num_set;
	struct timeval time;

	FD_ZERO(&rfds);
	FD_SET(xfp->pipe_fd[0], &rfds);
	memset(&time, 0, sizeof(time));
	num_set = select(xfp->pipe_fd[0] + 1, &rfds, 0, 0, &time);

	return (num_set == 1);
}

void xf_signal_event(xfPeerContext* xfp)
{
	int length;

	length = write(xfp->pipe_fd[1], "sig", 4);

	if (length != 4)
		printf("xf_signal_event: error\n");
}

void xf_clear_event(xfPeerContext* xfp)
{
	int length;

	while (xf_is_event_set(xfp))
	{
		length = read(xfp->pipe_fd[0], &length, 4);

		if (length != 4)
			printf("xf_clear_event: error\n");
	}
}

void* xf_monitor_graphics(void* param)
{
	xfInfo* xfi;
	XEvent xevent;
	uint32 sec, usec;
	XRectangle region;
	xfPeerContext* xfp;
	freerdp_peer* client;
	int x, y, width, height;
	XDamageNotifyEvent* notify;

	client = (freerdp_peer*) param;
	xfp = (xfPeerContext*) client->context;
	xfi = xfp->info;

	xfp->capture_buffer = (uint8*) xmalloc(xfi->width * xfi->height * 3);

	pthread_detach(pthread_self());

	stopwatch_start(xfp->stopwatch);

	while (1)
	{
		pthread_mutex_lock(&(xfp->mutex));

		while (XPending(xfi->display))
		{
			memset(&xevent, 0, sizeof(xevent));
			XNextEvent(xfi->display, &xevent);

			if (xevent.type == xfi->xdamage_notify_event)
			{
				notify = (XDamageNotifyEvent*) &xevent;

				x = notify->area.x;
				y = notify->area.y;
				width = notify->area.width;
				height = notify->area.height;

				region.x = x;
				region.y = y;
				region.width = width;
				region.height = height;

#ifdef WITH_XFIXES
				XFixesSetRegion(xfi->display, xfi->xdamage_region, &region, 1);
				XDamageSubtract(xfi->display, xfi->xdamage, xfi->xdamage_region, None);
#endif

				gdi_InvalidateRegion(xfp->hdc, x, y, width, height);

				stopwatch_stop(xfp->stopwatch);
				stopwatch_get_elapsed_time_in_useconds(xfp->stopwatch, &sec, &usec);

				if ((sec > 0) || (usec > 100))
					break;
			}
		}

		stopwatch_stop(xfp->stopwatch);
		stopwatch_get_elapsed_time_in_useconds(xfp->stopwatch, &sec, &usec);

		if ((sec > 0) || (usec > 100))
		{
			HGDI_RGN region;

			stopwatch_reset(xfp->stopwatch);
			stopwatch_start(xfp->stopwatch);

			region = xfp->hdc->hwnd->invalid;
			pthread_mutex_unlock(&(xfp->mutex));

			xf_signal_event(xfp);

			while (region->null == 0)
			{
				freerdp_usleep(33);
			}
		}
		else
		{
			pthread_mutex_unlock(&(xfp->mutex));
			freerdp_usleep(33);
		}
	}

	return NULL;
}

void xf_peer_live_rfx(freerdp_peer* client)
{
	xfPeerContext* xfp = (xfPeerContext*) client->context;

	if (xfp->activations == 1)
		pthread_create(&(xfp->thread), 0, xf_monitor_graphics, (void*) client);
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

void xf_peer_rfx_update(freerdp_peer* client, int x, int y, int width, int height)
{
	STREAM* s;
	xfInfo* xfi;
	RFX_RECT rect;
	XImage* image;
	rdpUpdate* update;
	xfPeerContext* xfp;
	SURFACE_BITS_COMMAND* cmd;

	update = client->update;
	xfp = (xfPeerContext*) client->context;
	cmd = &update->surface_bits_command;
	xfi = xfp->info;

	if (width * height <= 0)
		return;

	s = xf_peer_stream_init(xfp);

	image = xf_snapshot(xfp, x, y, width, height);

	freerdp_image_convert((uint8*) image->data, xfp->capture_buffer, width, height, 32, 24, xfi->clrconv);

	rect.x = 0;
	rect.y = 0;
	rect.width = width;
	rect.height = height;

	rfx_compose_message(xfp->rfx_context, s, &rect, 1, xfp->capture_buffer, width, height, width * 3);

	cmd->destLeft = x;
	cmd->destTop = y;
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

boolean xf_peer_get_fds(freerdp_peer* client, void** rfds, int* rcount)
{
	xfPeerContext* xfp = (xfPeerContext*) client->context;

	if (xfp->pipe_fd[0] == -1)
		return true;

	rfds[*rcount] = (void *)(long) xfp->pipe_fd[0];
	(*rcount)++;

	return true;
}

boolean xf_peer_check_fds(freerdp_peer* client)
{
	xfInfo* xfi;
	xfPeerContext* xfp;

	xfp = (xfPeerContext*) client->context;
	xfi = xfp->info;

	if (xfp->pipe_fd[0] == -1)
		return true;

	if (xfp->activated == false)
		return true;

	if (xf_is_event_set(xfp))
	{
		HGDI_RGN region;

		xf_clear_event(xfp);

		region = xfp->hdc->hwnd->invalid;

		if (region->null)
			return true;

		xf_peer_rfx_update(client, region->x, region->y, region->w, region->h);
		region->null = true;
	}

	return true;
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
		xfp->activations++;
	}

	return true;
}

void xf_peer_synchronize_event(rdpInput* input, uint32 flags)
{
	printf("Client sent a synchronize event (flags:0x%X)\n", flags);
}

void xf_peer_keyboard_event(rdpInput* input, uint16 flags, uint16 code)
{
	unsigned int keycode;
	boolean extended = false;
	xfPeerContext* xfp = (xfPeerContext*) input->context;
	xfInfo* xfi = xfp->info;

	printf("Client sent a keyboard event (flags:0x%X code:0x%X)\n", flags, code);

	if (flags & KBD_FLAGS_EXTENDED)
		extended = true;

	keycode = freerdp_kbd_get_keycode_by_scancode(code, extended);

	printf("keycode: %d\n", keycode);

	if (keycode != 0)
	{
#ifdef WITH_XTEST
		pthread_mutex_lock(&(xfp->mutex));

		if (flags & KBD_FLAGS_DOWN)
			XTestFakeKeyEvent(xfi->display, keycode, True, 0);
		else if (flags & KBD_FLAGS_RELEASE)
			XTestFakeKeyEvent(xfi->display, keycode, False, 0);

		pthread_mutex_unlock(&(xfp->mutex));
#endif
	}
}

void xf_peer_unicode_keyboard_event(rdpInput* input, uint16 code)
{
	printf("Client sent a unicode keyboard event (code:0x%X)\n", code);
}

void xf_peer_mouse_event(rdpInput* input, uint16 flags, uint16 x, uint16 y)
{
	int button = 0;
	boolean down = false;
	xfPeerContext* xfp = (xfPeerContext*) input->context;
	xfInfo* xfi = xfp->info;

	pthread_mutex_lock(&(xfp->mutex));
#ifdef WITH_XTEST

	if (flags & PTR_FLAGS_MOVE)
		XTestFakeMotionEvent(xfi->display, 0, x, y, CurrentTime);

	if (flags & PTR_FLAGS_BUTTON1)
		button = 1;
	else if (flags & PTR_FLAGS_BUTTON2)
		button = 2;
	else if (flags & PTR_FLAGS_BUTTON3)
		button = 3;

	if (flags & PTR_FLAGS_DOWN)
		down = true;

	if (button != 0)
		XTestFakeButtonEvent(xfi->display, button, down, CurrentTime);
#endif
	pthread_mutex_unlock(&(xfp->mutex));

	printf("Client sent a mouse event (flags:0x%X pos:%d,%d)\n", flags, x, y);
}

void xf_peer_extended_mouse_event(rdpInput* input, uint16 flags, uint16 x, uint16 y)
{
	xfPeerContext* xfp = (xfPeerContext*) input->context;
	xfInfo* xfi = xfp->info;

	pthread_mutex_lock(&(xfp->mutex));
#ifdef WITH_XTEST
	XTestFakeMotionEvent(xfi->display, 0, x, y, CurrentTime);
#endif
	pthread_mutex_unlock(&(xfp->mutex));

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
		if (xf_peer_get_fds(client, rfds, &rcount) != true)
		{
			printf("Failed to get xfreerdp file descriptor\n");
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
		{
			printf("Failed to check freerdp file descriptor\n");
			break;
		}
		if ((xf_peer_check_fds(client)) != true)
		{
			printf("Failed to check xfreerdp file descriptor\n");
			break;
		}
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
