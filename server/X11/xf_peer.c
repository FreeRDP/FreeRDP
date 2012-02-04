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
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <sys/select.h>
#include <freerdp/kbd/kbd.h>
#include <freerdp/codec/color.h>
#include <freerdp/utils/file.h>
#include <freerdp/utils/sleep.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/thread.h>

extern char* xf_pcap_file;
extern boolean xf_pcap_dump_realtime;

#include "xf_event.h"
#include "xf_input.h"
#include "xf_encode.h"

#include "xf_peer.h"

#ifdef WITH_XDAMAGE

void xf_xdamage_init(xfInfo* xfi)
{
	Bool pixmaps;
	int damage_event;
	int damage_error;
	int major, minor;
	XGCValues values;

	if (XShmQueryExtension(xfi->display) != False)
	{
		XShmQueryVersion(xfi->display, &major, &minor, &pixmaps);

		if (pixmaps != True)
		{
			printf("XShmQueryVersion failed\n");
			return;
		}
	}
	else
	{
		printf("XShmQueryExtension failed\n");
		return;
	}

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
	xfi->xdamage = XDamageCreate(xfi->display, xfi->root_window, XDamageReportDeltaRectangles);

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
	xfi->xdamage_gc = XCreateGC(xfi->display, xfi->root_window, GCSubwindowMode, &values);
	XSetFunction(xfi->display, xfi->xdamage_gc, GXcopy);
}

#endif

void xf_xshm_init(xfInfo* xfi)
{
	xfi->fb_shm_info.shmid = -1;
	xfi->fb_shm_info.shmaddr = (char*) -1;

	xfi->fb_image = XShmCreateImage(xfi->display, xfi->visual, xfi->depth,
			ZPixmap, NULL, &(xfi->fb_shm_info), xfi->width, xfi->height);

	if (xfi->fb_image == NULL)
	{
		printf("XShmCreateImage failed\n");
		return;
	}

	xfi->fb_shm_info.shmid = shmget(IPC_PRIVATE,
			xfi->fb_image->bytes_per_line * xfi->fb_image->height, IPC_CREAT | 0600);

	if (xfi->fb_shm_info.shmid == -1)
	{
		printf("shmget failed\n");
		return;
	}

	xfi->fb_shm_info.readOnly = False;
	xfi->fb_shm_info.shmaddr = shmat(xfi->fb_shm_info.shmid, 0, 0);
	xfi->fb_image->data = xfi->fb_shm_info.shmaddr;

	if (xfi->fb_shm_info.shmaddr == ((char*) -1))
	{
		printf("shmat failed\n");
		return;
	}

	XShmAttach(xfi->display, &(xfi->fb_shm_info));
	XSync(xfi->display, False);

	shmctl(xfi->fb_shm_info.shmid, IPC_RMID, 0);

	xfi->fb_pixmap = XShmCreatePixmap(xfi->display,
			xfi->root_window, xfi->fb_image->data, &(xfi->fb_shm_info),
			xfi->fb_image->width, xfi->fb_image->height, xfi->fb_image->depth);
}

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

	//xfi->use_xshm = true;
	xfi->display = XOpenDisplay(NULL);

	XInitThreads();

	if (xfi->display == NULL)
	{
		printf("failed to open display: %s\n", XDisplayName(NULL));
		exit(1);
	}

	xfi->xfds = ConnectionNumber(xfi->display);
	xfi->number = DefaultScreen(xfi->display);
	xfi->screen = ScreenOfDisplay(xfi->display, xfi->number);
	xfi->depth = DefaultDepthOfScreen(xfi->screen);
	xfi->width = WidthOfScreen(xfi->screen);
	xfi->height = HeightOfScreen(xfi->screen);
	xfi->root_window = DefaultRootWindow(xfi->display);

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

	xfi->clrconv = freerdp_clrconv_new(CLRCONV_ALPHA | CLRCONV_INVERT);

	XSelectInput(xfi->display, xfi->root_window, SubstructureNotifyMask);

#ifdef WITH_XDAMAGE
	xf_xdamage_init(xfi);
#endif

	xf_xshm_init(xfi);

	xfi->bytesPerPixel = 4;

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

	rfx_context_set_pixel_format(context->rfx_context, RFX_PIXEL_FORMAT_BGRA);

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
	xfInfo* xfi;
	xfPeerContext* xfp;

	client->context_size = sizeof(xfPeerContext);
	client->ContextNew = (psPeerContextNew) xf_peer_context_new;
	client->ContextFree = (psPeerContextFree) xf_peer_context_free;
	freerdp_peer_context_new(client);

	xfp = (xfPeerContext*) client->context;

	xfp->fps = 24;
	xfp->thread = 0;
	xfp->activations = 0;
	xfp->event_queue = xf_event_queue_new();

	xfi = xfp->info;
	xfp->hdc = gdi_CreateDC(xfi->clrconv, xfi->bpp);

	pthread_mutex_init(&(xfp->mutex), NULL);
}

