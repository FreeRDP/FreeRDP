/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include <winpr/crt.h>
#include <winpr/synch.h>

#include <freerdp/constants.h>
#include <freerdp/utils/tcp.h>
#include <freerdp/server/rdpsnd.h>

#include "sf_audin.h"
#include "sf_rdpsnd.h"

#include "sfreerdp.h"

#define SAMPLE_SERVER_USE_CLIENT_RESOLUTION 1
#define SAMPLE_SERVER_DEFAULT_WIDTH 1024
#define SAMPLE_SERVER_DEFAULT_HEIGHT 768

static char* test_pcap_file = NULL;
static BOOL test_dump_rfx_realtime = TRUE;

void test_peer_context_new(freerdp_peer* client, testPeerContext* context)
{
	context->rfx_context = rfx_context_new(TRUE);
	context->rfx_context->mode = RLGR3;
	context->rfx_context->width = SAMPLE_SERVER_DEFAULT_WIDTH;
	context->rfx_context->height = SAMPLE_SERVER_DEFAULT_HEIGHT;
	rfx_context_set_pixel_format(context->rfx_context, RDP_PIXEL_FORMAT_R8G8B8);

	context->nsc_context = nsc_context_new();
	nsc_context_set_pixel_format(context->nsc_context, RDP_PIXEL_FORMAT_R8G8B8);

	context->s = Stream_New(NULL, 65536);

	context->icon_x = -1;
	context->icon_y = -1;

	context->vcm = WTSCreateVirtualChannelManager(client);
}

void test_peer_context_free(freerdp_peer* client, testPeerContext* context)
{
	if (context)
	{
		if (context->debug_channel_thread)
		{
			SetEvent(context->stopEvent);
			WaitForSingleObject(context->debug_channel_thread, INFINITE);
			CloseHandle(context->debug_channel_thread);
		}

		Stream_Free(context->s, TRUE);
		free(context->icon_data);
		free(context->bg_data);

		rfx_context_free(context->rfx_context);
		nsc_context_free(context->nsc_context);

		if (context->debug_channel)
			WTSVirtualChannelClose(context->debug_channel);

		if (context->audin)
			audin_server_context_free(context->audin);

		if (context->rdpsnd)
			rdpsnd_server_context_free(context->rdpsnd);

		WTSDestroyVirtualChannelManager(context->vcm);
	}
}

static void test_peer_init(freerdp_peer* client)
{
	client->ContextSize = sizeof(testPeerContext);
	client->ContextNew = (psPeerContextNew) test_peer_context_new;
	client->ContextFree = (psPeerContextFree) test_peer_context_free;
	freerdp_peer_context_new(client);
}

static wStream* test_peer_stream_init(testPeerContext* context)
{
	Stream_Clear(context->s);
	Stream_SetPosition(context->s, 0);
	return context->s;
}

static void test_peer_begin_frame(freerdp_peer* client)
{
	rdpUpdate* update = client->update;
	SURFACE_FRAME_MARKER* fm = &update->surface_frame_marker;
	testPeerContext* context = (testPeerContext*) client->context;

	fm->frameAction = SURFACECMD_FRAMEACTION_BEGIN;
	fm->frameId = context->frame_id;
	update->SurfaceFrameMarker(update->context, fm);
}

static void test_peer_end_frame(freerdp_peer* client)
{
	rdpUpdate* update = client->update;
	SURFACE_FRAME_MARKER* fm = &update->surface_frame_marker;
	testPeerContext* context = (testPeerContext*) client->context;

	fm->frameAction = SURFACECMD_FRAMEACTION_END;
	fm->frameId = context->frame_id;
	update->SurfaceFrameMarker(update->context, fm);

	context->frame_id++;
}

