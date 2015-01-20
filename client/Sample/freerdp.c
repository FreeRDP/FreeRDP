/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Test UI
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

#ifndef _WIN32
#include <unistd.h>
#include <pthread.h>
#include <sys/select.h>
#else
#include <winsock2.h>
#include <Windows.h>
#include <ws2tcpip.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <freerdp/freerdp.h>
#include <freerdp/constants.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/client/file.h>
#include <freerdp/client/cmdline.h>
#include <freerdp/client/cliprdr.h>
#include <freerdp/channels/channels.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <freerdp/log.h>

#define TAG CLIENT_TAG("sample")

struct tf_context
{
	rdpContext _p;
};
typedef struct tf_context tfContext;

int tf_context_new(freerdp* instance, rdpContext* context)
{
	context->channels = freerdp_channels_new();
	return 0;
}

void tf_context_free(freerdp* instance, rdpContext* context)
{

}

void tf_begin_paint(rdpContext* context)
{
	rdpGdi* gdi = context->gdi;
	gdi->primary->hdc->hwnd->invalid->null = 1;
}

void tf_end_paint(rdpContext* context)
{
	rdpGdi* gdi = context->gdi;

	if (gdi->primary->hdc->hwnd->invalid->null)
		return;
}

BOOL tf_pre_connect(freerdp* instance)
{
	tfContext* tfc;
	rdpSettings* settings;

	tfc = (tfContext*) instance->context;

	settings = instance->settings;

	settings->OrderSupport[NEG_DSTBLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_PATBLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_SCRBLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_OPAQUE_RECT_INDEX] = TRUE;
	settings->OrderSupport[NEG_DRAWNINEGRID_INDEX] = TRUE;
	settings->OrderSupport[NEG_MULTIDSTBLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_MULTIPATBLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_MULTISCRBLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_MULTIOPAQUERECT_INDEX] = TRUE;
	settings->OrderSupport[NEG_MULTI_DRAWNINEGRID_INDEX] = TRUE;
	settings->OrderSupport[NEG_LINETO_INDEX] = TRUE;
	settings->OrderSupport[NEG_POLYLINE_INDEX] = TRUE;
	settings->OrderSupport[NEG_MEMBLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_MEM3BLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_SAVEBITMAP_INDEX] = TRUE;
	settings->OrderSupport[NEG_GLYPH_INDEX_INDEX] = TRUE;
	settings->OrderSupport[NEG_FAST_INDEX_INDEX] = TRUE;
	settings->OrderSupport[NEG_FAST_GLYPH_INDEX] = TRUE;
	settings->OrderSupport[NEG_POLYGON_SC_INDEX] = TRUE;
	settings->OrderSupport[NEG_POLYGON_CB_INDEX] = TRUE;
	settings->OrderSupport[NEG_ELLIPSE_SC_INDEX] = TRUE;
	settings->OrderSupport[NEG_ELLIPSE_CB_INDEX] = TRUE;

	freerdp_channels_pre_connect(instance->context->channels, instance);

	return TRUE;
}

BOOL tf_post_connect(freerdp* instance)
{
	rdpGdi* gdi;

	gdi_init(instance, CLRCONV_ALPHA | CLRCONV_INVERT | CLRBUF_16BPP | CLRBUF_32BPP, NULL);
	gdi = instance->context->gdi;

	instance->update->BeginPaint = tf_begin_paint;
	instance->update->EndPaint = tf_end_paint;

	freerdp_channels_post_connect(instance->context->channels, instance);

	return TRUE;
}

