/**
 * FreeRDP: A Remote Desktop Protocol Client
 * FreeRDP X11 Server
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
#include <signal.h>
#include <sys/select.h>
#include <freerdp/utils/memory.h>

#include "xf_peer.h"
#include "xfreerdp.h"

char* xf_pcap_file = NULL;

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
		printf("failed to open display: %s\n", XDisplayName(NULL));

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

	return xfi;
}

void xf_server_main_loop(freerdp_listener* instance)
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

	/* ignore SIGPIPE, otherwise an SSL_write failure could crash the server */
	signal(SIGPIPE, SIG_IGN);

	instance = freerdp_listener_new();
	instance->info = (void*) xf_info_init();
	instance->PeerAccepted = xf_peer_accepted;

	if (argc > 1)
		xf_pcap_file = argv[1];

	/* Open the server socket and start listening. */
	if (instance->Open(instance, NULL, 3389))
	{
		/* Entering the server main loop. In a real server the listener can be run in its own thread. */
		xf_server_main_loop(instance);
	}

	freerdp_listener_free(instance);

	return 0;
}

