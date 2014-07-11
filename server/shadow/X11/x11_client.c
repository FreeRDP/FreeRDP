/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2011-2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/tools/makecert.h>

#include "x11_shadow.h"

#ifdef WITH_XDAMAGE

void x11_shadow_xdamage_init(x11ShadowServer* server)
{
	int damage_event;
	int damage_error;
	int major, minor;
	XGCValues values;

	if (XDamageQueryExtension(server->display, &damage_event, &damage_error) == 0)
	{
		fprintf(stderr, "XDamageQueryExtension failed\n");
		return;
	}

	XDamageQueryVersion(server->display, &major, &minor);

	if (XDamageQueryVersion(server->display, &major, &minor) == 0)
	{
		fprintf(stderr, "XDamageQueryVersion failed\n");
		return;
	}
	else if (major < 1)
	{
		fprintf(stderr, "XDamageQueryVersion failed: major:%d minor:%d\n", major, minor);
		return;
	}

	server->xdamage_notify_event = damage_event + XDamageNotify;
	server->xdamage = XDamageCreate(server->display, server->root_window, XDamageReportDeltaRectangles);

	if (server->xdamage == None)
	{
		fprintf(stderr, "XDamageCreate failed\n");
		return;
	}

#ifdef WITH_XFIXES
	server->xdamage_region = XFixesCreateRegion(server->display, NULL, 0);

	if (server->xdamage_region == None)
	{
		fprintf(stderr, "XFixesCreateRegion failed\n");
		XDamageDestroy(server->display, server->xdamage);
		server->xdamage = None;
		return;
	}
#endif

	values.subwindow_mode = IncludeInferiors;
	server->xdamage_gc = XCreateGC(server->display, server->root_window, GCSubwindowMode, &values);
	XSetFunction(server->display, server->xdamage_gc, GXcopy);
}

#endif

int x11_shadow_xshm_init(x11ShadowServer* server)
{
	Bool pixmaps;
	int major, minor;

	if (XShmQueryExtension(server->display) != False)
	{
		XShmQueryVersion(server->display, &major, &minor, &pixmaps);

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

	server->fb_shm_info.shmid = -1;
	server->fb_shm_info.shmaddr = (char*) -1;

	server->fb_image = XShmCreateImage(server->display, server->visual, server->depth,
			ZPixmap, NULL, &(server->fb_shm_info), server->width, server->height);

	if (!server->fb_image)
	{
		fprintf(stderr, "XShmCreateImage failed\n");
		return -1;
	}

	server->fb_shm_info.shmid = shmget(IPC_PRIVATE,
			server->fb_image->bytes_per_line * server->fb_image->height, IPC_CREAT | 0600);

	if (server->fb_shm_info.shmid == -1)
	{
		fprintf(stderr, "shmget failed\n");
		return -1;
	}

	server->fb_shm_info.readOnly = False;
	server->fb_shm_info.shmaddr = shmat(server->fb_shm_info.shmid, 0, 0);
	server->fb_image->data = server->fb_shm_info.shmaddr;

	if (server->fb_shm_info.shmaddr == ((char*) -1))
	{
		fprintf(stderr, "shmat failed\n");
		return -1;
	}

	XShmAttach(server->display, &(server->fb_shm_info));
	XSync(server->display, False);

	shmctl(server->fb_shm_info.shmid, IPC_RMID, 0);

	fprintf(stderr, "display: %p root_window: %p width: %d height: %d depth: %d\n",
			server->display, (void*) server->root_window, server->fb_image->width, server->fb_image->height, server->fb_image->depth);

	server->fb_pixmap = XShmCreatePixmap(server->display,
			server->root_window, server->fb_image->data, &(server->fb_shm_info),
			server->fb_image->width, server->fb_image->height, server->fb_image->depth);

	return 0;
}

void x11_shadow_peer_context_new(freerdp_peer* client, x11ShadowClient* context)
{
	int i;
	int pf_count;
	int vi_count;
	XVisualInfo* vi;
	XVisualInfo* vis;
	XVisualInfo template;
	XPixmapFormatValues* pf;
	XPixmapFormatValues* pfs;
	x11ShadowServer* server;

	server = (x11ShadowServer*) client->context;
	context->server = server;

	/**
	 * Recent X11 servers drop support for shared pixmaps
	 * To see if your X11 server supports shared pixmaps, use:
	 * xdpyinfo -ext MIT-SHM | grep "shared pixmaps"
	 */
	server->use_xshm = TRUE;

	setenv("DISPLAY", ":0", 1); /* Set DISPLAY variable if not already set */

	if (!XInitThreads())
		fprintf(stderr, "warning: XInitThreads() failure\n");

	server->display = XOpenDisplay(NULL);

	if (!server->display)
	{
		fprintf(stderr, "failed to open display: %s\n", XDisplayName(NULL));
		exit(1);
	}

	server->xfds = ConnectionNumber(server->display);
	server->number = DefaultScreen(server->display);
	server->screen = ScreenOfDisplay(server->display, server->number);
	server->depth = DefaultDepthOfScreen(server->screen);
	server->width = WidthOfScreen(server->screen);
	server->height = HeightOfScreen(server->screen);
	server->root_window = DefaultRootWindow(server->display);

	pfs = XListPixmapFormats(server->display, &pf_count);

	if (!pfs)
	{
		fprintf(stderr, "XListPixmapFormats failed\n");
		exit(1);
	}

	for (i = 0; i < pf_count; i++)
	{
		pf = pfs + i;

		if (pf->depth == server->depth)
		{
			server->bpp = pf->bits_per_pixel;
			server->scanline_pad = pf->scanline_pad;
			break;
		}
	}
	XFree(pfs);

	ZeroMemory(&template, sizeof(template));
	template.class = TrueColor;
	template.screen = server->number;

	vis = XGetVisualInfo(server->display, VisualClassMask | VisualScreenMask, &template, &vi_count);

	if (!vis)
	{
		fprintf(stderr, "XGetVisualInfo failed\n");
		exit(1);
	}

	for (i = 0; i < vi_count; i++)
	{
		vi = vis + i;

		if (vi->depth == server->depth)
		{
			server->visual = vi->visual;
			break;
		}
	}
	XFree(vis);

	XSelectInput(server->display, server->root_window, SubstructureNotifyMask);

	if (server->use_xshm)
	{
		if (x11_shadow_xshm_init(server) < 0)
			server->use_xshm = FALSE;
	}

	if (server->use_xshm)
		printf("Using X Shared Memory Extension (XShm)\n");

#ifdef WITH_XDAMAGE
	x11_shadow_xdamage_init(server);
#endif

	x11_shadow_cursor_init(server);

	server->bytesPerPixel = 4;
	server->activePeerCount = 0;

	freerdp_keyboard_init(0);

	context->rfx_context = rfx_context_new(TRUE);
	context->rfx_context->mode = RLGR3;
	context->rfx_context->width = server->width;
	context->rfx_context->height = server->height;

	rfx_context_set_pixel_format(context->rfx_context, RDP_PIXEL_FORMAT_B8G8R8A8);

	context->s = Stream_New(NULL, 65536);
	Stream_Clear(context->s);
}

void x11_shadow_peer_context_free(freerdp_peer* client, x11ShadowClient* context)
{
	x11ShadowServer* server;

	if (context)
	{
		server = context->server;

		if (server->display)
			XCloseDisplay(server->display);

		Stream_Free(context->s, TRUE);
		rfx_context_free(context->rfx_context);
	}
}

void x11_shadow_peer_init(freerdp_peer* client)
{
	client->ContextSize = sizeof(x11ShadowClient);
	client->ContextNew = (psPeerContextNew) x11_shadow_peer_context_new;
	client->ContextFree = (psPeerContextFree) x11_shadow_peer_context_free;
	freerdp_peer_context_new(client);
}

BOOL x11_shadow_peer_capabilities(freerdp_peer* client)
{
	return TRUE;
}

BOOL x11_shadow_peer_post_connect(freerdp_peer* client)
{
	x11ShadowClient* context;
	x11ShadowServer* server;

	context = (x11ShadowClient*) client->context;
	server = context->server;

	fprintf(stderr, "Client %s is activated", client->hostname);
	if (client->settings->AutoLogonEnabled)
	{
		fprintf(stderr, " and wants to login automatically as %s\\%s",
			client->settings->Domain ? client->settings->Domain : "",
			client->settings->Username);
	}
	fprintf(stderr, "\n");

	fprintf(stderr, "Client requested desktop: %dx%dx%d\n",
		client->settings->DesktopWidth, client->settings->DesktopHeight, client->settings->ColorDepth);

	if (!client->settings->RemoteFxCodec)
	{
		fprintf(stderr, "Client does not support RemoteFX\n");
		return FALSE;
	}

	client->settings->DesktopWidth = server->width;
	client->settings->DesktopHeight = server->height;

	client->update->DesktopResize(client->update->context);

	return TRUE;
}

BOOL x11_shadow_peer_activate(freerdp_peer* client)
{
	x11ShadowClient* context = (x11ShadowClient*) client->context;
	x11ShadowServer* server = context->server;

	rfx_context_reset(context->rfx_context);
	context->activated = TRUE;

	server->activePeerCount++;

	context->monitorThread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE) x11_shadow_update_thread, (void*) client, 0, NULL);

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