static void test_peer_draw_background(freerdp_peer* client)
{
	int size;
	wStream* s;
	RFX_RECT rect;
	BYTE* rgb_data;
	rdpUpdate* update = client->update;
	SURFACE_BITS_COMMAND* cmd = &update->surface_bits_command;
	testPeerContext* context = (testPeerContext*) client->context;

	if (!client->settings->RemoteFxCodec && !client->settings->NSCodec)
		return;

	test_peer_begin_frame(client);

	s = test_peer_stream_init(context);

	rect.x = 0;
	rect.y = 0;
	rect.width = client->settings->DesktopWidth;
	rect.height = client->settings->DesktopHeight;

	size = rect.width * rect.height * 3;
	rgb_data = malloc(size);
	memset(rgb_data, 0xA0, size);

	if (client->settings->RemoteFxCodec)
	{
		rfx_compose_message(context->rfx_context, s,
			&rect, 1, rgb_data, rect.width, rect.height, rect.width * 3);
		cmd->codecID = client->settings->RemoteFxCodecId;
	}
	else
	{
		nsc_compose_message(context->nsc_context, s,
			rgb_data, rect.width, rect.height, rect.width * 3);
		cmd->codecID = client->settings->NSCodecId;
	}

	cmd->destLeft = 0;
	cmd->destTop = 0;
	cmd->destRight = rect.width;
	cmd->destBottom = rect.height;
	cmd->bpp = 32;
	cmd->width = rect.width;
	cmd->height = rect.height;
	cmd->bitmapDataLength = Stream_GetPosition(s);
	cmd->bitmapData = Stream_Buffer(s);
	update->SurfaceBits(update->context, cmd);

	free(rgb_data);

	test_peer_end_frame(client);
}

static void test_peer_load_icon(freerdp_peer* client)
{
	testPeerContext* context = (testPeerContext*) client->context;
	FILE* fp;
	int i;
	char line[50];
	BYTE* rgb_data;
	int c;

	if (!client->settings->RemoteFxCodec && !client->settings->NSCodec)
		return;

	if ((fp = fopen("test_icon.ppm", "r")) == NULL)
		return;

	/* P3 */
	fgets(line, sizeof(line), fp);
	/* Creater comment */
	fgets(line, sizeof(line), fp);
	/* width height */
	fgets(line, sizeof(line), fp);
	sscanf(line, "%d %d", &context->icon_width, &context->icon_height);
	/* Max */
	fgets(line, sizeof(line), fp);

	rgb_data = malloc(context->icon_width * context->icon_height * 3);

	for (i = 0; i < context->icon_width * context->icon_height * 3; i++)
	{
		if (fgets(line, sizeof(line), fp))
		{
			sscanf(line, "%d", &c);
			rgb_data[i] = (BYTE)c;
		}
	}

	context->icon_data = rgb_data;

	/* background with same size, which will be used to erase the icon from old position */
	context->bg_data = malloc(context->icon_width * context->icon_height * 3);
	memset(context->bg_data, 0xA0, context->icon_width * context->icon_height * 3);
}

