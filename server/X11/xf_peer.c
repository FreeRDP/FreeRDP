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

#include <assert.h>
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
#include <winpr/thread.h>

#include <freerdp/freerdp.h>
#include <freerdp/codec/color.h>
#include <freerdp/locale/keyboard.h>

#include "xf_input.h"
#include "xf_cursor.h"
#include "xf_encode.h"
#include "xf_update.h"
#include "xf_monitors.h"

#include "makecert.h"

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

int xf_xshm_init(xfInfo* xfi)
{
	Bool pixmaps;
	int major, minor;

	if (XShmQueryExtension(xfi->display) != False)
	{
		XShmQueryVersion(xfi->display, &major, &minor, &pixmaps);

		if (pixmaps != True)
		{
			fprintf(stderr, "XShmQueryVersion failed\n");
			return -1;
		}
	}
	else
	{
		fprintf(stderr, "XShmQueryExtension failed\n");
		return -1;
	}

	xfi->fb_shm_info.shmid = -1;
	xfi->fb_shm_info.shmaddr = (char*) -1;

	xfi->fb_image = XShmCreateImage(xfi->display, xfi->visual, xfi->depth,
			ZPixmap, NULL, &(xfi->fb_shm_info), xfi->width, xfi->height);

	if (!xfi->fb_image)
	{
		fprintf(stderr, "XShmCreateImage failed\n");
		return -1;
	}

	xfi->fb_shm_info.shmid = shmget(IPC_PRIVATE,
			xfi->fb_image->bytes_per_line * xfi->fb_image->height, IPC_CREAT | 0600);

	if (xfi->fb_shm_info.shmid == -1)
	{
		fprintf(stderr, "shmget failed\n");
		return -1;
	}

	xfi->fb_shm_info.readOnly = False;
	xfi->fb_shm_info.shmaddr = shmat(xfi->fb_shm_info.shmid, 0, 0);
	xfi->fb_image->data = xfi->fb_shm_info.shmaddr;

	if (xfi->fb_shm_info.shmaddr == ((char*) -1))
	{
		fprintf(stderr, "shmat failed\n");
		return -1;
	}

	XShmAttach(xfi->display, &(xfi->fb_shm_info));
	XSync(xfi->display, False);

	shmctl(xfi->fb_shm_info.shmid, IPC_RMID, 0);

	fprintf(stderr, "display: %p root_window: %p width: %d height: %d depth: %d\n",
			xfi->display, (void*) xfi->root_window, xfi->fb_image->width, xfi->fb_image->height, xfi->fb_image->depth);

	xfi->fb_pixmap = XShmCreatePixmap(xfi->display,
			xfi->root_window, xfi->fb_image->data, &(xfi->fb_shm_info),
			xfi->fb_image->width, xfi->fb_image->height, xfi->fb_image->depth);

	return 0;
}

void xf_info_free(xfInfo *info)
{
	assert(NULL != info);

	if (info->display)
		XCloseDisplay(info->display);
	
	freerdp_clrconv_free(info->clrconv);
	free(info);
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
	xfi->use_xshm = TRUE;

	setenv("DISPLAY", ":0", 1); /* Set DISPLAY variable if not already set */

	if (!XInitThreads())
		fprintf(stderr, "warning: XInitThreads() failure\n");

	xfi->display = XOpenDisplay(NULL);

	if (!xfi->display)
	{
		fprintf(stderr, "failed to open display: %s\n", XDisplayName(NULL));
		exit(1);
	}

	xf_list_monitors(xfi);

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

	if (!vis)
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

	if (xfi->use_xshm)
	{
		if (xf_xshm_init(xfi) < 0)
			xfi->use_xshm = FALSE;
	}

	if (xfi->use_xshm)
		printf("Using X Shared Memory Extension (XShm)\n");

#ifdef WITH_XDAMAGE
	xf_xdamage_init(xfi);
#endif

	xf_cursor_init(xfi);

	xfi->bytesPerPixel = 4;
	xfi->activePeerCount = 0;

	freerdp_keyboard_init(0);

	return xfi;
}

void xf_peer_context_new(freerdp_peer* client, xfPeerContext* context)
{
	context->info = xf_info_init();
	context->rfx_context = rfx_context_new(TRUE);
	context->rfx_context->mode = RLGR3;
	context->rfx_context->width = context->info->width;
	context->rfx_context->height = context->info->height;

	rfx_context_set_pixel_format(context->rfx_context, RDP_PIXEL_FORMAT_B8G8R8A8);

	context->s = Stream_New(NULL, 65536);
	Stream_Clear(context->s);

	context->updateReadyEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	context->updateSentEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
}

