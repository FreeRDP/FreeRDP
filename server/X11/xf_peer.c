/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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

#include <winpr/crt.h>
#include <winpr/file.h>
#include <winpr/path.h>
#include <winpr/synch.h>

#include <freerdp/freerdp.h>
#include <freerdp/codec/color.h>
#include <freerdp/locale/keyboard.h>

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

	if (xfi->use_xshm)
	{
		if (XShmQueryExtension(xfi->display) != False)
		{
			XShmQueryVersion(xfi->display, &major, &minor, &pixmaps);

			if (pixmaps != True)
			{
				fprintf(stderr, "XShmQueryVersion failed\n");
				return;
			}
		}
		else
		{
			fprintf(stderr, "XShmQueryExtension failed\n");
			return;
		}
	}

	if (XDamageQueryExtension(xfi->display, &damage_event, &damage_error) == 0)
	{
		fprintf(stderr, "XDamageQueryExtension failed\n");
		return;
	}

	XDamageQueryVersion(xfi->display, &major, &minor);

	if (XDamageQueryVersion(xfi->display, &major, &minor) == 0)
	{
		fprintf(stderr, "XDamageQueryVersion failed\n");
		return;
	}
	else if (major < 1)
	{
		fprintf(stderr, "XDamageQueryVersion failed: major:%d minor:%d\n", major, minor);
		return;
	}

	xfi->xdamage_notify_event = damage_event + XDamageNotify;
	xfi->xdamage = XDamageCreate(xfi->display, xfi->root_window, XDamageReportDeltaRectangles);

	if (xfi->xdamage == None)
	{
		fprintf(stderr, "XDamageCreate failed\n");
		return;
	}