static void test_peer_draw_icon(freerdp_peer* client, int x, int y)
{
	wStream* s;
	RFX_RECT rect;
	rdpUpdate* update = client->update;
	SURFACE_BITS_COMMAND* cmd = &update->surface_bits_command;
	testPeerContext* context = (testPeerContext*) client->context;

	if (client->update->dump_rfx)
		return;

	if (!context)
		return;

	if (context->icon_width < 1 || !context->activated)
		return;

	test_peer_begin_frame(client);

	rect.x = 0;
	rect.y = 0;
	rect.width = context->icon_width;
	rect.height = context->icon_height;

	if (context->icon_x >= 0)
	{
		s = test_peer_stream_init(context);
		if (client->settings->RemoteFxCodec)
		{
			rfx_compose_message(context->rfx_context, s,
				&rect, 1, context->bg_data, rect.width, rect.height, rect.width * 3);
			cmd->codecID = client->settings->RemoteFxCodecId;
		}
		else
		{
			nsc_compose_message(context->nsc_context, s,
				context->bg_data, rect.width, rect.height, rect.width * 3);
			cmd->codecID = client->settings->NSCodecId;
		}

		cmd->destLeft = context->icon_x;
		cmd->destTop = context->icon_y;
		cmd->destRight = context->icon_x + context->icon_width;
		cmd->destBottom = context->icon_y + context->icon_height;
		cmd->bpp = 32;
		cmd->width = context->icon_width;
		cmd->height = context->icon_height;
		cmd->bitmapDataLength = Stream_GetPosition(s);
		cmd->bitmapData = Stream_Buffer(s);
		update->SurfaceBits(update->context, cmd);
	}

	s = test_peer_stream_init(context);

	if (client->settings->RemoteFxCodec)
	{
		rfx_compose_message(context->rfx_context, s,
			&rect, 1, context->icon_data, rect.width, rect.height, rect.width * 3);
		cmd->codecID = client->settings->RemoteFxCodecId;
	}
	else
	{
		nsc_compose_message(context->nsc_context, s,
			context->icon_data, rect.width, rect.height, rect.width * 3);
		cmd->codecID = client->settings->NSCodecId;
	}

	cmd->destLeft = x;
	cmd->destTop = y;
	cmd->destRight = x + context->icon_width;
	cmd->destBottom = y + context->icon_height;
	cmd->bpp = 32;
	cmd->width = context->icon_width;
	cmd->height = context->icon_height;
	cmd->bitmapDataLength = Stream_GetPosition(s);
	cmd->bitmapData = Stream_Buffer(s);
	update->SurfaceBits(update->context, cmd);

	context->icon_x = x;
	context->icon_y = y;

	test_peer_end_frame(client);
}

static BOOL test_sleep_tsdiff(UINT32 *old_sec, UINT32 *old_usec, UINT32 new_sec, UINT32 new_usec)
{
	INT32 sec, usec;

	if (*old_sec==0 && *old_usec==0)
	{
		*old_sec = new_sec;
		*old_usec = new_usec;
		return TRUE;
	}

	sec = new_sec - *old_sec;
	usec = new_usec - *old_usec;

	if (sec<0 || (sec==0 && usec<0))
	{
		printf("Invalid time stamp detected.\n");
		return FALSE;
	}

	*old_sec = new_sec;
	*old_usec = new_usec;
	
	while (usec < 0) 
	{
		usec += 1000000;
		sec--;
	}
	
	if (sec > 0)
		Sleep(sec * 1000);
	
	if (usec > 0)
		USleep(usec);
	
	return TRUE;
}

void tf_peer_dump_rfx(freerdp_peer* client)
{
	wStream* s;
	UINT32 prev_seconds;
	UINT32 prev_useconds;
	rdpUpdate* update;
	rdpPcap* pcap_rfx;
	pcap_record record;

	s = Stream_New(NULL, 512);
	update = client->update;
	client->update->pcap_rfx = pcap_open(test_pcap_file, FALSE);
	pcap_rfx = client->update->pcap_rfx;

	if (pcap_rfx == NULL)
		return;

	prev_seconds = prev_useconds = 0;

	while (pcap_has_next_record(pcap_rfx))
	{
		pcap_get_next_record_header(pcap_rfx, &record);

		Stream_Buffer(s) = realloc(Stream_Buffer(s), record.length);
		record.data = Stream_Buffer(s);
		Stream_Capacity(s) = record.length;

		pcap_get_next_record_content(pcap_rfx, &record);
		Stream_Pointer(s) = Stream_Buffer(s) + Stream_Capacity(s);

		if (test_dump_rfx_realtime && test_sleep_tsdiff(&prev_seconds, &prev_useconds, record.header.ts_sec, record.header.ts_usec) == FALSE)
			break;

		update->SurfaceCommand(update->context, s);
	}
}