void xf_peer_context_free(freerdp_peer* client, xfPeerContext* context)
{
	if (context)
	{
		xf_info_free(context->info);

		CloseHandle(context->updateReadyEvent);
		CloseHandle(context->updateSentEvent);

		Stream_Free(context->s, TRUE);
		rfx_context_free(context->rfx_context);
	}
}

void xf_peer_init(freerdp_peer* client)
{
	xfInfo* xfi;
	xfPeerContext* xfp;

	client->ContextSize = sizeof(xfPeerContext);
	client->ContextNew = (psPeerContextNew) xf_peer_context_new;
	client->ContextFree = (psPeerContextFree) xf_peer_context_free;
	freerdp_peer_context_new(client);

	xfp = (xfPeerContext*) client->context;

	xfp->fps = 16;
	xfi = xfp->info;

	xfp->mutex = CreateMutex(NULL, FALSE, NULL);
}

void xf_peer_send_update(freerdp_peer* client)
{
	rdpUpdate* update;
	SURFACE_BITS_COMMAND* cmd;

	update = client->update;
	cmd = &update->surface_bits_command;

	if (cmd->bitmapDataLength)
		update->SurfaceBits(update->context, cmd);
}

BOOL xf_peer_get_fds(freerdp_peer* client, void** rfds, int* rcount)
{
	int fds;
	HANDLE event;
	xfPeerContext* xfp = (xfPeerContext*) client->context;

	event = xfp->updateReadyEvent;
	fds = GetEventFileDescriptor(event);
	rfds[*rcount] = (void*) (long) fds;
	(*rcount)++;

	return TRUE;
}

BOOL xf_peer_check_fds(freerdp_peer* client)
{
	xfInfo* xfi;
	xfPeerContext* xfp;

	xfp = (xfPeerContext*) client->context;
	xfi = xfp->info;

	if (WaitForSingleObject(xfp->updateReadyEvent, 0) == WAIT_OBJECT_0)
	{
		if (!xfp->activated)
			return TRUE;

		xf_peer_send_update(client);

		ResetEvent(xfp->updateReadyEvent);
		SetEvent(xfp->updateSentEvent);
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

	xfp->info->activePeerCount++;

	xfp->monitorThread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE) xf_update_thread, (void*) client, 0, NULL);

	return TRUE;
}

const char* makecert_argv[4] =
{
	"makecert",
	"-rdp",
	"-live",
	"-silent"
};

int makecert_argc = (sizeof(makecert_argv) / sizeof(char*));

int xf_generate_certificate(rdpSettings* settings)
{
	char* server_file_path;
	MAKECERT_CONTEXT* context;

	server_file_path = GetCombinedPath(settings->ConfigPath, "server");

	if (!PathFileExistsA(server_file_path))
		CreateDirectoryA(server_file_path, 0);

	settings->CertificateFile = GetCombinedPath(server_file_path, "server.crt");
	settings->PrivateKeyFile = GetCombinedPath(server_file_path, "server.key");

	if ((!PathFileExistsA(settings->CertificateFile)) ||
			(!PathFileExistsA(settings->PrivateKeyFile)))
	{
		context = makecert_context_new();

		makecert_context_process(context, makecert_argc, (char**) makecert_argv);

		makecert_context_set_output_file_name(context, "server");

		if (!PathFileExistsA(settings->CertificateFile))
			makecert_context_output_certificate_file(context, server_file_path);

		if (!PathFileExistsA(settings->PrivateKeyFile))
			makecert_context_output_private_key_file(context, server_file_path);

		makecert_context_free(context);
	}

	free(server_file_path);

	return 0;
}

static void* xf_peer_main_loop(void* arg)
{
	int i;
	int fds;
	int max_fds;
	int rcount;
	void* rfds[32];
	fd_set rfds_set;
	rdpSettings* settings;
	xfPeerContext* xfp;
	struct timeval timeout;
	freerdp_peer* client = (freerdp_peer*) arg;

	assert(NULL != client);

	ZeroMemory(rfds, sizeof(rfds));
	ZeroMemory(&timeout, sizeof(struct timeval));

	fprintf(stderr, "We've got a client %s\n", client->hostname);

	xf_peer_init(client);
	xfp = (xfPeerContext*) client->context;
	settings = client->settings;

	xf_generate_certificate(settings);

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

		timeout.tv_sec = 0;
		timeout.tv_usec = 100;

		if (select(max_fds + 1, &rfds_set, NULL, NULL, &timeout) == -1)
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
	
	freerdp_peer_context_free(client);
	freerdp_peer_free(client);

	ExitThread(0);

	return NULL;
}

void xf_peer_accepted(freerdp_listener* instance, freerdp_peer* client)
{
	HANDLE thread;

	thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) xf_peer_main_loop, client, 0, NULL);
}