int x11_shadow_generate_certificate(rdpSettings* settings)
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

static void* x11_shadow_client_thread(void* arg)
{
	int i;
	int fds;
	int max_fds;
	int rcount;
	void* rfds[32];
	fd_set rfds_set;
	rdpSettings* settings;
	x11ShadowClient* xfp;
	struct timeval timeout;
	freerdp_peer* client = (freerdp_peer*) arg;

	ZeroMemory(rfds, sizeof(rfds));
	ZeroMemory(&timeout, sizeof(struct timeval));

	fprintf(stderr, "We've got a client %s\n", client->hostname);

	x11_shadow_peer_init(client);

	xfp = (x11ShadowClient*) client->context;
	settings = client->settings;

	x11_shadow_generate_certificate(settings);

	settings->RemoteFxCodec = TRUE;
	settings->ColorDepth = 32;

	settings->NlaSecurity = FALSE;
	settings->TlsSecurity = TRUE;
	settings->RdpSecurity = FALSE;

	client->Capabilities = x11_shadow_peer_capabilities;
	client->PostConnect = x11_shadow_peer_post_connect;
	client->Activate = x11_shadow_peer_activate;

	x11_shadow_input_register_callbacks(client->input);

	client->Initialize(client);

	while (1)
	{
		rcount = 0;

		if (client->GetFileDescriptor(client, rfds, &rcount) != TRUE)
		{
			fprintf(stderr, "Failed to get FreeRDP file descriptor\n");
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
			//fprintf(stderr, "Failed to check freerdp file descriptor\n");
			//break;
		}
	}

	fprintf(stderr, "Client %s disconnected.\n", client->hostname);

	client->Disconnect(client);
	
	freerdp_peer_context_free(client);
	freerdp_peer_free(client);

	ExitThread(0);

	return NULL;
}

void x11_shadow_peer_accepted(freerdp_listener* instance, freerdp_peer* client)
{
	HANDLE thread;

	thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)
			x11_shadow_client_thread, client, 0, NULL);
}