static void* tf_debug_channel_thread_func(void* arg)
{
	void* fd;
	wStream* s;
	void* buffer;
	DWORD BytesReturned = 0;
	testPeerContext* context = (testPeerContext*) arg;

	if (WTSVirtualChannelQuery(context->debug_channel, WTSVirtualFileHandle, &buffer, &BytesReturned) == TRUE)
	{
		fd = *((void**) buffer);
		WTSFreeMemory(buffer);

		context->event = CreateWaitObjectEvent(NULL, TRUE, FALSE, fd);
	}

	s = Stream_New(NULL, 4096);

	WTSVirtualChannelWrite(context->debug_channel, (PCHAR) "test1", 5, NULL);

	while (1)
	{
		WaitForSingleObject(context->event, INFINITE);

		if (WaitForSingleObject(context->stopEvent, 0) == WAIT_OBJECT_0)
			break;

		Stream_SetPosition(s, 0);

		if (WTSVirtualChannelRead(context->debug_channel, 0, (PCHAR) Stream_Buffer(s),
			Stream_Capacity(s), &BytesReturned) == FALSE)
		{
			if (BytesReturned == 0)
				break;

			Stream_EnsureRemainingCapacity(s, BytesReturned);

			if (WTSVirtualChannelRead(context->debug_channel, 0, (PCHAR) Stream_Buffer(s),
				Stream_Capacity(s), &BytesReturned) == FALSE)
			{
				/* should not happen */
				break;
			}
		}

		Stream_SetPosition(s, BytesReturned);

		printf("got %d bytes\n", BytesReturned);
	}

	Stream_Free(s, TRUE);

	return 0;
}

BOOL tf_peer_post_connect(freerdp_peer* client)
{
	int i;
	testPeerContext* context = (testPeerContext*) client->context;

	/**
	 * This callback is called when the entire connection sequence is done, i.e. we've received the
	 * Font List PDU from the client and sent out the Font Map PDU.
	 * The server may start sending graphics output and receiving keyboard/mouse input after this
	 * callback returns.
	 */

	printf("Client %s is activated (osMajorType %d osMinorType %d)", client->local ? "(local)" : client->hostname,
			client->settings->OsMajorType, client->settings->OsMinorType);

	if (client->settings->AutoLogonEnabled)
	{
		printf(" and wants to login automatically as %s\\%s",
			client->settings->Domain ? client->settings->Domain : "",
			client->settings->Username);

		/* A real server may perform OS login here if NLA is not executed previously. */
	}
	printf("\n");

	printf("Client requested desktop: %dx%dx%d\n",
		client->settings->DesktopWidth, client->settings->DesktopHeight, client->settings->ColorDepth);

#if (SAMPLE_SERVER_USE_CLIENT_RESOLUTION == 1)
	context->rfx_context->width = client->settings->DesktopWidth;
	context->rfx_context->height = client->settings->DesktopHeight;
	printf("Using resolution requested by client.\n");
#else
	client->settings->DesktopWidth = context->rfx_context->width;
	client->settings->DesktopHeight = context->rfx_context->height;
	printf("Resizing client to %dx%d\n", client->settings->DesktopWidth, client->settings->DesktopHeight);
	client->update->DesktopResize(client->update->context);
#endif

	/* A real server should tag the peer as activated here and start sending updates in main loop. */
	test_peer_load_icon(client);

	/* Iterate all channel names requested by the client and activate those supported by the server */

	for (i = 0; i < client->settings->ChannelCount; i++)
	{
		if (client->settings->ChannelDefArray[i].joined)
		{
			if (strncmp(client->settings->ChannelDefArray[i].Name, "rdpdbg", 6) == 0)
			{
				context->debug_channel = WTSVirtualChannelManagerOpenEx(context->vcm, "rdpdbg", 0);

				if (context->debug_channel != NULL)
				{
					printf("Open channel rdpdbg.\n");

					context->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

					context->debug_channel_thread = CreateThread(NULL, 0,
							(LPTHREAD_START_ROUTINE) tf_debug_channel_thread_func, (void*) context, 0, NULL);
				}
			}
			else if (strncmp(client->settings->ChannelDefArray[i].Name, "rdpsnd", 6) == 0)
			{
				sf_peer_rdpsnd_init(context); /* Audio Output */
			}
		}
	}

	/* Dynamic Virtual Channels */

	sf_peer_audin_init(context); /* Audio Input */

	/* Return FALSE here would stop the execution of the peer main loop. */

	return TRUE;
}