#ifdef WITH_XFIXES
	xfi->xdamage_region = XFixesCreateRegion(xfi->display, NULL, 0);

	if (xfi->xdamage_region == None)
	{
		fprintf(stderr, "XFixesCreateRegion failed\n");
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

	if (!xfi->fb_image)
	{
		fprintf(stderr, "XShmCreateImage failed\n");
		return;
	}

	xfi->fb_shm_info.shmid = shmget(IPC_PRIVATE,
			xfi->fb_image->bytes_per_line * xfi->fb_image->height, IPC_CREAT | 0600);

	if (xfi->fb_shm_info.shmid == -1)
	{
		fprintf(stderr, "shmget failed\n");
		return;
	}

	xfi->fb_shm_info.readOnly = False;
	xfi->fb_shm_info.shmaddr = shmat(xfi->fb_shm_info.shmid, 0, 0);
	xfi->fb_image->data = xfi->fb_shm_info.shmaddr;

	if (xfi->fb_shm_info.shmaddr == ((char*) -1))
	{
		fprintf(stderr, "shmat failed\n");
		return;
	}

	XShmAttach(xfi->display, &(xfi->fb_shm_info));
	XSync(xfi->display, False);

	shmctl(xfi->fb_shm_info.shmid, IPC_RMID, 0);

	fprintf(stderr, "display: %p root_window: %p width: %d height: %d depth: %d\n",
			xfi->display, (void*) xfi->root_window, xfi->fb_image->width, xfi->fb_image->height, xfi->fb_image->depth);

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

	xfi = (xfInfo*) malloc(sizeof(xfInfo));
	ZeroMemory(xfi, sizeof(xfInfo));

	/**
	 * Recent X11 servers drop support for shared pixmaps
	 * To see if your X11 server supports shared pixmaps, use:
	 * xdpyinfo -ext MIT-SHM | grep "shared pixmaps"
	 */
	xfi->use_xshm = FALSE;

	if (!XInitThreads())
		fprintf(stderr, "warning: XInitThreads() failure\n");

	xfi->display = XOpenDisplay(NULL);

	if (!xfi->display)
	{
		fprintf(stderr, "failed to open display: %s\n", XDisplayName(NULL));
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

	if (!pfs)
	{
		fprintf(stderr, "XListPixmapFormats failed\n");
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

	ZeroMemory(&template, sizeof(template));
	template.class = TrueColor;
	template.screen = xfi->number;

	vis = XGetVisualInfo(xfi->display, VisualClassMask | VisualScreenMask, &template, &vi_count);

	if (vis == NULL)
	{
		fprintf(stderr, "XGetVisualInfo failed\n");
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

	if (xfi->use_xshm)
		xf_xshm_init(xfi);

	xfi->bytesPerPixel = 4;

	freerdp_keyboard_init(0);

	return xfi;
}

void xf_peer_context_new(freerdp_peer* client, xfPeerContext* context)
{
	context->info = xf_info_init();
	context->rfx_context = rfx_context_new();
	context->rfx_context->mode = RLGR3;
	context->rfx_context->width = context->info->width;
	context->rfx_context->height = context->info->height;

	rfx_context_set_pixel_format(context->rfx_context, RDP_PIXEL_FORMAT_B8G8R8A8);

	context->s = stream_new(65536);
}

void xf_peer_context_free(freerdp_peer* client, xfPeerContext* context)
{
	if (context)
	{
		stream_free(context->s);
		rfx_context_free(context->rfx_context);
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

	xfp->fps = 16;
	xfp->thread = 0;
	xfp->activations = 0;

	xfp->queue = MessageQueue_New();

	xfi = xfp->info;
	xfp->hdc = gdi_CreateDC(xfi->clrconv, xfi->bpp);

	pthread_mutex_init(&(xfp->mutex), NULL);
}

wStream* xf_peer_stream_init(xfPeerContext* context)
{
	stream_clear(context->s);
	stream_set_pos(context->s, 0);
	return context->s;
}

void xf_peer_live_rfx(freerdp_peer* client)
{
	xfPeerContext* xfp = (xfPeerContext*) client->context;
	pthread_create(&(xfp->thread), 0, xf_monitor_updates, (void*) client);
}

void xf_peer_rfx_update(freerdp_peer* client, int x, int y, int width, int height)
{
	wStream* s;
	BYTE* data;
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
		/**
		 * Passing an offset source rectangle to rfx_compose_message()
		 * leads to protocol errors, so offset the data pointer instead.
		 */

		rect.x = 0;
		rect.y = 0;
		rect.width = width;
		rect.height = height;

		image = xf_snapshot(xfp, x, y, width, height);

		data = (BYTE*) image->data;
		data = &data[(y * image->bytes_per_line) + (x * image->bits_per_pixel / 8)];

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

		data = (BYTE*) image->data;

		rfx_compose_message(xfp->rfx_context, s, &rect, 1, data,
				width, height, image->bytes_per_line);

		cmd->destLeft = x;
		cmd->destTop = y;
		cmd->destRight = x + width;
		cmd->destBottom = y + height;

		XDestroyImage(image);
	}

	cmd->bpp = 32;
	cmd->codecID = client->settings->RemoteFxCodecId;
	cmd->width = width;
	cmd->height = height;
	cmd->bitmapDataLength = stream_get_length(s);
	cmd->bitmapData = stream_get_head(s);

	update->SurfaceBits(update->context, cmd);
}

BOOL xf_peer_get_fds(freerdp_peer* client, void** rfds, int* rcount)
{
	int fds;
	HANDLE event;
	xfPeerContext* xfp = (xfPeerContext*) client->context;

	event = MessageQueue_Event(xfp->queue);
	fds = GetEventFileDescriptor(event);
	rfds[*rcount] = (void*) (long) fds;
	(*rcount)++;

	return TRUE;
}

BOOL xf_peer_check_fds(freerdp_peer* client)
{
	xfInfo* xfi;
	wMessage message;
	xfPeerContext* xfp;

	xfp = (xfPeerContext*) client->context;
	xfi = xfp->info;

	if (xfp->activated == FALSE)
		return TRUE;

	if (MessageQueue_Peek(xfp->queue, &message, TRUE))
	{
		if (message.id == MakeMessageId(PeerEvent, EncodeRegion))
		{
			UINT32 xy, wh;
			UINT16 x, y, w, h;

			xy = (UINT32) (size_t) message.wParam;
			wh = (UINT32) (size_t) message.lParam;

			x = ((xy & 0xFFFF0000) >> 16);
			y = (xy & 0x0000FFFF);

			w = ((wh & 0xFFFF0000) >> 16);
			h = (wh & 0x0000FFFF);

			if (w * h > 0)
				xf_peer_rfx_update(client, x, y, w, h);
		}
	}

	return TRUE;
}

BOOL xf_peer_capabilities(freerdp_peer* client)
{
	return TRUE;
}

BOOL xf_peer_post_connect(freerdp_peer* client)
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
	fprintf(stderr, "Client %s is activated", client->hostname);
	if (client->settings->AutoLogonEnabled)
	{
		fprintf(stderr, " and wants to login automatically as %s\\%s",
			client->settings->Domain ? client->settings->Domain : "",
			client->settings->Username);

		/* A real server may perform OS login here if NLA is not executed previously. */
	}
	fprintf(stderr, "\n");

	fprintf(stderr, "Client requested desktop: %dx%dx%d\n",
		client->settings->DesktopWidth, client->settings->DesktopHeight, client->settings->ColorDepth);

	if (!client->settings->RemoteFxCodec)
	{
		fprintf(stderr, "Client does not support RemoteFX\n");
		return FALSE;
	}

	/* A real server should tag the peer as activated here and start sending updates in main loop. */

	client->settings->DesktopWidth = xfi->width;
	client->settings->DesktopHeight = xfi->height;

	client->update->DesktopResize(client->update->context);

	/* Return FALSE here would stop the execution of the peer main loop. */
	return TRUE;
}

BOOL xf_peer_activate(freerdp_peer* client)
{
	xfPeerContext* xfp = (xfPeerContext*) client->context;

	rfx_context_reset(xfp->rfx_context);
	xfp->activated = TRUE;

	xf_peer_live_rfx(client);

	return TRUE;
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
	xfPeerContext* xfp;

	ZeroMemory(rfds, sizeof(rfds));

	fprintf(stderr, "We've got a client %s\n", client->hostname);

	xf_peer_init(client);
	xfp = (xfPeerContext*) client->context;

	settings = client->settings;

	/* Initialize the real server settings here */

	server_file_path = GetCombinedPath(settings->ConfigPath, "server");

	if (!PathFileExistsA(server_file_path))
		CreateDirectoryA(server_file_path, 0);

	settings->CertificateFile = GetCombinedPath(server_file_path, "server.crt");
	settings->PrivateKeyFile = GetCombinedPath(server_file_path, "server.key");

	settings->RemoteFxCodec = TRUE;
	settings->ColorDepth = 32;

	client->Capabilities = xf_peer_capabilities;
	client->PostConnect = xf_peer_post_connect;
	client->Activate = xf_peer_activate;

	xf_input_register_callbacks(client->input);

	client->Initialize(client);

	while (1)
	{
		rcount = 0;

		if (client->GetFileDescriptor(client, rfds, &rcount) != TRUE)
		{
			fprintf(stderr, "Failed to get FreeRDP file descriptor\n");
			break;
		}
		if (xf_peer_get_fds(client, rfds, &rcount) != TRUE)
		{
			fprintf(stderr, "Failed to get xfreerdp file descriptor\n");
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
				fprintf(stderr, "select failed\n");
				break;
			}
		}

		if (client->CheckFileDescriptor(client) != TRUE)
		{
			fprintf(stderr, "Failed to check freerdp file descriptor\n");
			break;
		}
		if ((xf_peer_check_fds(client)) != TRUE)
		{
			fprintf(stderr, "Failed to check xfreerdp file descriptor\n");
			break;
		}
	}

	fprintf(stderr, "Client %s disconnected.\n", client->hostname);

	client->Disconnect(client);
	
	pthread_cancel(xfp->thread);
	pthread_cancel(xfp->frame_rate_thread);
	
	pthread_join(xfp->thread, NULL);
	pthread_join(xfp->frame_rate_thread, NULL);
	
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