int tfreerdp_run(freerdp* instance)
{
	int i;
	int fds;
	int max_fds;
	int rcount;
	int wcount;
	void* rfds[32];
	void* wfds[32];
	fd_set rfds_set;
	fd_set wfds_set;
	rdpChannels* channels;

	channels = instance->context->channels;

	freerdp_connect(instance);

	while (1)
	{
		rcount = 0;
		wcount = 0;

		ZeroMemory(rfds, sizeof(rfds));
		ZeroMemory(wfds, sizeof(wfds));

		if (!freerdp_get_fds(instance, rfds, &rcount, wfds, &wcount))
		{
			WLog_ERR(TAG, "Failed to get FreeRDP file descriptor");
			break;
		}

		if (!freerdp_channels_get_fds(channels, instance, rfds, &rcount, wfds, &wcount))
		{
			WLog_ERR(TAG, "Failed to get channel manager file descriptor");
			break;
		}

		max_fds = 0;
		FD_ZERO(&rfds_set);
		FD_ZERO(&wfds_set);

		for (i = 0; i < rcount; i++)
		{
			fds = (int)(long)(rfds[i]);

			if (fds > max_fds)
				max_fds = fds;

			FD_SET(fds, &rfds_set);
		}

		if (max_fds == 0)
			break;

		if (select(max_fds + 1, &rfds_set, &wfds_set, NULL, NULL) == -1)
		{
			/* these are not really errors */
			if (!((errno == EAGAIN) ||
				(errno == EWOULDBLOCK) ||
				(errno == EINPROGRESS) ||
				(errno == EINTR))) /* signal occurred */
			{
				WLog_ERR(TAG, "tfreerdp_run: select failed");
				break;
			}
		}

		if (!freerdp_check_fds(instance))
		{
			WLog_ERR(TAG, "Failed to check FreeRDP file descriptor");
			break;
		}

		if (!freerdp_channels_check_fds(channels, instance))
		{
			WLog_ERR(TAG, "Failed to check channel manager file descriptor");
			break;
		}
	}

	freerdp_channels_disconnect(channels, instance);
	freerdp_disconnect(instance);

	freerdp_channels_close(channels, instance);
	freerdp_channels_free(channels);
	freerdp_free(instance);

	return 0;
}

void* tf_client_thread_proc(freerdp* instance)
{
	int i;
	int fds;
	int max_fds;
	int rcount;
	int wcount;
	void* rfds[32];
	void* wfds[32];
	fd_set rfds_set;
	fd_set wfds_set;
	rdpChannels* channels;

	channels = instance->context->channels;

	freerdp_connect(instance);

	while (1)
	{
		rcount = 0;
		wcount = 0;

		ZeroMemory(rfds, sizeof(rfds));
		ZeroMemory(wfds, sizeof(wfds));

		if (!freerdp_get_fds(instance, rfds, &rcount, wfds, &wcount))
		{
			WLog_ERR(TAG, "Failed to get FreeRDP file descriptor");
			break;
		}

		if (!freerdp_channels_get_fds(channels, instance, rfds, &rcount, wfds, &wcount))
		{
			WLog_ERR(TAG, "Failed to get channel manager file descriptor");
			break;
		}

		max_fds = 0;
		FD_ZERO(&rfds_set);
		FD_ZERO(&wfds_set);

		for (i = 0; i < rcount; i++)
		{
			fds = (int)(long)(rfds[i]);

			if (fds > max_fds)
				max_fds = fds;

			FD_SET(fds, &rfds_set);
		}

		if (max_fds == 0)
			break;

		if (select(max_fds + 1, &rfds_set, &wfds_set, NULL, NULL) == -1)
		{
			/* these are not really errors */
			if (!((errno == EAGAIN) ||
				(errno == EWOULDBLOCK) ||
				(errno == EINPROGRESS) ||
				(errno == EINTR))) /* signal occurred */
			{
				WLog_ERR(TAG, "tfreerdp_run: select failed");
				break;
			}
		}

		if (!freerdp_check_fds(instance))
		{
			WLog_ERR(TAG, "Failed to check FreeRDP file descriptor");
			break;
		}

		if (!freerdp_channels_check_fds(channels, instance))
		{
			WLog_ERR(TAG, "Failed to check channel manager file descriptor");
			break;
		}
	}

	freerdp_channels_close(channels, instance);
	freerdp_channels_free(channels);
	freerdp_free(instance);

	ExitThread(0);
	return NULL;
}

int main(int argc, char* argv[])
{
	int status;
	HANDLE thread;
	freerdp* instance;
	rdpChannels* channels;

	instance = freerdp_new();
	instance->PreConnect = tf_pre_connect;
	instance->PostConnect = tf_post_connect;

	instance->ContextSize = sizeof(tfContext);
	instance->ContextNew = tf_context_new;
	instance->ContextFree = tf_context_free;
	freerdp_context_new(instance);

	channels = instance->context->channels;

	status = freerdp_client_settings_parse_command_line(instance->settings, argc, argv);

	if (status < 0)
	{
		exit(0);
	}

	freerdp_client_load_addins(instance->context->channels, instance->settings);

	thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)
			tf_client_thread_proc, instance, 0, NULL);

	WaitForSingleObject(thread, INFINITE);

	return 0;
}