BOOL tf_peer_activate(freerdp_peer* client)
{
	testPeerContext* context = (testPeerContext*) client->context;

	rfx_context_reset(context->rfx_context);
	context->activated = TRUE;

	if (test_pcap_file != NULL)
	{
		client->update->dump_rfx = TRUE;
		tf_peer_dump_rfx(client);
	}
	else
	{
		test_peer_draw_background(client);
	}

	return TRUE;
}

void tf_peer_synchronize_event(rdpInput* input, UINT32 flags)
{
	printf("Client sent a synchronize event (flags:0x%X)\n", flags);
}

void tf_peer_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code)
{
	freerdp_peer* client = input->context->peer;
	rdpUpdate* update = client->update;
	testPeerContext* context = (testPeerContext*) input->context;

	printf("Client sent a keyboard event (flags:0x%X code:0x%X)\n", flags, code);

	if ((flags & 0x4000) && code == 0x22) /* 'g' key */
	{
		if (client->settings->DesktopWidth != 800)
		{
			client->settings->DesktopWidth = 800;
			client->settings->DesktopHeight = 600;
		}
		else
		{
			client->settings->DesktopWidth = SAMPLE_SERVER_DEFAULT_WIDTH;
			client->settings->DesktopHeight = SAMPLE_SERVER_DEFAULT_HEIGHT;
		}
		context->rfx_context->width = client->settings->DesktopWidth;
		context->rfx_context->height = client->settings->DesktopHeight;
		update->DesktopResize(update->context);
		context->activated = FALSE;
	}
	else if ((flags & 0x4000) && code == 0x2E) /* 'c' key */
	{
		if (context->debug_channel)
		{
			WTSVirtualChannelWrite(context->debug_channel, (PCHAR) "test2", 5, NULL);
		}
	}
	else if ((flags & 0x4000) && code == 0x2D) /* 'x' key */
	{
		client->Close(client);
	}
	else if ((flags & 0x4000) && code == 0x13) /* 'r' key */
	{
		if (!context->audin_open)
		{
			context->audin->Open(context->audin);
			context->audin_open = TRUE;
		}
		else
		{
			context->audin->Close(context->audin);
			context->audin_open = FALSE;
		}
	}
	else if ((flags & 0x4000) && code == 0x1F) /* 's' key */
	{

	}
}

void tf_peer_unicode_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code)
{
	printf("Client sent a unicode keyboard event (flags:0x%X code:0x%X)\n", flags, code);
}

void tf_peer_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	//printf("Client sent a mouse event (flags:0x%X pos:%d,%d)\n", flags, x, y);

	test_peer_draw_icon(input->context->peer, x + 10, y);
}

void tf_peer_extended_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	//printf("Client sent an extended mouse event (flags:0x%X pos:%d,%d)\n", flags, x, y);
}

static void tf_peer_refresh_rect(rdpContext* context, BYTE count, RECTANGLE_16* areas)
{
	BYTE i;

	printf("Client requested to refresh:\n");

	for (i = 0; i < count; i++)
	{
		printf("  (%d, %d) (%d, %d)\n", areas[i].left, areas[i].top, areas[i].right, areas[i].bottom);
	}
}

static void tf_peer_suppress_output(rdpContext* context, BYTE allow, RECTANGLE_16* area)
{
	if (allow > 0)
	{
		printf("Client restore output (%d, %d) (%d, %d).\n", area->left, area->top, area->right, area->bottom);
	}
	else
	{
		printf("Client minimized and suppress output.\n");
	}
}