STREAM* xf_peer_stream_init(xfPeerContext* context)
{
	stream_clear(context->s);
	stream_set_pos(context->s, 0);
	return context->s;
}

void xf_peer_live_rfx(freerdp_peer* client)
{
	xfPeerContext* xfp = (xfPeerContext*) client->context;

	if (xfp->activations == 1)
		pthread_create(&(xfp->thread), 0, xf_monitor_updates, (void*) client);
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
	uint8* data;
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

	if (xfi->use_xshm)
	{
		width = x + width;
		height = y + height;
		x = 0;
		y = 0;

		rect.x = x;
		rect.y = y;
		rect.width = width;
		rect.height = height;

		image = xf_snapshot(xfp, x, y, width, height);

		data = (uint8*) image->data;
		data = &data[(y * image->bytes_per_line) + (x * image->bits_per_pixel)];

		rfx_compose_message(xfp->rfx_context, s, &rect, 1, data,
				width, height, image->bytes_per_line);

		cmd->destLeft = x;
		cmd->destTop = y;
		cmd->destRight = x + width;
		cmd->destBottom = y + height;
	}
	else
	{
		rect.x = 0;
		rect.y = 0;
		rect.width = width;
		rect.height = height;

		image = xf_snapshot(xfp, x, y, width, height);

		rfx_compose_message(xfp->rfx_context, s, &rect, 1,
				(uint8*) image->data, width, height, width * xfi->bytesPerPixel);

		cmd->destLeft = x;
		cmd->destTop = y;
		cmd->destRight = x + width;
		cmd->destBottom = y + height;

		XDestroyImage(image);
	}

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

	if (xfp->event_queue->pipe_fd[0] == -1)
		return true;

	rfds[*rcount] = (void *)(long) xfp->event_queue->pipe_fd[0];
	(*rcount)++;

	return true;
}

boolean xf_peer_check_fds(freerdp_peer* client)
{
	xfInfo* xfi;
	xfEvent* event;
	xfPeerContext* xfp;
	HGDI_RGN invalid_region;

	xfp = (xfPeerContext*) client->context;
	xfi = xfp->info;

	if (xfp->activated == false)
		return true;

	event = xf_event_peek(xfp->event_queue);

	if (event != NULL)
	{
		if (event->type == XF_EVENT_TYPE_REGION)
		{
			xfEventRegion* region = (xfEventRegion*) xf_event_pop(xfp->event_queue);
			gdi_InvalidateRegion(xfp->hdc, region->x, region->y, region->width, region->height);
			xf_event_region_free(region);
		}
		else if (event->type == XF_EVENT_TYPE_FRAME_TICK)
		{
			event = xf_event_pop(xfp->event_queue);
			invalid_region = xfp->hdc->hwnd->invalid;

			if (invalid_region->null == false)
			{
				xf_peer_rfx_update(client, invalid_region->x, invalid_region->y,
					invalid_region->w, invalid_region->h);
			}

			invalid_region->null = 1;
			xfp->hdc->hwnd->ninvalid = 0;

			xf_event_free(event);
		}
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

void* xf_peer_main_loop(void* arg)
{
	int i;
	int fds;
	int max_fds;
	int rcount;
	void* rfds[32];
	fd_set rfds_set;
	rdpSettings* settings;
	char* server_file_path;
	freerdp_peer* client = (freerdp_peer*) arg;

	memset(rfds, 0, sizeof(rfds));

	printf("We've got a client %s\n", client->hostname);

	xf_peer_init(client);
	settings = client->settings;

	/* Initialize the real server settings here */

	if (settings->development_mode)
	{
		server_file_path = freerdp_construct_path(settings->development_path, "server/X11");
	}
	else
	{
		server_file_path = freerdp_construct_path(settings->config_path, "server");

		if (!freerdp_check_file_exists(server_file_path))
			freerdp_mkdir(server_file_path);
	}

	settings->cert_file = freerdp_construct_path(server_file_path, "server.crt");
	settings->privatekey_file = freerdp_construct_path(server_file_path, "server.key");

	settings->nla_security = false;
	settings->rfx_codec = true;

	client->Capabilities = xf_peer_capabilities;
	client->PostConnect = xf_peer_post_connect;
	client->Activate = xf_peer_activate;

	xf_input_register_callbacks(client->input);

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