static void* test_peer_mainloop(void* arg)
{
	int i;
	int fds;
	int max_fds;
	int rcount;
	void* rfds[32];
	fd_set rfds_set;
	testPeerContext* context;
	freerdp_peer* client = (freerdp_peer*) arg;

	test_peer_init(client);

	/* Initialize the real server settings here */
	client->settings->CertificateFile = _strdup("server.crt");
	client->settings->PrivateKeyFile = _strdup("server.key");
	client->settings->NlaSecurity = FALSE;
	client->settings->RemoteFxCodec = TRUE;
	client->settings->ColorDepth = 32;
	client->settings->SuppressOutput = TRUE;
	client->settings->RefreshRect = TRUE;

	client->PostConnect = tf_peer_post_connect;
	client->Activate = tf_peer_activate;

	client->input->SynchronizeEvent = tf_peer_synchronize_event;
	client->input->KeyboardEvent = tf_peer_keyboard_event;
	client->input->UnicodeKeyboardEvent = tf_peer_unicode_keyboard_event;
	client->input->MouseEvent = tf_peer_mouse_event;
	client->input->ExtendedMouseEvent = tf_peer_extended_mouse_event;

	client->update->RefreshRect = tf_peer_refresh_rect;
	client->update->SuppressOutput = tf_peer_suppress_output;

	client->settings->MultifragMaxRequestSize = 0xFFFFFF; /* FIXME */

	client->Initialize(client);
	context = (testPeerContext*) client->context;

	printf("We've got a client %s\n", client->local ? "(local)" : client->hostname);

	while (1)
	{
		rcount = 0;

		memset(rfds, 0, sizeof(rfds));
		if (client->GetFileDescriptor(client, rfds, &rcount) != TRUE)
		{
			printf("Failed to get FreeRDP file descriptor\n");
			break;
		}

#ifndef _WIN32
		/* winsock's select() only works with sockets ! */
		WTSVirtualChannelManagerGetFileDescriptor(context->vcm, rfds, &rcount);
#endif

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
#ifdef _WIN32
			/* error codes set by windows sockets are not made available through errno ! */
			int wsa_error = WSAGetLastError();
			if (!((wsa_error == WSAEWOULDBLOCK) ||
				(wsa_error == WSAEINPROGRESS) ||
				(wsa_error == WSAEINTR)))
			{
				printf("select failed (WSAGetLastError: %d)\n", wsa_error);
				break;
			}
#else
			/* these are not really errors */
			if (!((errno == EAGAIN) ||
				(errno == EWOULDBLOCK) ||
				(errno == EINPROGRESS) ||
				(errno == EINTR))) /* signal occurred */
			{
				printf("select failed (errno: %d)\n", errno);
				break;
			}
#endif
		}

		if (client->CheckFileDescriptor(client) != TRUE)
			break;

		if (WTSVirtualChannelManagerCheckFileDescriptor(context->vcm) != TRUE)
			break;
	}

	printf("Client %s disconnected.\n", client->local ? "(local)" : client->hostname);

	client->Disconnect(client);
	freerdp_peer_context_free(client);
	freerdp_peer_free(client);

	return NULL;
}

static void test_peer_accepted(freerdp_listener* instance, freerdp_peer* client)
{
	HANDLE hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) test_peer_mainloop, (void*) client, 0, NULL);
	if (hThread != NULL) {
		CloseHandle(hThread);
	}
}

static void test_server_mainloop(freerdp_listener* instance)
{
	int i;
	int fds;
	int max_fds;
	int rcount;
	void* rfds[32];
	fd_set rfds_set;

	while (1)
	{
		rcount = 0;

		memset(rfds, 0, sizeof(rfds));
		if (instance->GetFileDescriptor(instance, rfds, &rcount) != TRUE)
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

		if (instance->CheckFileDescriptor(instance) != TRUE)
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

	instance = freerdp_listener_new();

	instance->PeerAccepted = test_peer_accepted;

	if (argc > 1)
		test_pcap_file = argv[1];
	
	if (argc > 2 && !strcmp(argv[2], "--fast"))
		test_dump_rfx_realtime = FALSE;

	/* Open the server socket and start listening. */
	freerdp_wsa_startup();
	if (instance->Open(instance, NULL, 3389) &&
		instance->OpenLocal(instance, "/tmp/tfreerdp-server.0"))
	{
		/* Entering the server main loop. In a real server the listener can be run in its own thread. */
		test_server_mainloop(instance);
	}
	freerdp_wsa_cleanup();

	freerdp_listener_free(instance);

	return 0;
}

